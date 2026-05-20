#include "meld/daemon/incremental_analyzer.hpp"
#include "meld/compiler/type_checker.hpp"
#include "meld/parser/parser.hpp"
#include "meld/parser/ast.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
#include <unordered_set>

#include <boost/spirit/home/x3/support/ast/variant.hpp>

namespace meld::daemon {

namespace parser_ast = meld::parser::ast;
namespace x3 = boost::spirit::x3;

// ---------------------------------------------------------------------------
// AST conversion: walk the parser's expression variant and produce daemon
// ASTNode children with correct positions.
//
// Strategy: We parse with the real meld-core Parser (which internally uses the
// Lexer with line/column tracking). The parser's AST nodes inherit from
// x3::position_tagged but don't directly expose line/column. So we also
// tokenize with the Lexer to build a name→position map for declarations.
// ---------------------------------------------------------------------------

/// Helper: find the 0-based line number of a declaration name in the source.
/// We tokenize once and build a lookup from identifier tokens.
struct TokenPositionMap {
    struct Pos { uint32_t line; uint32_t column; };

    /// Build from source text. Stores ALL identifier/keyword token positions.
    void build(const std::string& source) {
        meld::parser::Lexer lexer(source);
        auto tokens = lexer.tokenize();
        for (const auto& tok : tokens) {
            if (tok.type == meld::parser::TokenType::IDENTIFIER ||
                tok.type == meld::parser::TokenType::KEYWORD) {
                // Store 0-based line (Lexer uses 1-based)
                entries_.push_back({tok.value,
                                    static_cast<uint32_t>(tok.line - 1),
                                    static_cast<uint32_t>(tok.column - 1)});
            }
        }
    }

    /// Find the position of a name, optionally after a given keyword.
    /// Returns 0-based line/column. If keyword is non-empty, finds the name
    /// that appears immediately after that keyword token.
    Pos find(const std::string& name, const std::string& keyword = "") const {
        if (!keyword.empty()) {
            for (size_t i = 0; i + 1 < entries_.size(); ++i) {
                if (entries_[i].value == keyword && entries_[i + 1].value == name) {
                    return {entries_[i + 1].line, entries_[i + 1].col};
                }
            }
        }
        // Fallback: find first occurrence of name
        for (const auto& e : entries_) {
            if (e.value == name) {
                return {e.line, e.col};
            }
        }
        return {0, 0};
    }

    /// Find the Nth occurrence of a name after a keyword (for duplicate names)
    Pos find_nth(const std::string& name, const std::string& keyword, size_t nth) const {
        size_t count = 0;
        if (!keyword.empty()) {
            for (size_t i = 0; i + 1 < entries_.size(); ++i) {
                if (entries_[i].value == keyword && entries_[i + 1].value == name) {
                    if (count == nth) {
                        return {entries_[i + 1].line, entries_[i + 1].col};
                    }
                    ++count;
                }
            }
        }
        return find(name, keyword);
    }

private:
    struct Entry { std::string value; uint32_t line; uint32_t col; };
    std::vector<Entry> entries_;
};

/// Visitor that converts parser AST expressions into daemon ASTNode children.
class ASTConverter : public boost::static_visitor<std::shared_ptr<ASTNode>> {
public:
    ASTConverter(const std::filesystem::path& file, const TokenPositionMap& positions)
        : file_(file), positions_(positions) {}

