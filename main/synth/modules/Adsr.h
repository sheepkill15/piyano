#pragma once

#include <cstdint>

namespace synth::modules {

struct Adsr {
    float attack_s = 0.01f;
    float decay_s = 0.10f;
    float sustain = 0.70f; // 0..1
    float release_s = 0.20f;

    // Cached reciprocals so tick() is multiply-only on the hot path. Callers
    // that mutate attack_s/decay_s/release_s directly must call refresh().
    float invAttack = 100.0f;
    float invDecay = 10.0f;
    float invRelease = 5.0f;

    enum class Phase : uint8_t { Attack, Decay, Sustain, Release, Idle };

    Phase phase = Phase::Idle;
    float level = 0.0f;
    float timeInPhase = 0.0f;
    float releaseStartLevel = 0.0f;

    void refresh() noexcept {
        invAttack  = (attack_s  > 0.0f) ? 1.0f / attack_s  : 0.0f;
        invDecay   = (decay_s   > 0.0f) ? 1.0f / decay_s   : 0.0f;
        invRelease = (release_s > 0.0f) ? 1.0f / release_s : 0.0f;
    }

    void noteOn() noexcept {
        phase = Phase::Attack;
        level = 0.0f;
        timeInPhase = 0.0f;
        refresh();
    }

    void noteOff() noexcept {
        if (phase == Phase::Idle) return;
        releaseStartLevel = level;
        phase = Phase::Release;
        timeInPhase = 0.0f;
        invRelease = (release_s > 0.0f) ? 1.0f / release_s : 0.0f;
    }

    void reset() noexcept {
        phase = Phase::Idle;
        level = 0.0f;
        timeInPhase = 0.0f;
        releaseStartLevel = 0.0f;
    }

    void tick(const float dt) noexcept {
        timeInPhase += dt;
        switch (phase) {
            case Phase::Attack: {
                if (attack_s > 0.0f && timeInPhase < attack_s) level = timeInPhase * invAttack;
                else { level = 1.0f; phase = Phase::Decay; timeInPhase = 0.0f; }
                break;
            }
            case Phase::Decay: {
                if (decay_s > 0.0f && timeInPhase < decay_s) {
                    const float t = timeInPhase * invDecay;
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
                    const float t = timeInPhase * invRelease;
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

    [[nodiscard]] bool isActive() const noexcept { return phase != Phase::Idle; }
};

} // namespace synth::modules

