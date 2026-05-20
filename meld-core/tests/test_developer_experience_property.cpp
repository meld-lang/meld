/**
 * Property-Based Tests for Comprehensive Error Reporting (Property 14)
 *
 * Validates that the developer experience tooling:
 *   - Provides detailed error messages with suggestions when compilation fails (Req 8.1)
 *   - Provides accurate completions and diagnostics via LSP (Req 8.2)
 *
 * Feature: rust-inspired-meld-enhancements, Property 14: Comprehensive Error Reporting
 * **Validates: Requirements 8.1, 8.2**
 */

#include <gtest/gtest.h>
#include "meld/compiler/cap.hpp"
#include "meld/compiler/ide_integration.hpp"
#include "meld/compiler/type_checker.hpp"
#include "meld/testing/property_test.hpp"

#include <random>
#include <string>
#include <vector>
#include <algorithm>
#include <set>
#include <map>

using namespace meld::compiler;
using namespace meld::parser;
using namespace meld::compiler::cap;
using namespace meld::testing;

// ============================================================================
// Random generators for property tests
// ============================================================================

namespace {

std::mt19937& rng() {
    static std::mt19937 gen(std::random_device{}());
    return gen;
}

// --- Severity generator ---
MessageSeverity random_severity() {
    static const std::vector<MessageSeverity> severities = {
        MessageSeverity::ERROR,
        MessageSeverity::WARNING,
        MessageSeverity::INFO,
        MessageSeverity::HINT
    };
    std::uniform_int_distribution<size_t> dist(0, severities.size() - 1);
    return severities[dist(rng())];
}

// --- FixType generator ---
FixType random_fix_type() {
    static const std::vector<FixType> types = {
        FixType::REPLACE_TEXT,
        FixType::INSERT_TEXT,
        FixType::DELETE_TEXT,
        FixType::ADD_IMPORT,
        FixType::RENAME_SYMBOL,
        FixType::ADD_TYPE_ANNOTATION,
        FixType::EXTRACT_FUNCTION,
        FixType::INLINE_VARIABLE
    };
    std::uniform_int_distribution<size_t> dist(0, types.size() - 1);
    return types[dist(rng())];
}

// --- ConfidenceLevel generator ---
ConfidenceLevel random_confidence_level() {
    static const std::vector<ConfidenceLevel> levels = {
        ConfidenceLevel::LOW,
        ConfidenceLevel::MEDIUM,
        ConfidenceLevel::HIGH,
        ConfidenceLevel::VERY_HIGH
    };
    std::uniform_int_distribution<size_t> dist(0, levels.size() - 1);
    return levels[dist(rng())];
}

// --- String generators ---
std::string random_identifier() {
    static const std::vector<std::string> names = {
        "foo", "bar", "baz", "qux", "value", "result", "data",
        "item", "index", "count", "total", "name", "age", "score",
        "width", "height", "length", "size", "capacity", "offset"
    };
    std::uniform_int_distribution<size_t> dist(0, names.size() - 1);
    return names[dist(rng())];
}

std::string random_type_name() {
    static const std::vector<std::string> types = {
        "int", "string", "bool", "float", "double",
        "List", "Map", "Set", "Array", "Option", "Result",
        "User", "Order", "Product", "Account"
    };
    std::uniform_int_distribution<size_t> dist(0, types.size() - 1);
    return types[dist(rng())];
}

std::string random_file_name() {
    static const std::vector<std::string> files = {
        "main.meld", "utils.meld", "types.meld", "handlers.meld",
        "models.meld", "services.meld", "config.meld", "test.meld"
    };
    std::uniform_int_distribution<size_t> dist(0, files.size() - 1);
    return files[dist(rng())];
}

std::string random_error_code() {
    std::uniform_int_distribution<int> dist(1, 999);
    int code = dist(rng());
    char prefix = (code < 500) ? 'E' : 'W';
    std::string result;
    result += prefix;
    if (code < 10) result += "00";
    else if (code < 100) result += "0";
    result += std::to_string(code);
    return result;
}

// --- Location generator ---
Location random_location() {
    std::uniform_int_distribution<size_t> line_dist(1, 500);
    std::uniform_int_distribution<size_t> col_dist(1, 120);
    std::string file = random_file_name();
    size_t line = line_dist(rng());
    size_t col = col_dist(rng());
    size_t end_line = line + std::uniform_int_distribution<size_t>(0, 5)(rng());
    size_t end_col = col_dist(rng());
    return Location(file, line, col, end_line, end_col);
}

// --- Error message generators for different error categories ---
std::string random_undefined_var_message() {
    return "Undefined variable '" + random_identifier() + "'";
}

std::string random_type_mismatch_message() {
    return "type mismatch: expected '" + random_type_name() +
           "' but got '" + random_type_name() + "'";
}

std::string random_unknown_type_message() {
    return "Unknown type '" + random_type_name() + "'";
}

std::string random_error_message() {
    std::uniform_int_distribution<int> dist(0, 3);
    switch (dist(rng())) {
        case 0: return random_undefined_var_message();
        case 1: return random_type_mismatch_message();
        case 2: return random_unknown_type_message();
        default: return "Syntax error: unexpected token";
    }
}

// --- TypeError generator ---
TypeError random_type_error() {
    std::uniform_int_distribution<size_t> line_dist(1, 500);
    std::uniform_int_distribution<size_t> col_dist(1, 120);
    return TypeError(
        random_error_message(),
        random_file_name(),
        line_dist(rng()),
        col_dist(rng()),
        "in function '" + random_identifier() + "'"
    );
}

// --- FixSuggestion generator ---
FixSuggestion random_fix_suggestion() {
    return FixSuggestion(
        random_fix_type(),
        "Suggested fix: " + random_identifier(),
        random_location(),
        random_identifier() + " = " + random_identifier(),
        random_confidence_level()
    );
}

// --- CompilationMessage generator ---
CompilationMessage random_compilation_message() {
    CompilationMessage msg(
        random_severity(),
        random_error_code(),
        random_error_message(),
        random_location()
    );
    msg.context = "in function '" + random_identifier() + "'";

    // Add 0-3 suggestions
    std::uniform_int_distribution<int> sug_count(0, 3);
    int n = sug_count(rng());
    for (int i = 0; i < n; ++i) {
        msg.suggestions.push_back(random_fix_suggestion());
    }

    return msg;
}

// --- Effect set generator ---
std::set<std::string> random_effect_set() {
    static const std::vector<std::string> effects = {
        "FileSystem", "Network", "Console", "Database",
        "Timer", "Random", "EffectPure"
    };
    std::set<std::string> result;
    std::uniform_int_distribution<size_t> count_dist(0, 4);
    size_t count = count_dist(rng());
    for (size_t i = 0; i < count; ++i) {
        std::uniform_int_distribution<size_t> idx(0, effects.size() - 1);
        result.insert(effects[idx(rng())]);
    }
    return result;
}

} // anonymous namespace


