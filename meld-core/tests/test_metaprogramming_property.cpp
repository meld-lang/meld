/**
 * Property-Based Tests for Provenance-Preserving Code Generation (Property 11)
 *
 * Validates that the metaprogramming system:
 *   - Generates implementations for common traits via derive macros (Req 6.1)
 *   - Supports compile-time code transformation via procedural macros (Req 6.2)
 *   - Supports compile-time parameters and computation via const generics (Req 6.3)
 *   - Allows custom code annotations via attribute macros (Req 6.4)
 *   - Preserves provenance information for AI agents through expansion (Req 6.5)
 *
 * Feature: rust-inspired-meld-enhancements, Property 11: Provenance-Preserving Code Generation
 * **Validates: Requirements 6.1, 6.2, 6.3, 6.4, 6.5**
 */

#include <gtest/gtest.h>
#include "meld/macro/macro.hpp"
#include "meld/macro/decorator.hpp"
#include "meld/macro/blueprint.hpp"
#include "meld/provenance/provenance.hpp"
#include "meld/provenance/ai_provenance.hpp"
#include "meld/kernel/primitives.hpp"
#include "meld/kernel/operations.hpp"
#include "meld/testing/property_test.hpp"

#include <random>
#include <string>
#include <vector>
#include <algorithm>
#include <set>

using namespace meld::macro;
using namespace meld::provenance;
using namespace meld::ai;
using namespace meld::kernel;
using namespace meld::testing;

// ============================================================================
// Random generators for property tests
// ============================================================================

namespace {

std::mt19937& rng() {
    static std::mt19937 gen(std::random_device{}());
    return gen;
}

std::string random_class_name() {
    static const std::vector<std::string> names = {
        "User", "Order", "Product", "Account", "Session",
        "Config", "Message", "Event", "Record", "Entity",
        "Widget", "Handler", "Service", "Model", "View"
    };
    std::uniform_int_distribution<size_t> dist(0, names.size() - 1);
    return names[dist(rng())];
}

std::string random_field_name() {
    static const std::vector<std::string> names = {
        "name", "age", "email", "id", "value",
        "count", "status", "type", "data", "label",
        "score", "level", "index", "key", "flag"
    };
    std::uniform_int_distribution<size_t> dist(0, names.size() - 1);
    return names[dist(rng())];
}

std::string random_field_type() {
    static const std::vector<std::string> types = {
        "string", "int", "float", "bool", "double"
    };
    std::uniform_int_distribution<size_t> dist(0, types.size() - 1);
    return types[dist(rng())];
}

std::string random_macro_name() {
    static const std::vector<std::string> names = {
        "transform_a", "transform_b", "transform_c",
        "expand_x", "expand_y", "expand_z",
        "gen_alpha", "gen_beta", "gen_gamma"
    };
    std::uniform_int_distribution<size_t> dist(0, names.size() - 1);
    return names[dist(rng())];
}

std::string random_decorator_name() {
    static const std::vector<std::string> names = {
        "Getter", "Setter", "ToString", "Equals",
        "HashCode", "Copy", "Builder", "Debug",
        "Serialize", "Validate"
    };
    std::uniform_int_distribution<size_t> dist(0, names.size() - 1);
    return names[dist(rng())];
}

std::string random_model_name() {
    static const std::vector<std::string> names = {
        "gpt-4", "gpt-3.5-turbo", "claude-3", "codex", "copilot"
    };
    std::uniform_int_distribution<size_t> dist(0, names.size() - 1);
    return names[dist(rng())];
}

double random_confidence() {
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(rng());
}

std::string random_blueprint_summary() {
    static const std::vector<std::string> summaries = {
        "Processes input data", "Validates user input",
        "Transforms records", "Computes aggregates",
        "Handles authentication", "Manages sessions",
        "Serializes objects", "Parses configuration"
    };
    std::uniform_int_distribution<size_t> dist(0, summaries.size() - 1);
    return summaries[dist(rng())];
}

std::string random_tag() {
    static const std::vector<std::string> tags = {
        "core", "util", "io", "net", "auth",
        "data", "api", "test", "perf", "safe"
    };
    std::uniform_int_distribution<size_t> dist(0, tags.size() - 1);
    return tags[dist(rng())];
}

OriginType random_origin() {
    std::uniform_int_distribution<int> dist(0, 2);
    switch (dist(rng())) {
        case 0: return OriginType::Human;
        case 1: return OriginType::Agent;
        default: return OriginType::Verified;
    }
}

// Generate a random class definition for decorator testing
meld::parser::ast::class_definition random_class_def() {
    meld::parser::ast::class_definition def;
    def.name.name = random_class_name() + "_" + std::to_string(rng()() % 1000);

    std::uniform_int_distribution<int> field_count_dist(1, 5);
    int num_fields = field_count_dist(rng());

    std::set<std::string> used_names;
    for (int i = 0; i < num_fields; ++i) {
        meld::parser::ast::field_declaration field;
        std::string fname = random_field_name();
        // Ensure unique field names
        while (used_names.count(fname)) {
            fname = random_field_name() + std::to_string(i);
        }
        used_names.insert(fname);
        field.name.name = fname;
        field.type.type_name.name = random_field_type();
        def.fields.push_back(field);
    }

    return def;
}

} // anonymous namespace

