/**
 * **Feature: meld-daemon, Property 51: Homoiconic Analysis Correctness**
 *
 * For any homoiconic Meld code, the LspChannel SHALL understand the
 * relationship between AST representation and runtime values.
 *
 * **Validates: Requirements 21.1**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "meld/daemon/meld_feature_provider.hpp"

#include <string>
#include <vector>

namespace meld::daemon {
namespace {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void add_node(SemanticModel& model,
              const std::filesystem::path& file,
              const std::string& name,
              const std::string& kind) {
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    root->name = file.stem().string();
    root->location = SourceLocation{file, 1, 0};

    auto child = std::make_shared<ASTNode>();
    child->kind = kind;
    child->name = name;
    child->type_info = "SomeType";
    child->location = SourceLocation{file, 5, 0};
    root->children.push_back(child);

    FileSemantics sem;
    sem.path = file;
    sem.ast = root;
    model.update_file(file, std::move(sem));
}

// ---------------------------------------------------------------------------
// Generators
// ---------------------------------------------------------------------------

rc::Gen<std::string> genKnownKind() {
    return rc::gen::elementOf(std::vector<std::string>{
        "function_definition", "val_declaration", "struct",
        "enum", "trait", "literal", "module"});
}

rc::Gen<std::string> genNodeName() {
    return rc::gen::map(
        rc::gen::inRange(2, 7),
        [](int len) {
            std::string s;
            for (int i = 0; i < len; ++i)
                s += static_cast<char>('a' + (i % 26));
            return s;
        });
}

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

/**
 * Property 51a: Known AST kinds produce a matching runtime type.
 */
RC_GTEST_PROP(HomoiconicAnalysis,
              KnownKindsProduceRuntimeType,
              ()) {
    auto kind = *genKnownKind();
    auto name = *genNodeName();

    SemanticModel model;
    std::filesystem::path file = "test.meld";
    add_node(model, file, name, kind);

    MeldFeatureProvider provider(model);
    auto result = provider.analyze_homoiconic(file, name);

    RC_ASSERT(result.ast_runtime_match == true);
    RC_ASSERT(result.node_kind == kind);
    RC_ASSERT(!result.runtime_type.empty());
    RC_ASSERT(result.runtime_type != "Unknown");
}

/**
 * Property 51b: Unknown AST kinds produce ast_runtime_match = false.
 */
RC_GTEST_PROP(HomoiconicAnalysis,
              UnknownKindDoesNotMatch,
              ()) {
    auto name = *genNodeName();

    SemanticModel model;
    std::filesystem::path file = "test.meld";
    add_node(model, file, name, "unknown_construct");

    MeldFeatureProvider provider(model);
    auto result = provider.analyze_homoiconic(file, name);

    RC_ASSERT(result.ast_runtime_match == false);
    RC_ASSERT(result.runtime_type == "Unknown");
}

/**
 * Property 51c: Analyzing a non-existent node returns a descriptive result.
 */
RC_GTEST_PROP(HomoiconicAnalysis,
              NonExistentNodeDescribed,
              ()) {
    SemanticModel model;
    std::filesystem::path file = "test.meld";

    // Empty model — no file indexed
    MeldFeatureProvider provider(model);
    auto result = provider.analyze_homoiconic(file, "nonexistent");

    RC_ASSERT(result.ast_runtime_match == false);
    RC_ASSERT(!result.description.empty());
}

}  // namespace
}  // namespace meld::daemon
