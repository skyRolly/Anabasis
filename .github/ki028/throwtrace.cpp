// KI-028 diagnostic interposer (macOS only; DIAGNOSTIC -- never linked into a product).
//
// Loaded into pluginval with DYLD_INSERT_LIBRARIES. It does four things:
//
//   1. Interposes __cxa_throw. When the thrown type's name contains
//      "bad_function_call" (or every throw, with KI028_TRACE_ALL=1), it writes the
//      throwing thread's identity and backtrace to stderr and to $KI028_TRACE_FILE,
//      one line per frame, with each frame's image path and image load address
//      (dladdr), so the frame can be symbolicated afterwards with
//          atos -o <binary or dSYM DWARF> -l <image base> <frame address>
//      It then calls the real __cxa_throw, so the process behaves exactly as before.
//
//   2. Installs a std::terminate handler that prints the terminating thread's
//      backtrace (useful when the throw site was a noexcept frame and the stack has
//      already been partially unwound), then chains to the previous handler so the
//      libc++abi "terminating due to uncaught exception" line is still printed.
//
//   3. With KI028_KEEP_DEFAULT_SIGNALS=1, refuses the signal() calls that pluginval's
//      MAIN EXECUTABLE makes for SIGFPE/SIGILL/SIGSEGV/SIGBUS/SIGABRT
//      (Source/CommandLine.cpp setupSignalHandling -> kill9WithSomeMercy, which turns
//      every crash into std::_Exit(SIGKILL) = exit 9 and so suppresses the macOS
//      crash report). Calls from any other image are passed through untouched.
//
//   4. With KI028_KEEP_DEFAULT_SIGNALS=1, installs its own SIGSEGV/SIGBUS/SIGILL/
//      SIGFPE/SIGABRT handler that prints the faulting thread's backtrace and the
//      fault address, then restores the default action and re-raises, so ReportCrash
//      still writes ~/Library/Logs/DiagnosticReports/pluginval-*.ips with every
//      thread's stack.
//
// Output lines are prefixed "KI028" and are machine-parsed by ki028-diag.yml:
//   KI028 EVENT <kind> pid=<pid> tid=<tid> main=<0|1> thread="<name>" t=<ns> type="<name>" [throw only:
//               tinfo=<p> tinfo_image="<path>" dtor=<p> dtor_image="<path>"
//               base_tinfo=<p> base_image="<path>" std_exception_tinfo=<p> base_is_std_exception=<0|1>]
//   KI028 FRAME tid=<tid> <index> <pc> <image base> <symbol+offset> <image path>
//   KI028 END tid=<tid>
//
// Build:  clang++ -std=c++17 -O1 -g -fno-omit-frame-pointer -dynamiclib \
//                 -o libthrowtrace.dylib throwtrace.cpp
// Use:    DYLD_INSERT_LIBRARIES=/abs/path/libthrowtrace.dylib pluginval ...
//         (pluginval must NOT carry the hardened-runtime flag; a Ninja build of
//          pluginval is ad-hoc signed without it.)

#include <cxxabi.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <fcntl.h>
#include <mach-o/dyld.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <typeinfo>
#include <exception>
#include <cstdint>
#include <initializer_list>
#include <unistd.h>

#define KI028_INTERPOSE(replacement, replacee)                                        \
    __attribute__((used)) static const struct { const void* r; const void* e; }       \
    ki028_interpose_##replacee __attribute__((section("__DATA,__interpose"))) =       \
        { reinterpret_cast<const void*> (&replacement), reinterpret_cast<const void*> (&replacee) };

extern "C" void __cxa_throw (void*, std::type_info*, void (*) (void*)) __attribute__((noreturn));