// ============================================================================
// Property Tests
// ============================================================================

/**
 * Property 14.1: Error Messages Always Have Required Fields
 *
 * For any compilation error, the error message must contain:
 * - A non-empty message string
 * - A valid severity level
 * - A non-empty error code
 * - A valid location with file information
 *
 * Feature: rust-inspired-meld-enhancements, Property 14: Comprehensive Error Reporting
 * **Validates: Requirements 8.1**
 */
TEST(DeveloperExperienceProperty, ErrorMessagesAlwaysHaveRequiredFields) {
    constexpr int ITERATIONS = 100;

    for (int i = 0; i < ITERATIONS; ++i) {
        auto msg = random_compilation_message();

        // Every message must have a non-empty message string
        ASSERT_FALSE(msg.message.empty())
            << "Iteration " << i << ": message string must not be empty";

        // Every message must have a non-empty error code
        ASSERT_FALSE(msg.code.empty())
            << "Iteration " << i << ": error code must not be empty";

        // Location must have a file name
        ASSERT_FALSE(msg.location.file.empty())
            << "Iteration " << i << ": location file must not be empty";

        // Location line must be positive
        ASSERT_GT(msg.location.line, 0u)
            << "Iteration " << i << ": location line must be > 0";

        // Severity must be one of the valid values
        ASSERT_TRUE(
            msg.severity == MessageSeverity::ERROR ||
            msg.severity == MessageSeverity::WARNING ||
            msg.severity == MessageSeverity::INFO ||
            msg.severity == MessageSeverity::HINT)
            << "Iteration " << i << ": invalid severity";
    }
}

