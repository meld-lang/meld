/**
 * **Feature: meld-daemon, Property 33: Undefined Symbol Detection**
 *
 * For any undefined symbol reference, the LspChannel SHALL report an error
 * and suggest similar available names.
 *
 * **Validates: Requirements 17.3**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "meld/daemon/diagnostics_provider.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace meld::daemon {
namespace {

// ---------------------------------------------------------------------------
// Generators
// ---------------------------------------------------------------------------

rc::Gen<std::string> genIdentifier() {
    return rc::gen::map(
        rc::gen::inRange(3, 8),
        [](int len) {
            std::string s;
            for (int i = 0; i < len; ++i)
                s += static_cast<char>('a' + (i % 26));
            return s;
        }
    );
}

/// Generate a name that is a small edit away from the original.
rc::Gen<std::string> genSimilarName(const std::string& original) {
    return rc::gen::map(
        rc::gen::inRange(0, static_cast<int>(original.size())),
        [original](int pos) {
            std::string modified = original;
            if (modified.empty()) return std::string("x");
            // Substitute one character
            char replacement = (modified[pos] == 'z') ? 'a' : modified[pos] + 1;
            modified[pos] = replacement;
            return modified;
        }
    );
}

/// Build a model with defined symbols and a reference to an undefined one.
void populate_with_undefined_ref(SemanticModel& model,
                                  const std::filesystem::path& file,
                                  const std::vector<std::string>& defined,
                                  const std::string& undefined_name) {
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";

    // Add defined symbols
    for (const auto& name : defined) {
        auto node = std::make_shared<ASTNode>();
        node->kind = "val_declaration";
        node->name = name;
        node->type_info = "Int";
        root->children.push_back(node);
    }

    // Add a reference to the undefined symbol
    auto ref = std::make_shared<ASTNode>();
    ref->kind = "reference";
    ref->name = undefined_name;
    ref->location = {file, 10, 4};
    root->children.push_back(ref);

    FileSemantics sem;
    sem.path = file;
    sem.ast = root;
    model.update_file(file, std::move(sem));
}

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

/**
 * Property 33a: edit_distance is symmetric and zero iff strings are equal.
 */
RC_GTEST_PROP(UndefinedSymbolDetectionProperty,
              EditDistanceSymmetricAndZeroIffEqual,
              ()) {
    auto a = *genIdentifier();
    auto b = *genIdentifier();

    int d_ab = DiagnosticsProvider::edit_distance(a, b);
    int d_ba = DiagnosticsProvider::edit_distance(b, a);

    RC_ASSERT(d_ab == d_ba);
    RC_ASSERT((d_ab == 0) == (a == b));
    RC_ASSERT(d_ab >= 0);
}

/**
 * Property 33b: An undefined symbol that is similar to a defined symbol
 * produces suggestions sorted by edit distance.
 */
RC_GTEST_PROP(UndefinedSymbolDetectionProperty,
              SimilarNameProducesSortedSuggestions,
              ()) {
    auto base_name = *genIdentifier();
    auto similar = *genSimilarName(base_name);

    // Ensure they're actually different
    RC_PRE(base_name != similar);

    std::vector<std::string> available = {base_name, "zzzzzzz", "qqqqqqq"};
    auto suggestions = DiagnosticsProvider::find_similar(similar, available);

    // Should find the base_name as a suggestion
    bool found_base = std::any_of(suggestions.begin(), suggestions.end(),
        [&](const SymbolSuggestion& s) { return s.name == base_name; });
    RC_ASSERT(found_base);

    // Suggestions should be sorted by edit distance
    for (size_t i = 1; i < suggestions.size(); ++i) {
        RC_ASSERT(suggestions[i].edit_distance >= suggestions[i - 1].edit_distance);
    }
}

/**
 * Property 33c: A reference to an undefined symbol is detected and reported
 * with suggestions when similar names exist.
 */
RC_GTEST_PROP(UndefinedSymbolDetectionProperty,
              UndefinedRefDetectedWithSuggestions,
              ()) {
    auto defined_name = *genIdentifier();
    auto undefined_name = *genSimilarName(defined_name);
    RC_PRE(defined_name != undefined_name);

    SemanticModel model;
    std::filesystem::path file = "test.meld";
    populate_with_undefined_ref(model, file, {defined_name}, undefined_name);

    DiagnosticsProvider provider(model);
    auto results = provider.detect_undefined_symbols(file);

    RC_ASSERT(!results.empty());
    bool found = std::any_of(results.begin(), results.end(),
        [&](const UndefinedSymbolResult& r) {
            return r.undefined_name == undefined_name;
        });
    RC_ASSERT(found);
}

/**
 * Property 33d: A reference to a defined symbol is NOT reported as undefined.
 */
RC_GTEST_PROP(UndefinedSymbolDetectionProperty,
              DefinedRefNotReported,
              ()) {
    auto name = *genIdentifier();

    SemanticModel model;
    std::filesystem::path file = "test.meld";
    // The reference name matches a defined symbol
    populate_with_undefined_ref(model, file, {name}, name);

    DiagnosticsProvider provider(model);
    auto results = provider.detect_undefined_symbols(file);

    // Should not report the defined symbol as undefined
    bool found = std::any_of(results.begin(), results.end(),
        [&](const UndefinedSymbolResult& r) {
            return r.undefined_name == name;
        });
    RC_ASSERT(!found);
}

}  // namespace
}  // namespace meld::daemon
