/**
 * **Feature: meld-daemon, Property 40: Hover Information Accuracy**
 *
 * For any symbol, hover SHALL display correct type information,
 * documentation, and signature details.
 *
 * **Validates: Requirements 18.5**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "meld/daemon/navigation_provider.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace meld::daemon {
namespace {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void populate_model(SemanticModel& model, const std::filesystem::path& file,
                    const std::vector<std::shared_ptr<ASTNode>>& nodes) {
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    root->name = "";
    root->children = nodes;
    FileSemantics sem;
    sem.path = file;
    sem.ast = root;
    model.update_file(file, std::move(sem));
}

std::shared_ptr<ASTNode> make_function(const std::string& name,
                                        const std::string& return_type,
                                        const std::vector<std::string>& effects = {}) {
    auto node = std::make_shared<ASTNode>();
    node->kind = "function_definition";
    node->name = name;
    node->type_info = return_type;
    node->effects = effects;
    return node;
}

std::shared_ptr<ASTNode> make_variable(const std::string& name,
                                        const std::string& type) {
    auto node = std::make_shared<ASTNode>();
    node->kind = "val_declaration";
    node->name = name;
    node->type_info = type;
    return node;
}

std::shared_ptr<ASTNode> make_type(const std::string& name,
                                    const std::string& kind = "struct") {
    auto node = std::make_shared<ASTNode>();
    node->kind = kind;
    node->name = name;
    node->type_info = "";
    return node;
}

// ---------------------------------------------------------------------------
// Generators
// ---------------------------------------------------------------------------

rc::Gen<std::string> genIdentifier() {
    return rc::gen::map(
        rc::gen::inRange(1, 8),
        [](int len) {
            std::string s;
            s.reserve(len);
            for (int i = 0; i < len; ++i)
                s += static_cast<char>('a' + (i % 26));
            return s;
        });
}

rc::Gen<std::string> genType() {
    return rc::gen::elementOf(std::vector<std::string>{
        "Int", "String", "Bool", "Float",
        "() -> Int", "(Int) -> String"});
}

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

/**
 * Property 40a: Hover on a function returns its name, type signature,
 * and kind == "function".
 */
RC_GTEST_PROP(HoverInformationProperty,
              FunctionHoverShowsTypeAndKind,
              ()) {
    auto name = *genIdentifier();
    auto ret_type = *genType();

    SemanticModel model;
    std::filesystem::path file = "test.meld";
    populate_model(model, file, {make_function(name, ret_type)});

    NavigationProvider provider(model);
    auto hover = provider.get_hover(file, name);

    RC_ASSERT(hover.has_value());
    RC_ASSERT(hover->name == name);
    RC_ASSERT(hover->kind == "function");
    RC_ASSERT(hover->type_signature == ret_type);
    // Documentation should mention the function name
    RC_ASSERT(hover->documentation.find(name) != std::string::npos);
}

/**
 * Property 40b: Hover on a variable returns its type information.
 */
RC_GTEST_PROP(HoverInformationProperty,
              VariableHoverShowsType,
              ()) {
    auto name = *genIdentifier();
    auto type = *genType();

    SemanticModel model;
    std::filesystem::path file = "test.meld";
    populate_model(model, file, {make_variable(name, type)});

    NavigationProvider provider(model);
    auto hover = provider.get_hover(file, name);

    RC_ASSERT(hover.has_value());
    RC_ASSERT(hover->name == name);
    RC_ASSERT(hover->kind == "variable");
    RC_ASSERT(hover->type_signature == type);
}

/**
 * Property 40c: Hover on a type (struct/enum/trait) returns kind info.
 */
RC_GTEST_PROP(HoverInformationProperty,
              TypeHoverShowsKind,
              ()) {
    auto name = *genIdentifier();
    auto kind = *rc::gen::elementOf(std::vector<std::string>{
        "struct", "enum", "trait"});

    SemanticModel model;
    std::filesystem::path file = "test.meld";
    populate_model(model, file, {make_type(name, kind)});

    NavigationProvider provider(model);
    auto hover = provider.get_hover(file, name);

    RC_ASSERT(hover.has_value());
    RC_ASSERT(hover->name == name);
    RC_ASSERT(hover->kind == kind);
    // Documentation should mention the kind
    RC_ASSERT(hover->documentation.find(kind) != std::string::npos);
}

/**
 * Property 40d: Hover on a function with effects includes effect annotations.
 */
RC_GTEST_PROP(HoverInformationProperty,
              FunctionWithEffectsShowsEffects,
              ()) {
    auto name = *genIdentifier();
    auto effects = *rc::gen::container<std::vector<std::string>>(
        3, rc::gen::elementOf(std::vector<std::string>{
            "IO", "Network", "State"}));

    SemanticModel model;
    std::filesystem::path file = "test.meld";
    populate_model(model, file, {make_function(name, "Int", effects)});

    NavigationProvider provider(model);
    auto hover = provider.get_hover(file, name);

    RC_ASSERT(hover.has_value());
    RC_ASSERT(hover->effects.size() == effects.size());
    for (size_t i = 0; i < effects.size(); ++i) {
        RC_ASSERT(hover->effects[i] == effects[i]);
    }
}

/**
 * Property 40e: Hover on a non-existent symbol returns nullopt.
 */
RC_GTEST_PROP(HoverInformationProperty,
              NonExistentSymbolReturnsNullopt,
              ()) {
    SemanticModel model;
    std::filesystem::path file = "test.meld";
    populate_model(model, file, {make_variable("existing", "Int")});

    NavigationProvider provider(model);
    auto hover = provider.get_hover(file, "nonexistent_xyz");
    RC_ASSERT(!hover.has_value());
}

/**
 * Property 40f: Cross-file hover — symbol defined in another file is found.
 */
RC_GTEST_PROP(HoverInformationProperty,
              CrossFileHoverFindsSymbol,
              ()) {
    auto name = *genIdentifier();
    auto type = *genType();

    SemanticModel model;
    std::filesystem::path file_a = "a.meld";
    std::filesystem::path file_b = "b.meld";
    populate_model(model, file_a, {});
    populate_model(model, file_b, {make_variable(name, type)});

    NavigationProvider provider(model);
    auto hover = provider.get_hover(file_a, name);

    RC_ASSERT(hover.has_value());
    RC_ASSERT(hover->name == name);
    RC_ASSERT(hover->type_signature == type);
}

}  // namespace
}  // namespace meld::daemon
