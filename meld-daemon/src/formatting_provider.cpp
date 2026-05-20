#include "meld/daemon/formatting_provider.hpp"

#include <algorithm>
#include <cctype>
#include <functional>
#include <sstream>

namespace meld::daemon {

FormattingProvider::FormattingProvider(const SemanticModel& model)
    : model_(model) {}

// ---------------------------------------------------------------------------
// Style conventions
// ---------------------------------------------------------------------------

std::string FormattingProvider::normalize_indentation(const std::string& line) {
    std::string result;
    result.reserve(line.size());
    size_t i = 0;
    // Convert leading tabs to 4 spaces each
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) {
        if (line[i] == '\t') {
            result += "    ";
        } else {
            result += ' ';
        }
        ++i;
    }
    result += line.substr(i);
    return result;
}

std::string FormattingProvider::normalize_whitespace(const std::string& line) {
    if (line.empty()) return line;

    // Preserve leading indentation
    size_t indent_end = 0;
    while (indent_end < line.size() &&
           (line[indent_end] == ' ' || line[indent_end] == '\t')) {
        ++indent_end;
    }
    std::string indent = line.substr(0, indent_end);
    std::string content = line.substr(indent_end);

    if (content.empty()) return "";

    // Replace any remaining tabs with spaces in content
    std::string detabbed;
    detabbed.reserve(content.size());
    for (char c : content) {
        if (c == '\t') {
            detabbed += ' ';
        } else {
            detabbed += c;
        }
    }
    content = std::move(detabbed);

    // Collapse multiple spaces into one (outside of string literals)
    std::string collapsed;
    collapsed.reserve(content.size());
    bool in_string = false;
    bool prev_space = false;
    for (size_t i = 0; i < content.size(); ++i) {
        char c = content[i];
        if (c == '"' && (i == 0 || content[i - 1] != '\\')) {
            in_string = !in_string;
        }
        if (!in_string && c == ' ') {
            if (!prev_space) {
                collapsed += c;
            }
            prev_space = true;
        } else {
            collapsed += c;
            prev_space = false;
        }
    }

    // Trim trailing whitespace
    while (!collapsed.empty() && collapsed.back() == ' ') {
        collapsed.pop_back();
    }

    return indent + collapsed;
}

std::string FormattingProvider::apply_style_conventions(const std::string& line) {
    std::string result = normalize_indentation(line);
    result = normalize_whitespace(result);
    return result;
}

std::string FormattingProvider::format_line(const std::string& line) {
    return apply_style_conventions(line);
}

// ---------------------------------------------------------------------------
// Document formatting (Req 19.1)
// ---------------------------------------------------------------------------

std::vector<TextEdit> FormattingProvider::format_document(
    const std::filesystem::path& file) const {
    std::vector<TextEdit> edits;

    auto ast = model_.get_ast(file);
    if (!ast) return edits;

    // Walk the AST and produce formatting edits for each node
    // For each named node, ensure consistent style
    std::function<void(const std::shared_ptr<ASTNode>&)> walk;
    walk = [&](const std::shared_ptr<ASTNode>& node) {
        if (!node) return;

        if (!node->name.empty()) {
            // Produce a formatting edit for this node's line
            std::string formatted = format_line(node->name);
            if (formatted != node->name) {
                TextEdit edit;
                edit.start_line = node->location.line;
                edit.start_column = node->location.column;
                edit.end_line = node->location.line;
                edit.end_column = node->location.column +
                                  static_cast<uint32_t>(node->name.size());
                edit.new_text = formatted;
                edits.push_back(edit);
            }
        }

        for (const auto& child : node->children) {
            walk(child);
        }
    };
    walk(ast);

    // Sort edits by position (top to bottom)
    std::sort(edits.begin(), edits.end(), [](const TextEdit& a, const TextEdit& b) {
        return a.start_line < b.start_line ||
               (a.start_line == b.start_line && a.start_column < b.start_column);
    });

    return edits;
}

// ---------------------------------------------------------------------------
// Range formatting (Req 19.2)
// ---------------------------------------------------------------------------

std::vector<TextEdit> FormattingProvider::format_range(
    const std::filesystem::path& file,
    const Range& range) const {
    // Get full document edits, then filter to the range
    auto all_edits = format_document(file);

    std::vector<TextEdit> range_edits;
    for (const auto& edit : all_edits) {
        // Include edit if it overlaps with the requested range
        bool after_start = edit.start_line > range.start_line ||
                           (edit.start_line == range.start_line &&
                            edit.start_column >= range.start_column);
        bool before_end = edit.end_line < range.end_line ||
                          (edit.end_line == range.end_line &&
                           edit.end_column <= range.end_column);
        if (after_start && before_end) {
            range_edits.push_back(edit);
        }
    }
    return range_edits;
}

