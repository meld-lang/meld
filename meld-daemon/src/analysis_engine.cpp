#include "analysis_engine.hpp"
#include <regex>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <set>

namespace meld::lsp::analysis {

AnalysisEngine::AnalysisEngine() = default;
AnalysisEngine::~AnalysisEngine() = default;

AnalysisResult AnalysisEngine::analyze(const std::string& uri, const std::string& content) {
    AnalysisResult result;
    result.uri = uri;

    if (content.empty()) {
        cache_[uri] = result;
        content_hashes_[uri] = 0;
        touch(uri);
        evict_if_needed();
        return result;
    }

    // Check content hash — skip re-analysis if unchanged
    size_t hash = std::hash<std::string>{}(content);
    auto hash_it = content_hashes_.find(uri);
    if (hash_it != content_hashes_.end() && hash_it->second == hash) {
        auto cache_it = cache_.find(uri);
        if (cache_it != cache_.end()) {
            touch(uri);
            return cache_it->second;
        }
    }

    // Extract function declarations: fnc name(params) or fnc name(params) -> RetType
    {
        std::regex re(R"(\bfnc\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*\(([^)]*)\)(\s*->\s*([a-zA-Z_][a-zA-Z0-9_]*))?)");
        auto begin = std::sregex_iterator(content.begin(), content.end(), re);
        auto end = std::sregex_iterator();
        for (auto it = begin; it != end; ++it) {
            SymbolInfo sym;
            sym.name = (*it)[1].str();
            sym.kind = SymbolKind::Function;
            size_t offset = static_cast<size_t>(it->position(1));
            int line = 0, col = 0;
            for (size_t i = 0; i < offset && i < content.size(); ++i) {
                if (content[i] == '\n') { line++; col = 0; } else { col++; }
            }
            sym.definition.uri = uri;
            sym.definition.line = line;
            sym.definition.character = col;
            sym.definition.end_line = line;
            sym.definition.end_character = col + static_cast<int>(sym.name.size());
            sym.type_signature = "fnc " + sym.name + "(" + (*it)[2].str() + ")";
            if ((*it)[4].matched) {
                sym.type_signature += " -> " + (*it)[4].str();
            }
            sym.documentation = "Function " + sym.name;
            result.symbols.push_back(sym);
        }
    }

    // Extract variable declarations: let/var name
    {
        std::regex re(R"(\b(let|var)\s+([a-zA-Z_][a-zA-Z0-9_]*))");
        auto begin = std::sregex_iterator(content.begin(), content.end(), re);
        auto end = std::sregex_iterator();
        for (auto it = begin; it != end; ++it) {
            SymbolInfo sym;
            sym.name = (*it)[2].str();
            sym.kind = SymbolKind::Variable;
            size_t offset = static_cast<size_t>(it->position(2));
            int line = 0, col = 0;
            for (size_t i = 0; i < offset && i < content.size(); ++i) {
                if (content[i] == '\n') { line++; col = 0; } else { col++; }
            }
            sym.definition.uri = uri;
            sym.definition.line = line;
            sym.definition.character = col;
            sym.definition.end_line = line;
            sym.definition.end_character = col + static_cast<int>(sym.name.size());
            std::string decl_kind = (*it)[1].str();
            sym.type_signature = decl_kind + " " + sym.name;
            sym.documentation = (decl_kind == "let" ? "Immutable" : "Mutable") +
                                std::string(" variable ") + sym.name;
            result.symbols.push_back(sym);
        }
    }

    // Extract struct declarations: struct Name { ... }
    {
        std::regex re(R"(\bstruct\s+([A-Z][a-zA-Z0-9_]*))");
        auto begin = std::sregex_iterator(content.begin(), content.end(), re);
        auto end = std::sregex_iterator();
        for (auto it = begin; it != end; ++it) {
            SymbolInfo sym;
            sym.name = (*it)[1].str();
            sym.kind = SymbolKind::Struct;
            size_t offset = static_cast<size_t>(it->position(1));
            int line = 0, col = 0;
            for (size_t i = 0; i < offset && i < content.size(); ++i) {
                if (content[i] == '\n') { line++; col = 0; } else { col++; }
            }
            sym.definition.uri = uri;
            sym.definition.line = line;
            sym.definition.character = col;
            sym.definition.end_line = line;
            sym.definition.end_character = col + static_cast<int>(sym.name.size());
            sym.type_signature = "struct " + sym.name;
            sym.documentation = "Struct " + sym.name;
            result.symbols.push_back(sym);
        }
    }

    // Extract enum declarations: enum Name
    {
        std::regex re(R"(\benum\s+([A-Z][a-zA-Z0-9_]*))");
        auto begin = std::sregex_iterator(content.begin(), content.end(), re);
        auto end = std::sregex_iterator();
        for (auto it = begin; it != end; ++it) {
            SymbolInfo sym;
            sym.name = (*it)[1].str();
            sym.kind = SymbolKind::Enum;
            size_t offset = static_cast<size_t>(it->position(1));
            int line = 0, col = 0;
            for (size_t i = 0; i < offset && i < content.size(); ++i) {
                if (content[i] == '\n') { line++; col = 0; } else { col++; }
            }
            sym.definition.uri = uri;
            sym.definition.line = line;
            sym.definition.character = col;
            sym.definition.end_line = line;
            sym.definition.end_character = col + static_cast<int>(sym.name.size());
            sym.type_signature = "enum " + sym.name;
            sym.documentation = "Enum " + sym.name;
            result.symbols.push_back(sym);
        }
    }

    // Extract trait declarations: trait Name
    {
        std::regex re(R"(\btrait\s+([A-Z][a-zA-Z0-9_]*))");
        auto begin = std::sregex_iterator(content.begin(), content.end(), re);
        auto end = std::sregex_iterator();
        for (auto it = begin; it != end; ++it) {
            SymbolInfo sym;
            sym.name = (*it)[1].str();
            sym.kind = SymbolKind::Trait;
            size_t offset = static_cast<size_t>(it->position(1));
            int line = 0, col = 0;
            for (size_t i = 0; i < offset && i < content.size(); ++i) {
                if (content[i] == '\n') { line++; col = 0; } else { col++; }
            }
            sym.definition.uri = uri;
            sym.definition.line = line;
            sym.definition.character = col;
            sym.definition.end_line = line;
            sym.definition.end_character = col + static_cast<int>(sym.name.size());
            sym.type_signature = "trait " + sym.name;
            sym.documentation = "Trait " + sym.name;
            result.symbols.push_back(sym);
        }
    }

    // Extract effect declarations: effect Name
    {
        std::regex re(R"(\beffect\s+([A-Z][a-zA-Z0-9_]*))");
        auto begin = std::sregex_iterator(content.begin(), content.end(), re);
        auto end = std::sregex_iterator();
        for (auto it = begin; it != end; ++it) {
            SymbolInfo sym;
            sym.name = (*it)[1].str();
            sym.kind = SymbolKind::Effect;
            size_t offset = static_cast<size_t>(it->position(1));
            int line = 0, col = 0;
            for (size_t i = 0; i < offset && i < content.size(); ++i) {
                if (content[i] == '\n') { line++; col = 0; } else { col++; }
            }
            sym.definition.uri = uri;
            sym.definition.line = line;
            sym.definition.character = col;
            sym.definition.end_line = line;
            sym.definition.end_character = col + static_cast<int>(sym.name.size());
            sym.type_signature = "effect " + sym.name;
            sym.documentation = "Effect " + sym.name;
            result.symbols.push_back(sym);
        }
    }

    // Cache the result and update hash/LRU
    cache_[uri] = result;
    content_hashes_[uri] = hash;
    touch(uri);
    evict_if_needed();
    return result;
}

std::optional<SymbolInfo> AnalysisEngine::resolve_symbol(
    const std::string& uri, int line, int character) {
    auto it = cache_.find(uri);
    if (it == cache_.end()) {
        return std::nullopt;
    }

    touch(uri);

    // Find the symbol whose definition range contains the given position
    for (const auto& sym : it->second.symbols) {
        if (sym.definition.uri == uri &&
            sym.definition.line == line &&
            character >= sym.definition.character &&
            character <= sym.definition.end_character) {
            return sym;
        }
    }

    // Also check if the position is on a reference to a known symbol
    // by looking at the cached content — we need to find the identifier at position
    return std::nullopt;
}

std::vector<SymbolInfo> AnalysisEngine::get_document_symbols(const std::string& uri) {
    auto it = cache_.find(uri);
    if (it != cache_.end()) {
        touch(uri);
        return it->second.symbols;
    }
    return {};
}

void AnalysisEngine::update(const std::string& uri, const std::string& content) {
    analyze(uri, content);
}

void AnalysisEngine::invalidate(const std::string& uri) {
    cache_.erase(uri);
    content_hashes_.erase(uri);
    lru_order_.erase(
        std::remove(lru_order_.begin(), lru_order_.end(), uri),
        lru_order_.end());
}

// ---------------------------------------------------------------------------
// Cache management helpers
// ---------------------------------------------------------------------------

size_t AnalysisEngine::cache_size() const {
    return cache_.size();
}

size_t AnalysisEngine::max_cache_size() const {
    return max_cache_size_;
}

void AnalysisEngine::set_max_cache_size(size_t size) {
    max_cache_size_ = size;
    evict_if_needed();
}

bool AnalysisEngine::is_content_unchanged(const std::string& uri, const std::string& content) const {
    auto it = content_hashes_.find(uri);
    if (it == content_hashes_.end()) return false;
    return it->second == std::hash<std::string>{}(content);
}

std::vector<std::string> AnalysisEngine::access_order() const {
    return lru_order_;
}

void AnalysisEngine::touch(const std::string& uri) {
    // Remove existing entry if present
    lru_order_.erase(
        std::remove(lru_order_.begin(), lru_order_.end(), uri),
        lru_order_.end());
    // Push to back (most recently used)
    lru_order_.push_back(uri);
}

void AnalysisEngine::evict_if_needed() {
    while (cache_.size() > max_cache_size_ && !lru_order_.empty()) {
        const std::string& victim = lru_order_.front();
        cache_.erase(victim);
        content_hashes_.erase(victim);
        lru_order_.erase(lru_order_.begin());
    }
}

// ---------------------------------------------------------------------------
// Levenshtein distance
// ---------------------------------------------------------------------------

int AnalysisEngine::levenshtein_distance(const std::string& a, const std::string& b) {
    const size_t m = a.size();
    const size_t n = b.size();
    std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1, 0));

    for (size_t i = 0; i <= m; ++i) dp[i][0] = static_cast<int>(i);
    for (size_t j = 0; j <= n; ++j) dp[0][j] = static_cast<int>(j);

    for (size_t i = 1; i <= m; ++i) {
        for (size_t j = 1; j <= n; ++j) {
            int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            dp[i][j] = std::min({
                dp[i - 1][j] + 1,
                dp[i][j - 1] + 1,
                dp[i - 1][j - 1] + cost
            });
        }
    }
    return dp[m][n];
}

