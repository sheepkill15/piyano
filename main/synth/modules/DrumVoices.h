#pragma once

#include <cmath>

#include "synth/dsp/WaveTables.h"
#include "synth/modules/Adsr.h"
#include "synth/modules/Noise.h"
#include "synth/modules/Filter.h"
#include "synth/modules/Shaper.h"
#include "engine/AudioContext.h"

namespace synth::modules {

inline void adsrHit(Adsr& e, const float a, const float d, const float s, const float r) noexcept {
    e.attack_s = a;
    e.decay_s = d;
    e.sustain = s;
    e.release_s = r;
    e.noteOn();
}

struct Kick {
    float phase = 0.0f;
    float freq = 60.0f;
    float pitchEnv = 0.0f;
    Adsr env;
    Drive drive;

    void trigger(const float vel) noexcept {
        adsrHit(env, 0.001f, 0.08f, 0.0f, 0.10f);
        pitchEnv = 1.0f;
        drive.preGain = 1.5f + vel * 2.5f;
        drive.postGain = 0.8f;
    }

    float tick() noexcept {
        pitchEnv *= 0.9992f;
        const float f = freq * (1.0f + 6.0f * pitchEnv);
        phase += f * engine::gAudio.invSampleRate;
        if (phase >= 1.0f) phase -= 1.0f;
        env.tick(engine::gAudio.invSampleRate);
        const float s = dsp::sineLU(phase) * env.level;
        return drive.tick(s);
    }
};

struct Snare {
    WhiteNoise noise;
    Svf bp;
    Adsr env;
    float tonePhase = 0.0f;
    float toneFreq = 180.0f;

    void trigger(const float vel) noexcept {
        adsrHit(env, 0.001f, 0.10f, 0.0f, 0.12f);
        bp.mode = SvfMode::BandPass;
        bp.set(2000.0f, 0.6f);
        toneFreq = 160.0f + vel * 80.0f;
    }

    float tick() noexcept {
        env.tick(engine::gAudio.invSampleRate);
        const float n = bp.tick(noise.tick()) * 0.7f;
        tonePhase += toneFreq * engine::gAudio.invSampleRate;
        if (tonePhase >= 1.0f) tonePhase -= 1.0f;
        const float t = dsp::sineLU(tonePhase) * 0.3f;
        return (n + t) * env.level;
    }
};

struct Hat {
    WhiteNoise noise;
    Svf hp;
    Adsr env;

    void trigger(const float vel) noexcept {
        adsrHit(env, 0.0005f, 0.03f + vel * 0.02f, 0.0f, 0.02f);
        hp.mode = SvfMode::HighPass;
        hp.set(6000.0f, 0.2f);
    }

    float tick() noexcept {
        env.tick(engine::gAudio.invSampleRate);
        const float n = hp.tick(noise.tick());
        return n * env.level * 0.5f;
    }
};

} // namespace synth::modules