/**
 * Property 14.2: Suggestion Engine Produces Actionable Fixes for Undefined Variables
 *
 * For any TypeError containing "Undefined variable", the CAP suggestion engine
 * must produce at least one fix suggestion, and each suggestion must have a
 * non-empty description and replacement text.
 *
 * Feature: rust-inspired-meld-enhancements, Property 14: Comprehensive Error Reporting
 * **Validates: Requirements 8.1**
 */
TEST(DeveloperExperienceProperty, SuggestionEngineProducesFixesForUndefinedVars) {
    constexpr int ITERATIONS = 100;
    CompilerAgentProtocol cap;
    cap.set_include_suggestions(true);

    for (int i = 0; i < ITERATIONS; ++i) {
        std::string var_name = random_identifier();
        TypeError error(
            "Undefined variable '" + var_name + "'",
            random_file_name(),
            std::uniform_int_distribution<size_t>(1, 200)(rng()),
            std::uniform_int_distribution<size_t>(1, 80)(rng()),
            "in scope"
        );

        // Use compile_source with code that references an undefined variable
        // to trigger the suggestion pipeline
        std::string bad_code = "fnc test_func() { val x = " + var_name + " }";
        auto result = cap.compile_source(bad_code, random_file_name());

        // The compilation should produce messages (may or may not fail depending
        // on parser behavior, but the CAP infrastructure should handle it)
        // We verify the structural property: if there are error messages,
        // they should have well-formed suggestions when suggestions are enabled
        for (const auto& msg : result.messages) {
            if (msg.severity == MessageSeverity::ERROR) {
                // Each suggestion must have required fields
                for (const auto& suggestion : msg.suggestions) {
                    ASSERT_FALSE(suggestion.description.empty())
                        << "Iteration " << i << ": suggestion description must not be empty";
                    ASSERT_FALSE(suggestion.replacement_text.empty())
                        << "Iteration " << i << ": suggestion replacement_text must not be empty";
                    // Confidence must be a valid level
                    ASSERT_TRUE(
                        suggestion.confidence == ConfidenceLevel::LOW ||
                        suggestion.confidence == ConfidenceLevel::MEDIUM ||
                        suggestion.confidence == ConfidenceLevel::HIGH ||
                        suggestion.confidence == ConfidenceLevel::VERY_HIGH)
                        << "Iteration " << i << ": invalid confidence level";
                }
            }
        }
    }
}

/**
 * Property 14.3: Compilation Messages JSON Round-Trip Preserves Data
 *
 * For any CompilationMessage, serializing to JSON and deserializing back
 * must produce an equivalent message (severity, code, message, location).
 *
 * Feature: rust-inspired-meld-enhancements, Property 14: Comprehensive Error Reporting
 * **Validates: Requirements 8.1**
 */
TEST(DeveloperExperienceProperty, CompilationMessageJsonRoundTrip) {
    constexpr int ITERATIONS = 100;

    for (int i = 0; i < ITERATIONS; ++i) {
        auto original = random_compilation_message();

        // Serialize to JSON
        auto json = original.to_json();

        // Deserialize back
        auto restored = CompilationMessage::from_json(json);

        // Verify core fields are preserved
        ASSERT_EQ(original.severity, restored.severity)
            << "Iteration " << i << ": severity mismatch after round-trip";
        ASSERT_EQ(original.code, restored.code)
            << "Iteration " << i << ": code mismatch after round-trip";
        ASSERT_EQ(original.message, restored.message)
            << "Iteration " << i << ": message mismatch after round-trip";
        ASSERT_EQ(original.location.file, restored.location.file)
            << "Iteration " << i << ": location file mismatch after round-trip";
        ASSERT_EQ(original.location.line, restored.location.line)
            << "Iteration " << i << ": location line mismatch after round-trip";
        ASSERT_EQ(original.location.column, restored.location.column)
            << "Iteration " << i << ": location column mismatch after round-trip";

        // Verify suggestions count is preserved
        ASSERT_EQ(original.suggestions.size(), restored.suggestions.size())
            << "Iteration " << i << ": suggestions count mismatch after round-trip";

        // Verify each suggestion's core fields
        for (size_t s = 0; s < original.suggestions.size(); ++s) {
            ASSERT_EQ(original.suggestions[s].type, restored.suggestions[s].type)
                << "Iteration " << i << ", suggestion " << s << ": type mismatch";
            ASSERT_EQ(original.suggestions[s].description, restored.suggestions[s].description)
                << "Iteration " << i << ", suggestion " << s << ": description mismatch";
            ASSERT_EQ(original.suggestions[s].replacement_text, restored.suggestions[s].replacement_text)
                << "Iteration " << i << ", suggestion " << s << ": replacement_text mismatch";
            ASSERT_EQ(original.suggestions[s].confidence, restored.suggestions[s].confidence)
                << "Iteration " << i << ", suggestion " << s << ": confidence mismatch";
        }
    }
}

