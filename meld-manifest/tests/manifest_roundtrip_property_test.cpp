#include "meld/manifest/manifest.hpp"

#include <gtest/gtest.h>
#include <rapidcheck.h>

namespace meld::manifest {
namespace {

/// RapidCheck generator for EffectBitmask
rc::Gen<EffectBitmask> genEffectBitmask() {
    return rc::gen::map(rc::gen::inRange<uint8_t>(0, (1 << kEffectCount)),
                        [](uint8_t raw) { return EffectBitmask(raw); });
}

/// RapidCheck generator for ResourceBounds
rc::Gen<ResourceBounds> genResourceBounds() {
    return rc::gen::build<ResourceBounds>(
        rc::gen::set(&ResourceBounds::allowed_domains,
                     rc::gen::container<std::vector<std::string>>(
                         rc::gen::nonEmpty(rc::gen::string<std::string>()))),
        rc::gen::set(&ResourceBounds::read_paths,
                     rc::gen::container<std::vector<std::string>>(
                         rc::gen::nonEmpty(rc::gen::string<std::string>()))),
        rc::gen::set(&ResourceBounds::write_paths,
                     rc::gen::container<std::vector<std::string>>(
                         rc::gen::nonEmpty(rc::gen::string<std::string>()))),
        rc::gen::set(&ResourceBounds::exec_paths,
                     rc::gen::container<std::vector<std::string>>(
                         rc::gen::nonEmpty(rc::gen::string<std::string>()))));
}

/// RapidCheck generator for SymbolEffectEntry
rc::Gen<SymbolEffectEntry> genSymbolEffectEntry() {
    return rc::gen::build<SymbolEffectEntry>(
        rc::gen::set(&SymbolEffectEntry::symbol_name,
                     rc::gen::nonEmpty(rc::gen::string<std::string>())),
        rc::gen::set(&SymbolEffectEntry::effects, genEffectBitmask()),
        rc::gen::set(&SymbolEffectEntry::bounds, genResourceBounds()),
        rc::gen::set(&SymbolEffectEntry::is_ffi, rc::gen::arbitrary<bool>()));
}

/// RapidCheck generator for Manifest
rc::Gen<Manifest> genManifest() {
    return rc::gen::build<Manifest>(
        rc::gen::set(&Manifest::format_version, rc::gen::arbitrary<uint32_t>()),
        rc::gen::set(&Manifest::project_name,
                     rc::gen::nonEmpty(rc::gen::string<std::string>())),
        rc::gen::set(&Manifest::project_version,
                     rc::gen::nonEmpty(rc::gen::string<std::string>())),
        rc::gen::set(&Manifest::code_hash,
                     rc::gen::nonEmpty(rc::gen::string<std::string>())),
        rc::gen::set(&Manifest::symbols,
                     rc::gen::container<std::vector<SymbolEffectEntry>>(
                         genSymbolEffectEntry())));
}

TEST(ManifestRoundTripProperty, SerializeDeserializeIsIdentity) {
    rc::check("serialize then deserialize produces equivalent Manifest",
              [](void) {
                  auto manifest = *genManifest();
                  auto bytes = serialize_manifest(manifest);
                  auto restored = deserialize_manifest(bytes);
                  RC_ASSERT(restored.has_value());
                  RC_ASSERT(*restored == manifest);
              });
}

TEST(ManifestRoundTripProperty, SerializationIsNotEmpty) {
    rc::check("serialized manifest is never empty",
              [](void) {
                  auto manifest = *genManifest();
                  auto bytes = serialize_manifest(manifest);
                  RC_ASSERT(!bytes.empty());
              });
}

}  // namespace
}  // namespace meld::manifest
