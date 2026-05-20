#include "meld/compiler/ide_integration.hpp"
#include "meld/compiler/effect_checker.hpp"
#include "meld/compiler/hold_type_inference_pass.hpp"
#include <sstream>
#include <algorithm>
#include <iostream>

namespace meld::compiler {

IDEIntegration::IDEIntegration() = default;

// TASK 35.9: Generate inlay hints for inferred effects
// Requirement 41.8: Display inferred effect annotations as ghost text
// TASK 9.2 (implicit-effect-calls): The inferred_effects map already includes effects
// discovered from implicit_effect_call nodes (via EffectChecker::infer_effects_for_functions
// → analyze_function_body_effects → analyze_expression_effects → analyze_implicit_effect_call).
// Ghost text annotation suggestions therefore automatically reflect implicit effect calls.
// Requirement 5.4.
std::vector<InlayHint> IDEIntegration::generate_effect_inlay_hints(
    const std::vector<parser::ast::function_definition>& functions,
    const std::map<std::string, std::set<std::string>>& inferred_effects) {
    
    std::vector<InlayHint> hints;
    
    for (const auto& func_def : functions) {
        // Get inferred effects for this function
        auto effects_it = inferred_effects.find(func_def.name.name);
        if (effects_it == inferred_effects.end()) {
            continue; // No inferred effects for this function
        }
        
        const auto& func_inferred_effects = effects_it->second;
        
        // Generate effect annotation hint
        auto annotation_hint = generate_effect_annotation_hint(func_def, func_inferred_effects);
        
        // Convert to inlay hint
        auto inlay_hint = effect_annotation_to_inlay_hint(annotation_hint);
        
        // Only add if this is a ghost annotation (not manually written)
        if (inlay_hint.is_ghost_annotation) {
            hints.push_back(inlay_hint);
        }
    }
    
    return hints;
}

// Generate effect annotation hint for a single function
EffectAnnotationHint IDEIntegration::generate_effect_annotation_hint(
    const parser::ast::function_definition& func_def,
    const std::set<std::string>& inferred_effects) {
    
    EffectAnnotationHint annotation;
    annotation.function_name = func_def.name.name;
    annotation.position = get_annotation_position(func_def);
    annotation.inferred_effects = inferred_effects;
    annotation.annotation_text = generate_uses_annotation_text(inferred_effects);
    annotation.is_manually_written = has_manual_uses_annotation(func_def);
    
    // Check if annotation needs update
    if (annotation.is_manually_written) {
        auto declared_effects = extract_declared_effects(func_def);
        annotation.needs_update = effects_differ(declared_effects, inferred_effects);
    } else {
        // No manual annotation, so this is a ghost annotation
        annotation.needs_update = false;
    }
    
    return annotation;
}

// Convert effect annotation hint to inlay hint
InlayHint IDEIntegration::effect_annotation_to_inlay_hint(const EffectAnnotationHint& annotation) {
    InlayHint hint;
    hint.position = annotation.position;
    hint.text = annotation.annotation_text;
    hint.kind = InlayHintKind::Effect;
    hint.is_ghost_annotation = !annotation.is_manually_written;
    
    // Generate tooltip
    std::ostringstream tooltip;
    tooltip << "Inferred effects for function '" << annotation.function_name << "':\\n";
    
    if (annotation.inferred_effects.empty() || 
        (annotation.inferred_effects.size() == 1 && 
         annotation.inferred_effects.find("EffectPure") != annotation.inferred_effects.end())) {
        tooltip << "- Pure function (no side effects)";
    } else {
        for (const auto& effect : annotation.inferred_effects) {
            if (effect != "EffectPure") {
                tooltip << "- " << effect << "\\n";
            }
        }
    }
    
    if (annotation.is_manually_written && annotation.needs_update) {
        tooltip << "\\nWarning: Manual annotation differs from inferred effects";
    } else if (!annotation.is_manually_written) {
        tooltip << "\\nThis annotation will be written to the file on save/format";
    }
    
    hint.tooltip = tooltip.str();
    
    return hint;
}

// Generate @uses(...) annotation text from effects
std::string IDEIntegration::generate_uses_annotation_text(const std::set<std::string>& effects) {
    if (effects.empty() || 
        (effects.size() == 1 && effects.find("EffectPure") != effects.end())) {
        return "@uses()"; // Pure function
    }
    
    std::ostringstream oss;
    oss << "@uses(";
    
    bool first = true;
    for (const auto& effect : effects) {
        if (effect == "EffectPure") continue; // Skip pure effect in mixed sets
        
        if (!first) {
            oss << ", ";
        }
        oss << effect;
        first = false;
    }
    
    oss << ")";
    return oss.str();
}

// Check if function has manually written @uses annotation
bool IDEIntegration::has_manual_uses_annotation(const parser::ast::function_definition& func_def) {
    // Check if function has effects clause or @uses annotation
    // This is a simplified check - in a real implementation, we'd parse annotations
    return func_def.has_effects || !func_def.effects_clause.empty();
}

// Get position before function signature (where annotation should appear)
SourcePosition IDEIntegration::get_annotation_position(const parser::ast::function_definition& func_def) {
    // In a real implementation, this would use the actual source location from the AST
    // For now, we'll use placeholder positions based on function name
    
    // Position should be on the line before the function declaration
    SourcePosition pos;
    pos.line = 1; // Default position
    pos.column = 0;
    
    return pos;
}

// TASK 65: Generate tenancy-state inlay hints from type inference results
// Requirements: 175.1-175.4
std::vector<InlayHint> IDEIntegration::generate_tenancy_inlay_hints(
    const HoldTypeInferenceResult& inference_result
) {
    std::vector<InlayHint> hints;
    
    // Creator Rule: show inferred Hold[T] for constructor-initialized variables
    for (const auto& inf : inference_result.inferences) {
        SourcePosition pos;
        pos.line = static_cast<int>(inf.line);
        pos.column = static_cast<int>(inf.column);
        
        hints.emplace_back(
            pos,
            " : " + inf.inferred_type,  // e.g., " : Hold[User]"
            InlayHintKind::Type,
            true  // is_ghost_annotation
        );
    }
    
    // Guest Rule: show inferred View[T] for bare class-typed parameters
    for (const auto& inf : inference_result.guest_rule_inferences) {
        SourcePosition pos;
        pos.line = static_cast<int>(inf.line);
        pos.column = static_cast<int>(inf.column);
        
        hints.emplace_back(
            pos,
            " as " + inf.inferred_type,  // e.g., " as View[User]"
            InlayHintKind::Type,
            true  // is_ghost_annotation
        );
    }
    
    return hints;
}

// Export inlay hints in LSP-compatible JSON format
std::string IDEIntegration::export_inlay_hints_as_json(const std::vector<InlayHint>& hints) {
    std::ostringstream json;
    json << "[";
    
    bool first = true;
    for (const auto& hint : hints) {
        if (!first) {
            json << ",";
        }
        
        json << "{";
        json << "\"position\":{\"line\":" << hint.position.line << ",\"character\":" << hint.position.column << "},";
        json << "\"label\":\"" << hint.text << "\",";
        
        // Map kind to LSP InlayHintKind
        int lsp_kind = 1; // Type by default
        switch (hint.kind) {
            case InlayHintKind::Type: lsp_kind = 1; break;
            case InlayHintKind::Parameter: lsp_kind = 2; break;
            case InlayHintKind::Effect: lsp_kind = 1; break; // Use Type kind for effects
            case InlayHintKind::Other: lsp_kind = 1; break;
        }
        json << "\"kind\":" << lsp_kind;
        
        if (!hint.tooltip.empty()) {
            json << ",\"tooltip\":\"" << hint.tooltip << "\"";
        }
        
        // Add custom property for ghost annotations
        if (hint.is_ghost_annotation) {
            json << ",\"data\":{\"isGhostAnnotation\":true}";
        }
        
        json << "}";
        first = false;
    }
    
    json << "]";
    return json.str();
}

// Export effect annotations in LSP-compatible JSON format
std::string IDEIntegration::export_effect_annotations_as_json(const std::vector<EffectAnnotationHint>& annotations) {
    std::ostringstream json;
    json << "[";
    
    bool first = true;
    for (const auto& annotation : annotations) {
        if (!first) {
            json << ",";
        }
        
        json << "{";
        json << "\"functionName\":\"" << annotation.function_name << "\",";
        json << "\"position\":{\"line\":" << annotation.position.line << ",\"character\":" << annotation.position.column << "},";
        json << "\"annotationText\":\"" << annotation.annotation_text << "\",";
        json << "\"isManuallyWritten\":" << (annotation.is_manually_written ? "true" : "false") << ",";
        json << "\"needsUpdate\":" << (annotation.needs_update ? "true" : "false") << ",";
        
        json << "\"inferredEffects\":[";
        bool first_effect = true;
        for (const auto& effect : annotation.inferred_effects) {
            if (!first_effect) {
                json << ",";
            }
            json << "\"" << effect << "\"";
            first_effect = false;
        }
        json << "]";
        
        json << "}";
        first = false;
    }
    
    json << "]";
    return json.str();
}

// TASK 35.9: Update ghost text as code changes
// Track changes and update hints accordingly
void IDEIntegration::on_document_change(const std::string& file_path, 
                                       const std::string& new_content,
                                       const std::vector<parser::ast::function_definition>& updated_functions,
                                       const std::map<std::string, std::set<std::string>>& updated_inferred_effects) {
    
    // Clear old hints for this file
    clear_hints_for_file(file_path);
    
    // Track which functions have changed effects
    std::set<std::string> functions_with_changed_effects;
    
    // Check for changes in inferred effects
    auto& last_effects = last_known_effects_[file_path];
    for (const auto& [func_name, new_effects] : updated_inferred_effects) {
        auto last_it = last_effects.find(func_name);
        if (last_it == last_effects.end() || last_it->second != new_effects) {
            functions_with_changed_effects.insert(func_name);
            last_effects[func_name] = new_effects;
        }
    }
    
    // Remove functions that no longer exist
    for (auto it = last_effects.begin(); it != last_effects.end();) {
        if (updated_inferred_effects.find(it->first) == updated_inferred_effects.end()) {
            functions_with_changed_effects.insert(it->first);
            it = last_effects.erase(it);
        } else {
            ++it;
        }
    }
    
    // Update changed annotations tracking
    changed_annotations_[file_path] = functions_with_changed_effects;
    
    // Generate new inlay hints
    auto new_hints = generate_effect_inlay_hints(updated_functions, updated_inferred_effects);
    inlay_hints_cache_[file_path] = new_hints;
    
    // Generate new effect annotations
    std::vector<EffectAnnotationHint> new_annotations;
    for (const auto& func_def : updated_functions) {
        auto effects_it = updated_inferred_effects.find(func_def.name.name);
        if (effects_it != updated_inferred_effects.end()) {
            auto annotation = generate_effect_annotation_hint(func_def, effects_it->second);
            new_annotations.push_back(annotation);
        }
    }
    effect_annotations_cache_[file_path] = new_annotations;
    
    // Update file timestamp
    file_timestamps_[file_path] = std::chrono::system_clock::now();
    
    // In a real implementation, we would notify the IDE/LSP client about the changes
    std::cout << "Updated ghost annotations for file: " << file_path 
              << " (" << new_hints.size() << " hints, " 
              << functions_with_changed_effects.size() << " functions changed)" << std::endl;
}

// TASK 35.9: Enhanced real-time update capabilities
// Track incremental changes for better performance
void IDEIntegration::on_incremental_change(const std::string& file_path,
                                          int start_line, int end_line,
                                          const std::string& new_text) {
    
    std::cout << "Incremental change detected in " << file_path 
              << " (lines " << start_line << "-" << end_line << ")" << std::endl;
    
    // Get current hints for the file
    auto current_hints = get_inlay_hints_for_file(file_path);
    
    // Filter out hints that might be affected by the change
    std::vector<InlayHint> unaffected_hints;
    std::vector<InlayHint> affected_hints;
    
    for (const auto& hint : current_hints) {
        if (hint.position.line < start_line || hint.position.line > end_line) {
            // Adjust line numbers for hints after the changed region
            InlayHint adjusted_hint = hint;
            if (hint.position.line > end_line) {
                // Calculate line offset based on the change
                int line_diff = std::count(new_text.begin(), new_text.end(), '\n') - (end_line - start_line);
                adjusted_hint.position.line += line_diff;
            }
            unaffected_hints.push_back(adjusted_hint);
        } else {
            affected_hints.push_back(hint);
        }
    }
    
    // Update cache with unaffected hints (affected ones will be regenerated)
    inlay_hints_cache_[file_path] = unaffected_hints;
    
    std::cout << "Preserved " << unaffected_hints.size() 
              << " hints, " << affected_hints.size() << " need regeneration" << std::endl;
}

// Check if ghost annotations need refresh for a specific function
bool IDEIntegration::needs_ghost_annotation_refresh(const std::string& file_path,
                                                   const std::string& function_name,
                                                   const std::set<std::string>& new_inferred_effects) {
    
    auto file_it = last_known_effects_.find(file_path);
    if (file_it == last_known_effects_.end()) {
        return true; // No previous effects known, needs refresh
    }
    
    auto func_it = file_it->second.find(function_name);
    if (func_it == file_it->second.end()) {
        return true; // Function not previously known, needs refresh
    }
    
    return func_it->second != new_inferred_effects; // Effects changed
}

// Get ghost annotations that have changed since last update
std::vector<EffectAnnotationHint> IDEIntegration::get_changed_ghost_annotations(const std::string& file_path) {
    std::vector<EffectAnnotationHint> changed_annotations;
    
    auto changed_it = changed_annotations_.find(file_path);
    if (changed_it == changed_annotations_.end()) {
        return changed_annotations; // No changes tracked
    }
    
    auto all_annotations = get_effect_annotations_for_file(file_path);
    const auto& changed_function_names = changed_it->second;
    
    for (const auto& annotation : all_annotations) {
        if (changed_function_names.find(annotation.function_name) != changed_function_names.end()) {
            changed_annotations.push_back(annotation);
        }
    }
    
    // Clear the changed annotations after retrieving them
    changed_annotations_[file_path].clear();
    
    return changed_annotations;
}

// Get current inlay hints for a file
std::vector<InlayHint> IDEIntegration::get_inlay_hints_for_file(const std::string& file_path) const {
    auto it = inlay_hints_cache_.find(file_path);
    if (it != inlay_hints_cache_.end()) {
        return it->second;
    }
    return {};
}

// Get current effect annotations for a file
std::vector<EffectAnnotationHint> IDEIntegration::get_effect_annotations_for_file(const std::string& file_path) const {
    auto it = effect_annotations_cache_.find(file_path);
    if (it != effect_annotations_cache_.end()) {
        return it->second;
    }
    return {};
}

// Clear cached hints for a file
void IDEIntegration::clear_hints_for_file(const std::string& file_path) {
    inlay_hints_cache_.erase(file_path);
    effect_annotations_cache_.erase(file_path);
}

// Helper: Compare declared effects with inferred effects
bool IDEIntegration::effects_differ(const std::set<std::string>& declared, 
                                   const std::set<std::string>& inferred) const {
    return declared != inferred;
}

// Helper: Extract declared effects from function
std::set<std::string> IDEIntegration::extract_declared_effects(const parser::ast::function_definition& func_def) const {
    std::set<std::string> declared_effects;
    
    for (const auto& effect : func_def.effects_clause) {
        declared_effects.insert(effect.name);
    }
    
    // If no effects declared, function is considered pure by default
    if (!func_def.has_effects) {
        declared_effects.insert("EffectPure");
    }
    
    return declared_effects;
}

} // namespace meld::compiler