namespace
{
int traceFd = -1;
bool traceAll = false;
bool keepDefaultSignals = false;
std::terminate_handler previousTerminate = nullptr;
pthread_mutex_t writeLock = PTHREAD_MUTEX_INITIALIZER;

void emit (const char* fmt, ...) __attribute__((format (printf, 1, 2)));

void emit (const char* fmt, ...)
{
    char line[2048];
    va_list args;
    va_start (args, fmt);
    int n = vsnprintf (line, sizeof (line) - 1, fmt, args);
    va_end (args);

    if (n < 0)
        return;

    if (n > (int) sizeof (line) - 2)
        n = (int) sizeof (line) - 2;

    line[n++] = '\n';
    (void) ::write (STDERR_FILENO, line, (size_t) n);

    if (traceFd >= 0)
        (void) ::write (traceFd, line, (size_t) n);
}

unsigned long long nowNs()
{
    struct timespec ts;
    clock_gettime (CLOCK_REALTIME, &ts);
    return (unsigned long long) ts.tv_sec * 1000000000ull + (unsigned long long) ts.tv_nsec;
}

void dumpStack (const char* kind, const char* typeName, int skip, bool fromSignal = false, const char* extra = "")
{
    void* frames[128];
    const int count = backtrace (frames, 128);

    char threadName[64] = {};
    pthread_getname_np (pthread_self(), threadName, sizeof (threadName));
    uint64_t tid = 0;
    pthread_threadid_np (pthread_self(), &tid);

    // A signal can arrive while this thread already holds the lock (a fault inside
    // emit); never block there. Every FRAME line carries the tid, so an unlocked
    // write is still attributable if two threads interleave.
    const bool locked = fromSignal ? pthread_mutex_trylock (&writeLock) == 0
                                   : pthread_mutex_lock (&writeLock) == 0;

    emit ("KI028 EVENT %s pid=%d tid=%llu main=%d thread=\"%s\" t=%llu type=\"%s\" %s",
          kind, (int) getpid(), (unsigned long long) tid, pthread_main_np(), threadName,
          nowNs(), typeName != nullptr ? typeName : "", extra != nullptr ? extra : "");

    for (int i = skip; i < count; ++i)
    {
        Dl_info info;
        memset (&info, 0, sizeof (info));
        const bool found = dladdr (frames[i], &info) != 0;

        const char* image = (found && info.dli_fname != nullptr) ? info.dli_fname : "?";
        const char* sym   = (found && info.dli_sname != nullptr) ? info.dli_sname : "?";
        const unsigned long long off = (found && info.dli_saddr != nullptr)
                                       ? (unsigned long long) ((const char*) frames[i] - (const char*) info.dli_saddr)
                                       : 0ull;

        emit ("KI028 FRAME tid=%llu %d %p %p %s+%llu %s",
              (unsigned long long) tid, i - skip, frames[i], found ? info.dli_fbase : nullptr, sym, off, image);
    }

    emit ("KI028 END tid=%llu", (unsigned long long) tid);

    if (locked)
        pthread_mutex_unlock (&writeLock);
}

const char* demangledTypeName (std::type_info* tinfo, char* buffer, size_t size)
{
    if (tinfo == nullptr)
        return "(null type_info)";

    const char* mangled = tinfo->name();
    int status = 0;

    if (char* d = abi::__cxa_demangle (mangled, nullptr, nullptr, &status))
    {
        snprintf (buffer, size, "%s", d);
        free (d);
        return buffer;
    }

    return mangled;
}

void terminateHandler()
{
    dumpStack ("terminate", nullptr, 1);

    if (previousTerminate != nullptr)
        previousTerminate();

    abort();
}

void faultHandler (int sig, siginfo_t* si, void*)
{
    char label[96];
    snprintf (label, sizeof (label), "signal-%d addr=%p", sig, si != nullptr ? si->si_addr : nullptr);
    dumpStack (label, nullptr, 1, true);

    ::signal (sig, SIG_DFL);
    raise (sig);
}

bool callerIsMainExecutable (const void* returnAddress)
{
    Dl_info info;

    if (dladdr (returnAddress, &info) == 0)
        return false;

    return info.dli_fbase == (const void*) _dyld_get_image_header (0);
}

bool isCrashSignal (int sig)
{
    return sig == SIGFPE || sig == SIGILL || sig == SIGSEGV || sig == SIGBUS || sig == SIGABRT;
}
} // namespace

