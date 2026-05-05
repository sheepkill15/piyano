#pragma once

#include <array>
#include <cstdint>

#include "Patch.h"

#include "synth/modules/Filter.h"

namespace synth::modules {

struct FilterBank {
    static constexpr uint8_t MAX_ENVS = patch::MAX_ENVS;
    static constexpr uint8_t MAX_LFOS = patch::MAX_LFOS;
    static constexpr uint8_t MAX_FILTERS = patch::MAX_FILTERS;

    std::array<Svf, MAX_FILTERS> filters{};

    void reset() noexcept {
        for (auto& f : filters) f.reset();
    }

    void bakePatch(const patch::Patch& patch) noexcept {
        for (uint8_t i = 0; i < MAX_FILTERS; ++i) {
            auto& f = filters[i];
            f.reset();
            f.mode = toSvfMode_(patch.filters[i].mode);
            f.set(patch.filters[i].cutoffHz, patch.filters[i].resonance);
        }
    }

    void noteOn(const patch::Patch& patch, const float vel) noexcept {
        for (uint8_t i = 0; i < patch.filterCount && i < MAX_FILTERS; ++i) {
            auto& f = filters[i];
            f.reset();
            const auto& fd = patch.filters[i];
            const float cutoff = fd.cutoffHz + fd.velToCutoffHz * vel;
            f.mode = toSvfMode_(fd.mode);
            f.set(cutoff, fd.resonance);
        }
    }

    void updateCutoffs(const patch::Patch& patch,
                       const float vel,
                       const std::array<float, MAX_LFOS>& lfoBuf,
                       const std::array<float, MAX_ENVS>& envLevels) noexcept {
        for (uint8_t fi = 0; fi < patch.filterCount; ++fi) {
            const auto& fd = patch.filters[fi];

            float envLvl = 0.0f;
            if (fd.envIndex >= 1 && fd.envIndex < MAX_ENVS) {
                envLvl = envLevels[static_cast<uint8_t>(fd.envIndex)];
            }

            float cut = fd.cutoffHz + (fd.cutoffPeakHz - fd.cutoffHz) * envLvl;
            cut += fd.velToCutoffHz * vel;
            if (fd.lfoIndex >= 0) {
                cut += lfoBuf[static_cast<uint8_t>(fd.lfoIndex)] * fd.lfoCutoffHz;
            }
            if (cut < 20.0f) cut = 20.0f;

            filters[fi].set(cut, fd.resonance);
        }
    }

    float process(const patch::Patch& patch, float sig) noexcept {
        for (uint8_t fi = 0; fi < patch.filterCount; ++fi) {
            sig = filters[fi].tick(sig);
        }
        return sig;
    }

private:
    static SvfMode toSvfMode_(const patch::FilterMode m) noexcept {
        using P = patch::FilterMode;
        using S = SvfMode;
        switch (m) {
            case P::LowPass:  return S::LowPass;
            case P::BandPass: return S::BandPass;
            case P::HighPass: return S::HighPass;
        }
        return S::LowPass;
    }
};

} // namespace synth::modules