    // Function definition
    std::shared_ptr<ASTNode> operator()(const x3::forward_ast<parser_ast::function_definition>& ast) const {
        const auto& func = ast.get();
        auto node = std::make_shared<ASTNode>();
        node->kind = "function";
        node->name = func.name.name;
        auto pos = positions_.find(func.name.name, "fnc");
        node->location = {file_, pos.line, pos.column};

        // Return type info
        if (func.has_return_type) {
            node->type_info = func.return_type.type_name.name;
        }

        // Effects (declared via @uses)
        if (func.has_effects) {
            for (const auto& eff : func.effects_clause) {
                node->effects.push_back(eff.name);
            }
        }

        // Infer effects from body (collect implicit_effect_call nodes)
        if (node->effects.empty()) {
            std::function<void(const parser_ast::expression&)> collect;
            collect = [&](const parser_ast::expression& expr) {
                if (auto* iec = boost::get<x3::forward_ast<parser_ast::implicit_effect_call>>(&expr)) {
                    node->effects.push_back(iec->get().effect_name.name + "." + iec->get().operation_name.name);
                }
                if (auto* fc = boost::get<x3::forward_ast<parser_ast::function_call>>(&expr)) {
                    for (auto& a : fc->get().arguments) collect(a.get());
                }
            };
            for (auto& stmt : func.body.get().statements) collect(stmt.get());
        }

        // Parameters as children
        for (const auto& param : func.parameters) {
            auto param_node = std::make_shared<ASTNode>();
            param_node->kind = "parameter";
            param_node->name = param.name.name;
            param_node->type_info = param.type.type_name.name;
            param_node->location = {file_, pos.line, pos.column};
            node->children.push_back(param_node);
        }

        return node;
    }

    // Val declaration
    std::shared_ptr<ASTNode> operator()(const x3::forward_ast<parser_ast::val_declaration>& ast) const {
        const auto& decl = ast.get();
        auto node = std::make_shared<ASTNode>();
        node->kind = "val_declaration";
        node->name = decl.name.name;
        auto pos = positions_.find(decl.name.name, "val");
        node->location = {file_, pos.line, pos.column};
        if (decl.has_type_annotation) {
            node->type_info = decl.type_ann.get().type_name.name;
        }
        return node;
    }

    // Var declaration
    std::shared_ptr<ASTNode> operator()(const x3::forward_ast<parser_ast::var_declaration>& ast) const {
        const auto& decl = ast.get();
        auto node = std::make_shared<ASTNode>();
        node->kind = "var_declaration";
        node->name = decl.name.name;
        auto pos = positions_.find(decl.name.name, "var");
        node->location = {file_, pos.line, pos.column};
        if (decl.has_type_annotation) {
            node->type_info = decl.type_ann.get().type_name.name;
        }
        return node;
    }

    // Struct definition
    std::shared_ptr<ASTNode> operator()(const x3::forward_ast<parser_ast::struct_definition>& ast) const {
        const auto& def = ast.get();
        auto node = std::make_shared<ASTNode>();
        node->kind = "struct";
        node->name = def.name.name;
        auto pos = positions_.find(def.name.name, "struct");
        node->location = {file_, pos.line, pos.column};

        // Fields as children
        for (const auto& field : def.fields) {
            auto child = std::make_shared<ASTNode>();
            child->kind = "field";
            child->name = field.name.name;
            child->type_info = field.type.type_name.name;
            child->location = {file_, pos.line, pos.column};
            node->children.push_back(child);
        }
        return node;
    }

    // Class definition
    std::shared_ptr<ASTNode> operator()(const x3::forward_ast<parser_ast::class_definition>& ast) const {
        const auto& def = ast.get();
        auto node = std::make_shared<ASTNode>();
        node->kind = "class";
        node->name = def.name.name;
        auto pos = positions_.find(def.name.name, "class");
        node->location = {file_, pos.line, pos.column};

        // Fields as children
        for (const auto& field : def.fields) {
            auto child = std::make_shared<ASTNode>();
            child->kind = "field";
            child->name = field.name.name;
            child->type_info = field.type.type_name.name;
            child->location = {file_, pos.line, pos.column};
            node->children.push_back(child);
        }
        return node;
    }

    // Enum definition
    std::shared_ptr<ASTNode> operator()(const x3::forward_ast<parser_ast::enum_definition>& ast) const {
        const auto& def = ast.get();
        auto node = std::make_shared<ASTNode>();
        node->kind = "enum";
        node->name = def.name.name;
        auto pos = positions_.find(def.name.name, "enum");
        node->location = {file_, pos.line, pos.column};

        // Variants as children
        for (const auto& variant : def.variants) {
            auto child = std::make_shared<ASTNode>();
            child->kind = "enum_variant";
            child->name = variant.name.name;
            child->location = {file_, pos.line, pos.column};
            node->children.push_back(child);
        }
        return node;
    }

