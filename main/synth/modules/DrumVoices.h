#pragma once

#include <cmath>

#include "synth/dsp/WaveTables.h"
#include "synth/modules/Adsr.h"
#include "synth/modules/Oscillator.h"
#include "synth/modules/Noise.h"
#include "synth/modules/Filter.h"
#include "synth/modules/Shaper.h"

namespace synth::modules {

struct Kick {
    float sampleRate = 44100.0f;
    float phase = 0.0f;
    float freq = 60.0f;
    float pitchEnv = 0.0f;
    Adsr env;
    Drive drive;

    void setSampleRate(float sr) noexcept { sampleRate = sr; }
    void trigger(float vel) noexcept {
        env.attack_s = 0.001f;
        env.decay_s = 0.08f;
        env.sustain = 0.0f;
        env.release_s = 0.10f;
        env.noteOn();
        pitchEnv = 1.0f;
        drive.preGain = 1.5f + vel * 2.5f;
        drive.postGain = 0.8f;
    }

    inline float tick() noexcept {
        pitchEnv *= 0.9992f;
        const float f = freq * (1.0f + 6.0f * pitchEnv);
        phase += f / sampleRate;
        if (phase >= 1.0f) phase -= floorf(phase);
        env.tick(1.0f / sampleRate);
        const float s = synth::dsp::sineLU(phase) * env.level;
        return drive.tick(s);
    }
};

struct Snare {
    float sampleRate = 44100.0f;
    WhiteNoise noise;
    Svf bp;
    Adsr env;
    float tonePhase = 0.0f;
    float toneFreq = 180.0f;

    void setSampleRate(float sr) noexcept { sampleRate = sr; }
    void trigger(float vel) noexcept {
        env.attack_s = 0.001f;
        env.decay_s = 0.10f;
        env.sustain = 0.0f;
        env.release_s = 0.12f;
        env.noteOn();
        bp.mode = SvfMode::BandPass;
        bp.set(2000.0f, 0.6f, sampleRate);
        toneFreq = 160.0f + vel * 80.0f;
    }

    inline float tick() noexcept {
        env.tick(1.0f / sampleRate);
        const float n = bp.tick(noise.tick()) * 0.7f;
        tonePhase += toneFreq / sampleRate;
        if (tonePhase >= 1.0f) tonePhase -= 1.0f;
        const float t = synth::dsp::sineLU(tonePhase) * 0.3f;
        return (n + t) * env.level;
    }
};

struct Hat {
    float sampleRate = 44100.0f;
    WhiteNoise noise;
    Svf hp;
    Adsr env;

    void setSampleRate(float sr) noexcept { sampleRate = sr; }
    void trigger(float vel) noexcept {
        env.attack_s = 0.0005f;
        env.decay_s = 0.03f + vel * 0.02f;
        env.sustain = 0.0f;
        env.release_s = 0.02f;
        env.noteOn();
        hp.mode = SvfMode::HighPass;
        hp.set(6000.0f, 0.2f, sampleRate);
    }

    inline float tick() noexcept {
        env.tick(1.0f / sampleRate);
        const float n = hp.tick(noise.tick());
        return n * env.level * 0.5f;
    }
};

} // namespace synth::modules
