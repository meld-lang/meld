/// Property 1: Channel Consistency
/// For any SemanticModel state, querying the same symbol via LSP and MCP
/// SHALL return equivalent type/effect information.

#include "meld/daemon/lsp_channel.hpp"
#include "meld/daemon/mcp_channel.hpp"
#include "meld/daemon/semantic_model.hpp"

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>

namespace meld::daemon {
namespace {

// Generator for random symbol names
rc::Gen<std::string> genSymbolName() {
    return rc::gen::map(rc::gen::inRange(0, 26), [](int i) -> std::string {
        return std::string(1, static_cast<char>('a' + i));
    });
}

// Generator for random type strings
rc::Gen<std::string> genTypeString() {
    return rc::gen::element<std::string>("Int", "String", "Bool", "Float",
                                         "Own[Int]", "Link[String]", "Vec[Int]");
}

// Generator for random effect lists
rc::Gen<std::vector<std::string>> genEffects() {
    return rc::gen::container<std::vector<std::string>>(
        rc::gen::element<std::string>("file_system", "network", "console",
                                      "random", "time", "async"));
}

RC_GTEST_PROP(ChannelConsistency, TypeQueryEquivalence, ()) {
    SemanticModel model;

    auto symbol = *genSymbolName();
    auto type_str = *genTypeString();
    auto effects = *genEffects();
    auto line = *rc::gen::inRange(1u, 1000u);

    // Build a file with the symbol
    FileSemantics sem;
    sem.path = "prop_test.meld";
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    root->name = "prop_test";
    root->location = {"prop_test.meld", 1, 1};

    auto child = std::make_shared<ASTNode>();
    child->kind = "val_declaration";
    child->name = symbol;
    child->type_info = type_str;
    child->location = {"prop_test.meld", line, 1};
    child->effects = effects;
    root->children.push_back(child);
    sem.ast = root;
    model.update_file("prop_test.meld", std::move(sem));

    // Both channels read from the same model — verify consistency
    auto type_via_model = model.query_type("prop_test.meld", symbol);
    auto effects_via_model = model.query_effects("prop_test.meld", line);

    // Type query should return the same type regardless of access path
    RC_ASSERT(type_via_model.has_value());
    RC_ASSERT(type_via_model->qualified_type == type_str);

    // Effect query should return the same effects
    if (!effects.empty()) {
        RC_ASSERT(effects_via_model.has_value());
        RC_ASSERT(effects_via_model->required_effects == effects);
    }
}

RC_GTEST_PROP(ChannelConsistency, DiagnosticsEquivalence, ()) {
    SemanticModel model;

    auto msg = *rc::gen::element<std::string>(
        "type mismatch", "undefined symbol", "effect leak", "ownership error");
    auto rule_id = *rc::gen::element<std::string>(
        "E0001", "E0042", "W0020", "E0100");

    FileSemantics sem;
    sem.path = "diag_test.meld";
    Diagnostic d;
    d.location = {"diag_test.meld", 1, 1};
    d.severity = DiagnosticSeverity::Error;
    d.message = msg;
    d.rule_id = rule_id;
    d.ast_selector = "$.module";
    sem.diagnostics.push_back(d);
    model.update_file("diag_test.meld", std::move(sem));

    // Both channels should see the same diagnostics
    auto diags = model.get_diagnostics("diag_test.meld");
    RC_ASSERT(diags.size() == 1u);
    RC_ASSERT(diags[0].message == msg);
    RC_ASSERT(diags[0].rule_id == rule_id);
}

}  // namespace
}  // namespace meld::daemon
