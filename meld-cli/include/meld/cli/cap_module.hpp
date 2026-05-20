#pragma once

#include "command_handler.hpp"
#include "meld/compiler/cap.hpp"
#include <memory>
#include <filesystem>

namespace meld::cli {

// CAP (Compiler-Agent Protocol) module for structured compilation output
class CAPModule : public BaseCommandHandler {
public:
    CAPModule();
    
    // BaseCommandHandler interface
    CommandResult execute(const CommandArgs& args) override;
    std::string get_help() const override;
    std::string get_usage() const override;
    std::vector<std::string> get_completions(const std::string& partial) const override;
    bool validate_args(const CommandArgs& args, std::string& error_message) const override;
    
private:
    // CAP-specific operations
    CommandResult compile_single_file(const std::filesystem::path& file_path, 
                                    const CommandArgs& args);
    CommandResult compile_batch_files(const std::vector<std::filesystem::path>& files,
                                    const CommandArgs& args);
    CommandResult compile_incremental(const std::vector<std::filesystem::path>& changed_files,
                                    const CommandArgs& args);
    
    // Output formatting
    void output_json(const compiler::cap::CompilationResult& result, bool pretty) const;
    void output_json(const compiler::cap::BatchCompilationResult& result, bool pretty) const;
    void output_summary(const compiler::cap::CompilationResult& result) const;
    void output_summary(const compiler::cap::BatchCompilationResult& result) const;
    
    // Configuration parsing
    void configure_cap_from_args(const CommandArgs& args);
    
    // CAP instance
    std::unique_ptr<compiler::cap::CompilerAgentProtocol> cap_;
};

} // namespace meld::cli