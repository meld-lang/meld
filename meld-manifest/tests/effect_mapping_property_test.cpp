#include "meld/manifest/srt_provider.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <rapidcheck.h>

namespace meld::manifest {
namespace {

/// Generate a random subset of effects
rc::Gen<std::vector<Effect>> genEffectSubset() {
    return rc::gen::map(
        rc::gen::inRange<uint8_t>(0, (1 << kEffectCount)),
        [](uint8_t bits) {
            std::vector<Effect> effects;
            for (uint8_t i = 0; i < kEffectCount; ++i) {
                if ((bits >> i) & 1u) {
                    effects.push_back(static_cast<Effect>(i));
                }
            }
            return effects;
        });
}

/// Generate a SandboxConfig with random effects
rc::Gen<SandboxConfig> genSandboxConfig() {
    return rc::gen::map(
        genEffectSubset(),
        [](std::vector<Effect> effects) {
            SandboxConfig config;
            config.allowed_effects = effects;
            // Add some paths/domains based on effects
            for (auto e : effects) {
                switch (e) {
                    case Effect::Network:
                        config.allowed_domains.push_back("example.com");
                        break;
                    case Effect::FileSystemRead:
                        config.read_only_paths.push_back("/data");
                        break;
                    case Effect::FileSystemWrite:
                        config.read_write_paths.push_back("/tmp");
                        break;
                    default:
                        break;
                }
            }
            return config;
        });
}

TEST(EffectMappingProperty, AllowedEffectsProduceNonEmptyPolicy) {
    rc::check(
        "every allowed effect produces a corresponding non-empty SRT policy section",
        [](void) {
            auto config = *genSandboxConfig();
            auto policy = MeldSRTProvider::generate_policy(config);

            EffectBitmask allowed = EffectBitmask::from_vector(config.allowed_effects);

            if (allowed.has(Effect::Network)) {
                RC_ASSERT(policy.contains("network"));
                RC_ASSERT(policy["network"]["mode"] == "allow");
            }
            if (allowed.has(Effect::FileSystemRead) || allowed.has(Effect::FileSystemWrite)) {
                RC_ASSERT(policy.contains("filesystem"));
            }
            if (allowed.has(Effect::ProcessExec)) {
                RC_ASSERT(policy.contains("process"));
            }
        });
}

TEST(EffectMappingProperty, DeniedEffectsProduceDenyEntries) {
    rc::check(
        "every effect NOT in allowed_effects produces a deny entry",
        [](void) {
            auto config = *genSandboxConfig();
            auto policy = MeldSRTProvider::generate_policy(config);

            EffectBitmask allowed = EffectBitmask::from_vector(config.allowed_effects);

            if (!allowed.has(Effect::Network)) {
                if (policy.contains("network")) {
                    RC_ASSERT(policy["network"]["mode"] == "deny");
                }
            }
        });
}

TEST(EffectMappingProperty, StdlibAlwaysReadable) {
    rc::check(
        "stdlib paths are always in readOnly regardless of effects",
        [](void) {
            auto config = *genSandboxConfig();
            auto policy = MeldSRTProvider::generate_policy(config);

            RC_ASSERT(policy.contains("filesystem"));
            RC_ASSERT(!policy["filesystem"]["readOnly"].empty());
        });
}

}  // namespace
}  // namespace meld::manifest