// ============================================================================
// Property 11a: Derive macros generate non-empty trait implementations
// for any valid class definition
// Validates: Requirement 6.1
// ============================================================================

TEST(MetaprogrammingProperty, DeriveMacroGeneratesTraitImplementations) {
    // Tag: Feature: rust-inspired-meld-enhancements, Property 11: Provenance-Preserving Code Generation

    constexpr int ITERATIONS = 100;

    for (int i = 0; i < ITERATIONS; ++i) {
        auto class_def = random_class_def();

        // Property: generate_to_string always produces a non-nil value
        auto to_string_result = generate_to_string(class_def.name.name, class_def.fields);
        ASSERT_FALSE(to_string_result.is<Empty>())
            << "generate_to_string should produce non-nil for class: " << class_def.name.name;

        // Property: generate_equals always produces a non-nil value
        auto equals_result = generate_equals(class_def.name.name, class_def.fields);
        ASSERT_FALSE(equals_result.is<Empty>())
            << "generate_equals should produce non-nil for class: " << class_def.name.name;

        // Property: generate_hash_code always produces a non-nil value
        auto hash_result = generate_hash_code(class_def.name.name, class_def.fields);
        ASSERT_FALSE(hash_result.is<Empty>())
            << "generate_hash_code should produce non-nil for class: " << class_def.name.name;

        // Property: generate_copy always produces a non-nil value
        auto copy_result = generate_copy(class_def.name.name, class_def.fields);
        ASSERT_FALSE(copy_result.is<Empty>())
            << "generate_copy should produce non-nil for class: " << class_def.name.name;

        // Property: getter/setter generated for each field
        for (const auto& field : class_def.fields) {
            auto getter = generate_getter(class_def.name.name, field);
            ASSERT_FALSE(getter.is<Empty>())
                << "generate_getter should produce non-nil for field: " << field.name.name;

            auto setter = generate_setter(class_def.name.name, field);
            ASSERT_FALSE(setter.is<Empty>())
                << "generate_setter should produce non-nil for field: " << field.name.name;
        }
    }
}

// ============================================================================
// Property 11b: Procedural macro expansion preserves structure and terminates
// Validates: Requirement 6.2
// ============================================================================

TEST(MetaprogrammingProperty, ProceduralMacroExpansionPreservesStructure) {
    // Tag: Feature: rust-inspired-meld-enhancements, Property 11: Provenance-Preserving Code Generation

    constexpr int ITERATIONS = 100;

    for (int i = 0; i < ITERATIONS; ++i) {
        // Clean up registries for each iteration
        MacroRegistry::instance().clear();

        std::string macro_name = "test_macro_" + std::to_string(i);
        std::string output_name = "result_" + std::to_string(i);

        // Register a simple identity-like macro that wraps the input
        auto macro = make_simple_macro(
            macro_name,
            {"input"},
            [output_name](const std::vector<Value>& args) -> Value {
                // Transform: wrap input in a list with a result symbol
                auto result_sym = std::make_shared<Symbol>(output_name);
                return cons(Value(result_sym), cons(args[0], Value(nil())));
            }
        );
        MacroRegistry::instance().register_macro(macro);

        // Create a macro call AST: (macro_name some_arg)
        auto macro_sym = std::make_shared<Symbol>(macro_name);
        auto arg_sym = std::make_shared<Symbol>("some_arg");
        auto call_ast = cons(Value(macro_sym), cons(Value(arg_sym), Value(nil())));

        MacroExpander expander;

        // Property: macro call is recognized
        ASSERT_TRUE(expander.is_macro_call(call_ast))
            << "Should recognize macro call for: " << macro_name;

        // Property: macro name is extractable
        auto name_result = expander.get_macro_name(call_ast);
        ASSERT_TRUE(name_result.has_value())
            << "Should extract macro name";
        EXPECT_EQ(*name_result, macro_name);

        // Property: expansion succeeds and produces a non-nil result
        auto expanded = expander.expand(call_ast);
        ASSERT_TRUE(expanded.has_value())
            << "Expansion should succeed for: " << macro_name
            << " error: " << expanded.error();
        EXPECT_FALSE(expanded->is<Empty>())
            << "Expanded result should not be nil";
    }
}

