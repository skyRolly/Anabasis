#pragma once

// ============================================================================
//  EngineParameters — the POD snapshot that crosses the wrapper→engine
//  boundary (ADR-0001). Built ONCE per block on the audio thread from cached
//  APVTS atomics + the InternalState mirrors (ADR-0011: never read piecemeal
//  mid-block), then adopted by AnabasisEngine::process.
//
//  Plain aggregates only: no JUCE types, no pointers, no methods with state.
//  The DSP core sees the world through this struct and nothing else.
// ============================================================================

namespace anabasis
{

enum class OversampleFactor : int { off = 0, x2, x4, x8, x16 };
enum class OsPhaseMode      : int { minimum = 0, linear };

struct EngineParameters
{
    // -- view / monitor -----------------------------------------------------
    bool  bypass            = false;
    bool  loudnessComp      = false;
    bool  deltaMonitor      = false;

    // -- input --------------------------------------------------------------
    float inputGainDb       = 0.0f;

    // -- detectors ----------------------------------------------------------
    float scHpfFreqHz       = 20.0f;

    // -- compressor (pass-through at P1; carried so the boundary is stable) --
    float compRatio         = 1.5f;
    float compThresholdDb   = 0.0f;
    float compAttackMs      = 30.0f;
    float compReleaseMs     = 200.0f;
    bool  compAutoRelease   = true;
    float compKneeDb        = 6.0f;
    int   compDetector      = 0;      // 0 RMS, 1 Peak
    float compMix           = 1.0f;   // 0..1

    // -- clipper / colour (pass-through at P1) ------------------------------
    float clipShape         = 0.5f;
    float clipDriveDb       = 0.0f;
    float clipMix           = 1.0f;
    int   colourModel       = 1;      // 0 Clean, 1 Tape, 2 Tube, 3 Transistor
    float colourBalance     = 0.0f;
    float colourTone        = 0.0f;
    float colourDepth       = 0.0f;   // 0..1
    float dynTiltDb         = 0.0f;

    // -- limiter ------------------------------------------------------------
    float limGainDb         = 0.0f;
    float lookaheadMs       = 2.0f;   // engaged value; the AUDIO delay stays at the
                                      // full 10 ms allowance regardless (ADR-0004)
    float limReleaseMs      = 100.0f;
    bool  limAutoRelease    = true;
    int   limStyle          = 0;      // 0 Transparent, 1 Punchy, 2 Loud
    float stereoLink        = 1.0f;   // 0..1
    float transientPreserve = 0.5f;   // 0..1
    bool  truePeakMode      = true;

    // -- eq (pass-through at P1) --------------------------------------------
    float eqTiltDb          = 0.0f;
    float eqLowShelfFreqHz  = 100.0f;
    float eqLowShelfGainDb  = 0.0f;
    float eqHighShelfFreqHz = 8000.0f;
    float eqHighShelfGainDb = 0.0f;
    float eqBell1FreqHz     = 300.0f;
    float eqBell1GainDb     = 0.0f;
    float eqBell1Q          = 1.0f;
    float eqBell2FreqHz     = 3000.0f;
    float eqBell2GainDb     = 0.0f;
    float eqBell2Q          = 1.0f;
    int   eqPosition        = 0;      // 0 Pre, 1 Post

    // -- shared / output ----------------------------------------------------
    float ceilingDbTp       = -1.0f;
    int   ditherMode        = 0;      // 0 Off, 1 16-bit, 2 24-bit
    bool  ditherShaping     = false;

    // -- host-hidden engine config (InternalState atomic mirrors, §4.3) -----
    OversampleFactor oversample = OversampleFactor::off;
    OsPhaseMode      osPhase    = OsPhaseMode::minimum;
    bool             forceMaxOffline = false;   // int_offlineQuality == Force Max
    bool             nonRealtime     = false;   // isNonRealtime() at block time
};

} // namespace anabasis
