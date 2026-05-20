/**
 * **Feature: meld-daemon, Property 28: Function Signature Help**
 *
 * For any function call context, signature help SHALL provide accurate
 * parameter information and documentation.
 *
 * **Validates: Requirements 16.3**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "meld/daemon/completion_provider.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace meld::daemon {
namespace {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Build a model with a function that has the given parameters.
void populate_with_function(SemanticModel& model,
                            const std::filesystem::path& file,
                            const std::string& func_name,
                            const std::vector<std::pair<std::string, std::string>>& params,
                            const std::string& return_type) {
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";

    auto func = std::make_shared<ASTNode>();
    func->kind = "function_definition";
    func->name = func_name;
    func->type_info = return_type;

    for (const auto& [pname, ptype] : params) {
        auto param = std::make_shared<ASTNode>();
        param->kind = "parameter";
        param->name = pname;
        param->type_info = ptype;
        func->children.push_back(param);
    }

    root->children.push_back(func);

    FileSemantics sem;
    sem.path = file;
    sem.ast = root;
    model.update_file(file, std::move(sem));
}

// ---------------------------------------------------------------------------
// Generators
// ---------------------------------------------------------------------------

rc::Gen<std::string> genParamName() {
    return rc::gen::map(rc::gen::inRange(0, 10), [](int i) {
        return "param" + std::to_string(i);
    });
}

rc::Gen<std::string> genTypeName() {
    return rc::gen::elementOf(std::vector<std::string>{
        "Int", "Float", "String", "Bool", "Unit"
    });
}

rc::Gen<std::pair<std::string, std::string>> genParam() {
    return rc::gen::pair(genParamName(), genTypeName());
}

rc::Gen<std::string> genFuncName() {
    return rc::gen::map(rc::gen::inRange(1, 6), [](int len) {
        std::string s = "fn";
        for (int i = 0; i < len; ++i)
            s += static_cast<char>('a' + (i % 26));
        return s;
    });
}

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

/**
 * Property 28a: Signature help returns the correct number of parameters.
 */
RC_GTEST_PROP(FunctionSignatureHelpProperty,
              SignatureHelpReturnsCorrectParameterCount,
              ()) {
    auto func_name = *genFuncName();
    auto params = *rc::gen::container<std::vector<std::pair<std::string, std::string>>>(
        3, genParam());
    auto ret_type = *genTypeName();

    SemanticModel model;
    std::filesystem::path file = "test.meld";
    populate_with_function(model, file, func_name, params, ret_type);

    CompletionProvider provider(model);
    std::string line_text = func_name + "(";
    auto result = provider.get_signature_help(
        file, 5, static_cast<uint32_t>(line_text.size()), line_text);

    RC_ASSERT(result.signatures.size() == 1);
    RC_ASSERT(result.signatures[0].parameters.size() == params.size());
}

/**
 * Property 28b: Signature help parameter names match the function definition.
 */
RC_GTEST_PROP(FunctionSignatureHelpProperty,
              SignatureHelpParameterNamesMatch,
              ()) {
    auto func_name = *genFuncName();
    auto params = *rc::gen::container<std::vector<std::pair<std::string, std::string>>>(
        3, genParam());
    auto ret_type = *genTypeName();

    SemanticModel model;
    std::filesystem::path file = "test.meld";
    populate_with_function(model, file, func_name, params, ret_type);

    CompletionProvider provider(model);
    std::string line_text = func_name + "(";
    auto result = provider.get_signature_help(
        file, 5, static_cast<uint32_t>(line_text.size()), line_text);

    RC_ASSERT(!result.signatures.empty());
    const auto& sig = result.signatures[0];
    for (size_t i = 0; i < params.size(); ++i) {
        RC_ASSERT(sig.parameters[i].name == params[i].first);
        RC_ASSERT(sig.parameters[i].type == params[i].second);
    }
}

/**
 * Property 28c: Active parameter index advances with commas.
 */
RC_GTEST_PROP(FunctionSignatureHelpProperty,
              ActiveParameterAdvancesWithCommas,
              ()) {
    auto func_name = *genFuncName();
    int param_count = *rc::gen::inRange(2, 5);
    std::vector<std::pair<std::string, std::string>> params;
    for (int i = 0; i < param_count; ++i) {
        params.emplace_back("p" + std::to_string(i), "Int");
    }

    SemanticModel model;
    std::filesystem::path file = "test.meld";
    populate_with_function(model, file, func_name, params, "Int");

    CompletionProvider provider(model);

    // Test with 0 commas (first param)
    std::string line0 = func_name + "(x";
    auto r0 = provider.get_signature_help(
        file, 5, static_cast<uint32_t>(line0.size()), line0);
    RC_ASSERT(!r0.signatures.empty());
    RC_ASSERT(r0.signatures[0].active_parameter == 0);

    // Test with 1 comma (second param)
    std::string line1 = func_name + "(x, y";
    auto r1 = provider.get_signature_help(
        file, 5, static_cast<uint32_t>(line1.size()), line1);
    RC_ASSERT(!r1.signatures.empty());
    RC_ASSERT(r1.signatures[0].active_parameter == 1);
}

/**
 * Property 28d: Signature help for unknown function returns empty.
 */
RC_GTEST_PROP(FunctionSignatureHelpProperty,
              UnknownFunctionReturnsEmptySignature,
              ()) {
    auto known_name = *genFuncName();
    auto unknown_name = known_name + "_unknown";

    SemanticModel model;
    std::filesystem::path file = "test.meld";
    populate_with_function(model, file, known_name,
                           {{"x", "Int"}}, "Int");

    CompletionProvider provider(model);
    std::string line_text = unknown_name + "(";
    auto result = provider.get_signature_help(
        file, 5, static_cast<uint32_t>(line_text.size()), line_text);

    RC_ASSERT(result.signatures.empty());
}

}  // namespace
}  // namespace meld::daemon
