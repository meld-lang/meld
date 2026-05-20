#include "meld/compiler/llvm/codegen.hpp"
#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/get.hpp>

namespace meld::compiler::llvm_backend {

LLVMCodegen::LLVMCodegen() = default;

std::string LLVMCodegen::next_string_id() {
    return "@.str." + std::to_string(string_counter_++);
}

void LLVMCodegen::emit_prelude() {
    ir_ << "; ModuleID = 'meld_module'\n";
    ir_ << "source_filename = \"meld_module\"\n";
    ir_ << "target triple = \"arm64-apple-macosx14.0.0\"\n\n";
    ir_ << "declare i32 @puts(ptr noundef)\n\n";
}

void LLVMCodegen::emit_string_constants() {
    for (size_t i = 0; i < string_constants_.size(); ++i) {
        const auto& s = string_constants_[i];
        ir_ << "@.str." << i << " = private unnamed_addr constant ["
            << (s.size() + 1) << " x i8] c\"" << s << "\\00\"\n";
    }
    if (!string_constants_.empty()) ir_ << "\n";
}

void LLVMCodegen::emit_println(const std::string& value) {
    auto id = next_string_id();
    string_constants_.push_back(value);
    ir_ << "  %call = call i32 @puts(ptr noundef " << id << ")\n";
}

void LLVMCodegen::emit_function(const parser::ast::function_definition& fn) {
    bool is_main = (fn.name.name == "main");
    if (is_main) {
        ir_ << "define i32 @main() {\n";
        ir_ << "entry:\n";
    } else {
        ir_ << "define void @" << fn.name.name << "() {\n";
        ir_ << "entry:\n";
    }

    // Emit body — walk statements looking for function calls to println
    for (const auto& stmt : fn.body.get().statements) {
        if (auto* call = boost::get<boost::spirit::x3::forward_ast<parser::ast::function_call>>(&stmt.get())) {
            if (call->get().function_name.name == "println") {
                if (!call->get().arguments.empty()) {
                    if (auto* str = boost::get<parser::ast::string_literal>(&call->get().arguments[0].get())) {
                        emit_println(str->value);
                    }
                }
            }
        }
    }

    if (is_main) {
        ir_ << "  ret i32 0\n";
    } else {
        ir_ << "  ret void\n";
    }
    ir_ << "}\n\n";
}

std::string LLVMCodegen::emit_program(const std::vector<parser::ast::expression>& ast) {
    // First pass: collect functions
    std::vector<const parser::ast::function_definition*> functions;
    for (const auto& expr : ast) {
        if (auto* fn = boost::get<boost::spirit::x3::forward_ast<parser::ast::function_definition>>(&expr)) {
            functions.push_back(&fn->get());
        }
    }

    // Emit string constants first (we need a pre-pass for println args)
    for (auto* fn : functions) {
        for (const auto& stmt : fn->body.get().statements) {
            if (auto* call = boost::get<boost::spirit::x3::forward_ast<parser::ast::function_call>>(&stmt.get())) {
                if (call->get().function_name.name == "println" && !call->get().arguments.empty()) {
                    if (auto* str = boost::get<parser::ast::string_literal>(&call->get().arguments[0].get())) {
                        string_constants_.push_back(str->value);
                    }
                }
            }
        }
    }

    // Reset counter for emission
    string_counter_ = 0;

    // Emit IR
    emit_prelude();
    emit_string_constants();

    string_counter_ = 0;
    for (auto* fn : functions) {
        emit_function(*fn);
    }

    return ir_.str();
}

} // namespace meld::compiler::llvm_backend