    // Import declaration
    std::shared_ptr<ASTNode> operator()(const x3::forward_ast<parser_ast::import_declaration>& ast) const {
        const auto& imp = ast.get();
        auto node = std::make_shared<ASTNode>();
        node->kind = "import";
        // Join namespace path
        std::string joined;
        for (size_t i = 0; i < imp.namespace_path.size(); ++i) {
            if (i > 0) joined += ".";
            joined += imp.namespace_path[i];
        }
        node->name = joined;
        auto pos = positions_.find(imp.is_imp ? "imp" : "import", "");
        node->location = {file_, pos.line, pos.column};
        return node;
    }

    // Type definition (unified)
    std::shared_ptr<ASTNode> operator()(const x3::forward_ast<parser_ast::typealias_declaration>& ast) const {
        const auto& def = ast.get();
        auto node = std::make_shared<ASTNode>();
        node->kind = "type";
        node->name = def.alias_name.name;
        auto pos = positions_.find(def.alias_name.name, "typealias");
        node->location = {file_, pos.line, pos.column};
        node->type_info = def.target_type.type_name.name;
        return node;
    }

    // Identifier (top-level)
    std::shared_ptr<ASTNode> operator()(const parser_ast::identifier& id) const {
        auto node = std::make_shared<ASTNode>();
        node->kind = "identifier";
        node->name = id.name;
        auto pos = positions_.find(id.name);
        node->location = {file_, pos.line, pos.column};
        return node;
    }

