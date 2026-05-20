#include "meld/compiler/annotation_persister.hpp"
#include <algorithm>
#include <regex>
#include <iostream>
#include <filesystem>

namespace meld::compiler {

AnnotationPersister::AnnotationPersister() = default;

// TASK 35.10: Write inferred @uses(...) annotations to source file on save/format
// Requirement 41.9: Write inferred @uses annotations into the source code
SourceModificationResult AnnotationPersister::write_annotations_to_source(
    const std::string& source_code,
    const std::vector<parser::ast::function_definition>& functions,
    const std::map<std::string, std::set<std::string>>& inferred_effects) {
    
    SourceModificationResult result;
    
    try {
        auto lines = split_lines(source_code);
        
        // Process functions in reverse order to maintain line numbers
        auto sorted_functions = functions;
        std::sort(sorted_functions.begin(), sorted_functions.end(),
                 [](const auto& a, const auto& b) {
                     // Sort by function name as a stable ordering fallback
                     return a.name.name > b.name.name;
                 });
        
        for (const auto& func_def : sorted_functions) {
            auto effects_it = inferred_effects.find(func_def.name.name);
            if (effects_it == inferred_effects.end()) {
                continue; // No inferred effects for this function
            }
            
            const auto& func_effects = normalize_effects(effects_it->second);
            int function_line = find_function_line(lines, func_def);
            
            if (function_line == -1) {
                result.errors.push_back("Could not find function " + func_def.name.name + " in source");
                continue;
            }
            
            // Check if function already has @uses annotation
            if (has_uses_annotation(lines, function_line)) {
                auto existing_effects = extract_existing_uses_annotation(lines, function_line);
                auto normalized_existing = normalize_effects(existing_effects);
                
                if (!effects_equivalent(normalized_existing, func_effects)) {
                    // Check if this is a manual annotation that should be preserved
                    int annotation_line = find_annotation_line(lines, function_line);
                    if (preserve_manual_annotations_ && is_manual_annotation(lines, annotation_line)) {
                        result.warnings.push_back("Function " + func_def.name.name + 
                                                " has manual @uses annotation that differs from inferred effects");
                        continue;
                    }
                    
                    // Update existing annotation
                    std::string new_annotation = generate_uses_annotation_text(func_effects);
                    update_existing_annotation(lines, annotation_line, new_annotation);
                    result.annotations_updated++;
                }
            } else {
                // Add new annotation
                std::string annotation_text = generate_uses_annotation_text(func_effects);
                insert_annotation_before_function(lines, function_line, annotation_text);
                result.annotations_added++;
            }
        }
        
        result.modified_source = join_lines(lines);
        result.success = true;
        
    } catch (const std::exception& e) {
        result.success = false;
        result.errors.push_back("Error writing annotations: " + std::string(e.what()));
    }
    
    return result;
}

// TASK 35.10: Update annotations automatically when implementation changes
// Requirement 41.10: Update @uses annotation on next save when function changes
SourceModificationResult AnnotationPersister::update_annotations_in_source(
    const std::string& source_code,
    const std::vector<parser::ast::function_definition>& functions,
    const std::map<std::string, std::set<std::string>>& inferred_effects) {
    
    // This is essentially the same as write_annotations_to_source but with different semantics
    // The difference is that this method is called specifically when implementations change
    return write_annotations_to_source(source_code, functions, inferred_effects);
}

// Write annotations to file on disk
bool AnnotationPersister::persist_annotations_to_file(
    const std::string& file_path,
    const std::vector<parser::ast::function_definition>& functions,
    const std::map<std::string, std::set<std::string>>& inferred_effects) {
    
    try {
        // Read current file content
        std::ifstream file(file_path);
        if (!file) {
            std::cerr << "Could not read file: " << file_path << std::endl;
            return false;
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        file.close();
        
        std::string source_code = buffer.str();
        
        // Write annotations to source
        auto result = write_annotations_to_source(source_code, functions, inferred_effects);
        
        if (!result.success) {
            std::cerr << "Failed to write annotations for file: " << file_path << std::endl;
            for (const auto& error : result.errors) {
                std::cerr << "  Error: " << error << std::endl;
            }
            return false;
        }
        
        // Create backup if file will be modified
        if (result.annotations_added > 0 || result.annotations_updated > 0) {
            std::string backup_path = file_path + ".bak";
            std::filesystem::copy_file(file_path, backup_path, 
                                     std::filesystem::copy_options::overwrite_existing);
        }
        
        // Write modified content back to file
        std::ofstream output_file(file_path);
        if (!output_file) {
            std::cerr << "Could not write to file: " << file_path << std::endl;
            return false;
        }
        
        output_file << result.modified_source;
        output_file.close();
        
        // Log the changes
        if (result.annotations_added > 0 || result.annotations_updated > 0) {
            std::cout << "Updated annotations in " << file_path << ": "
                      << result.annotations_added << " added, "
                      << result.annotations_updated << " updated" << std::endl;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error persisting annotations to file " << file_path 
                  << ": " << e.what() << std::endl;
        return false;
    }
}

// Check if source file needs annotation updates
bool AnnotationPersister::needs_annotation_update(
    const std::string& source_code,
    const std::vector<parser::ast::function_definition>& functions,
    const std::map<std::string, std::set<std::string>>& inferred_effects) {
    
    auto lines = split_lines(source_code);
    
    for (const auto& func_def : functions) {
        auto effects_it = inferred_effects.find(func_def.name.name);
        if (effects_it == inferred_effects.end()) {
            continue;
        }
        
        const auto& func_effects = normalize_effects(effects_it->second);
        int function_line = find_function_line(lines, func_def);
        
        if (function_line == -1) {
            continue; // Can't find function
        }
        
        if (!has_uses_annotation(lines, function_line)) {
            return true; // Missing annotation
        }
        
        auto existing_effects = extract_existing_uses_annotation(lines, function_line);
        auto normalized_existing = normalize_effects(existing_effects);
        
        if (!effects_equivalent(normalized_existing, func_effects)) {
            return true; // Annotation needs update
        }
    }
    
    return false;
}

// Get functions that need annotation updates
std::vector<std::string> AnnotationPersister::get_functions_needing_updates(
    const std::string& source_code,
    const std::vector<parser::ast::function_definition>& functions,
    const std::map<std::string, std::set<std::string>>& inferred_effects) {
    
    std::vector<std::string> functions_needing_updates;
    auto lines = split_lines(source_code);
    
    for (const auto& func_def : functions) {
        auto effects_it = inferred_effects.find(func_def.name.name);
        if (effects_it == inferred_effects.end()) {
            continue;
        }
        
        const auto& func_effects = normalize_effects(effects_it->second);
        int function_line = find_function_line(lines, func_def);
        
        if (function_line == -1) {
            continue;
        }
        
        bool needs_update = false;
        
        if (!has_uses_annotation(lines, function_line)) {
            needs_update = true; // Missing annotation
        } else {
            auto existing_effects = extract_existing_uses_annotation(lines, function_line);
            auto normalized_existing = normalize_effects(existing_effects);
            
            if (!effects_equivalent(normalized_existing, func_effects)) {
                needs_update = true; // Annotation differs
            }
        }
        
        if (needs_update) {
            functions_needing_updates.push_back(func_def.name.name);
        }
    }
    
    return functions_needing_updates;
}

// Internal methods

std::vector<std::string> AnnotationPersister::split_lines(const std::string& source_code) {
    std::vector<std::string> lines;
    std::istringstream stream(source_code);
    std::string line;
    
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    
    return lines;
}

std::string AnnotationPersister::join_lines(const std::vector<std::string>& lines) {
    std::ostringstream result;
    
    for (size_t i = 0; i < lines.size(); ++i) {
        result << lines[i];
        if (i < lines.size() - 1) {
            result << "\n";
        }
    }
    
    return result.str();
}

// Find function in source lines
int AnnotationPersister::find_function_line(const std::vector<std::string>& lines, 
                                           const parser::ast::function_definition& func_def) {
    
    // Look for function declaration pattern: "fnc function_name"
    std::string pattern = "fnc\\s+" + func_def.name.name + "\\s*\\(";
    std::regex func_regex(pattern);
    
    for (size_t i = 0; i < lines.size(); ++i) {
        if (std::regex_search(lines[i], func_regex)) {
            return static_cast<int>(i);
        }
    }
    
    // Fallback: look for just the function name with parentheses
    std::string fallback_pattern = func_def.name.name + "\\s*\\(";
    std::regex fallback_regex(fallback_pattern);
    
    for (size_t i = 0; i < lines.size(); ++i) {
        if (std::regex_search(lines[i], fallback_regex)) {
            return static_cast<int>(i);
        }
    }
    
    return -1; // Function not found
}

// Check if function already has @uses annotation
bool AnnotationPersister::has_uses_annotation(const std::vector<std::string>& lines, int function_line) {
    // Look backwards from function line to find @uses annotation
    for (int i = function_line - 1; i >= 0 && i >= function_line - 5; --i) {
        std::string trimmed = trim(lines[i]);
        if (trimmed.empty()) {
            continue; // Skip empty lines
        }
        if (is_uses_annotation_line(trimmed)) {
            return true;
        }
        if (!trimmed.empty() && trimmed[0] != '@') {
            break; // Found non-annotation, non-empty line
        }
    }
    return false;
}

// Extract existing @uses annotation
std::set<std::string> AnnotationPersister::extract_existing_uses_annotation(
    const std::vector<std::string>& lines, int function_line) {
    
    for (int i = function_line - 1; i >= 0 && i >= function_line - 5; --i) {
        std::string trimmed = trim(lines[i]);
        if (is_uses_annotation_line(trimmed)) {
            return parse_uses_annotation(trimmed);
        }
        if (!trimmed.empty() && trimmed[0] != '@') {
            break;
        }
    }
    return {};
}

// Generate @uses annotation text
std::string AnnotationPersister::generate_uses_annotation_text(const std::set<std::string>& effects) {
    if (effects.empty()) {
        return "@uses()"; // Pure function
    }
    
    std::ostringstream oss;
    oss << "@uses(";
    
    if (annotation_style_ == "multiline" && effects.size() > 3) {
        // Multi-line style for many effects
        oss << "\n";
        bool first = true;
        for (const auto& effect : effects) {
            if (!first) {
                oss << ",\n";
            }
            oss << "    " << effect;
            first = false;
        }
        oss << "\n)";
    } else {
        // Compact style
        bool first = true;
        for (const auto& effect : effects) {
            if (!first) {
                oss << ", ";
            }
            oss << effect;
            first = false;
        }
        oss << ")";
    }
    
    return oss.str();
}

// Insert annotation before function
void AnnotationPersister::insert_annotation_before_function(
    std::vector<std::string>& lines,
    int function_line,
    const std::string& annotation_text) {
    
    std::string indentation = get_line_indentation(lines[function_line]);
    std::string full_annotation = indentation + annotation_text;
    
    if (add_blank_line_before_annotation_ && function_line > 0 && 
        !trim(lines[function_line - 1]).empty()) {
        // Add blank line before annotation if previous line is not empty
        lines.insert(lines.begin() + function_line, "");
        lines.insert(lines.begin() + function_line + 1, full_annotation);
    } else {
        lines.insert(lines.begin() + function_line, full_annotation);
    }
}

// Update existing annotation
void AnnotationPersister::update_existing_annotation(
    std::vector<std::string>& lines,
    int annotation_line,
    const std::string& new_annotation_text) {
    
    std::string indentation = get_line_indentation(lines[annotation_line]);
    lines[annotation_line] = indentation + new_annotation_text;
}

// Remove existing annotation
void AnnotationPersister::remove_existing_annotation(
    std::vector<std::string>& lines,
    int annotation_line) {
    
    lines.erase(lines.begin() + annotation_line);
}

// Find annotation line for function
int AnnotationPersister::find_annotation_line(const std::vector<std::string>& lines, int function_line) {
    for (int i = function_line - 1; i >= 0 && i >= function_line - 5; --i) {
        std::string trimmed = trim(lines[i]);
        if (is_uses_annotation_line(trimmed)) {
            return i;
        }
        if (!trimmed.empty() && trimmed[0] != '@') {
            break;
        }
    }
    return -1;
}

// Check if line contains @uses annotation
bool AnnotationPersister::is_uses_annotation_line(const std::string& line) {
    std::string trimmed = trim(line);
    return trimmed.find("@uses(") == 0;
}

// Parse @uses annotation from line
std::set<std::string> AnnotationPersister::parse_uses_annotation(const std::string& line) {
    std::set<std::string> effects;
    
    // Extract content between @uses( and )
    std::regex uses_regex(R"(@uses\s*\(\s*(.*?)\s*\))");
    std::smatch match;
    
    if (std::regex_search(line, match, uses_regex)) {
        std::string effects_str = match[1].str();
        
        if (!effects_str.empty()) {
            // Split by comma and trim each effect
            std::istringstream stream(effects_str);
            std::string effect;
            
            while (std::getline(stream, effect, ',')) {
                effect = trim(effect);
                if (!effect.empty()) {
                    effects.insert(effect);
                }
            }
        }
    }
    
    return effects;
}

// Check if effects are equivalent
bool AnnotationPersister::effects_equivalent(const std::set<std::string>& effects1, 
                                           const std::set<std::string>& effects2) {
    return effects1 == effects2;
}

// Normalize effects (handle EffectPure)
std::set<std::string> AnnotationPersister::normalize_effects(const std::set<std::string>& effects) {
    std::set<std::string> normalized = effects;
    
    // If we have other effects, remove EffectPure
    if (normalized.size() > 1 && normalized.find("EffectPure") != normalized.end()) {
        normalized.erase("EffectPure");
    }
    
    // If we only have EffectPure or empty set, represent as empty (pure function)
    if (normalized.size() == 1 && normalized.find("EffectPure") != normalized.end()) {
        normalized.clear();
    }
    
    return normalized;
}

// Check if annotation is manually written (heuristics)
bool AnnotationPersister::is_manual_annotation(const std::vector<std::string>& lines, int annotation_line) {
    if (annotation_line < 0 || annotation_line >= static_cast<int>(lines.size())) {
        return false;
    }
    
    // Heuristics to detect manual annotations:
    // 1. Has comments on the same line or nearby
    // 2. Has unusual formatting
    // 3. Has effects that don't match common patterns
    
    std::string line = lines[annotation_line];
    
    // Check for inline comments
    if (line.find("//") != std::string::npos) {
        return true;
    }
    
    // Check for comments on adjacent lines
    if (annotation_line > 0) {
        std::string prev_line = trim(lines[annotation_line - 1]);
        if (prev_line.find("//") != std::string::npos || prev_line.find("/*") != std::string::npos) {
            return true;
        }
    }
    
    if (annotation_line < static_cast<int>(lines.size()) - 1) {
        std::string next_line = trim(lines[annotation_line + 1]);
        if (next_line.find("//") != std::string::npos || next_line.find("/*") != std::string::npos) {
            return true;
        }
    }
    
    // For now, assume all annotations without obvious manual markers are auto-generated
    return false;
}

// Get indentation for line
std::string AnnotationPersister::get_line_indentation(const std::string& line) {
    size_t first_non_space = line.find_first_not_of(" \t");
    if (first_non_space == std::string::npos) {
        return ""; // Line is all whitespace
    }
    return line.substr(0, first_non_space);
}

// Trim whitespace from string
std::string AnnotationPersister::trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

} // namespace meld::compiler