std::vector<std::string> AnalysisEngine::find_similar_names(
    const std::string& name,
    const std::vector<std::string>& candidates,
    int max_distance) {

    std::vector<std::pair<int, std::string>> scored;
    for (const auto& candidate : candidates) {
        if (candidate == name) continue;
        int dist = levenshtein_distance(name, candidate);
        if (dist <= max_distance) {
            scored.push_back({dist, candidate});
        }
    }
    std::sort(scored.begin(), scored.end());

    std::vector<std::string> result;
    for (const auto& [dist, s] : scored) {
        result.push_back(s);
        if (result.size() >= 3) break;  // limit to top 3 suggestions
    }
    return result;
}

// ---------------------------------------------------------------------------
// Helpers for source scanning
// ---------------------------------------------------------------------------

namespace {

/// Compute (line, col) for a given offset in content
std::pair<int, int> offset_to_line_col(const std::string& content, size_t offset) {
    int line = 0, col = 0;
    for (size_t i = 0; i < offset && i < content.size(); ++i) {
        if (content[i] == '\n') { line++; col = 0; }
        else { col++; }
    }
    return {line, col};
}

/// Extract all variable declarations with their type annotations.
/// Returns: {name, declared_type, offset_of_name}
struct VarDecl {
    std::string name;
    std::string declared_type;
    size_t name_offset = 0;
};

std::vector<VarDecl> extract_typed_declarations(const std::string& content) {
    std::vector<VarDecl> decls;
    // Match: let/var name : Type = ...
    std::regex decl_re(R"(\b(let|var)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*:\s*([a-zA-Z_][a-zA-Z0-9_]*))");
    auto begin = std::sregex_iterator(content.begin(), content.end(), decl_re);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        VarDecl d;
        d.name = (*it)[2].str();
        d.declared_type = (*it)[3].str();
        d.name_offset = static_cast<size_t>(it->position(2));
        decls.push_back(d);
    }
    return decls;
}

