#include "meld/manifest/mdebug_sidecar.hpp"

#include <gtest/gtest.h>
#include <rapidcheck.h>

namespace meld::manifest {
namespace {

/// RapidCheck generator for LifecycleState
rc::Gen<LifecycleState> genLifecycleState() {
    return rc::gen::element(
        LifecycleState::Valid,
        LifecycleState::Moved,
        LifecycleState::PotentiallyDangling);
}

/// RapidCheck generator for AstToPcEntry
rc::Gen<AstToPcEntry> genAstToPcEntry() {
    return rc::gen::build<AstToPcEntry>(
        rc::gen::set(&AstToPcEntry::pc_start, rc::gen::arbitrary<uint64_t>()),
        rc::gen::set(&AstToPcEntry::pc_end, rc::gen::arbitrary<uint64_t>()),
        rc::gen::set(&AstToPcEntry::ast_selector,
                     rc::gen::nonEmpty(rc::gen::string<std::string>())));
}

/// RapidCheck generator for OwnershipTraceEntry
rc::Gen<OwnershipTraceEntry> genOwnershipTraceEntry() {
    return rc::gen::build<OwnershipTraceEntry>(
        rc::gen::set(&OwnershipTraceEntry::instruction_offset,
                     rc::gen::arbitrary<uint64_t>()),
        rc::gen::set(&OwnershipTraceEntry::reference_id,
                     rc::gen::arbitrary<uint32_t>()),
        rc::gen::set(&OwnershipTraceEntry::strong_count_status,
                     rc::gen::arbitrary<uint32_t>()),
        rc::gen::set(&OwnershipTraceEntry::weak_count_status,
                     rc::gen::arbitrary<uint32_t>()),
        rc::gen::set(&OwnershipTraceEntry::lifecycle, genLifecycleState()));
}

/// RapidCheck generator for MdebugSidecar
rc::Gen<MdebugSidecar> genMdebugSidecar() {
    return rc::gen::build<MdebugSidecar>(
        rc::gen::set(&MdebugSidecar::format_version,
                     rc::gen::just(MdebugSidecar::kCurrentVersion)),
        rc::gen::set(&MdebugSidecar::debug_id,
                     rc::gen::nonEmpty(rc::gen::string<std::string>())),
        rc::gen::set(&MdebugSidecar::dwarf_data,
                     rc::gen::container<std::vector<uint8_t>>(
                         rc::gen::arbitrary<uint8_t>())),
        rc::gen::set(&MdebugSidecar::ast_to_pc_index,
                     rc::gen::container<std::vector<AstToPcEntry>>(
                         genAstToPcEntry())),
        rc::gen::set(&MdebugSidecar::ownership_traces,
                     rc::gen::container<std::vector<OwnershipTraceEntry>>(
                         genOwnershipTraceEntry())));
}

// Property 3: MdebugSidecar Round-Trip
// For any valid MdebugSidecar, serializing (with zstd compression) then
// deserializing SHALL produce an equivalent MdebugSidecar.
TEST(MdebugSidecarRoundTripProperty, SerializeDeserializeIsIdentity) {
    rc::check("serialize then deserialize produces equivalent MdebugSidecar",
              [](void) {
                  auto sidecar = *genMdebugSidecar();
                  auto bytes = serialize_mdebug(sidecar);
                  auto restored = deserialize_mdebug(bytes);
                  RC_ASSERT(restored.has_value());
                  RC_ASSERT(*restored == sidecar);
              });
}

TEST(MdebugSidecarRoundTripProperty, SerializationIsNotEmpty) {
    rc::check("serialized MdebugSidecar is never empty",
              [](void) {
                  auto sidecar = *genMdebugSidecar();
                  auto bytes = serialize_mdebug(sidecar);
                  RC_ASSERT(!bytes.empty());
              });
}

TEST(MdebugSidecarRoundTripProperty, EmptySectionsRoundTrip) {
    rc::check("sidecar with empty sections round-trips correctly",
              [](void) {
                  MdebugSidecar sidecar;
                  sidecar.debug_id = *rc::gen::nonEmpty(
                      rc::gen::string<std::string>());
                  // Leave dwarf_data, ast_to_pc_index, ownership_traces empty
                  auto bytes = serialize_mdebug(sidecar);
                  auto restored = deserialize_mdebug(bytes);
                  RC_ASSERT(restored.has_value());
                  RC_ASSERT(*restored == sidecar);
              });
}

}  // namespace
}  // namespace meld::manifest