    // Catch-all for other expression types we don't need to convert
    template <typename T>
    std::shared_ptr<ASTNode> operator()(const T&) const {
        return nullptr;  // Skip non-declaration expressions
    }

private:
    std::filesystem::path file_;
    const TokenPositionMap& positions_;
};

IncrementalAnalyzer::IncrementalAnalyzer(SemanticModel& model)
    : model_(model) {}

std::vector<std::filesystem::path> IncrementalAnalyzer::analyze_change(
    const std::filesystem::path& changed_file) {
    std::vector<std::filesystem::path> updated;

    // 1. Re-parse the changed file
    auto semantics = parse_file(changed_file);

    // 2. Update reverse dependency map from imports
    {
        std::lock_guard<std::mutex> lock(deps_mutex_);
        for (const auto& imp : semantics.imports) {
            reverse_deps_[imp].insert(changed_file.string());
        }
    }

    // 3. Run type checking and effect inference
    auto type_diags = type_check(changed_file);
    auto effect_diags = infer_effects(changed_file);
    semantics.diagnostics.insert(semantics.diagnostics.end(),
                                 type_diags.begin(), type_diags.end());
    semantics.diagnostics.insert(semantics.diagnostics.end(),
                                 effect_diags.begin(), effect_diags.end());

    // 4. Update the SemanticModel
    model_.update_file(changed_file, std::move(semantics));
    updated.push_back(changed_file);

    // 5. Re-check direct dependents
    auto dependents = get_dependents(changed_file);
    for (const auto& dep_str : dependents) {
        std::filesystem::path dep_path(dep_str);
        auto dep_semantics = parse_file(dep_path);
        auto dep_type_diags = type_check(dep_path);
        auto dep_effect_diags = infer_effects(dep_path);
        dep_semantics.diagnostics.insert(dep_semantics.diagnostics.end(),
                                         dep_type_diags.begin(), dep_type_diags.end());
        dep_semantics.diagnostics.insert(dep_semantics.diagnostics.end(),
                                         dep_effect_diags.begin(), dep_effect_diags.end());
        model_.update_file(dep_path, std::move(dep_semantics));
        updated.push_back(dep_path);
    }

    return updated;
}

std::vector<std::filesystem::path> IncrementalAnalyzer::analyze_change(
    const std::filesystem::path& file, const std::string& content) {
    std::vector<std::filesystem::path> updated;

    // Parse from in-memory content (not disk)
    auto semantics = parse_content(file, content);

    {
        std::lock_guard<std::mutex> lock(deps_mutex_);
        for (const auto& imp : semantics.imports) {
            reverse_deps_[imp].insert(file.string());
        }
    }

    auto type_diags = type_check(file);
    auto effect_diags = infer_effects(file);
    semantics.diagnostics.insert(semantics.diagnostics.end(),
                                 type_diags.begin(), type_diags.end());
    semantics.diagnostics.insert(semantics.diagnostics.end(),
                                 effect_diags.begin(), effect_diags.end());

    model_.update_file(file, std::move(semantics));
    updated.push_back(file);

    return updated;
}

std::vector<std::filesystem::path> IncrementalAnalyzer::analyze_changes(
    const std::vector<FileChangeEvent>& events) {
    std::vector<std::filesystem::path> all_updated;

    for (const auto& event : events) {
        if (event.type == FileChangeType::Deleted) {
            model_.remove_file(event.path);
            // Re-check dependents of the deleted file
            auto dependents = get_dependents(event.path);
            for (const auto& dep_str : dependents) {
                auto updated = analyze_change(dep_str);
                all_updated.insert(all_updated.end(), updated.begin(), updated.end());
            }
        } else {
            auto updated = analyze_change(event.path);
            all_updated.insert(all_updated.end(), updated.begin(), updated.end());
        }
    }

    return all_updated;
}

std::vector<std::filesystem::path> IncrementalAnalyzer::analyze_workspace(
    const std::filesystem::path& root) {
    std::vector<std::filesystem::path> analyzed;
    namespace fs = std::filesystem;

    // Skip directories that don't contain Meld source
    static const std::vector<std::string> skip_prefixes = {
        "bazel-", ".meld", ".git", ".hypothesis", "node_modules",
        ".kiro", ".vscode", "third_party"
    };

    std::error_code ec;
    for (const auto& entry : fs::recursive_directory_iterator(root,
            fs::directory_options::skip_permission_denied, ec)) {
        if (ec) { ec.clear(); continue; }

        // Skip excluded directories
        if (entry.is_directory()) {
            auto dirname = entry.path().filename().string();
            bool skip = false;
            for (const auto& prefix : skip_prefixes) {
                if (dirname.find(prefix) == 0) {
                    skip = true;
                    break;
                }
            }
            if (skip) {
                // Can't easily skip with recursive_directory_iterator,
                // but we can skip files under these dirs by checking the path
                continue;
            }
        }

        if (!entry.is_regular_file() || entry.path().extension() != ".meld")
            continue;

        // Skip files under excluded directories
        auto path_str = entry.path().string();
        bool in_excluded = false;
        for (const auto& prefix : skip_prefixes) {
            if (path_str.find("/" + prefix) != std::string::npos) {
                in_excluded = true;
                break;
            }
        }
        if (in_excluded) continue;

        try {
            auto semantics = parse_file(entry.path());
            auto type_diags = type_check(entry.path());
            auto effect_diags = infer_effects(entry.path());
            semantics.diagnostics.insert(semantics.diagnostics.end(),
                                         type_diags.begin(), type_diags.end());
            semantics.diagnostics.insert(semantics.diagnostics.end(),
                                         effect_diags.begin(), effect_diags.end());

            // Build reverse dependency map
            {
                std::lock_guard<std::mutex> lock(deps_mutex_);
                for (const auto& imp : semantics.imports) {
                    reverse_deps_[imp].insert(entry.path().string());
                }

                // Register file in import index
                auto rel = fs::relative(entry.path(), root);
                auto import_path = rel.string();
                // Remove .meld extension
                if (import_path.size() > 5 && import_path.substr(import_path.size()-5) == ".meld")
                    import_path = import_path.substr(0, import_path.size()-5);
                // Replace / with .
                std::replace(import_path.begin(), import_path.end(), '/', '.');
                // Strip common prefixes
                for (const auto& prefix : {"meld-core.", "meld-examples.examples."}) {
                    if (import_path.find(prefix) == 0) {
                        import_path = import_path.substr(std::strlen(prefix));
                        break;
                    }
                }
                import_index_[import_path] = entry.path();
            }

            model_.update_file(entry.path(), std::move(semantics));
            analyzed.push_back(entry.path());

            // Yield between files so LSP requests aren't blocked
            std::this_thread::yield();
        } catch (...) {
            // Skip files that cause parse/analysis errors
        }
    }

    return analyzed;
}

std::unordered_set<std::string> IncrementalAnalyzer::get_dependents(
    const std::filesystem::path& file) const {
    std::lock_guard<std::mutex> lock(deps_mutex_);
    auto it = reverse_deps_.find(file.string());
    if (it != reverse_deps_.end()) {
        return it->second;
    }
    return {};
}

std::optional<std::filesystem::path> IncrementalAnalyzer::resolve_import(
    const std::string& import_path) const {
    std::lock_guard<std::mutex> lock(deps_mutex_);
    auto it = import_index_.find(import_path);
    if (it != import_index_.end()) return it->second;
    return std::nullopt;
}

FileSemantics IncrementalAnalyzer::parse_file(const std::filesystem::path& path) {
    // Read file content from disk
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        FileSemantics semantics;
        semantics.path = path;
        Diagnostic diag;
        diag.location = {path, 0, 0};
        diag.severity = DiagnosticSeverity::Error;
        diag.message = "Cannot open file: " + path.string();
        diag.rule_id = "E0001-file-not-found";
        semantics.diagnostics.push_back(std::move(diag));
        return semantics;
    }