// ---------------------------------------------------------------------------
// Symbol reference finding
// ---------------------------------------------------------------------------

void FormattingProvider::find_refs_in_ast(
    const std::shared_ptr<ASTNode>& node,
    const std::filesystem::path& file,
    const std::string& symbol_name,
    std::vector<SymbolReference>& results) const {
    if (!node) return;

    if (node->name == symbol_name && !node->name.empty()) {
        const auto& k = node->kind;
        bool is_def = (k == "function_definition" || k == "fnc" ||
                       k == "struct" || k == "enum" || k == "trait" ||
                       k == "type_alias" || k == "val_declaration" ||
                       k == "var_declaration" || k == "let_declaration");
        results.push_back(SymbolReference{
            file, node->location.line, node->location.column,
            is_def ? "definition" : "reference"});
    }

    for (const auto& child : node->children) {
        find_refs_in_ast(child, file, symbol_name, results);
    }
}

std::vector<FormattingProvider::SymbolReference>
FormattingProvider::find_all_references(const std::string& symbol_name) const {
    std::vector<SymbolReference> results;
    if (symbol_name.empty()) return results;

    for (const auto& f : model_.get_indexed_files()) {
        auto ast = model_.get_ast(f);
        find_refs_in_ast(ast, f, symbol_name, results);
    }
    return results;
}

// ---------------------------------------------------------------------------
// Conflict detection (Req 19.4)
// ---------------------------------------------------------------------------

bool FormattingProvider::symbol_exists(const std::string& name) const {
    for (const auto& f : model_.get_indexed_files()) {
        auto ast = model_.get_ast(f);
        if (!ast) continue;
        std::function<bool(const std::shared_ptr<ASTNode>&)> search;
        search = [&](const std::shared_ptr<ASTNode>& node) -> bool {
            if (!node) return false;
            if (node->name == name && !node->name.empty()) return true;
            for (const auto& child : node->children) {
                if (search(child)) return true;
            }
            return false;
        };
        if (search(ast)) return true;
    }
    return false;
}

bool FormattingProvider::name_conflicts_in_scope(
    const std::string& old_name,
    const std::string& new_name) const {
    // A conflict exists if the new name already exists as a different symbol
    if (old_name == new_name) return false;
    return symbol_exists(new_name);
}

bool FormattingProvider::has_naming_conflict(
    const std::string& old_name,
    const std::string& new_name) const {
    if (old_name.empty() || new_name.empty()) return true;
    if (old_name == new_name) return false;

    // Check if new_name is a valid identifier
    if (!std::isalpha(static_cast<unsigned char>(new_name[0])) &&
        new_name[0] != '_') {
        return true;
    }
    for (char c : new_name) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
            return true;
        }
    }

    return name_conflicts_in_scope(old_name, new_name);
}

// ---------------------------------------------------------------------------
// Symbol renaming (Req 19.3)
// ---------------------------------------------------------------------------

RenameResult FormattingProvider::rename_symbol(
    const std::string& old_name,
    const std::string& new_name) const {
    RenameResult result;

    // Validate inputs
    if (old_name.empty()) {
        result.error_message = "Old symbol name is empty";
        return result;
    }
    if (new_name.empty()) {
        result.error_message = "New symbol name is empty";
        return result;
    }

    // Check for conflicts (Req 19.4)
    if (has_naming_conflict(old_name, new_name)) {
        result.error_message = "Rename would cause a naming conflict: '" +
                               new_name + "' already exists or is invalid";
        return result;
    }

    // Find all references
    auto refs = find_all_references(old_name);
    if (refs.empty()) {
        result.error_message = "Symbol '" + old_name + "' not found";
        return result;
    }

    // Group edits by file
    std::unordered_map<std::string, std::vector<TextEdit>> edits_by_file;
    for (const auto& ref : refs) {
        TextEdit edit;
        edit.start_line = ref.line;
        edit.start_column = ref.column;
        edit.end_line = ref.line;
        edit.end_column = ref.column + static_cast<uint32_t>(old_name.size());
        edit.new_text = new_name;
        edits_by_file[ref.file.string()].push_back(edit);
    }

    // Build result
    result.success = true;
    for (auto& [path_str, edits] : edits_by_file) {
        // Sort edits within each file
        std::sort(edits.begin(), edits.end(),
                  [](const TextEdit& a, const TextEdit& b) {
                      return a.start_line < b.start_line ||
                             (a.start_line == b.start_line &&
                              a.start_column < b.start_column);
                  });
        result.file_edits.emplace_back(
            std::filesystem::path(path_str), std::move(edits));
    }

    return result;
}

}  // namespace meld::daemon