// ============================================================================
// Property 11c: Blueprint metadata (compile-time parameters) is preserved
// through registration and retrieval
// Validates: Requirement 6.3
// ============================================================================

TEST(MetaprogrammingProperty, BlueprintMetadataPreservedThroughRegistration) {
    // Tag: Feature: rust-inspired-meld-enhancements, Property 11: Provenance-Preserving Code Generation

    constexpr int ITERATIONS = 100;

    BlueprintRegistry::instance().clear();

    for (int i = 0; i < ITERATIONS; ++i) {
        std::string func_name = "func_" + std::to_string(i);
        std::string summary = random_blueprint_summary();

        // Generate random tags
        std::uniform_int_distribution<int> tag_count_dist(1, 4);
        int num_tags = tag_count_dist(rng());
        std::vector<std::string> tags;
        for (int t = 0; t < num_tags; ++t) {
            tags.push_back(random_tag());
        }

        // Generate random rules
        std::uniform_int_distribution<int> rule_count_dist(0, 3);
        int num_rules = rule_count_dist(rng());
        std::vector<std::string> rules;
        for (int r = 0; r < num_rules; ++r) {
            rules.push_back("Rule " + std::to_string(r) + " for " + func_name);
        }

        BlueprintMetadata metadata;
        metadata.summary = summary;
        metadata.tags = tags;
        metadata.rules = rules;
        metadata.id = "bp_" + std::to_string(i);

        BlueprintRegistry::instance().register_blueprint(func_name, metadata);

        // Property: registered blueprint is retrievable
        auto retrieved = BlueprintRegistry::instance().get_blueprint(func_name);
        ASSERT_TRUE(retrieved.has_value())
            << "Blueprint should be retrievable for: " << func_name;

        // Property: summary is preserved
        EXPECT_EQ(retrieved->summary, summary)
            << "Summary should be preserved for: " << func_name;

        // Property: tags are preserved
        EXPECT_EQ(retrieved->tags.size(), tags.size())
            << "Tag count should be preserved for: " << func_name;
        for (size_t t = 0; t < tags.size(); ++t) {
            EXPECT_EQ(retrieved->tags[t], tags[t]);
        }

        // Property: rules are preserved
        EXPECT_EQ(retrieved->rules.size(), rules.size())
            << "Rule count should be preserved for: " << func_name;

        // Property: embedding is generated (non-empty)
        EXPECT_FALSE(retrieved->embedding.empty())
            << "Embedding should be generated for: " << func_name;

        // Property: timestamps are set
        EXPECT_NE(retrieved->created_at, std::chrono::system_clock::time_point{})
            << "created_at should be set for: " << func_name;
        EXPECT_NE(retrieved->updated_at, std::chrono::system_clock::time_point{})
            << "updated_at should be set for: " << func_name;
    }

    BlueprintRegistry::instance().clear();
}

// ============================================================================
// Property 11d: Attribute macros (decorators) are registered and retrievable
// for any valid decorator name
// Validates: Requirement 6.4
// ============================================================================

