#pragma once

#include <cstdint>

namespace synth::modules {

struct Adsr {
    float attack_s = 0.01f;
    float decay_s = 0.10f;
    float sustain = 0.70f; // 0..1
    float release_s = 0.20f;

    enum class Phase : uint8_t { Attack, Decay, Sustain, Release, Idle };

    Phase phase = Phase::Idle;
    float level = 0.0f;
    float timeInPhase = 0.0f;
    float releaseStartLevel = 0.0f;

    void noteOn() noexcept {
        phase = Phase::Attack;
        level = 0.0f;
        timeInPhase = 0.0f;
    }

    void noteOff() noexcept {
        if (phase == Phase::Idle) return;
        releaseStartLevel = level;
        phase = Phase::Release;
        timeInPhase = 0.0f;
    }

    void reset() noexcept {
        phase = Phase::Idle;
        level = 0.0f;
        timeInPhase = 0.0f;
        releaseStartLevel = 0.0f;
    }

    void tick(float dt) noexcept {
        timeInPhase += dt;
        switch (phase) {
            case Phase::Attack: {
                if (attack_s > 0.0f && timeInPhase < attack_s) level = timeInPhase / attack_s;
                else { level = 1.0f; phase = Phase::Decay; timeInPhase = 0.0f; }
                break;
            }
            case Phase::Decay: {
                if (decay_s > 0.0f && timeInPhase < decay_s) {
                    const float t = timeInPhase / decay_s;
                    level = 1.0f + (sustain - 1.0f) * t;
                } else { level = sustain; phase = Phase::Sustain; timeInPhase = 0.0f; }
                break;
            }
            case Phase::Sustain: {
                level = sustain;
                break;
            }
            case Phase::Release: {
                if (release_s > 0.0f && timeInPhase < release_s) {
                    const float t = timeInPhase / release_s;
                    level = releaseStartLevel * (1.0f - t);
                } else { level = 0.0f; phase = Phase::Idle; }
                break;
            }
            case Phase::Idle: {
                level = 0.0f;
                break;
            }
        }
    }

    bool isActive() const noexcept { return phase != Phase::Idle; }
};

} // namespace synth::modules