    std::ostringstream oss;
    oss << ifs.rdbuf();
    return parse_content(path, oss.str());
}

FileSemantics IncrementalAnalyzer::parse_content(const std::filesystem::path& path,
                                                  const std::string& content) {
    FileSemantics semantics;
    semantics.path = path;

    // Create root module node
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    root->name = path.stem().string();
    root->location = {path, 0, 0};

    // Build token position map for accurate line/column info
    TokenPositionMap positions;
    positions.build(content);

    // Parse with the real meld-core parser
    meld::parser::Parser parser;
    std::vector<parser_ast::expression> ast_exprs;
    bool parse_ok = parser.parse_file(content, ast_exprs);

    if (parse_ok && !ast_exprs.empty()) {
        // Convert parser AST to daemon ASTNode tree
        ASTConverter converter(path, positions);

        for (const auto& expr : ast_exprs) {
            auto child = boost::apply_visitor(converter, expr);
            if (child) {
                // Extract imports for dependency tracking
                if (child->kind == "import") {
                    semantics.imports.push_back(child->name);
                }
                // Extract exports (top-level named declarations)
                if (!child->name.empty() && child->kind != "import") {
                    semantics.exports.push_back(child->name);
                }
                root->children.push_back(child);
            }
        }
    } else {
        // Parser failed — fall back to line-scanning for basic symbol extraction.
        // This ensures we still get some symbols even for files with syntax errors.
        if (!parser.error_message().empty()) {
            Diagnostic diag;
            diag.location = {path, 0, 0};
            diag.severity = DiagnosticSeverity::Warning;
            diag.message = "Parse warning: " + parser.error_message();
            diag.rule_id = "W0001-parse-fallback";
            semantics.diagnostics.push_back(std::move(diag));
        }

        // Line-scanning fallback for basic declarations
        std::istringstream lines(content);
        std::string line;
        uint32_t line_num = 0;
        while (std::getline(lines, line)) {
            // Check for declaration keywords and extract names
            auto try_extract = [&](const std::string& keyword, const std::string& kind) {
                auto kw_pos = line.find(keyword + " ");
                if (kw_pos != std::string::npos) {
                    // Skip if keyword is inside a comment
                    auto comment_pos = line.find("//");
                    if (comment_pos != std::string::npos && comment_pos < kw_pos) return;
                    auto name_start = kw_pos + keyword.size() + 1;
                    // Skip whitespace
                    while (name_start < line.size() && line[name_start] == ' ')
                        ++name_start;
                    // Extract name (alphanumeric, underscore, or hyphen for kebab-case)
                    std::string name;
                    for (size_t i = name_start; i < line.size(); ++i) {
                        char c = line[i];
                        if (std::isalnum(c) || c == '_' || c == '-')
                            name += c;
                        else
                            break;
                    }
                    // Trim trailing hyphens
                    while (!name.empty() && name.back() == '-')
                        name.pop_back();
                    if (!name.empty()) {
                        auto child = std::make_shared<ASTNode>();
                        child->kind = kind;
                        child->name = name;
                        child->location = {path, line_num,
                                           static_cast<uint32_t>(name_start)};
                        root->children.push_back(child);
                        if (kind != "import") {
                            semantics.exports.push_back(name);
                        }
                    }
                }
            };

            try_extract("fnc", "function");
            try_extract("val", "val_declaration");
            try_extract("var", "var_declaration");
            try_extract("struct", "struct");
            try_extract("class", "class");
            try_extract("enum", "enum");
            try_extract("trait", "trait");
            try_extract("type", "type");

            // Extract imports
            if (line.find("imp ") == 0 || line.find("import ") == 0) {
                std::string prefix = (line.find("imp ") == 0) ? "imp " : "import ";
                auto module_name = line.substr(prefix.size());
                while (!module_name.empty() &&
                       (module_name.back() == '\r' || module_name.back() == ' '))
                    module_name.pop_back();
                if (!module_name.empty()) {
                    semantics.imports.push_back(module_name);
                    auto child = std::make_shared<ASTNode>();
                    child->kind = "import";
                    child->name = module_name;
                    child->location = {path, line_num, 0};
                    root->children.push_back(child);
                }
            }

            // Extract all identifier usages on this line for find-references.
            // Skip comment lines.
            {
                auto trimmed = line;
                size_t first_non_space = trimmed.find_first_not_of(" \t");
                if (first_non_space != std::string::npos &&
                    trimmed.substr(first_non_space, 2) != "//") {
                    size_t i = 0;
                    while (i < line.size()) {
                        // Skip strings
                        if (line[i] == '"') {
                            ++i;
                            while (i < line.size() && line[i] != '"') {
                                if (line[i] == '\\') ++i;
                                ++i;
                            }
                            if (i < line.size()) ++i;
                            continue;
                        }
                        // Skip line comments
                        if (i + 1 < line.size() && line[i] == '/' && line[i+1] == '/') {
                            break;
                        }
                        // Extract identifier (alphanumeric, underscore, hyphen)
                        if (std::isalpha(static_cast<unsigned char>(line[i])) || line[i] == '_') {
                            size_t start = i;
                            std::string ident;
                            while (i < line.size() &&
                                   (std::isalnum(static_cast<unsigned char>(line[i])) ||
                                    line[i] == '_' || line[i] == '-')) {
                                ident += line[i];
                                ++i;
                            }
                            // Trim trailing hyphens
                            while (!ident.empty() && ident.back() == '-')
                                ident.pop_back();
                            // Skip keywords and very short identifiers
                            static const std::unordered_set<std::string> keywords = {
                                "val", "var", "fnc", "rtn", "if", "else", "for",
                                "while", "match", "case", "struct", "class", "enum",
                                "trait", "type", "imp", "import", "true", "false",
                                "null", "in", "is", "as", "not", "and", "or",
                                "effect", "handle", "perform", "resume", "spawn",
                                "async", "await", "test", "require", "ensure"
                            };
                            if (ident.size() > 1 && keywords.find(ident) == keywords.end()) {
                                auto ref = std::make_shared<ASTNode>();
                                ref->kind = "reference";
                                ref->name = ident;
                                ref->location = {path, line_num,
                                                 static_cast<uint32_t>(start)};
                                root->children.push_back(ref);
                            }
                        } else {
                            ++i;
                        }
                    }
                }
            }

            ++line_num;
        }
    }

    // ── Unconditional identifier usage scanner ────────────────────────
    // Runs for ALL files (parser success or fallback) so find-references
    // and rename can locate usages, not just declarations.
    {
        std::istringstream scan_lines(content);
        std::string scan_line;
        uint32_t scan_line_num = 0;
        while (std::getline(scan_lines, scan_line)) {
            size_t first = scan_line.find_first_not_of(" \t");
            if (first != std::string::npos && scan_line.substr(first, 2) == "//") {
                ++scan_line_num;
                continue;
            }
            size_t i = 0;
            while (i < scan_line.size()) {
                if (scan_line[i] == '"') {
                    ++i;
                    while (i < scan_line.size() && scan_line[i] != '"') {
                        if (scan_line[i] == '\\') ++i;
                        ++i;
                    }
                    if (i < scan_line.size()) ++i;
                    continue;
                }
                if (i + 1 < scan_line.size() && scan_line[i] == '/' && scan_line[i+1] == '/') break;
                if (std::isalpha(static_cast<unsigned char>(scan_line[i])) || scan_line[i] == '_') {
                    size_t start = i;
                    std::string ident;
                    while (i < scan_line.size() &&
                           (std::isalnum(static_cast<unsigned char>(scan_line[i])) ||
                            scan_line[i] == '_' || scan_line[i] == '-')) {
                        ident += scan_line[i];
                        ++i;
                    }
                    while (!ident.empty() && ident.back() == '-') ident.pop_back();
                    static const std::unordered_set<std::string> kw = {
                        "val","var","fnc","rtn","if","else","for","while","match",
                        "case","struct","class","enum","trait","type","imp","import",
                        "true","false","null","in","is","as","not","and","or",
                        "effect","handle","perform","resume","spawn","async","await",
                        "test","require","ensure"
                    };
                    if (ident.size() > 1 && kw.find(ident) == kw.end()) {
                        auto ref = std::make_shared<ASTNode>();
                        ref->kind = "reference";
                        ref->name = ident;
                        ref->location = {path, scan_line_num, static_cast<uint32_t>(start)};
                        root->children.push_back(ref);
                    }
                } else {
                    ++i;
                }
            }
            ++scan_line_num;
        }
    }

    semantics.ast = root;
    return semantics;
}