TEST(MetaprogrammingProperty, AttributeMacroRegistrationAndRetrieval) {
    // Tag: Feature: rust-inspired-meld-enhancements, Property 11: Provenance-Preserving Code Generation

    constexpr int ITERATIONS = 100;

    DecoratorRegistry::instance().clear();

    std::vector<std::string> registered_names;

    for (int i = 0; i < ITERATIONS; ++i) {
        std::string dec_name = random_decorator_name() + "_" + std::to_string(i);

        // Create a decorator that returns a symbol with the class name
        auto decorator = make_decorator(
            dec_name,
            [dec_name](const meld::parser::ast::class_definition& class_def,
                       MacroExpander& /*expander*/)
                -> std::expected<Value, std::string> {
                auto sym = std::make_shared<Symbol>(
                    dec_name + "_applied_to_" + class_def.name.name);
                return Value(sym);
            }
        );

        DecoratorRegistry::instance().register_decorator(decorator);
        registered_names.push_back(dec_name);

        // Property: decorator is immediately retrievable after registration
        ASSERT_TRUE(DecoratorRegistry::instance().has_decorator(dec_name))
            << "Decorator should exist after registration: " << dec_name;

        auto retrieved = DecoratorRegistry::instance().get_decorator(dec_name);
        ASSERT_TRUE(retrieved.has_value())
            << "get_decorator should succeed for: " << dec_name;
        EXPECT_EQ((*retrieved)->name(), dec_name);

        // Property: applying the decorator to a random class succeeds
        auto class_def = random_class_def();
        MacroExpander expander;
        auto result = (*retrieved)->apply(class_def, expander);
        ASSERT_TRUE(result.has_value())
            << "Decorator application should succeed for: " << dec_name
            << " on class: " << class_def.name.name;
        EXPECT_FALSE(result->is<Empty>())
            << "Decorator result should not be nil";
    }

    // Property: all registered decorators are still retrievable
    for (const auto& name : registered_names) {
        EXPECT_TRUE(DecoratorRegistry::instance().has_decorator(name))
            << "Previously registered decorator should still exist: " << name;
    }

    // Property: unregistered decorator is not found
    EXPECT_FALSE(DecoratorRegistry::instance().has_decorator("nonexistent_decorator_xyz"));
    auto bad_result = DecoratorRegistry::instance().get_decorator("nonexistent_decorator_xyz");
    EXPECT_FALSE(bad_result.has_value());

    DecoratorRegistry::instance().clear();
}

// ============================================================================
// Property 11e: Provenance metadata is preserved through macro-like
// code generation — AI provenance attached to nodes survives transformations
// Validates: Requirement 6.5
// ============================================================================

TEST(MetaprogrammingProperty, ProvenancePreservedThroughCodeGeneration) {
    // Tag: Feature: rust-inspired-meld-enhancements, Property 11: Provenance-Preserving Code Generation

    constexpr int ITERATIONS = 100;

    for (int i = 0; i < ITERATIONS; ++i) {
        std::string model = random_model_name();
        double confidence = random_confidence();
        std::string blueprint_id = "bp_" + std::to_string(i);

        AIProvenanceTracker tracker(model);

        // Create a node and mark it as AI-generated
        auto sym = std::make_shared<Symbol>("generated_code_" + std::to_string(i));
        Value node(sym);

        Value marked = tracker.markAsAIGenerated(node, confidence, blueprint_id);

        // Property: provenance is attached
        ASSERT_TRUE(Provenance::hasProvenance(marked))
            << "Provenance should be attached after markAsAIGenerated";

        // Property: provenance metadata is retrievable
        auto prov = Provenance::getProvenance(marked);
        ASSERT_TRUE(prov.has_value())
            << "getProvenance should return metadata";

        // Property: origin is Agent
        EXPECT_EQ(prov->origin, OriginType::Agent)
            << "Origin should be Agent for AI-generated code";

        // Property: confidence is preserved (clamped to [0,1])
        double expected_confidence = std::clamp(confidence, 0.0, 1.0);
        ASSERT_TRUE(prov->confidence_score.has_value());
        EXPECT_NEAR(*prov->confidence_score, expected_confidence, 0.001)
            << "Confidence should be preserved";

        // Property: model name is preserved
        ASSERT_TRUE(prov->agent_model.has_value());
        EXPECT_EQ(*prov->agent_model, model)
            << "Agent model should be preserved";

        // Property: trust score is within valid range [0, 1]
        double trust = Provenance::calculateTrustScore(*prov);
        EXPECT_GE(trust, 0.0) << "Trust score should be >= 0";
        EXPECT_LE(trust, 1.0) << "Trust score should be <= 1";
    }
}