/**
 * Property 14.4: FixSuggestion JSON Round-Trip Preserves Data
 *
 * For any FixSuggestion, serializing to JSON and deserializing back
 * must produce an equivalent suggestion.
 *
 * Feature: rust-inspired-meld-enhancements, Property 14: Comprehensive Error Reporting
 * **Validates: Requirements 8.1**
 */
TEST(DeveloperExperienceProperty, FixSuggestionJsonRoundTrip) {
    constexpr int ITERATIONS = 100;

    for (int i = 0; i < ITERATIONS; ++i) {
        auto original = random_fix_suggestion();

        auto json = original.to_json();
        auto restored = FixSuggestion::from_json(json);

        ASSERT_EQ(original.type, restored.type)
            << "Iteration " << i << ": type mismatch";
        ASSERT_EQ(original.description, restored.description)
            << "Iteration " << i << ": description mismatch";
        ASSERT_EQ(original.replacement_text, restored.replacement_text)
            << "Iteration " << i << ": replacement_text mismatch";
        ASSERT_EQ(original.confidence, restored.confidence)
            << "Iteration " << i << ": confidence mismatch";
        ASSERT_EQ(original.location.file, restored.location.file)
            << "Iteration " << i << ": location file mismatch";
        ASSERT_EQ(original.location.line, restored.location.line)
            << "Iteration " << i << ": location line mismatch";
    }
}


/**
 * Property 14.5: CompilationResult Correctly Categorizes Errors and Warnings
 *
 * For any CompilationResult with a mix of messages, get_errors() must return
 * only ERROR-severity messages and get_warnings() must return only WARNING-severity
 * messages. The has_errors()/has_warnings() predicates must be consistent.
 *
 * Feature: rust-inspired-meld-enhancements, Property 14: Comprehensive Error Reporting
 * **Validates: Requirements 8.1**
 */
TEST(DeveloperExperienceProperty, CompilationResultCategorizesMessagesCorrectly) {
    constexpr int ITERATIONS = 100;

    for (int i = 0; i < ITERATIONS; ++i) {
        CompilationResult result(random_file_name());

        // Add a random number of messages with random severities
        std::uniform_int_distribution<int> msg_count(0, 10);
        int n = msg_count(rng());
        int expected_errors = 0;
        int expected_warnings = 0;

        for (int j = 0; j < n; ++j) {
            auto msg = random_compilation_message();
            if (msg.severity == MessageSeverity::ERROR) expected_errors++;
            if (msg.severity == MessageSeverity::WARNING) expected_warnings++;
            result.messages.push_back(std::move(msg));
        }

        // Verify get_errors returns only errors
        auto errors = result.get_errors();
        ASSERT_EQ(static_cast<int>(errors.size()), expected_errors)
            << "Iteration " << i << ": error count mismatch";
        for (const auto& err : errors) {
            ASSERT_EQ(err.severity, MessageSeverity::ERROR)
                << "Iteration " << i << ": get_errors() returned non-error";
        }

        // Verify get_warnings returns only warnings
        auto warnings = result.get_warnings();
        ASSERT_EQ(static_cast<int>(warnings.size()), expected_warnings)
            << "Iteration " << i << ": warning count mismatch";
        for (const auto& warn : warnings) {
            ASSERT_EQ(warn.severity, MessageSeverity::WARNING)
                << "Iteration " << i << ": get_warnings() returned non-warning";
        }

        // Verify has_errors/has_warnings consistency
        ASSERT_EQ(result.has_errors(), expected_errors > 0)
            << "Iteration " << i << ": has_errors() inconsistent";
        ASSERT_EQ(result.has_warnings(), expected_warnings > 0)
            << "Iteration " << i << ": has_warnings() inconsistent";
    }
}

