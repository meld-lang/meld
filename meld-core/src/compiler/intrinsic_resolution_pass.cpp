/// @file intrinsic_resolution_pass.cpp
/// @brief Semantic Analyzer — Intrinsic Resolution Pass implementation
///
/// Scans all trait and class declarations for @intrinsic annotations during
/// module loading. Builds a registry mapping intrinsic names to their
/// annotated types.
///
/// Requirements: 5.2, 5.3

#include "meld/compiler/intrinsic_resolution_pass.hpp"
#include "meld/compat/visit.hpp"
#include <algorithm>

namespace meld::compiler {

// ===========================================================================
// IntrinsicResolutionRegistry
// ===========================================================================

void IntrinsicResolutionRegistry::register_entry(const IntrinsicEntry& entry) {
    size_t idx = entries_.size();
    entries_.push_back(entry);
    index_[entry.intrinsic_name].push_back(idx);
}

std::vector<const IntrinsicEntry*>
IntrinsicResolutionRegistry::lookup(const std::string& intrinsic_name) const {
    std::vector<const IntrinsicEntry*> result;
    auto it = index_.find(intrinsic_name);
    if (it != index_.end()) {
        for (size_t idx : it->second) {
            result.push_back(&entries_[idx]);
        }
    }
    return result;
}

const IntrinsicEntry*
IntrinsicResolutionRegistry::lookup_primary(const std::string& intrinsic_name) const {
    auto it = index_.find(intrinsic_name);
    if (it != index_.end() && !it->second.empty()) {
        return &entries_[it->second.front()];
    }
    return nullptr;
}

bool IntrinsicResolutionRegistry::has(const std::string& intrinsic_name) const {
    return index_.count(intrinsic_name) > 0;
}

std::optional<std::string>
IntrinsicResolutionRegistry::entity_for(const std::string& intrinsic_name) const {
    auto* entry = lookup_primary(intrinsic_name);
    if (entry) {
        return entry->entity_name;
    }
    return std::nullopt;
}

void IntrinsicResolutionRegistry::clear() {
    entries_.clear();
    index_.clear();
}

// ===========================================================================
// IntrinsicResolutionPass
// ===========================================================================

IntrinsicResolutionPass::IntrinsicResolutionPass() = default;

IntrinsicResolutionResult IntrinsicResolutionPass::run(
    const std::vector<parser::ast::expression>& expressions,
    const std::string& source_file
) {
    IntrinsicResolutionResult result;

    // Step 1: Seed the registry with std.mem built-in intrinsics.
    // These are always available regardless of what the source declares.
    register_std_mem_intrinsics(result.registry);

    // Step 2: Scan all top-level declarations for @intrinsic annotations.
    for (const auto& expr : expressions) {
        // Scan class definitions
        if (auto* class_fwd = boost::get<
                boost::spirit::x3::forward_ast<parser::ast::class_definition>>(&expr)) {
            scan_class_definition(
                class_fwd->get(), source_file,
                result.registry, result.diagnostics);
        }
        // Scan struct definitions (traits are represented as structs in the AST)
        else if (auto* struct_fwd = boost::get<
                boost::spirit::x3::forward_ast<parser::ast::struct_definition>>(&expr)) {
            scan_struct_definition(
                struct_fwd->get(), source_file,
                result.registry, result.diagnostics);
        }
        // Scan function definitions (for @intrinsic(memory_move))
        else if (auto* func_fwd = boost::get<
                boost::spirit::x3::forward_ast<parser::ast::function_definition>>(&expr)) {
            scan_function_definition(
                func_fwd->get(), source_file,
                result.registry, result.diagnostics);
        }
    }

    // Store the registry for later access.
    last_registry_ = result.registry;

    return result;
}

void IntrinsicResolutionPass::register_std_mem_intrinsics(
    IntrinsicResolutionRegistry& registry
) {
    // @intrinsic(memory_strategy) → Storable trait
    registry.register_entry(IntrinsicEntry{
        .intrinsic_name = std_mem::IntrinsicTag::memory_strategy,
        .entity_name = "Storable",
        .entity_kind = IntrinsicEntityKind::Trait,
        .source_file = "std.mem",
        .line = 0,
        .column = 0
    });

    // @intrinsic(managed_container) → List
    registry.register_entry(IntrinsicEntry{
        .intrinsic_name = std_mem::IntrinsicTag::managed_container,
        .entity_name = "List",
        .entity_kind = IntrinsicEntityKind::Container,
        .source_file = "std.mem",
        .line = 0,
        .column = 0
    });

    // @intrinsic(managed_container) → Map
    registry.register_entry(IntrinsicEntry{
        .intrinsic_name = std_mem::IntrinsicTag::managed_container,
        .entity_name = "Map",
        .entity_kind = IntrinsicEntityKind::Container,
        .source_file = "std.mem",
        .line = 0,
        .column = 0
    });

    // @intrinsic(managed_container) → Set
    registry.register_entry(IntrinsicEntry{
        .intrinsic_name = std_mem::IntrinsicTag::managed_container,
        .entity_name = "Set",
        .entity_kind = IntrinsicEntityKind::Container,
        .source_file = "std.mem",
        .line = 0,
        .column = 0
    });

    // @intrinsic(managed_container) → Queue
    registry.register_entry(IntrinsicEntry{
        .intrinsic_name = std_mem::IntrinsicTag::managed_container,
        .entity_name = "Queue",
        .entity_kind = IntrinsicEntityKind::Container,
        .source_file = "std.mem",
        .line = 0,
        .column = 0
    });

    // @intrinsic(memory_move) → std.mem.move
    registry.register_entry(IntrinsicEntry{
        .intrinsic_name = std_mem::IntrinsicTag::memory_move,
        .entity_name = "std.mem.move",
        .entity_kind = IntrinsicEntityKind::Function,
        .source_file = "std.mem",
        .line = 0,
        .column = 0
    });
}

// ---------------------------------------------------------------------------
// AST scanning methods
// ---------------------------------------------------------------------------

void IntrinsicResolutionPass::scan_class_definition(
    const parser::ast::class_definition& class_def,
    const std::string& source_file,
    IntrinsicResolutionRegistry& registry,
    std::vector<IntrinsicDiagnostic>& diagnostics
) {
    const std::string& name = class_def.name.name;

    // Check if this class name matches a known intrinsic-annotated type.
    // In the current AST, @intrinsic annotations are not yet first-class
    // AST nodes, so we detect them by matching known type names from
    // std.mem (Own, Link) and container types (List, Map, Set, Queue).
    //
    // When the parser gains @intrinsic annotation support, this will
    // switch to reading the annotation directly from the AST node.

    // Hold[T] and View[T] implement Storable (@intrinsic(memory_strategy))
    if (name == "Own" || name == "Link") {
        // These are Storable implementors, not the trait itself.
        // The trait (Storable) is already registered via register_std_mem_intrinsics().
        // We register the class as associated with memory_strategy so
        // downstream passes can identify Own/Link types.
        registry.register_entry(IntrinsicEntry{
            .intrinsic_name = std_mem::IntrinsicTag::memory_strategy,
            .entity_name = name,
            .entity_kind = IntrinsicEntityKind::Class,
            .source_file = source_file,
            .line = 0,
            .column = 0
        });

        diagnostics.push_back(IntrinsicDiagnostic{
            .level = IntrinsicDiagnostic::Level::Info,
            .message = "Resolved @intrinsic(memory_strategy) implementor: " + name,
            .source_file = source_file
        });
    }

    // Check for managed container types
    if (name == "List" || name == "Map" || name == "Set" || name == "Queue") {
        // Only register if not already present from std.mem seeding
        // (avoid duplicates when user re-declares a container type)
        auto existing = registry.lookup(std_mem::IntrinsicTag::managed_container);
        bool already_registered = std::any_of(
            existing.begin(), existing.end(),
            [&name](const IntrinsicEntry* e) { return e->entity_name == name; }
        );

        if (!already_registered) {
            registry.register_entry(IntrinsicEntry{
                .intrinsic_name = std_mem::IntrinsicTag::managed_container,
                .entity_name = name,
                .entity_kind = IntrinsicEntityKind::Container,
                .source_file = source_file,
                .line = 0,
                .column = 0
            });
        }
    }
}

void IntrinsicResolutionPass::scan_function_definition(
    const parser::ast::function_definition& func_def,
    const std::string& source_file,
    IntrinsicResolutionRegistry& registry,
    std::vector<IntrinsicDiagnostic>& diagnostics
) {
    const std::string& name = func_def.name.name;

    // Detect std.mem.move() — annotated with @intrinsic(memory_move).
    // Match by function name "move" in the std.mem module context.
    if (name == "move" || name == "mem_move") {
        auto existing = registry.lookup(std_mem::IntrinsicTag::memory_move);
        bool already_registered = std::any_of(
            existing.begin(), existing.end(),
            [&name](const IntrinsicEntry* e) { return e->entity_name == name; }
        );

        if (!already_registered) {
            registry.register_entry(IntrinsicEntry{
                .intrinsic_name = std_mem::IntrinsicTag::memory_move,
                .entity_name = name,
                .entity_kind = IntrinsicEntityKind::Function,
                .source_file = source_file,
                .line = 0,
                .column = 0
            });

            diagnostics.push_back(IntrinsicDiagnostic{
                .level = IntrinsicDiagnostic::Level::Info,
                .message = "Resolved @intrinsic(memory_move) function: " + name,
                .source_file = source_file
            });
        }
    }
}

void IntrinsicResolutionPass::scan_struct_definition(
    const parser::ast::struct_definition& def,
    const std::string& source_file,
    IntrinsicResolutionRegistry& registry,
    std::vector<IntrinsicDiagnostic>& diagnostics
) {
    const std::string& name = def.name.name;

    // Detect the Storable trait — annotated with @intrinsic(memory_strategy).
    // In Meld, traits use the `trt` keyword but are represented as structs
    // in the current AST. Match by name "Storable".
    if (name == "Storable") {
        // Check if already registered from std.mem seeding
        auto existing = registry.lookup(std_mem::IntrinsicTag::memory_strategy);
        bool already_registered = std::any_of(
            existing.begin(), existing.end(),
            [](const IntrinsicEntry* e) {
                return e->entity_name == "Storable"
                    && e->entity_kind == IntrinsicEntityKind::Trait;
            }
        );

        if (!already_registered) {
            registry.register_entry(IntrinsicEntry{
                .intrinsic_name = std_mem::IntrinsicTag::memory_strategy,
                .entity_name = "Storable",
                .entity_kind = IntrinsicEntityKind::Trait,
                .source_file = source_file,
                .line = 0,
                .column = 0
            });
        }

        diagnostics.push_back(IntrinsicDiagnostic{
            .level = IntrinsicDiagnostic::Level::Info,
            .message = "Resolved @intrinsic(memory_strategy) trait: Storable",
            .source_file = source_file
        });
    }
}

// ---------------------------------------------------------------------------
// Annotation extraction helpers
// ---------------------------------------------------------------------------

std::optional<std::string> IntrinsicResolutionPass::extract_intrinsic_annotation(
    const std::string& entity_name,
    const std::vector<std::string>& annotations
) {
    // First check explicit annotations list (future AST support)
    for (const auto& ann : annotations) {
        // Match @intrinsic(tag_name) pattern
        if (ann.starts_with("intrinsic(") && ann.ends_with(")")) {
            std::string tag = ann.substr(10, ann.size() - 11);
            if (is_known_intrinsic_tag(tag)) {
                return tag;
            }
        }
        // Also match bare intrinsic tag names
        if (is_known_intrinsic_tag(ann)) {
            return ann;
        }
    }

    // Fall back to name-based detection for known std.mem types
    if (entity_name == "Storable") {
        return std::string(std_mem::IntrinsicTag::memory_strategy);
    }
    if (entity_name == "Own" || entity_name == "Link") {
        return std::string(std_mem::IntrinsicTag::memory_strategy);
    }
    if (entity_name == "vec" || entity_name == "dict" || entity_name == "set") {
        return std::string(std_mem::IntrinsicTag::managed_container);
    }
    if (entity_name == "List" || entity_name == "Map" || entity_name == "Set" || entity_name == "Queue") {
        return std::string(std_mem::IntrinsicTag::managed_container);
    }
    if (entity_name == "move" || entity_name == "mem_move") {
        return std::string(std_mem::IntrinsicTag::memory_move);
    }

    return std::nullopt;
}

bool IntrinsicResolutionPass::is_known_intrinsic_tag(const std::string& tag) {
    return tag == std_mem::IntrinsicTag::memory_strategy
        || tag == std_mem::IntrinsicTag::managed_container
        || tag == std_mem::IntrinsicTag::memory_move;
}

} // namespace meld::compiler