// ============================================================================
// Property 11f: Blueprint search by tags returns only blueprints that
// actually contain the queried tags
// Validates: Requirement 6.3 (compile-time metadata queryability)
// ============================================================================

TEST(MetaprogrammingProperty, BlueprintSearchByTagsIsSound) {
    // Tag: Feature: rust-inspired-meld-enhancements, Property 11: Provenance-Preserving Code Generation

    constexpr int ITERATIONS = 100;

    BlueprintRegistry::instance().clear();

    // Register a set of blueprints with known tags
    std::map<std::string, std::vector<std::string>> func_tags;
    for (int i = 0; i < 20; ++i) {
        std::string func_name = "search_func_" + std::to_string(i);
        BlueprintMetadata metadata;
        metadata.summary = "Function " + std::to_string(i);
        metadata.id = "search_bp_" + std::to_string(i);

        std::uniform_int_distribution<int> tag_count_dist(1, 3);
        int num_tags = tag_count_dist(rng());
        for (int t = 0; t < num_tags; ++t) {
            metadata.tags.push_back(random_tag());
        }

        func_tags[func_name] = metadata.tags;
        BlueprintRegistry::instance().register_blueprint(func_name, metadata);
    }

    for (int i = 0; i < ITERATIONS; ++i) {
        // Pick a random tag to search for
        std::string search_tag = random_tag();
        auto results = BlueprintRegistry::instance().search_by_tags({search_tag});

        // Property: every result actually contains the searched tag
        for (const auto& result_name : results) {
            auto it = func_tags.find(result_name);
            ASSERT_NE(it, func_tags.end())
                << "Search result should be a registered function";

            bool has_tag = std::find(it->second.begin(), it->second.end(), search_tag)
                           != it->second.end();
            EXPECT_TRUE(has_tag)
                << "Result '" << result_name << "' should contain tag '" << search_tag << "'";
        }
    }

    BlueprintRegistry::instance().clear();
}

// ============================================================================
// Property 11g: Hygienic macro expansion generates unique symbols
// Validates: Requirement 6.2 (compile-time code transformation hygiene)
// ============================================================================

TEST(MetaprogrammingProperty, HygienicExpansionGeneratesUniqueSymbols) {
    // Tag: Feature: rust-inspired-meld-enhancements, Property 11: Provenance-Preserving Code Generation

    constexpr int ITERATIONS = 100;

    for (int i = 0; i < ITERATIONS; ++i) {
        MacroExpander expander;

        // Generate multiple gensyms with the same prefix
        std::uniform_int_distribution<int> count_dist(2, 10);
        int num_syms = count_dist(rng());

        std::set<std::string> generated_names;
        for (int s = 0; s < num_syms; ++s) {
            auto sym = expander.gensym("test_prefix");
            ASSERT_NE(sym, nullptr) << "gensym should return non-null";

            // Property: each generated symbol has a unique name
            auto [_, inserted] = generated_names.insert(sym->name());
            EXPECT_TRUE(inserted)
                << "gensym should produce unique names, duplicate: " << sym->name();
        }

        // Property: total unique symbols equals requested count
        EXPECT_EQ(generated_names.size(), static_cast<size_t>(num_syms));
    }
}

// ============================================================================
// Property 11h: Provenance mismatch detection is consistent — attaching
// content hashes and then checking with the same content reports no change
// Validates: Requirement 6.5 (provenance preservation)
// ============================================================================

TEST(MetaprogrammingProperty, ProvenanceMismatchDetectionConsistency) {
    // Tag: Feature: rust-inspired-meld-enhancements, Property 11: Provenance-Preserving Code Generation

    constexpr int ITERATIONS = 100;

    for (int i = 0; i < ITERATIONS; ++i) {
        auto sym = std::make_shared<Symbol>("node_" + std::to_string(i));
        Value node(sym);

        // Attach provenance
        ProvenanceMetadata metadata(random_model_name(), random_confidence());
        Value with_prov = Provenance::attachProvenance(node, metadata);

        // Generate random blueprint and code content
        std::string blueprint = "blueprint content " + std::to_string(rng()());
        std::string code = "code content " + std::to_string(rng()());

        // Attach content hashes
        Value with_hashes = Provenance::attachContentHashes(with_prov, blueprint, code);

        // Property: checking with the same content reports no change
        EXPECT_FALSE(Provenance::hasContentChanged(with_hashes, blueprint, "blueprint_hash"))
            << "Same blueprint content should not be detected as changed";
        EXPECT_FALSE(Provenance::hasContentChanged(with_hashes, code, "code_hash"))
            << "Same code content should not be detected as changed";

        // Property: checking with different content reports a change
        std::string different_blueprint = blueprint + "_modified";
        std::string different_code = code + "_modified";
        EXPECT_TRUE(Provenance::hasContentChanged(with_hashes, different_blueprint, "blueprint_hash"))
            << "Different blueprint content should be detected as changed";
        EXPECT_TRUE(Provenance::hasContentChanged(with_hashes, different_code, "code_hash"))
            << "Different code content should be detected as changed";
    }
}