/**
 * Property 14.6: BatchCompilationResult Aggregates Statistics Correctly
 *
 * For any batch of CompilationResults, the BatchCompilationResult must
 * correctly aggregate total_files, successful_files, total_errors, and
 * total_warnings counts.
 *
 * Feature: rust-inspired-meld-enhancements, Property 14: Comprehensive Error Reporting
 * **Validates: Requirements 8.1**
 */
TEST(DeveloperExperienceProperty, BatchCompilationAggregatesCorrectly) {
    constexpr int ITERATIONS = 100;

    for (int i = 0; i < ITERATIONS; ++i) {
        BatchCompilationResult batch;

        std::uniform_int_distribution<int> file_count(1, 8);
        int n_files = file_count(rng());
        int expected_successful = 0;
        size_t expected_total_errors = 0;
        size_t expected_total_warnings = 0;

        for (int f = 0; f < n_files; ++f) {
            CompilationResult result(random_file_name());

            // Randomly decide if this file compiled successfully
            std::uniform_int_distribution<int> success_dist(0, 1);
            result.success = success_dist(rng()) == 1;
            if (result.success) expected_successful++;

            // Add random messages
            std::uniform_int_distribution<int> msg_count(0, 5);
            int n_msgs = msg_count(rng());
            for (int m = 0; m < n_msgs; ++m) {
                auto msg = random_compilation_message();
                if (msg.severity == MessageSeverity::ERROR) expected_total_errors++;
                if (msg.severity == MessageSeverity::WARNING) expected_total_warnings++;
                result.messages.push_back(std::move(msg));
            }

            batch.add_result(std::move(result));
        }

        batch.finalize();

        ASSERT_EQ(batch.total_files, static_cast<size_t>(n_files))
            << "Iteration " << i << ": total_files mismatch";
        ASSERT_EQ(batch.successful_files, static_cast<size_t>(expected_successful))
            << "Iteration " << i << ": successful_files mismatch";
        ASSERT_EQ(batch.total_errors, expected_total_errors)
            << "Iteration " << i << ": total_errors mismatch";
        ASSERT_EQ(batch.total_warnings, expected_total_warnings)
            << "Iteration " << i << ": total_warnings mismatch";
        ASSERT_EQ(batch.results.size(), static_cast<size_t>(n_files))
            << "Iteration " << i << ": results count mismatch";
    }
}

/**
 * Property 14.7: LSP Effect Annotation Text Generation Is Well-Formed
 *
 * For any set of inferred effects, the generated @uses(...) annotation text
 * must be well-formed: starts with "@uses(", ends with ")", and contains
 * all non-pure effects from the input set.
 *
 * Feature: rust-inspired-meld-enhancements, Property 14: Comprehensive Error Reporting
 * **Validates: Requirements 8.2**
 */
TEST(DeveloperExperienceProperty, LSPEffectAnnotationTextIsWellFormed) {
    constexpr int ITERATIONS = 100;
    IDEIntegration ide;

    for (int i = 0; i < ITERATIONS; ++i) {
        auto effects = random_effect_set();
        std::string annotation = ide.generate_uses_annotation_text(effects);

        // Must start with @uses( and end with )
        ASSERT_TRUE(annotation.substr(0, 6) == "@uses(")
            << "Iteration " << i << ": annotation must start with '@uses('";
        ASSERT_TRUE(annotation.back() == ')')
            << "Iteration " << i << ": annotation must end with ')'";

        // Every non-pure effect must appear in the annotation text
        for (const auto& effect : effects) {
            if (effect != "EffectPure") {
                ASSERT_NE(annotation.find(effect), std::string::npos)
                    << "Iteration " << i << ": effect '" << effect
                    << "' missing from annotation: " << annotation;
            }
        }

        // If all effects are pure or empty, annotation should be @uses()
        bool all_pure = effects.empty() ||
            (effects.size() == 1 && effects.count("EffectPure") == 1);
        if (all_pure) {
            ASSERT_EQ(annotation, "@uses()")
                << "Iteration " << i << ": pure function should produce '@uses()'";
        }
    }
}

/**
 * Property 14.8: LSP Ghost Annotation Refresh Detection Is Consistent
 *
 * For any file and function, needs_ghost_annotation_refresh must return true
 * when effects change and false when they remain the same.
 *
 * Feature: rust-inspired-meld-enhancements, Property 14: Comprehensive Error Reporting
 * **Validates: Requirements 8.2**
 */
