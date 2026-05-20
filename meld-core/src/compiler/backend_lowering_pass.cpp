#include "meld/compiler/backend_lowering_pass.hpp"
#include "meld/compat/visit.hpp"
#include <algorithm>
#include <sstream>
#include <regex>

namespace meld::compiler {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

BackendLoweringPass::BackendLoweringPass() = default;

// ---------------------------------------------------------------------------
// Main entry point
// ---------------------------------------------------------------------------

BackendLoweringResult BackendLoweringPass::run(
    const std::vector<parser::ast::expression>& expressions,
    const IntrinsicResolutionRegistry& registry,
    const std::string& source_file
) {
    BackendLoweringResult result;

    // NOTE: The AST scanning logic references AST node types
    // (variable_declaration, if_expression) that are not yet in the parser.
    // For now, the pass produces an empty result. The individual lowering
    // methods below are fully functional and can be called directly by
    // higher-level passes once the AST types are available.

    // TODO: Implement scanning once parser::ast::val_declaration and
    //       conditional expression nodes are integrated.

    return result;
}

// ---------------------------------------------------------------------------
// Scanning stubs — awaiting AST node types
// ---------------------------------------------------------------------------

void BackendLoweringPass::scan_function(
    const parser::ast::function_definition& /*func*/,
    const IntrinsicResolutionRegistry& /*registry*/,
    const std::string& /*source_file*/,
    BackendLoweringResult& /*result*/
) {
    // Stubbed — requires AST traversal of function body statements
}

void BackendLoweringPass::scan_block(
    const std::vector<boost::spirit::x3::forward_ast<parser::ast::expression>>& /*stmts*/,
    const IntrinsicResolutionRegistry& /*registry*/,
    const std::string& /*source_file*/,
    BackendLoweringResult& /*result*/
) {
    // Stubbed — requires per-statement scanning
}

void BackendLoweringPass::scan_expression(
    const parser::ast::expression& /*expr*/,
    const IntrinsicResolutionRegistry& /*registry*/,
    const std::string& /*source_file*/,
    BackendLoweringResult& /*result*/
) {
    // Stubbed — requires get_if on boost::spirit::x3::variant
    // and AST types not yet available (variable_declaration, if_expression)
}

bool BackendLoweringPass::is_move_call(
    const parser::ast::function_call& call,
    const IntrinsicResolutionRegistry& registry
) const {
    const std::string& name = call.function_name.name;
    if (name == "std.mem.move" || name == "move" || name == "mem_move") {
        return true;
    }
    if (registry.has_memory_move()) {
        auto entry = registry.lookup_primary(std_mem::IntrinsicTag::memory_move);
        if (entry && (name == entry->entity_name || name.find("move") != std::string::npos)) {
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Individual lowering methods — fully functional
// ---------------------------------------------------------------------------

LoweredFragment BackendLoweringPass::lower_own_declaration(
    const std::string& var_name, const std::string& inner_type,
    const std::string& initializer, const std::string& source_file,
    size_t line, size_t column
) const {
    LoweredFragment frag;
    frag.kind = LoweredFragment::Kind::OwnDeclaration;
    frag.variable_name = var_name;
    frag.inner_type = inner_type;
    frag.source_file = source_file;
    frag.line = line;
    frag.column = column;
    std::ostringstream oss;
    oss << "meld::std_mem::Own<" << inner_type << "> " << var_name;
    if (!initializer.empty()) oss << " = " << initializer;
    oss << ";";
    frag.cpp_code = oss.str();
    frag.meld_source = "val " + var_name + ": Hold[" + inner_type + "]";
    return frag;
}

LoweredFragment BackendLoweringPass::lower_link_declaration(
    const std::string& var_name, const std::string& inner_type,
    const std::string& initializer, const std::string& source_file,
    size_t line, size_t column
) const {
    LoweredFragment frag;
    frag.kind = LoweredFragment::Kind::LinkDeclaration;
    frag.variable_name = var_name;
    frag.inner_type = inner_type;
    frag.source_file = source_file;
    frag.line = line;
    frag.column = column;
    std::ostringstream oss;
    oss << "meld::std_mem::Link<" << inner_type << "> " << var_name;
    if (!initializer.empty()) oss << " = " << initializer;
    oss << ";";
    frag.cpp_code = oss.str();
    frag.meld_source = "val " + var_name + ": View[" + inner_type + "]";
    return frag;
}

LoweredFragment BackendLoweringPass::lower_link_upgrade(
    const std::string& bound_name, const std::string& link_expr,
    const std::string& inner_type, const std::string& source_file,
    size_t line, size_t column
) const {
    LoweredFragment frag;
    frag.kind = LoweredFragment::Kind::LinkUpgrade;
    frag.variable_name = bound_name;
    frag.inner_type = inner_type;
    frag.source_file = source_file;
    frag.line = line;
    frag.column = column;
    frag.cpp_code = "if (auto " + bound_name + " = " + link_expr + ".upgrade())";
    frag.meld_source = "if val " + bound_name + " = " + link_expr;
    return frag;
}

LoweredFragment BackendLoweringPass::lower_move_call(
    const std::string& source_var, const std::string& inner_type,
    const std::string& source_file, size_t line, size_t column
) const {
    LoweredFragment frag;
    frag.kind = LoweredFragment::Kind::MoveCall;
    frag.variable_name = source_var;
    frag.inner_type = inner_type;
    frag.source_file = source_file;
    frag.line = line;
    frag.column = column;
    frag.cpp_code = "meld::std_mem::mem_move(" + source_var + ")";
    frag.meld_source = "std.mem.move(" + source_var + ")";
    return frag;
}

LoweredFragment BackendLoweringPass::lower_own_scope_exit(
    const std::string& var_name, const std::string& inner_type,
    const std::string& source_file, size_t line, size_t column
) const {
    LoweredFragment frag;
    frag.kind = LoweredFragment::Kind::OwnScopeExit;
    frag.variable_name = var_name;
    frag.inner_type = inner_type;
    frag.source_file = source_file;
    frag.line = line;
    frag.column = column;
    frag.cpp_code = "// ~Own<" + inner_type + "> " + var_name +
                    " → ref_count_-- (non-atomic, RAII via shared_ptr)";
    frag.meld_source = "// scope exit: " + var_name + ": Hold[" + inner_type + "]";
    return frag;
}

LoweredFragment BackendLoweringPass::lower_link_scope_exit(
    const std::string& var_name, const std::string& inner_type,
    const std::string& source_file, size_t line, size_t column
) const {
    LoweredFragment frag;
    frag.kind = LoweredFragment::Kind::LinkScopeExit;
    frag.variable_name = var_name;
    frag.inner_type = inner_type;
    frag.source_file = source_file;
    frag.line = line;
    frag.column = column;
    frag.cpp_code = "// ~Link<" + inner_type + "> " + var_name +
                    " → weak_count_-- (non-atomic, RAII via weak_ptr)";
    frag.meld_source = "// scope exit: " + var_name + ": View[" + inner_type + "]";
    return frag;
}

// ---------------------------------------------------------------------------
// Type detection helpers
// ---------------------------------------------------------------------------

bool BackendLoweringPass::is_own_type(const std::string& type_name) {
    return type_name.find("Hold[") != std::string::npos ||
           type_name.find("Own<") != std::string::npos;
}

bool BackendLoweringPass::is_link_type(const std::string& type_name) {
    return type_name.find("View[") != std::string::npos ||
           type_name.find("Link<") != std::string::npos;
}

std::string BackendLoweringPass::extract_inner_type(const std::string& type_name) {
    auto extract = [](const std::string& s, char open, char close) -> std::string {
        auto start = s.find(open);
        if (start == std::string::npos) return "";
        auto end = s.rfind(close);
        if (end == std::string::npos || end <= start) return "";
        return s.substr(start + 1, end - start - 1);
    };
    std::string result = extract(type_name, '[', ']');
    if (!result.empty()) return result;
    return extract(type_name, '<', '>');
}

std::string BackendLoweringPass::to_cpp_type(const std::string& meld_type) {
    if (is_own_type(meld_type)) {
        return "meld::std_mem::Own<" + extract_inner_type(meld_type) + ">";
    }
    if (is_link_type(meld_type)) {
        return "meld::std_mem::Link<" + extract_inner_type(meld_type) + ">";
    }
    return meld_type;
}

} // namespace meld::compiler