//==============================================================================
extern "C" __attribute__((noreturn))
void ki028_cxa_throw (void* thrown, std::type_info* tinfo, void (*dest) (void*))
{
    char buffer[256];
    const char* name = demangledTypeName (tinfo, buffer, sizeof (buffer));

    if (traceAll || strstr (name, "bad_function_call") != nullptr)
    {
        // Which image owns the thrown type's RTTI and the exception object's
        // destructor. Without a key function (libc++ ABI v1) every image that
        // throws std::bad_function_call carries its own weak copy of both, so
        // these name the throwing image even if the stack is unreadable -- and
        // they bear on the observed abort line lacking the ": what()" suffix
        // that libc++abi prints when the type is catchable as std::exception.
        Dl_info ti, di, bi;
        memset (&ti, 0, sizeof (ti));
        memset (&di, 0, sizeof (di));
        memset (&bi, 0, sizeof (bi));
        const bool tiFound = tinfo != nullptr && dladdr ((const void*) tinfo, &ti) != 0;
        const bool diFound = dest != nullptr && dladdr ((const void*) dest, &di) != 0;

        // std::bad_function_call derives from std::exception by single public
        // inheritance, so its RTTI is an Itanium __si_class_type_info:
        // { vptr, name, base type_info* }. Its base pointer, compared with the
        // std::exception type_info this (system-libc++) image sees, is what
        // libc++abi's terminate handler effectively tests before it appends
        // ": what()" -- which the recorded abort line lacks.
        const std::type_info* base = nullptr;

        if (tinfo != nullptr && strstr (name, "bad_function_call") != nullptr)
            base = static_cast<const std::type_info*> (reinterpret_cast<const void* const*> (tinfo)[2]);

        const bool biFound = base != nullptr && dladdr ((const void*) base, &bi) != 0;
        const std::type_info* stdException = &typeid (std::exception);

        char extra[1536];
        snprintf (extra, sizeof (extra),
                  "tinfo=%p tinfo_image=\"%s\" dtor=%p dtor_image=\"%s\" "
                  "base_tinfo=%p base_image=\"%s\" std_exception_tinfo=%p base_is_std_exception=%d",
                  (void*) tinfo, tiFound && ti.dli_fname != nullptr ? ti.dli_fname : "?",
                  (void*) dest, diFound && di.dli_fname != nullptr ? di.dli_fname : "?",
                  (const void*) base, biFound && bi.dli_fname != nullptr ? bi.dli_fname : "?",
                  (const void*) stdException, base == stdException ? 1 : 0);
        dumpStack ("throw", name, 1, false, extra);
    }

    // Inside the interposing image, this reaches the real __cxa_throw.
    __cxa_throw (thrown, tinfo, dest);
}

KI028_INTERPOSE (ki028_cxa_throw, __cxa_throw)

extern "C" sig_t ki028_signal (int sig, sig_t handler)
{
    if (keepDefaultSignals && isCrashSignal (sig)
        && callerIsMainExecutable (__builtin_return_address (0)))
    {
        emit ("KI028 NOTE refused signal(%d) from the main executable (KI028_KEEP_DEFAULT_SIGNALS=1)", sig);
        return SIG_DFL;
    }

    return ::signal (sig, handler);
}

KI028_INTERPOSE (ki028_signal, signal)

//==============================================================================
__attribute__((constructor))
static void ki028Init()
{
    if (const char* path = getenv ("KI028_TRACE_FILE"); path != nullptr && *path != '\0')
        traceFd = ::open (path, O_WRONLY | O_CREAT | O_APPEND, 0644);

    if (const char* all = getenv ("KI028_TRACE_ALL"); all != nullptr && all[0] == '1')
        traceAll = true;

    if (const char* keep = getenv ("KI028_KEEP_DEFAULT_SIGNALS"); keep != nullptr && keep[0] == '1')
        keepDefaultSignals = true;

    previousTerminate = std::set_terminate (terminateHandler);

    if (keepDefaultSignals)
    {
        struct sigaction sa;
        memset (&sa, 0, sizeof (sa));
        sa.sa_sigaction = faultHandler;
        sa.sa_flags = SA_SIGINFO | SA_RESETHAND;
        sigemptyset (&sa.sa_mask);

        for (int sig : { SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT })
            sigaction (sig, &sa, nullptr);
    }

    emit ("KI028 LOADED pid=%d trace_all=%d keep_default_signals=%d",
          (int) getpid(), traceAll ? 1 : 0, keepDefaultSignals ? 1 : 0);
}