std::vector<Diagnostic> IncrementalAnalyzer::type_check(
    const std::filesystem::path& path) {
    std::vector<Diagnostic> diags;

    // Read and parse the file
    std::ifstream ifs(path);
    if (!ifs.is_open()) return diags;

    std::ostringstream buf;
    buf << ifs.rdbuf();
    auto content = buf.str();

    // Parse with the meld-core parser
    meld::parser::Parser parser;
    std::vector<meld::parser::ast::expression> ast_exprs;
    if (!parser.parse_file(content, ast_exprs) || ast_exprs.empty()) {
        return diags;  // Parse failed — parse errors are reported separately
    }

    // Run the type checker
    try {
        meld::compiler::TypeChecker checker;
        auto result = checker.check_program(ast_exprs);
        if (!result) {
            // Type errors found
            for (const auto& err : result.error()) {
                Diagnostic d;
                d.location = {path, static_cast<uint32_t>(err.line),
                              static_cast<uint32_t>(err.column)};
                d.severity = DiagnosticSeverity::Error;
                d.message = err.message;
                d.rule_id = "E0100-type-error";
                if (!err.context.empty()) {
                    d.message += " (" + err.context + ")";
                }
                diags.push_back(std::move(d));
            }
        }
    } catch (...) {
        // Type checker crashed — don't propagate
    }

    // Also flag TODO/FIXME comments as hints
    std::istringstream lines(content);
    std::string line;
    uint32_t line_num = 0;
    while (std::getline(lines, line)) {
        auto comment_pos = line.find("//");
        if (comment_pos != std::string::npos) {
            auto comment = line.substr(comment_pos + 2);
            if (comment.find("TODO") != std::string::npos ||
                comment.find("FIXME") != std::string::npos) {
                Diagnostic d;
                d.location = {path, line_num, static_cast<uint32_t>(comment_pos)};
                d.severity = DiagnosticSeverity::Hint;
                d.message = comment;
                d.rule_id = "H0001-todo";
                diags.push_back(std::move(d));
            }
        }
        ++line_num;
    }

    return diags;
}

