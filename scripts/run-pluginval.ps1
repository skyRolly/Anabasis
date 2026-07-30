# ============================================================================
#  Anabasis -- pluginval validation (Windows). Mirrors scripts/run-pluginval.sh:
#  same strictness + mode arguments, same "3 consecutive passes per mode" gate.
#
#  Usage: pwsh scripts/run-pluginval.ps1 -Strictness 10 -Mode deterministic
#                                        -Mode randomise
#
#  Both modes run 3 CONSECUTIVE passes; ALL must pass. Mirrors run-pluginval.sh's
#  crash-retry policy: a REAL validation assertion (a small, clean exit code) fails
#  the step IMMEDIATELY; an abnormal termination / crash (a large Win32 exception
#  code, a negative code, or no code) is retried, and STILL fails after the retries.
#
#  KEY: pluginval.exe is a GUI-subsystem app, so it must be launched via
#  System.Diagnostics.Process and explicitly WAITED on (Invoke-Pluginval below) to
#  obtain a trustworthy exit code -- the call operator (`& $pv`) returns immediately
#  with a $null $LASTEXITCODE, which both false-GREENS the step (null -> exit 0) and,
#  with a retry loop, false-REDS it while spawning concurrent background validators
#  (garbled interleaved output). The exit code is the only signal, and it is only
#  trustworthy after an explicit wait.
#
#  Network domain needed: github.com (pluginval download).
# ============================================================================
param(
    [int]    $Strictness = 8,
    [string] $Mode       = "deterministic"
)

# --- Setup (download/extract): real errors should stop the script. ----------
$ErrorActionPreference = "Stop"

$root  = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root "build"
$tools = Join-Path $root ".tools"
New-Item -ItemType Directory -Force -Path $tools | Out-Null

$vst3 = Get-ChildItem -Recurse -Path $build -Filter Anabasis.vst3 -Directory | Select-Object -First 1
if (-not $vst3) { Write-Host "Anabasis.vst3 not found -- build first (scripts/build.sh)."; exit 1 }

$pv = Join-Path $tools "pluginval.exe"
if (-not (Test-Path $pv)) {
    Write-Host "Fetching pluginval (pluginval_Windows.zip)..."
    Invoke-WebRequest -Uri "https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_Windows.zip" -OutFile "$tools\pluginval.zip"
    Expand-Archive -Force "$tools\pluginval.zip" -DestinationPath $tools
}

switch ($Mode) {
    "randomise"     { $modeArgs = @("--randomise");        $passes = 3 }
    "deterministic" { $modeArgs = @("--random-seed", "0"); $passes = 3 }
    default         { Write-Host "Unknown mode '$Mode' (expected deterministic|randomise)"; exit 2 }
}

# --- pluginval invocation: the EXIT CODE is the only signal. -----------------
$ErrorActionPreference = "Continue"
$PSNativeCommandUseErrorActionPreference = $false

function Invoke-Pluginval {
    param([string] $Exe, [string[]] $PvArgs)
    $psi = [System.Diagnostics.ProcessStartInfo]::new()
    $psi.FileName = $Exe
    foreach ($a in $PvArgs) { [void] $psi.ArgumentList.Add($a) }
    $psi.UseShellExecute = $false   # inherit console (stream output) AND enable a real .ExitCode
    $proc = [System.Diagnostics.Process]::Start($psi)
    $proc.WaitForExit()             # actually WAIT for the validation to finish
    return $proc.ExitCode
}

# NO skip flags. Windows runs the SAME test set as Linux and macOS, which is what
# `docs/policies/TESTING_POLICY.md` and `docs/procedures/CI_CD.md` claim -- the
# claim and the script must not diverge.
#
# Known hazard for P5 (recorded here so it is not rediscovered from scratch, NOT
# pre-emptively worked around): the sibling product Anamorph passes
# `--skip-gui-tests` on Windows because the GitHub `windows-latest` runner is
# GPU-less and cannot host its editor's "Editor Automation" test -- it fails there
# in both GL mode (the GDI-generic OpenGL 1.1 renderer has no GL2 shader/VBO entry
# points) and CPU mode. That is an environmental limit of the runner, evidenced
# against THAT product's editor (its KI-007), and it may well recur here once
# Anabasis has an editor at P5.
#
# It is deliberately NOT inherited now: Anabasis has no editor, so the flag would
# suppress nothing while quietly contradicting the "uniform and blocking on every
# platform" gate. If P5 reproduces the failure, add the flag THEN, with a
# KNOWN_ISSUES entry recording the measured coverage loss -- a skipped test
# category must always be visible in the documentation, never only in a script.
$guiArgs = @()

# Each pass gets up to $attempts tries against the REAL exit code: 0 is a pass; a
# small non-zero (1..255) is a real validation failure and fails the step
# immediately; a null / negative / >=256 code is an abnormal termination (Win32
# exception) and is retried, then still fails after the retries.
$pvArgs = @('--strictness-level', "$Strictness") + $modeArgs + $guiArgs + @('--validate', $vst3.FullName, '--timeout-ms', '600000')
Write-Host "Validating $($vst3.FullName) at strictness $Strictness -- mode=$Mode ($passes consecutive pass(es) required)"
$attempts = 3
for ($p = 1; $p -le $passes; $p++) {
    $passed = $false
    for ($a = 1; $a -le $attempts; $a++) {
        $rc = Invoke-Pluginval -Exe $pv -PvArgs $pvArgs
        if ($rc -eq 0) {
            Write-Host "pluginval: PASSED ($Mode pass $p/$passes) at strictness $Strictness (attempt $a/$attempts)"
            $passed = $true
            break
        }
        # $null MUST be tested first: `$null -lt 0` and `$null -ge 256` are both $false,
        # so without this a null code would fall through to the "real failure" branch.
        $crashed = ($null -eq $rc) -or ($rc -lt 0) -or ($rc -ge 256)
        if (-not $crashed) {
            Write-Host "pluginval: FAILED ($Mode pass $p/$passes) at strictness $Strictness (exit $rc) -- real validation failure, not a crash."
            exit $rc
        }
        $shown = if ($null -eq $rc) { 'none (abnormal termination)' } else { $rc }
        Write-Host "pluginval: crashed ($Mode pass $p/$passes, exit $shown -- abnormal termination). Retry $a/$attempts."
    }
    if (-not $passed) {
        Write-Host "pluginval: still crashing ($Mode pass $p/$passes) after $attempts attempts -- treating as a failure."
        exit 1
    }
}
Write-Host "pluginval: ALL $passes $Mode pass(es) succeeded at strictness $Strictness"
exit 0