/// Determine the type of a literal expression
std::string infer_literal_type(const std::string& expr) {
    // Trim whitespace
    size_t s = expr.find_first_not_of(" \t");
    if (s == std::string::npos) return "";
    size_t e = expr.find_last_not_of(" \t");
    std::string trimmed = expr.substr(s, e - s + 1);

    if (trimmed.empty()) return "";

    // String literal
    if (trimmed.front() == '"') return "String";

    // Boolean
    if (trimmed == "true" || trimmed == "false") return "Bool";

    // Float (contains a dot and digits)
    if (trimmed.find('.') != std::string::npos) {
        bool all_digits_or_dot = true;
        for (char c : trimmed) {
            if (!std::isdigit(static_cast<unsigned char>(c)) && c != '.' && c != '-') {
                all_digits_or_dot = false;
                break;
            }
        }
        if (all_digits_or_dot) return "Float";
    }

    // Integer
    {
        bool all_digits = true;
        size_t start = 0;
        if (!trimmed.empty() && trimmed[0] == '-') start = 1;
        if (start < trimmed.size()) {
            for (size_t i = start; i < trimmed.size(); ++i) {
                if (!std::isdigit(static_cast<unsigned char>(trimmed[i]))) {
                    all_digits = false;
                    break;
                }
            }
            if (all_digits) return "Int";
        }
    }

    return "";
}

/// Extract all declared symbol names (variables, functions, parameters, types)
struct DeclaredSymbol {
    std::string name;
    std::string kind;  // "variable", "function", "type", "parameter"
};

std::vector<DeclaredSymbol> extract_all_declared_symbols(const std::string& content) {
    std::vector<DeclaredSymbol> symbols;

    // Variables: let/var name
    {
        std::regex re(R"(\b(?:let|var)\s+([a-zA-Z_][a-zA-Z0-9_]*))");
        auto begin = std::sregex_iterator(content.begin(), content.end(), re);
        auto end = std::sregex_iterator();
        for (auto it = begin; it != end; ++it) {
            symbols.push_back({(*it)[1].str(), "variable"});
        }
    }

    // Functions: fnc name
    {
        std::regex re(R"(\bfnc\s+([a-zA-Z_][a-zA-Z0-9_]*))");
        auto begin = std::sregex_iterator(content.begin(), content.end(), re);
        auto end = std::sregex_iterator();
        for (auto it = begin; it != end; ++it) {
            symbols.push_back({(*it)[1].str(), "function"});
        }
    }

    // Types: struct/enum/trait name
    {
        std::regex re(R"(\b(?:struct|enum|trait)\s+([a-zA-Z_][a-zA-Z0-9_]*))");
        auto begin = std::sregex_iterator(content.begin(), content.end(), re);
        auto end = std::sregex_iterator();
        for (auto it = begin; it != end; ++it) {
            symbols.push_back({(*it)[1].str(), "type"});
        }
    }

    // Function parameters: fnc name(param1: Type, param2: Type)
    {
        std::regex fnc_re(R"(\bfnc\s+[a-zA-Z_][a-zA-Z0-9_]*\s*\(([^)]*)\))");
        auto begin = std::sregex_iterator(content.begin(), content.end(), fnc_re);
        auto end = std::sregex_iterator();
        for (auto it = begin; it != end; ++it) {
            std::string params = (*it)[1].str();
            std::regex param_re(R"(([a-zA-Z_][a-zA-Z0-9_]*)\s*:)");
            auto pbegin = std::sregex_iterator(params.begin(), params.end(), param_re);
            auto pend = std::sregex_iterator();
            for (auto pit = pbegin; pit != pend; ++pit) {
                symbols.push_back({(*pit)[1].str(), "parameter"});
            }
        }
    }

    return symbols;
}

/// Built-in Meld keywords and type names that are always available
std::vector<std::string> get_builtin_names() {
    return {
        // Keywords
        "fnc", "let", "var", "if", "else", "match", "return",
        "import", "struct", "enum", "trait", "impl", "effect",
        "handle", "perform", "async", "await", "for", "while",
        "true", "false",
        // Built-in types
        "Int", "Float", "String", "Bool", "Void", "List", "Map",
        "Option", "Result"
    };
}

/// Check if a name is a Meld keyword or built-in
bool is_keyword_or_builtin(const std::string& name) {
    static const auto builtins = get_builtin_names();
    return std::find(builtins.begin(), builtins.end(), name) != builtins.end();
}

/// Extract identifier references used in expressions (not declarations)
struct SymbolRef {
    std::string name;
    size_t offset;
};

