#pragma once

#include "meld/parser/ast.hpp"
#include "meld/compiler/hold_type_inference_pass.hpp"
#include <string>
#include <vector>
#include <set>
#include <map>
#include <chrono>

namespace meld::compiler {

// Position in source code (line and column)
struct SourcePosition {
    int line = 0;
    int column = 0;
    
    SourcePosition() = default;
    SourcePosition(int l, int c) : line(l), column(c) {}
};

// Range in source code
struct SourceRange {
    SourcePosition start;
    SourcePosition end;
    
    SourceRange() = default;
    SourceRange(SourcePosition s, SourcePosition e) : start(s), end(e) {}
};

// Inlay hint kind (LSP compatible)
enum class InlayHintKind {
    Type,           // Type hints
    Parameter,      // Parameter name hints
    Effect,         // Effect annotation hints (ghost text)
    Other
};

// Inlay hint for IDE display
struct InlayHint {
    SourcePosition position;        // Where to display the hint
    std::string text;                // The hint text to display
    InlayHintKind kind;             // Kind of hint
    std::string tooltip;            // Optional tooltip text
    bool is_ghost_annotation;       // True if this is a ghost annotation (not yet written to disk)
    
    InlayHint() = default;
    InlayHint(SourcePosition pos, std::string txt, InlayHintKind k, bool ghost = false)
        : position(pos), text(std::move(txt)), kind(k), is_ghost_annotation(ghost) {}
};

// Effect annotation hint (specialized inlay hint for effects)
struct EffectAnnotationHint {
    std::string function_name;
    SourcePosition position;        // Position before function signature
    std::set<std::string> inferred_effects;
    std::string annotation_text;    // Generated @uses(...) text
    bool is_manually_written;       // True if annotation exists in source
    bool needs_update;              // True if inferred effects differ from declared
    
    EffectAnnotationHint() = default;
    EffectAnnotationHint(std::string name, SourcePosition pos, std::set<std::string> effects)
        : function_name(std::move(name)), position(pos), inferred_effects(std::move(effects)),
          is_manually_written(false), needs_update(false) {}
};

// IDE integration manager
class IDEIntegration {
public:
    IDEIntegration();
    
    // TASK 35.9: Generate inlay hints for inferred effects
    // Requirement 41.8: Display inferred effect annotations as ghost text
    // TASK 9.2 (implicit-effect-calls): Ghost text reflects implicit effect calls
    // because the inferred_effects map is produced by EffectChecker::infer_effects_for_functions,
    // which includes effects from implicit_effect_call AST nodes. Requirement 5.4.
    std::vector<InlayHint> generate_effect_inlay_hints(
        const std::vector<parser::ast::function_definition>& functions,
        const std::map<std::string, std::set<std::string>>& inferred_effects);
    
    // Generate effect annotation hint for a single function
    EffectAnnotationHint generate_effect_annotation_hint(
        const parser::ast::function_definition& func_def,
        const std::set<std::string>& inferred_effects);
    
    // Convert effect annotation hint to inlay hint
    InlayHint effect_annotation_to_inlay_hint(const EffectAnnotationHint& annotation);
    
    // Generate @uses(...) annotation text from effects
    std::string generate_uses_annotation_text(const std::set<std::string>& effects);
    
    // Check if function has manually written @uses annotation
    bool has_manual_uses_annotation(const parser::ast::function_definition& func_def);
    
    // Get position before function signature (where annotation should appear)
    SourcePosition get_annotation_position(const parser::ast::function_definition& func_def);
    
    /// Generate tenancy-state inlay hints from type inference results.
    /// Shows inferred Hold[T] for Creator Rule and View[T] for Guest Rule.
    /// Requirements: 175.1-175.4
    std::vector<InlayHint> generate_tenancy_inlay_hints(
        const HoldTypeInferenceResult& inference_result
    );
    
    // Export inlay hints in LSP-compatible JSON format
    std::string export_inlay_hints_as_json(const std::vector<InlayHint>& hints);
    
    // Export effect annotations in LSP-compatible JSON format
    std::string export_effect_annotations_as_json(const std::vector<EffectAnnotationHint>& annotations);
    
    // TASK 35.9: Update ghost text as code changes
    // Track changes and update hints accordingly
    void on_document_change(const std::string& file_path, 
                           const std::string& new_content,
                           const std::vector<parser::ast::function_definition>& updated_functions,
                           const std::map<std::string, std::set<std::string>>& updated_inferred_effects);
    
    // TASK 35.9: Enhanced real-time update capabilities
    // Track incremental changes for better performance
    void on_incremental_change(const std::string& file_path,
                              int start_line, int end_line,
                              const std::string& new_text);
    
    // Check if ghost annotations need refresh for a specific function
    bool needs_ghost_annotation_refresh(const std::string& file_path,
                                       const std::string& function_name,
                                       const std::set<std::string>& new_inferred_effects);
    
    // Get ghost annotations that have changed since last update
    std::vector<EffectAnnotationHint> get_changed_ghost_annotations(const std::string& file_path);
    
    // Get current inlay hints for a file
    std::vector<InlayHint> get_inlay_hints_for_file(const std::string& file_path) const;
    
    // Get current effect annotations for a file
    std::vector<EffectAnnotationHint> get_effect_annotations_for_file(const std::string& file_path) const;
    
    // Clear cached hints for a file
    void clear_hints_for_file(const std::string& file_path);
    
private:
    // Cache of inlay hints per file
    std::map<std::string, std::vector<InlayHint>> inlay_hints_cache_;
    
    // Cache of effect annotations per file
    std::map<std::string, std::vector<EffectAnnotationHint>> effect_annotations_cache_;
    
    // TASK 35.9: Enhanced change tracking for real-time updates
    // Track last known effects for each function to detect changes
    std::map<std::string, std::map<std::string, std::set<std::string>>> last_known_effects_;
    
    // Track which annotations have changed since last update
    std::map<std::string, std::set<std::string>> changed_annotations_;
    
    // Track file modification timestamps for change detection
    std::map<std::string, std::chrono::system_clock::time_point> file_timestamps_;
    
    // Helper: Compare declared effects with inferred effects
    bool effects_differ(const std::set<std::string>& declared, 
                       const std::set<std::string>& inferred) const;
    
    // Helper: Extract declared effects from function
    std::set<std::string> extract_declared_effects(const parser::ast::function_definition& func_def) const;
};

} // namespace meld::compiler