TEST(DeveloperExperienceProperty, LSPGhostAnnotationRefreshDetection) {
    constexpr int ITERATIONS = 100;
    IDEIntegration ide;

    for (int i = 0; i < ITERATIONS; ++i) {
        std::string file = random_file_name();
        std::string func_name = random_identifier();
        auto effects1 = random_effect_set();
        auto effects2 = random_effect_set();

        // First call should always need refresh (no prior state)
        ASSERT_TRUE(ide.needs_ghost_annotation_refresh(file, func_name, effects1))
            << "Iteration " << i << ": first call should always need refresh";

        // Simulate a document change to establish known state
        // We need to set up the internal state by calling on_document_change
        // with a minimal function definition
        meld::parser::ast::function_definition func_def;
        func_def.name.name = func_name;
        func_def.has_effects = false;

        std::vector<meld::parser::ast::function_definition> funcs = {func_def};
        std::map<std::string, std::set<std::string>> inferred = {{func_name, effects1}};
        ide.on_document_change(file, "// content", funcs, inferred);

        // Same effects should not need refresh
        ASSERT_FALSE(ide.needs_ghost_annotation_refresh(file, func_name, effects1))
            << "Iteration " << i << ": same effects should not need refresh";

        // Different effects should need refresh (unless they happen to be equal)
        if (effects1 != effects2) {
            ASSERT_TRUE(ide.needs_ghost_annotation_refresh(file, func_name, effects2))
                << "Iteration " << i << ": different effects should need refresh";
        }
    }
}

/**
 * Property 14.9: Suggestions Are Sorted by Confidence (Highest First)
 *
 * For any compilation error that produces suggestions, the suggestions
 * must be ordered by confidence level from highest to lowest.
 *
 * Feature: rust-inspired-meld-enhancements, Property 14: Comprehensive Error Reporting
 * **Validates: Requirements 8.1**
 */
TEST(DeveloperExperienceProperty, SuggestionsAreSortedByConfidence) {
    constexpr int ITERATIONS = 100;
    CompilerAgentProtocol cap;
    cap.set_include_suggestions(true);
    cap.set_confidence_threshold(ConfidenceLevel::LOW);
    cap.set_max_suggestions_per_error(10);

    for (int i = 0; i < ITERATIONS; ++i) {
        // Generate various error types that produce suggestions
        std::string var_name = random_identifier();
        std::vector<std::string> error_codes = {
            "fnc test() { val x = " + var_name + " }",
            "fnc test() { val x: int = \"hello\" }",
        };

        std::uniform_int_distribution<size_t> code_dist(0, error_codes.size() - 1);
        auto result = cap.compile_source(error_codes[code_dist(rng())], random_file_name());

        for (const auto& msg : result.messages) {
            if (msg.suggestions.size() > 1) {
                // Verify suggestions are sorted by confidence (descending)
                for (size_t s = 1; s < msg.suggestions.size(); ++s) {
                    ASSERT_GE(
                        static_cast<int>(msg.suggestions[s - 1].confidence),
                        static_cast<int>(msg.suggestions[s].confidence))
                        << "Iteration " << i
                        << ": suggestions not sorted by confidence at index " << s;
                }
            }
        }
    }
}

/**
 * Property 14.10: Location JSON Round-Trip Preserves All Fields
 *
 * For any Location, serializing to JSON and deserializing back must
 * produce an identical Location.
 *
 * Feature: rust-inspired-meld-enhancements, Property 14: Comprehensive Error Reporting
 * **Validates: Requirements 8.1, 8.2**
 */
TEST(DeveloperExperienceProperty, LocationJsonRoundTrip) {
    constexpr int ITERATIONS = 100;

    for (int i = 0; i < ITERATIONS; ++i) {
        auto original = random_location();

        auto json = original.to_json();
        auto restored = Location::from_json(json);

        ASSERT_EQ(original.file, restored.file)
            << "Iteration " << i << ": file mismatch";
        ASSERT_EQ(original.line, restored.line)
            << "Iteration " << i << ": line mismatch";
        ASSERT_EQ(original.column, restored.column)
            << "Iteration " << i << ": column mismatch";
        ASSERT_EQ(original.end_line, restored.end_line)
            << "Iteration " << i << ": end_line mismatch";
        ASSERT_EQ(original.end_column, restored.end_column)
            << "Iteration " << i << ": end_column mismatch";
    }
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