std::vector<SymbolRef> extract_symbol_references(const std::string& content) {
    std::vector<SymbolRef> refs;

    // Find all identifiers in the content
    std::regex id_re(R"(\b([a-zA-Z_][a-zA-Z0-9_]*)\b)");
    auto begin = std::sregex_iterator(content.begin(), content.end(), id_re);
    auto end = std::sregex_iterator();

    // Also find all declaration positions to exclude them
    // Declarations: let/var NAME, fnc NAME, struct/enum/trait NAME, param: Type
    std::set<size_t> decl_offsets;

    // let/var declarations
    {
        std::regex re(R"(\b(?:let|var)\s+([a-zA-Z_][a-zA-Z0-9_]*))");
        auto b = std::sregex_iterator(content.begin(), content.end(), re);
        auto e = std::sregex_iterator();
        for (auto it = b; it != e; ++it) {
            decl_offsets.insert(static_cast<size_t>(it->position(1)));
        }
    }
    // fnc declarations
    {
        std::regex re(R"(\bfnc\s+([a-zA-Z_][a-zA-Z0-9_]*))");
        auto b = std::sregex_iterator(content.begin(), content.end(), re);
        auto e = std::sregex_iterator();
        for (auto it = b; it != e; ++it) {
            decl_offsets.insert(static_cast<size_t>(it->position(1)));
        }
    }
    // struct/enum/trait declarations
    {
        std::regex re(R"(\b(?:struct|enum|trait)\s+([a-zA-Z_][a-zA-Z0-9_]*))");
        auto b = std::sregex_iterator(content.begin(), content.end(), re);
        auto e = std::sregex_iterator();
        for (auto it = b; it != e; ++it) {
            decl_offsets.insert(static_cast<size_t>(it->position(1)));
        }
    }
    // Parameter declarations (name before ':' in function params)
    {
        std::regex fnc_re(R"(\bfnc\s+[a-zA-Z_][a-zA-Z0-9_]*\s*\(([^)]*)\))");
        auto fb = std::sregex_iterator(content.begin(), content.end(), fnc_re);
        auto fe = std::sregex_iterator();
        for (auto fit = fb; fit != fe; ++fit) {
            size_t params_start = static_cast<size_t>(fit->position(1));
            std::string params = (*fit)[1].str();
            std::regex param_re(R"(([a-zA-Z_][a-zA-Z0-9_]*)\s*:)");
            auto pb = std::sregex_iterator(params.begin(), params.end(), param_re);
            auto pe = std::sregex_iterator();
            for (auto pit = pb; pit != pe; ++pit) {
                decl_offsets.insert(params_start + static_cast<size_t>(pit->position(1)));
            }
        }
    }
    // Type annotations (after ':' or '->')
    std::set<size_t> type_annotation_offsets;
    {
        std::regex re(R"((?::\s*|->s*)([a-zA-Z_][a-zA-Z0-9_]*))");
        auto b = std::sregex_iterator(content.begin(), content.end(), re);
        auto e = std::sregex_iterator();
        for (auto it = b; it != e; ++it) {
            type_annotation_offsets.insert(static_cast<size_t>(it->position(1)));
        }
    }

    for (auto it = begin; it != end; ++it) {
        size_t pos = static_cast<size_t>(it->position(1));
        std::string name = (*it)[1].str();

        // Skip declaration sites
        if (decl_offsets.count(pos)) continue;
        // Skip type annotations
        if (type_annotation_offsets.count(pos)) continue;
        // Skip keywords and builtins
        if (is_keyword_or_builtin(name)) continue;

        refs.push_back({name, pos});
    }

    return refs;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Type checking
// ---------------------------------------------------------------------------

TypeCheckResult AnalysisEngine::check_types(const std::string& uri, const std::string& content) {
    TypeCheckResult result;
    result.uri = uri;

    if (content.empty()) return result;

    // Find typed variable declarations with assignments:
    // let/var name : Type = expression
    std::regex typed_assign_re(
        R"(\b(?:let|var)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*:\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*([^\n;]+))");

    auto begin = std::sregex_iterator(content.begin(), content.end(), typed_assign_re);
    auto end = std::sregex_iterator();

    for (auto it = begin; it != end; ++it) {
        std::string var_name = (*it)[1].str();
        std::string declared_type = (*it)[2].str();
        std::string expr = (*it)[3].str();
        size_t assign_offset = static_cast<size_t>(it->position(3));

        std::string inferred = infer_literal_type(expr);
        if (inferred.empty()) continue;  // can't infer, skip

        if (inferred != declared_type) {
            auto [line, col] = offset_to_line_col(content, assign_offset);
            TypeError err;
            err.line = line;
            err.character = col;
            err.end_line = line;
            err.end_character = col + static_cast<int>(expr.size());
            err.expected_type = declared_type;
            err.actual_type = inferred;
            err.message = "Type mismatch: expected '" + declared_type +
                          "' but found '" + inferred + "'";
            err.suggestion = "Change the type annotation to '" + inferred +
                             "' or change the value to match '" + declared_type + "'";
            result.errors.push_back(std::move(err));
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
// Undefined symbol detection
// ---------------------------------------------------------------------------

UndefinedSymbolResult AnalysisEngine::detect_undefined_symbols(
    const std::string& uri, const std::string& content) {

    UndefinedSymbolResult result;
    result.uri = uri;

    if (content.empty()) return result;

    // Collect all declared symbol names
    auto declared = extract_all_declared_symbols(content);
    std::vector<std::string> declared_names;
    for (const auto& sym : declared) {
        declared_names.push_back(sym.name);
    }

    // Add built-in names
    auto builtins = get_builtin_names();
    declared_names.insert(declared_names.end(), builtins.begin(), builtins.end());

    // Find all symbol references
    auto refs = extract_symbol_references(content);

    // Check each reference against declared names
    std::set<std::string> declared_set(declared_names.begin(), declared_names.end());

    for (const auto& ref : refs) {
        if (declared_set.count(ref.name)) continue;

        auto [line, col] = offset_to_line_col(content, ref.offset);
        UndefinedSymbolError err;
        err.line = line;
        err.character = col;
        err.end_line = line;
        err.end_character = col + static_cast<int>(ref.name.size());
        err.symbol_name = ref.name;
        err.message = "Undefined symbol '" + ref.name + "'";
        err.suggestions = find_similar_names(ref.name, declared_names);
        if (!err.suggestions.empty()) {
            err.message += ". Did you mean '" + err.suggestions[0] + "'?";
        }
        result.errors.push_back(std::move(err));
    }

    return result;
}

// ---------------------------------------------------------------------------
// Refinement constraint validation (Req 3.4)
// ---------------------------------------------------------------------------

RefinementConstraintResult AnalysisEngine::validate_refinement_constraints(
    const std::string& uri, const std::string& content) {

    RefinementConstraintResult result;
    result.uri = uri;
    if (content.empty()) return result;

    // Match: type Name = BaseType where { predicate }
    std::regex ref_re(R"(\btype\s+([A-Z][a-zA-Z0-9_]*)\s*=\s*([A-Z][a-zA-Z0-9_]*)\s+where\s*\{([^}]*)\})");
    auto begin = std::sregex_iterator(content.begin(), content.end(), ref_re);
    auto end = std::sregex_iterator();

    for (auto it = begin; it != end; ++it) {
        std::string type_name = (*it)[1].str();
        std::string base_type = (*it)[2].str();
        std::string constraint = (*it)[3].str();
        size_t offset = static_cast<size_t>(it->position());

        // Trim constraint
        size_t s = constraint.find_first_not_of(" \t\n\r");
        size_t e = constraint.find_last_not_of(" \t\n\r");
        if (s != std::string::npos) {
            constraint = constraint.substr(s, e - s + 1);
        }

        // Validate constraint is non-empty
        if (constraint.empty() || constraint.find_first_not_of(" \t\n\r") == std::string::npos) {
            auto [line, col] = offset_to_line_col(content, offset);
            RefinementConstraintError err;
            err.line = line;
            err.character = col;
            err.end_line = line;
            err.end_character = col + static_cast<int>((*it)[0].str().size());
            err.type_name = type_name;
            err.constraint = constraint;
            err.message = "Refinement type '" + type_name + "' has empty constraint predicate";
            result.errors.push_back(std::move(err));
            continue;
        }

        // Validate constraint references 'it' (the value being constrained)
        if (constraint.find("it") == std::string::npos) {
            auto [line, col] = offset_to_line_col(content, offset);
            RefinementConstraintError err;
            err.line = line;
            err.character = col;
            err.end_line = line;
            err.end_character = col + static_cast<int>((*it)[0].str().size());
            err.type_name = type_name;
            err.constraint = constraint;
            err.message = "Refinement type '" + type_name +
                          "' constraint must reference 'it' to constrain the value";
            result.errors.push_back(std::move(err));
            continue;
        }

        // Validate constraint has a comparison operator
        bool has_operator = false;
        for (const auto& op : {">=", "<=", "!=", "==", ">", "<", "&&", "||"}) {
            if (constraint.find(op) != std::string::npos) {
                has_operator = true;
                break;
            }
        }
        if (!has_operator) {
            auto [line, col] = offset_to_line_col(content, offset);
            RefinementConstraintError err;
            err.line = line;
            err.character = col;
            err.end_line = line;
            err.end_character = col + static_cast<int>((*it)[0].str().size());
            err.type_name = type_name;
            err.constraint = constraint;
            err.message = "Refinement type '" + type_name +
                          "' constraint must contain a comparison operator";
            result.errors.push_back(std::move(err));
        }
    }

    // Also detect malformed refinement types (missing closing brace)
    std::regex malformed_re(R"(\btype\s+([A-Z][a-zA-Z0-9_]*)\s*=\s*[A-Z][a-zA-Z0-9_]*\s+where\s*\{[^}]*$)");
    std::istringstream stream(content);
    std::string line_str;
    int line_num = 0;
    while (std::getline(stream, line_str)) {
        std::smatch m;
        if (std::regex_search(line_str, m, malformed_re)) {
            RefinementConstraintError err;
            err.line = line_num;
            err.character = static_cast<int>(m.position());
            err.end_line = line_num;
            err.end_character = static_cast<int>(line_str.size());
            err.type_name = m[1].str();
            err.constraint = "";
            err.message = "Malformed refinement type '" + m[1].str() + "': missing closing '}'";
            result.errors.push_back(std::move(err));
        }
        line_num++;
    }

    return result;
}

// ---------------------------------------------------------------------------
// Dispatch ambiguity detection (Req 3.5)
// ---------------------------------------------------------------------------

DispatchAmbiguityResult AnalysisEngine::detect_dispatch_ambiguity(
    const std::string& uri, const std::string& content) {

    DispatchAmbiguityResult result;
    result.uri = uri;
    if (content.empty()) return result;

    // Collect all function declarations with their parameter types
    struct FuncDecl {
        std::string name;
        std::string params;
        std::string signature;
        size_t offset;
    };

    std::vector<FuncDecl> decls;
    std::regex fnc_re(R"(\bfnc\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*\(([^)]*)\))");
    auto begin = std::sregex_iterator(content.begin(), content.end(), fnc_re);
    auto end = std::sregex_iterator();

    for (auto it = begin; it != end; ++it) {
        FuncDecl d;
        d.name = (*it)[1].str();
        d.params = (*it)[2].str();
        d.signature = (*it)[0].str();
        d.offset = static_cast<size_t>(it->position());

        // Extract parameter types
        std::string param_types;
        std::regex param_re(R"([a-zA-Z_][a-zA-Z0-9_]*\s*:\s*([a-zA-Z_][a-zA-Z0-9_]*))");
        auto pb = std::sregex_iterator(d.params.begin(), d.params.end(), param_re);
        auto pe = std::sregex_iterator();
        for (auto pit = pb; pit != pe; ++pit) {
            if (!param_types.empty()) param_types += ", ";
            param_types += (*pit)[1].str();
        }
        d.params = param_types;
        decls.push_back(d);
    }

    // Group by function name
    std::unordered_map<std::string, std::vector<FuncDecl>> by_name;
    for (const auto& d : decls) {
        by_name[d.name].push_back(d);
    }

    // Check for ambiguity: same name + same parameter types
    for (const auto& [name, funcs] : by_name) {
        if (funcs.size() < 2) continue;

        // Check for overlapping parameter types
        for (size_t i = 0; i < funcs.size(); ++i) {
            for (size_t j = i + 1; j < funcs.size(); ++j) {
                if (funcs[i].params == funcs[j].params) {
                    auto [line, col] = offset_to_line_col(content, funcs[j].offset);
                    DispatchAmbiguityError err;
                    err.line = line;
                    err.character = col;
                    err.end_line = line;
                    err.end_character = col + static_cast<int>(funcs[j].signature.size());
                    err.function_name = name;
                    err.message = "Ambiguous dispatch: function '" + name +
                                  "' has multiple declarations with identical parameter types";
                    err.conflicting_signatures.push_back(funcs[i].signature);
                    err.conflicting_signatures.push_back(funcs[j].signature);
                    result.errors.push_back(std::move(err));
                }
            }
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
// Homoiconic construct validation (Req 7.1)
// ---------------------------------------------------------------------------

HomoiconicResult AnalysisEngine::validate_homoiconic_constructs(
    const std::string& uri, const std::string& content) {

    HomoiconicResult result;
    result.uri = uri;
    if (content.empty()) return result;

    // Count and validate quote { ... } constructs
    std::regex quote_re(R"(\bquote\s*\{)");
    auto qb = std::sregex_iterator(content.begin(), content.end(), quote_re);
    auto qe = std::sregex_iterator();
    for (auto it = qb; it != qe; ++it) {
        result.quote_count++;
        size_t start = static_cast<size_t>(it->position()) + it->length() - 1;
        // Verify matching closing brace
        int depth = 1;
        size_t pos = start + 1;
        while (pos < content.size() && depth > 0) {
            if (content[pos] == '{') depth++;
            else if (content[pos] == '}') depth--;
            pos++;
        }
        if (depth != 0) {
            auto [line, col] = offset_to_line_col(content, static_cast<size_t>(it->position()));
            HomoiconicError err;
            err.line = line;
            err.character = col;
            err.end_line = line;
            err.end_character = col + static_cast<int>(it->length());
            err.construct = "quote";
            err.message = "Unmatched 'quote' block: missing closing '}'";
            result.errors.push_back(std::move(err));
        }
    }

    // Count and validate unquote(...) constructs
    std::regex unquote_re(R"(\bunquote\s*\()");
    auto ub = std::sregex_iterator(content.begin(), content.end(), unquote_re);
    auto ue = std::sregex_iterator();
    for (auto it = ub; it != ue; ++it) {
        result.unquote_count++;
        size_t start = static_cast<size_t>(it->position()) + it->length() - 1;
        // Verify matching closing paren
        int depth = 1;
        size_t pos = start + 1;
        while (pos < content.size() && depth > 0) {
            if (content[pos] == '(') depth++;
            else if (content[pos] == ')') depth--;
            pos++;
        }
        if (depth != 0) {
            auto [line, col] = offset_to_line_col(content, static_cast<size_t>(it->position()));
            HomoiconicError err;
            err.line = line;
            err.character = col;
            err.end_line = line;
            err.end_character = col + static_cast<int>(it->length());
            err.construct = "unquote";
            err.message = "Unmatched 'unquote' call: missing closing ')'";
            result.errors.push_back(std::move(err));
        }
    }

    // Check for unquote outside of quote context
    // Simple heuristic: unquote should appear within a quote block
    if (result.unquote_count > 0 && result.quote_count == 0) {
        // Find first unquote
        auto first_uq = std::sregex_iterator(content.begin(), content.end(), unquote_re);
        if (first_uq != std::sregex_iterator()) {
            auto [line, col] = offset_to_line_col(content, static_cast<size_t>(first_uq->position()));
            HomoiconicError err;
            err.line = line;
            err.character = col;
            err.end_line = line;
            err.end_character = col + static_cast<int>(first_uq->length());
            err.construct = "unquote";
            err.message = "'unquote' used outside of any 'quote' block";
            result.errors.push_back(std::move(err));
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
// Macro definition validation (Req 7.2)
// ---------------------------------------------------------------------------

MacroResult AnalysisEngine::validate_macro_definitions(
    const std::string& uri, const std::string& content) {

    MacroResult result;
    result.uri = uri;
    if (content.empty()) return result;

    // Match: macro name(params) { body } or macro name { body }
    std::regex macro_re(R"(\bmacro\s+([a-zA-Z_][a-zA-Z0-9_]*)(\s*\([^)]*\))?\s*\{)");
    auto mb = std::sregex_iterator(content.begin(), content.end(), macro_re);
    auto me = std::sregex_iterator();

    for (auto it = mb; it != me; ++it) {
        result.macro_count++;
        std::string macro_name = (*it)[1].str();
        size_t brace_pos = static_cast<size_t>(it->position()) + it->length() - 1;

        // Verify matching closing brace
        int depth = 1;
        size_t pos = brace_pos + 1;
        while (pos < content.size() && depth > 0) {
            if (content[pos] == '{') depth++;
            else if (content[pos] == '}') depth--;
            pos++;
        }

        if (depth != 0) {
            auto [line, col] = offset_to_line_col(content, static_cast<size_t>(it->position()));
            MacroError err;
            err.line = line;
            err.character = col;
            err.end_line = line;
            err.end_character = col + static_cast<int>(it->length());
            err.macro_name = macro_name;
            err.message = "Macro '" + macro_name + "' has unmatched opening brace";
            result.errors.push_back(std::move(err));
            continue;
        }

        // Check for empty macro body
        std::string body = content.substr(brace_pos + 1, pos - brace_pos - 2);
        if (body.find_first_not_of(" \t\n\r") == std::string::npos) {
            auto [line, col] = offset_to_line_col(content, static_cast<size_t>(it->position()));
            MacroError err;
            err.line = line;
            err.character = col;
            err.end_line = line;
            err.end_character = col + static_cast<int>(it->length());
            err.macro_name = macro_name;
            err.message = "Macro '" + macro_name + "' has empty body";
            result.errors.push_back(std::move(err));
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
// Refinement type evaluation (Req 7.3)
// ---------------------------------------------------------------------------

RefinementTypeResult AnalysisEngine::evaluate_refinement_types(
    const std::string& uri, const std::string& content) {

    RefinementTypeResult result;
    result.uri = uri;
    if (content.empty()) return result;

    // Match: type Name = BaseType where { predicate }
    std::regex ref_re(R"(\btype\s+([A-Z][a-zA-Z0-9_]*)\s*=\s*([A-Z][a-zA-Z0-9_]*)\s+where\s*\{([^}]*)\})");
    auto begin = std::sregex_iterator(content.begin(), content.end(), ref_re);
    auto end = std::sregex_iterator();

    for (auto it = begin; it != end; ++it) {
        result.refinement_count++;
        std::string type_name = (*it)[1].str();
        std::string base_type = (*it)[2].str();
        std::string predicate = (*it)[3].str();
        size_t offset = static_cast<size_t>(it->position());

        // Trim predicate
        size_t s = predicate.find_first_not_of(" \t\n\r");
        size_t e = predicate.find_last_not_of(" \t\n\r");
        if (s != std::string::npos) {
            predicate = predicate.substr(s, e - s + 1);
        }

        // Validate predicate has balanced parentheses
        int paren_depth = 0;
        for (char c : predicate) {
            if (c == '(') paren_depth++;
            else if (c == ')') paren_depth--;
            if (paren_depth < 0) break;
        }
        if (paren_depth != 0) {
            auto [line, col] = offset_to_line_col(content, offset);
            RefinementTypeError err;
            err.line = line;
            err.character = col;
            err.end_line = line;
            err.end_character = col + static_cast<int>((*it)[0].str().size());
            err.type_name = type_name;
            err.predicate = predicate;
            err.message = "Refinement type '" + type_name +
                          "' has unbalanced parentheses in predicate";
            result.errors.push_back(std::move(err));
            continue;
        }

        // Validate predicate doesn't use assignment (= instead of ==)
        // Look for single = not preceded or followed by = or ! or < or >
        for (size_t i = 0; i < predicate.size(); ++i) {
            if (predicate[i] == '=') {
                bool is_comparison = false;
                if (i > 0 && (predicate[i-1] == '=' || predicate[i-1] == '!' ||
                              predicate[i-1] == '<' || predicate[i-1] == '>')) {
                    is_comparison = true;
                }
                if (i + 1 < predicate.size() && predicate[i+1] == '=') {
                    is_comparison = true;
                }
                if (!is_comparison) {
                    auto [line, col] = offset_to_line_col(content, offset);
                    RefinementTypeError err;
                    err.line = line;
                    err.character = col;
                    err.end_line = line;
                    err.end_character = col + static_cast<int>((*it)[0].str().size());
                    err.type_name = type_name;
                    err.predicate = predicate;
                    err.message = "Refinement type '" + type_name +
                                  "' predicate uses assignment '=' instead of comparison '=='";
                    result.errors.push_back(std::move(err));
                    break;
                }
            }
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
// Multiple dispatch resolution (Req 7.4)
// ---------------------------------------------------------------------------

DispatchResolutionResult AnalysisEngine::resolve_multiple_dispatch(
    const std::string& uri, const std::string& content) {

    DispatchResolutionResult result;
    result.uri = uri;
    if (content.empty()) return result;

    // Collect function declarations with parameter types
    struct FuncSig {
        std::string name;
        std::vector<std::string> param_types;
        std::string full_sig;
        size_t offset;
    };

    std::vector<FuncSig> signatures;
    std::regex fnc_re(R"(\bfnc\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*\(([^)]*)\))");
    auto fb = std::sregex_iterator(content.begin(), content.end(), fnc_re);
    auto fe = std::sregex_iterator();

    for (auto it = fb; it != fe; ++it) {
        FuncSig sig;
        sig.name = (*it)[1].str();
        sig.full_sig = (*it)[0].str();
        sig.offset = static_cast<size_t>(it->position());

        std::string params = (*it)[2].str();
        std::regex param_re(R"([a-zA-Z_][a-zA-Z0-9_]*\s*:\s*([a-zA-Z_][a-zA-Z0-9_]*))");
        auto pb = std::sregex_iterator(params.begin(), params.end(), param_re);
        auto pe = std::sregex_iterator();
        for (auto pit = pb; pit != pe; ++pit) {
            sig.param_types.push_back((*pit)[1].str());
        }
        signatures.push_back(sig);
    }

    // Collect function calls: name(args)
    std::regex call_re(R"(\b([a-zA-Z_][a-zA-Z0-9_]*)\s*\(([^)]*)\))");
    auto cb = std::sregex_iterator(content.begin(), content.end(), call_re);
    auto ce = std::sregex_iterator();

    // Build set of declared function names
    std::set<std::string> declared_funcs;
    for (const auto& sig : signatures) {
        declared_funcs.insert(sig.name);
    }

    for (auto it = cb; it != ce; ++it) {
        std::string call_name = (*it)[1].str();
        size_t call_offset = static_cast<size_t>(it->position());

        // Skip if this is a declaration (preceded by 'fnc')
        if (call_offset >= 4) {
            std::string before = content.substr(call_offset > 20 ? call_offset - 20 : 0,
                                                call_offset > 20 ? 20 : call_offset);
            if (before.find("fnc") != std::string::npos) {
                // Check if fnc is close enough to be the declaration keyword
                size_t fnc_pos = before.rfind("fnc");
                std::string between = before.substr(fnc_pos + 3);
                if (between.find_first_not_of(" \t") == std::string::npos ||
                    between.find(call_name) != std::string::npos) {
                    continue;
                }
            }
        }

        // Skip keywords
        if (call_name == "if" || call_name == "while" || call_name == "for" ||
            call_name == "match" || call_name == "macro" || call_name == "unquote") {
            continue;
        }

        // Only check calls to functions that have multiple dispatch declarations
        std::vector<FuncSig> matching;
        for (const auto& sig : signatures) {
            if (sig.name == call_name) {
                matching.push_back(sig);
            }
        }

        // Count args in the call
        std::string args = (*it)[2].str();
        int arg_count = 0;
        if (!args.empty() && args.find_first_not_of(" \t") != std::string::npos) {
            arg_count = 1;
            for (char c : args) {
                if (c == ',') arg_count++;
            }
        }

        // Check if any overload matches the arg count
        if (matching.size() >= 2) {
            bool any_match = false;
            for (const auto& sig : matching) {
                if (static_cast<int>(sig.param_types.size()) == arg_count) {
                    any_match = true;
                    break;
                }
            }
            if (!any_match && arg_count > 0) {
                auto [line, col] = offset_to_line_col(content, call_offset);
                DispatchResolutionError err;
                err.line = line;
                err.character = col;
                err.end_line = line;
                err.end_character = col + static_cast<int>((*it)[0].str().size());
                err.function_name = call_name;
                err.message = "No matching overload for '" + call_name +
                              "' with " + std::to_string(arg_count) + " argument(s)";
                for (const auto& sig : matching) {
                    err.candidate_signatures.push_back(sig.full_sig);
                }
                result.errors.push_back(std::move(err));
            }
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
// Tree initialization validation (Req 7.5)
// ---------------------------------------------------------------------------

TreeInitResult AnalysisEngine::validate_tree_initialization(
    const std::string& uri, const std::string& content) {

    TreeInitResult result;
    result.uri = uri;
    if (content.empty()) return result;

    // Collect struct definitions and their fields
    struct StructDef {
        std::string name;
        std::vector<std::string> fields;
    };

    std::vector<StructDef> structs;
    std::regex struct_re(R"(\bstruct\s+([A-Z][a-zA-Z0-9_]*)\s*\{([^}]*)\})");
    auto sb = std::sregex_iterator(content.begin(), content.end(), struct_re);
    auto se = std::sregex_iterator();

    for (auto it = sb; it != se; ++it) {
        StructDef sd;
        sd.name = (*it)[1].str();
        std::string body = (*it)[2].str();

        // Extract field names: field_name: Type
        std::regex field_re(R"(([a-zA-Z_][a-zA-Z0-9_]*)\s*:)");
        auto fb = std::sregex_iterator(body.begin(), body.end(), field_re);
        auto fe = std::sregex_iterator();
        for (auto fit = fb; fit != fe; ++fit) {
            sd.fields.push_back((*fit)[1].str());
        }
        structs.push_back(sd);
    }

    // Find tree initializations: TypeName { field: value, ... }
    // But NOT struct definitions (preceded by 'struct')
    std::regex init_re(R"(\b([A-Z][a-zA-Z0-9_]*)\s*\{([^}]*)\})");
    auto ib = std::sregex_iterator(content.begin(), content.end(), init_re);
    auto ie = std::sregex_iterator();

    for (auto it = ib; it != ie; ++it) {
        std::string type_name = (*it)[1].str();
        size_t offset = static_cast<size_t>(it->position());

        // Skip struct definitions
        if (offset >= 7) {
            std::string before = content.substr(offset > 20 ? offset - 20 : 0,
                                                offset > 20 ? 20 : offset);
            if (before.find("struct") != std::string::npos) {
                size_t struct_pos = before.rfind("struct");
                std::string between = before.substr(struct_pos + 6);
                if (between.find_first_not_of(" \t\n\r") == std::string::npos) {
                    continue;
                }
            }
        }

        // Skip type/enum/trait/macro definitions
        if (offset >= 6) {
            std::string before = content.substr(offset > 20 ? offset - 20 : 0,
                                                offset > 20 ? 20 : offset);
            bool is_def = false;
            for (const auto& kw : {"type", "enum", "trait", "macro", "where"}) {
                if (before.find(kw) != std::string::npos) {
                    is_def = true;
                    break;
                }
            }
            if (is_def) continue;
        }

        std::string body = (*it)[2].str();

        // Extract field names used in initialization
        std::regex field_re(R"(([a-zA-Z_][a-zA-Z0-9_]*)\s*:)");
        auto fb = std::sregex_iterator(body.begin(), body.end(), field_re);
        auto fe = std::sregex_iterator();

        std::vector<std::string> init_fields;
        for (auto fit = fb; fit != fe; ++fit) {
            init_fields.push_back((*fit)[1].str());
        }

        if (init_fields.empty()) continue;

        result.init_count++;

        // Find matching struct definition
        const StructDef* matching_struct = nullptr;
        for (const auto& sd : structs) {
            if (sd.name == type_name) {
                matching_struct = &sd;
                break;
            }
        }

        if (!matching_struct) continue;  // Can't validate without struct def

        // Check for unknown fields
        std::set<std::string> valid_fields(matching_struct->fields.begin(),
                                           matching_struct->fields.end());
        for (const auto& field : init_fields) {
            if (valid_fields.find(field) == valid_fields.end()) {
                auto [line, col] = offset_to_line_col(content, offset);
                TreeInitError err;
                err.line = line;
                err.character = col;
                err.end_line = line;
                err.end_character = col + static_cast<int>((*it)[0].str().size());
                err.struct_name = type_name;
                err.field_name = field;
                err.message = "Unknown field '" + field + "' in initialization of '" +
                              type_name + "'";
                result.errors.push_back(std::move(err));
            }
        }
    }

    return result;
}

} // namespace meld::lsp::analysis

// ---------------------------------------------------------------------------
// Find references (Req 4.2)
// ---------------------------------------------------------------------------

namespace meld::lsp::analysis {

std::vector<Location> AnalysisEngine::find_references(
    const std::string& uri, const std::string& content,
    const std::string& symbol_name) {

    std::vector<Location> refs;
    if (content.empty() || symbol_name.empty()) return refs;

    // Find all occurrences of the symbol name as a whole word
    std::regex sym_re("\\b" + symbol_name + "\\b");
    auto begin = std::sregex_iterator(content.begin(), content.end(), sym_re);
    auto end = std::sregex_iterator();

    for (auto it = begin; it != end; ++it) {
        size_t offset = static_cast<size_t>(it->position());
        int line = 0, col = 0;
        for (size_t i = 0; i < offset && i < content.size(); ++i) {
            if (content[i] == '\n') { line++; col = 0; }
            else { col++; }
        }
        Location loc;
        loc.uri = uri;
        loc.line = line;
        loc.character = col;
        loc.end_line = line;
        loc.end_character = col + static_cast<int>(symbol_name.size());
        refs.push_back(loc);
    }

    return refs;
}

// ---------------------------------------------------------------------------
// Search symbols (Req 4.4)
// ---------------------------------------------------------------------------

std::vector<SymbolInfo> AnalysisEngine::search_symbols(const std::string& query) {
    std::vector<SymbolInfo> results;
    if (query.empty()) {
        // Return all symbols from all cached documents
        for (const auto& [uri, analysis_result] : cache_) {
            for (const auto& sym : analysis_result.symbols) {
                results.push_back(sym);
            }
        }
        return results;
    }

    // Case-insensitive substring match
    std::string lower_query = query;
    std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    for (const auto& [uri, analysis_result] : cache_) {
        for (const auto& sym : analysis_result.symbols) {
            std::string lower_name = sym.name;
            std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if (lower_name.find(lower_query) != std::string::npos) {
                results.push_back(sym);
            }
        }
    }

    return results;
}

} // namespace meld::lsp::analysis

// ---------------------------------------------------------------------------
// Cross-file resolution (Req 6.3)
// ---------------------------------------------------------------------------

namespace meld::lsp::analysis {

std::vector<std::string> AnalysisEngine::get_dependencies(
    const std::string& /*uri*/, const std::string& content) {

    std::vector<std::string> deps;
    if (content.empty()) return deps;

    // Match: import module_name (simple import on its own line)
    {
        std::regex re(R"(\bimport\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*[\n\r])");
        auto begin = std::sregex_iterator(content.begin(), content.end(), re);
        auto end = std::sregex_iterator();
        for (auto it = begin; it != end; ++it) {
            deps.push_back((*it)[1].str());
        }
        // Also check if the file ends with a simple import (no trailing newline)
        std::regex re_eof(R"(\bimport\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*$)");
        std::smatch m;
        if (std::regex_search(content, m, re_eof)) {
            std::string name = m[1].str();
            if (std::find(deps.begin(), deps.end(), name) == deps.end()) {
                deps.push_back(name);
            }
        }
    }

    // Match: import { symbol, symbol2 } from "module"
    {
        std::regex re(R"RE(\bimport\s*\{\s*([a-zA-Z_][a-zA-Z0-9_]*(?:\s*,\s*[a-zA-Z_][a-zA-Z0-9_]*)*)\s*\}\s*from\s*"([^"]+)")RE");
        auto begin = std::sregex_iterator(content.begin(), content.end(), re);
        auto end = std::sregex_iterator();
        for (auto it = begin; it != end; ++it) {
            std::string symbols_str = (*it)[1].str();
            std::regex sym_re(R"([a-zA-Z_][a-zA-Z0-9_]*)");
            auto sb = std::sregex_iterator(symbols_str.begin(), symbols_str.end(), sym_re);
            auto se = std::sregex_iterator();
            for (auto sit = sb; sit != se; ++sit) {
                deps.push_back((*sit)[0].str());
            }
        }
    }

    return deps;
}

std::optional<SymbolInfo> AnalysisEngine::resolve_import(
    const std::string& /*uri*/, const std::string& /*content*/,
    const std::string& import_name) {

    if (import_name.empty()) return std::nullopt;

    for (const auto& [cached_uri, cached_result] : cache_) {
        for (const auto& sym : cached_result.symbols) {
            if (sym.name == import_name) {
                return sym;
            }
        }
    }

    return std::nullopt;
}

std::vector<Location> AnalysisEngine::get_cross_file_references(
    const std::string& /*uri*/, const std::string& /*content*/,
    const std::string& symbol_name) {

    std::vector<Location> refs;
    if (symbol_name.empty()) return refs;

    for (const auto& [cached_uri, cached_result] : cache_) {
        for (const auto& sym : cached_result.symbols) {
            if (sym.name == symbol_name) {
                refs.push_back(sym.definition);
            }
        }
    }

    return refs;
}

} // namespace meld::lsp::analysis