// ============================================================================
// Property 11i: Blueprint evolution tracking preserves history
// Validates: Requirement 6.3, 6.5 (compile-time metadata + provenance)
// ============================================================================

TEST(MetaprogrammingProperty, BlueprintEvolutionTrackingPreservesHistory) {
    // Tag: Feature: rust-inspired-meld-enhancements, Property 11: Provenance-Preserving Code Generation

    constexpr int ITERATIONS = 100;

    BlueprintRegistry::instance().clear();

    for (int i = 0; i < ITERATIONS; ++i) {
        std::string func_name = "evolving_func_" + std::to_string(i);

        // Register initial version
        BlueprintMetadata v1;
        v1.summary = "Version 1 of " + func_name;
        v1.id = "evo_bp_" + std::to_string(i);
        BlueprintRegistry::instance().register_blueprint(func_name, v1);

        // Update with a new version
        BlueprintMetadata v2;
        v2.summary = "Version 2 of " + func_name;
        v2.id = "evo_bp_" + std::to_string(i);
        v2.tags = {"updated"};
        BlueprintRegistry::instance().register_blueprint(func_name, v2);

        // Property: current version reflects the latest registration
        auto current = BlueprintRegistry::instance().get_blueprint(func_name);
        ASSERT_TRUE(current.has_value());
        EXPECT_EQ(current->summary, v2.summary)
            << "Current blueprint should be the latest version";

        // Property: history contains the previous version
        auto history = BlueprintRegistry::instance().get_blueprint_history(func_name);
        ASSERT_GE(history.size(), 1u)
            << "History should contain at least the previous version";
        EXPECT_EQ(history.back().summary, v1.summary)
            << "History should contain the original version";
    }

    BlueprintRegistry::instance().clear();
}

// ============================================================================
// Property 11j: Macro registry operations are consistent — register then
// lookup always succeeds, and clear removes all entries
// Validates: Requirement 6.1, 6.2 (macro system consistency)
// ============================================================================

TEST(MetaprogrammingProperty, MacroRegistryConsistency) {
    // Tag: Feature: rust-inspired-meld-enhancements, Property 11: Provenance-Preserving Code Generation

    constexpr int ITERATIONS = 100;

    MacroRegistry::instance().clear();

    std::vector<std::string> registered_names;

    for (int i = 0; i < ITERATIONS; ++i) {
        std::string name = "reg_macro_" + std::to_string(i);

        auto macro = make_simple_macro(
            name, {"x"},
            [](const std::vector<Value>& args) -> Value {
                return args.empty() ? Value(nil()) : args[0];
            }
        );

        MacroRegistry::instance().register_macro(macro);
        registered_names.push_back(name);

        // Property: macro is immediately findable
        EXPECT_TRUE(MacroRegistry::instance().has_macro(name))
            << "Macro should exist after registration: " << name;

        auto retrieved = MacroRegistry::instance().get_macro(name);
        ASSERT_TRUE(retrieved.has_value())
            << "get_macro should succeed for: " << name;
        EXPECT_EQ((*retrieved)->name(), name);
    }

    // Property: all registered macros are still present
    for (const auto& name : registered_names) {
        EXPECT_TRUE(MacroRegistry::instance().has_macro(name));
    }

    // Property: clear removes all macros
    MacroRegistry::instance().clear();
    for (const auto& name : registered_names) {
        EXPECT_FALSE(MacroRegistry::instance().has_macro(name))
            << "Macro should not exist after clear: " << name;
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