std::vector<Diagnostic> IncrementalAnalyzer::infer_effects(
    const std::filesystem::path& path) {
    std::vector<Diagnostic> diags;

    // Effect inference: flag perform/spawn calls outside handle blocks
    // and flag functions that use effects without declaring them in their signature.
    std::ifstream ifs(path);
    if (!ifs.is_open()) return diags;

    std::string content((std::istreambuf_iterator<char>(ifs)),
                         std::istreambuf_iterator<char>());

    std::istringstream lines(content);
    std::string line;
    uint32_t line_num = 0;
    int handle_depth = 0;

    while (std::getline(lines, line)) {
        // Skip comment lines
        auto first_non_space = line.find_first_not_of(" \t");
        if (first_non_space != std::string::npos && line.substr(first_non_space, 2) == "//") {
            ++line_num;
            continue;
        }

        // Track handle block nesting
        for (size_t i = 0; i < line.size(); ++i) {
            if (line[i] == '"') { // Skip strings
                ++i;
                while (i < line.size() && line[i] != '"') {
                    if (line[i] == '\\') ++i;
                    ++i;
                }
                continue;
            }
            if (i + 1 < line.size() && line[i] == '/' && line[i+1] == '/') break;
        }

        if (line.find("handle(") != std::string::npos || line.find("handle {") != std::string::npos) {
            handle_depth++;
        }
        // Count closing braces (simplified — doesn't handle nested braces perfectly)
        if (handle_depth > 0) {
            for (char c : line) {
                if (c == '}') { handle_depth--; if (handle_depth < 0) handle_depth = 0; }
            }
        }

        // Flag perform calls outside handle blocks
        auto perform_pos = line.find("perform ");
        if (perform_pos != std::string::npos && handle_depth == 0) {
            // Check it's not in a comment
            auto comment_pos = line.find("//");
            if (comment_pos == std::string::npos || comment_pos > perform_pos) {
                Diagnostic d;
                d.location = {path, line_num, static_cast<uint32_t>(perform_pos)};
                d.severity = DiagnosticSeverity::Warning;
                d.message = "Effect operation outside handle block — effects may leak";
                d.rule_id = "W0010-unhandled-effect";
                diags.push_back(std::move(d));
            }
        }

        // Flag implicit effect calls (Effect.operation) outside handle blocks
        // Pattern: CapitalWord.word(
        if (handle_depth == 0) {
            size_t i = 0;
            while (i < line.size()) {
                if (std::isupper(static_cast<unsigned char>(line[i]))) {
                    size_t start = i;
                    while (i < line.size() && (std::isalnum(static_cast<unsigned char>(line[i])) || line[i] == '_'))
                        ++i;
                    if (i < line.size() && line[i] == '.') {
                        ++i;
                        size_t method_start = i;
                        while (i < line.size() && (std::isalnum(static_cast<unsigned char>(line[i])) || line[i] == '_' || line[i] == '-'))
                            ++i;
                        if (i < line.size() && line[i] == '(' && i > method_start) {
                            // Looks like Effect.operation( — check if it's not in a comment
                            auto comment_pos = line.find("//");
                            if (comment_pos == std::string::npos || comment_pos > start) {
                                Diagnostic d;
                                d.location = {path, line_num, static_cast<uint32_t>(start)};
                                d.severity = DiagnosticSeverity::Info;
                                d.message = "Implicit effect call — ensure this is handled";
                                d.rule_id = "I0010-implicit-effect";
                                diags.push_back(std::move(d));
                            }
                        }
                    }
                } else {
                    ++i;
                }
            }
        }

        ++line_num;
    }

    return diags;
}

}  // namespace meld::daemon
