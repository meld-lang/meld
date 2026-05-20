/**
 * **Feature: meld-daemon, Property 55: Tree Initialization Validation**
 *
 * For any tree initialization syntax, constructor block syntax and nested
 * property assignments SHALL be validated correctly.
 *
 * **Validates: Requirements 21.5**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "meld/daemon/meld_feature_provider.hpp"

#include <set>
#include <string>
#include <vector>

namespace meld::daemon {
namespace {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void add_tree_init(SemanticModel& model,
                   const std::filesystem::path& file,
                   const std::string& type_name,
                   const std::string& kind,
                   const std::vector<std::string>& props,
                   bool add_nested = false) {
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    root->name = file.stem().string();
    root->location = SourceLocation{file, 1, 0};

    auto init_node = std::make_shared<ASTNode>();
    init_node->kind = kind;
    init_node->name = type_name;
    init_node->location = SourceLocation{file, 3, 0};

    uint32_t line = 4;
    for (const auto& prop : props) {
        auto prop_node = std::make_shared<ASTNode>();
        prop_node->kind = "property";
        prop_node->name = prop;
        prop_node->type_info = "Int";
        prop_node->location = SourceLocation{file, line++, 4};

        if (add_nested) {
            auto nested = std::make_shared<ASTNode>();
            nested->kind = "property";
            nested->name = "inner_" + prop;
            nested->location = SourceLocation{file, line++, 8};
            prop_node->children.push_back(nested);
        }

        init_node->children.push_back(prop_node);
    }

    root->children.push_back(init_node);

    FileSemantics sem;
    sem.path = file;
    sem.ast = root;
    model.update_file(file, std::move(sem));
}

// ---------------------------------------------------------------------------
// Generators
// ---------------------------------------------------------------------------

rc::Gen<std::vector<std::string>> genUniqueProps(int max_count) {
    return rc::gen::map(
        rc::gen::inRange(1, max_count + 1),
        [](int count) {
            std::vector<std::string> props;
            for (int i = 0; i < count; ++i) {
                props.push_back("prop_" + std::to_string(i));
            }
            return props;
        });
}

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

/**
 * Property 55a: Valid tree_init nodes with unique properties pass validation.
 */
RC_GTEST_PROP(TreeInitializationValidation,
              ValidTreeInitPasses,
              ()) {
    auto props = *genUniqueProps(5);

    SemanticModel model;
    std::filesystem::path file = "init.meld";
    add_tree_init(model, file, "Widget", "tree_init", props);

    MeldFeatureProvider provider(model);
    auto result = provider.validate_tree_init(file, "Widget");

    RC_ASSERT(result.valid_syntax == true);
    RC_ASSERT(result.valid_nesting == true);
    RC_ASSERT(result.errors.empty());
    RC_ASSERT(result.property_names.size() == props.size());
}

/**
 * Property 55b: Duplicate property names produce an error.
 */
RC_GTEST_PROP(TreeInitializationValidation,
              DuplicatePropertiesDetected,
              ()) {
    SemanticModel model;
    std::filesystem::path file = "init.meld";
    // Add duplicate property names
    add_tree_init(model, file, "BadWidget", "tree_init",
                  {"name", "value", "name"});

    MeldFeatureProvider provider(model);
    auto result = provider.validate_tree_init(file, "BadWidget");

    RC_ASSERT(result.valid_syntax == true);
    RC_ASSERT(!result.errors.empty());
    // Should mention duplicate
    bool found_dup = false;
    for (const auto& err : result.errors) {
        if (err.find("Duplicate") != std::string::npos) {
            found_dup = true;
            break;
        }
    }
    RC_ASSERT(found_dup);
}

/**
 * Property 55c: Non-tree-init nodes fail validation.
 */
RC_GTEST_PROP(TreeInitializationValidation,
              NonTreeInitFails,
              ()) {
    SemanticModel model;
    std::filesystem::path file = "init.meld";
    add_tree_init(model, file, "RegularFn", "function_definition", {"x"});

    MeldFeatureProvider provider(model);
    auto result = provider.validate_tree_init(file, "RegularFn");

    RC_ASSERT(result.valid_syntax == false);
    RC_ASSERT(!result.errors.empty());
}

/**
 * Property 55d: Nested properties are validated for duplicates.
 */
RC_GTEST_PROP(TreeInitializationValidation,
              NestedPropertiesValidated,
              ()) {
    auto props = *genUniqueProps(3);

    SemanticModel model;
    std::filesystem::path file = "init.meld";
    add_tree_init(model, file, "NestedWidget", "tree_init", props, true);

    MeldFeatureProvider provider(model);
    auto result = provider.validate_tree_init(file, "NestedWidget");

    RC_ASSERT(result.valid_syntax == true);
    // Unique nested properties should pass nesting validation
    RC_ASSERT(result.valid_nesting == true);
}

/**
 * Property 55e: Missing type returns an error.
 */
RC_GTEST_PROP(TreeInitializationValidation,
              MissingTypeReturnsError,
              ()) {
    SemanticModel model;
    MeldFeatureProvider provider(model);

    auto result = provider.validate_tree_init("init.meld", "NonExistent");

    RC_ASSERT(result.valid_syntax == false);
    RC_ASSERT(!result.errors.empty());
}

}  // namespace
}  // namespace meld::daemon
