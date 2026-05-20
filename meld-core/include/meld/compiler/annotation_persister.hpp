#pragma once

#include "meld/parser/ast.hpp"
#include "meld/compiler/ide_integration.hpp"
#include <string>
#include <vector>
#include <set>
#include <map>
#include <fstream>
#include <sstream>

namespace meld::compiler {

// Source modification result
struct SourceModificationResult {
    bool success = false;
    std::string modified_source;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    size_t annotations_added = 0;
    size_t annotations_updated = 0;
    size_t annotations_removed = 0;
};

// Annotation persistence manager
class AnnotationPersister {
public:
    AnnotationPersister();
    
    // TASK 35.10: Write inferred @uses(...) annotations to source file on save/format
    // Requirement 41.9: Write inferred @uses annotations into the source code
    SourceModificationResult write_annotations_to_source(
        const std::string& source_code,
        const std::vector<parser::ast::function_definition>& functions,
        const std::map<std::string, std::set<std::string>>& inferred_effects);
    
    // TASK 35.10: Update annotations automatically when implementation changes
    // Requirement 41.10: Update @uses annotation on next save when function changes
    SourceModificationResult update_annotations_in_source(
        const std::string& source_code,
        const std::vector<parser::ast::function_definition>& functions,
        const std::map<std::string, std::set<std::string>>& inferred_effects);
    
    // Write annotations to file on disk
    bool persist_annotations_to_file(
        const std::string& file_path,
        const std::vector<parser::ast::function_definition>& functions,
        const std::map<std::string, std::set<std::string>>& inferred_effects);
    
    // Check if source file needs annotation updates
    bool needs_annotation_update(
        const std::string& source_code,
        const std::vector<parser::ast::function_definition>& functions,
        const std::map<std::string, std::set<std::string>>& inferred_effects);
    
    // Get functions that need annotation updates
    std::vector<std::string> get_functions_needing_updates(
        const std::string& source_code,
        const std::vector<parser::ast::function_definition>& functions,
        const std::map<std::string, std::set<std::string>>& inferred_effects);
    
    // Configuration
    void set_preserve_manual_annotations(bool preserve) { preserve_manual_annotations_ = preserve; }
    void set_add_blank_line_before_annotation(bool add_blank_line) { add_blank_line_before_annotation_ = add_blank_line; }
    void set_annotation_style(const std::string& style) { annotation_style_ = style; }
    
private:
    // Configuration options
    bool preserve_manual_annotations_ = true;
    bool add_blank_line_before_annotation_ = true;
    std::string annotation_style_ = "compact"; // "compact" or "multiline"
    
    // Internal methods
    std::vector<std::string> split_lines(const std::string& source_code);
    std::string join_lines(const std::vector<std::string>& lines);
    
    // Find function in source lines
    int find_function_line(const std::vector<std::string>& lines, 
                          const parser::ast::function_definition& func_def);
    
    // Check if function already has @uses annotation
    bool has_uses_annotation(const std::vector<std::string>& lines, int function_line);
    
    // Extract existing @uses annotation
    std::set<std::string> extract_existing_uses_annotation(
        const std::vector<std::string>& lines, int function_line);
    
    // Generate @uses annotation text
    std::string generate_uses_annotation_text(const std::set<std::string>& effects);
    
    // Insert annotation before function
    void insert_annotation_before_function(
        std::vector<std::string>& lines,
        int function_line,
        const std::string& annotation_text);
    
    // Update existing annotation
    void update_existing_annotation(
        std::vector<std::string>& lines,
        int annotation_line,
        const std::string& new_annotation_text);
    
    // Remove existing annotation
    void remove_existing_annotation(
        std::vector<std::string>& lines,
        int annotation_line);
    
    // Find annotation line for function
    int find_annotation_line(const std::vector<std::string>& lines, int function_line);
    
    // Check if line contains @uses annotation
    bool is_uses_annotation_line(const std::string& line);
    
    // Parse @uses annotation from line
    std::set<std::string> parse_uses_annotation(const std::string& line);
    
    // Check if effects are equivalent
    bool effects_equivalent(const std::set<std::string>& effects1, 
                           const std::set<std::string>& effects2);
    
    // Normalize effects (handle EffectPure)
    std::set<std::string> normalize_effects(const std::set<std::string>& effects);
    
    // Check if annotation is manually written (heuristics)
    bool is_manual_annotation(const std::vector<std::string>& lines, int annotation_line);
    
    // Get indentation for line
    std::string get_line_indentation(const std::string& line);
    
    // Trim whitespace from string
    std::string trim(const std::string& str);
};

} // namespace meld::compiler