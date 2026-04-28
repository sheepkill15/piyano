#pragma once

#include <cstdint>
#include <array>

namespace synth::modulation {

enum class Source : uint8_t {
    Velocity = 0,
    KeyTrack,
    Gate,
    Env1,
    Env2,
    Lfo1,
    Lfo2,
    ModWheel,
    PitchBend,
    Random1,
    Count
};

enum class Destination : uint8_t {
    Amp = 0,
    Pan,
    Pitch,
    FilterCutoff,
    FilterResonance,
    Drive,
    FmIndex,
    FmRatio,
    Count
};

struct Route {
    Source src = Source::Velocity;
    Destination dst = Destination::Amp;
    float amount = 0.0f; // bipolar recommended [-1..1]
    bool enabled = false;
};

template <size_t MaxRoutes>
class ModMatrix {
public:
    static constexpr size_t kMaxRoutes = MaxRoutes;

    void clear() noexcept {
        for (auto& r : routes_) r = {};
    }

    const Route& route(size_t i) const noexcept { return routes_[i]; }
    Route& routeMutable(size_t i) noexcept { return routes_[i]; }

    void setRoute(size_t i, Source s, Destination d, float amt, bool en = true) noexcept {
        if (i >= MaxRoutes) return;
        routes_[i].src = s;
        routes_[i].dst = d;
        routes_[i].amount = amt;
        routes_[i].enabled = en;
    }

    void apply(const std::array<float, static_cast<size_t>(Source::Count)>& sources,
               std::array<float, static_cast<size_t>(Destination::Count)>& destOut) const noexcept {
        for (auto& v : destOut) v = 0.0f;
        for (const auto& r : routes_) {
            if (!r.enabled) continue;
            const float s = sources[static_cast<size_t>(r.src)];
            destOut[static_cast<size_t>(r.dst)] += s * r.amount;
        }
    }

private:
    std::array<Route, MaxRoutes> routes_ = {};
};

} // namespace synth::modulation

