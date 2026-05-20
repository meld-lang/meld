// rust_backend.hpp — Rust Code Generation Backend (Req 34.2)
// Transpiles Meld IR to Rust source code

#pragma once
#include "meld/compiler/ir.hpp"
#include <string>
#include <map>

namespace meld::compiler {

struct RustGenerationOptions {
    std::string edition = "2021";
    bool generate_cargo_toml = true;
    bool use_anyhow = true;         // Use anyhow for error handling
};

class RustBackend {
public:
    explicit RustBackend(const RustGenerationOptions& options = {});

    std::map<std::string, std::string> generate_rust_files(const ir::Module& module);

private:
    RustGenerationOptions options_;
    std::map<std::string, std::string> type_mappings_;

    void initialize_type_mappings();
    std::string map_type(const std::string& meld_type) const;
    std::string sanitize_rust_identifier(const std::string& name) const;
    std::string generate_rust_module(const ir::Module& module);
    std::string generate_cargo_toml(const ir::Module& module);
    std::string generate_rust_struct(const std::string& name, const ir::Module& module);
    std::string generate_rust_function(const ir::Function& func);
    std::string generate_rust_instruction(const ir::Instruction& inst);
};

} // namespace meld::compiler
