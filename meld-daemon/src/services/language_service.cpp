#include "language_service.hpp"
#include <algorithm>
#include <sstream>
#include <regex>

namespace meld::lsp::services {

LanguageService::LanguageService() = default;
LanguageService::~LanguageService() = default;

std::vector<std::string> LanguageService::token_type_names() {
    return {
        "keyword",
        "function",
        "variable",
        "type",
        "parameter",
        "property",
        "string",
        "number",
        "comment",
        "operator",
        "decorator",
        "macro",
        "namespace",
        "event"  // for effects
    };
}

json LanguageService::get_semantic_tokens_legend() const {
    return json{
        {"tokenTypes", token_type_names()},
        {"tokenModifiers", json::array({
            "declaration",
            "definition",
            "readonly",
            "static",
            "deprecated",
            "async",
            "modification",
            "documentation"
        })}
    };
}

std::vector<SemanticToken> LanguageService::get_semantic_tokens(
    const std::string& /*uri*/, const std::string& content) {

    std::vector<SemanticToken> tokens;
    if (content.empty()) {
        return tokens;
    }

    // Basic keyword-based tokenization for initial implementation.
    // Full integration with the Meld parser will be done in Task 3.
    static const std::vector<std::pair<std::string, SemanticTokenType>> keywords = {
        {"fnc", SemanticTokenType::Keyword},
        {"let", SemanticTokenType::Keyword},
        {"var", SemanticTokenType::Keyword},
        {"if", SemanticTokenType::Keyword},
        {"else", SemanticTokenType::Keyword},
        {"match", SemanticTokenType::Keyword},
        {"return", SemanticTokenType::Keyword},
        {"import", SemanticTokenType::Keyword},
        {"struct", SemanticTokenType::Keyword},
        {"enum", SemanticTokenType::Keyword},
        {"trait", SemanticTokenType::Keyword},
        {"impl", SemanticTokenType::Keyword},
        {"effect", SemanticTokenType::Keyword},
        {"handle", SemanticTokenType::Keyword},
        {"perform", SemanticTokenType::Keyword},
        {"async", SemanticTokenType::Keyword},
        {"await", SemanticTokenType::Keyword},
        {"for", SemanticTokenType::Keyword},
        {"while", SemanticTokenType::Keyword},
        {"true", SemanticTokenType::Keyword},
        {"false", SemanticTokenType::Keyword},
    };

    int line = 0;
    int col = 0;

    size_t i = 0;
    while (i < content.size()) {
        char c = content[i];

        // Track line/column
        if (c == '\n') {
            line++;
            col = 0;
            i++;
            continue;
        }

        // Skip whitespace
        if (std::isspace(c)) {
            col++;
            i++;
            continue;
        }

        // Line comments
        if (c == '/' && i + 1 < content.size() && content[i + 1] == '/') {
            int start_col = col;
            size_t end = content.find('\n', i);
            int len = (end == std::string::npos)
                ? static_cast<int>(content.size() - i)
                : static_cast<int>(end - i);
            tokens.push_back({line, start_col, len, SemanticTokenType::Comment});
            col += len;
            i += len;
            continue;
        }

        // String literals
        if (c == '"') {
            int start_col = col;
            i++; col++;
            while (i < content.size() && content[i] != '"' && content[i] != '\n') {
                if (content[i] == '\\' && i + 1 < content.size()) {
                    i++; col++;
                }
                i++; col++;
            }
            if (i < content.size() && content[i] == '"') {
                i++; col++;
            }
            tokens.push_back({line, start_col, col - start_col, SemanticTokenType::String});
            continue;
        }

        // Numbers
        if (std::isdigit(c)) {
            int start_col = col;
            while (i < content.size() && (std::isdigit(content[i]) || content[i] == '.')) {
                i++; col++;
            }
            tokens.push_back({line, start_col, col - start_col, SemanticTokenType::Number});
            continue;
        }

        // Decorator / effect annotation
        if (c == '@') {
            int start_col = col;
            i++; col++;
            while (i < content.size() && (std::isalnum(content[i]) || content[i] == '_')) {
                i++; col++;
            }
            tokens.push_back({line, start_col, col - start_col, SemanticTokenType::Decorator});
            continue;
        }

        // Identifiers and keywords
        if (std::isalpha(c) || c == '_') {
            int start_col = col;
            size_t start_i = i;
            while (i < content.size() && (std::isalnum(content[i]) || content[i] == '_')) {
                i++; col++;
            }
            std::string word = content.substr(start_i, i - start_i);

            SemanticTokenType type = SemanticTokenType::Variable;
            for (const auto& [kw, kw_type] : keywords) {
                if (word == kw) {
                    type = kw_type;
                    break;
                }
            }
            tokens.push_back({line, start_col, static_cast<int>(word.size()), type});
            continue;
        }

        // Operators
        if (c == '+' || c == '-' || c == '*' || c == '/' || c == '=' ||
            c == '<' || c == '>' || c == '!' || c == '&' || c == '|' || c == '|') {
            tokens.push_back({line, col, 1, SemanticTokenType::Operator});
            i++; col++;
            continue;
        }

        // Skip other characters (braces, parens, etc.)
        i++; col++;
    }

    return tokens;
}

std::vector<int> LanguageService::encode_semantic_tokens(const std::vector<SemanticToken>& tokens) {
    std::vector<int> encoded;
    if (tokens.empty()) return encoded;

    // Sort tokens by position
    auto sorted = tokens;
    std::sort(sorted.begin(), sorted.end(), [](const SemanticToken& a, const SemanticToken& b) {
        return a.line < b.line || (a.line == b.line && a.start_char < b.start_char);
    });

    int prev_line = 0;
    int prev_char = 0;

    for (const auto& token : sorted) {
        int delta_line = token.line - prev_line;
        int delta_char = (delta_line == 0) ? (token.start_char - prev_char) : token.start_char;

        encoded.push_back(delta_line);
        encoded.push_back(delta_char);
        encoded.push_back(token.length);
        encoded.push_back(static_cast<int>(token.type));
        encoded.push_back(token.modifiers);

        prev_line = token.line;
        prev_char = token.start_char;
    }

    return encoded;
}

std::vector<Diagnostic> LanguageService::get_diagnostics(
    const std::string& /*uri*/, const std::string& content) {

    std::vector<Diagnostic> diagnostics;
    if (content.empty()) {
        return diagnostics;
    }

    // Compute line lengths for bounds clamping
    std::vector<int> line_lengths;
    {
        std::istringstream stream(content);
        std::string line_str;
        while (std::getline(stream, line_str)) {
            line_lengths.push_back(static_cast<int>(line_str.size()));
        }
        if (line_lengths.empty()) {
            line_lengths.push_back(static_cast<int>(content.size()));
        }
    }
    int total_lines = static_cast<int>(line_lengths.size());

    auto clamp_diag = [&](Diagnostic& d) {
        if (d.line < 0) d.line = 0;
        if (d.line >= total_lines) d.line = total_lines - 1;
        if (d.end_line < 0) d.end_line = 0;
        if (d.end_line >= total_lines) d.end_line = total_lines - 1;
        int max_col = line_lengths[d.line];
        if (d.character < 0) d.character = 0;
        if (d.character > max_col) d.character = max_col;
        int max_end_col = line_lengths[d.end_line];
        if (d.end_character < 0) d.end_character = 0;
        if (d.end_character > max_end_col) d.end_character = max_end_col;
    };

    // 1. Check for unclosed string literals
    {
        int line = 0;
        int col = 0;
        for (size_t i = 0; i < content.size(); ++i) {
            char c = content[i];
            if (c == '\n') { line++; col = 0; continue; }
            if (c == '"') {
                int str_line = line;
                int str_col = col;
                i++; col++;
                bool closed = false;
                while (i < content.size() && content[i] != '\n') {
                    if (content[i] == '\\' && i + 1 < content.size()) {
                        i++; col++;
                    }
                    if (content[i] == '"') {
                        closed = true;
                        col++;
                        break;
                    }
                    i++; col++;
                }
                if (!closed) {
                    Diagnostic d;
                    d.severity = DiagnosticSeverity::Error;
                    d.message = "Unterminated string literal";
                    d.source = "meld";
                    d.line = str_line;
                    d.character = str_col;
                    d.end_line = str_line;
                    d.end_character = col;
                    clamp_diag(d);
                    diagnostics.push_back(std::move(d));
                }
            }
            col++;
        }
    }

    // 2. Check for unbalanced braces/parens/brackets
    {
        struct Bracket {
            char ch;
            int line;
            int col;
        };
        std::vector<Bracket> stack;
        int line = 0;
        int col = 0;
        bool in_string = false;
        bool in_line_comment = false;

        for (size_t i = 0; i < content.size(); ++i) {
            char c = content[i];
            if (c == '\n') {
                line++; col = 0;
                in_line_comment = false;
                in_string = false; // single-line strings don't span newlines
                continue;
            }
            if (in_line_comment) { col++; continue; }
            if (!in_string && c == '/' && i + 1 < content.size() && content[i + 1] == '/') {
                in_line_comment = true;
                col++;
                continue;
            }
            if (!in_string && c == '"') {
                in_string = true;
                col++;
                continue;
            }
            if (in_string) {
                if (c == '\\' && i + 1 < content.size()) { i++; col += 2; continue; }
                if (c == '"') in_string = false;
                col++;
                continue;
            }

            if (c == '{' || c == '(' || c == '[') {
                stack.push_back({c, line, col});
            } else if (c == '}' || c == ')' || c == ']') {
                char expected = (c == '}') ? '{' : (c == ')') ? '(' : '[';
                if (!stack.empty() && stack.back().ch == expected) {
                    stack.pop_back();
                } else {
                    Diagnostic d;
                    d.severity = DiagnosticSeverity::Error;
                    d.message = std::string("Unmatched '") + c + "'";
                    d.source = "meld";
                    d.line = line;
                    d.character = col;
                    d.end_line = line;
                    d.end_character = col + 1;
                    clamp_diag(d);
                    diagnostics.push_back(std::move(d));
                }
            }
            col++;
        }

        // Report unclosed opening brackets
        for (const auto& b : stack) {
            Diagnostic d;
            d.severity = DiagnosticSeverity::Error;
            d.message = std::string("Unclosed '") + b.ch + "'";
            d.source = "meld";
            d.line = b.line;
            d.character = b.col;
            d.end_line = b.line;
            d.end_character = b.col + 1;
            clamp_diag(d);
            diagnostics.push_back(std::move(d));
        }
    }

    // 3. Check for invalid tokens (characters not valid in Meld)
    {
        int line = 0;
        int col = 0;
        bool in_string = false;
        bool in_line_comment = false;

        for (size_t i = 0; i < content.size(); ++i) {
            char c = content[i];
            if (c == '\n') {
                line++; col = 0;
                in_line_comment = false;
                in_string = false;
                continue;
            }
            if (in_line_comment) { col++; continue; }
            if (!in_string && c == '/' && i + 1 < content.size() && content[i + 1] == '/') {
                in_line_comment = true;
                col++;
                continue;
            }
            if (!in_string && c == '"') { in_string = true; col++; continue; }
            if (in_string) {
                if (c == '\\' && i + 1 < content.size()) { i++; col += 2; continue; }
                if (c == '"') in_string = false;
                col++;
                continue;
            }

            // Valid characters: alphanumeric, whitespace, operators, punctuation
            bool valid = std::isalnum(static_cast<unsigned char>(c)) ||
                         std::isspace(static_cast<unsigned char>(c)) ||
                         c == '_' || c == '@' || c == '#' ||
                         c == '+' || c == '-' || c == '*' || c == '/' ||
                         c == '=' || c == '<' || c == '>' || c == '!' ||
                         c == '&' || c == '|' || c == '^' || c == '~' ||
                         c == '%' || c == '?' || c == '.' ||
                         c == '(' || c == ')' || c == '{' || c == '}' ||
                         c == '[' || c == ']' || c == ',' || c == ';' ||
                         c == ':' || c == '"' || c == '\'';

            if (!valid) {
                Diagnostic d;
                d.severity = DiagnosticSeverity::Error;
                d.message = std::string("Invalid character '") + c + "'";
                d.source = "meld";
                d.line = line;
                d.character = col;
                d.end_line = line;
                d.end_character = col + 1;
                clamp_diag(d);
                diagnostics.push_back(std::move(d));
            }
            col++;
        }
    }

    return diagnostics;
}

// ─── Cursor context detection helpers ───────────────────────────────────────

namespace {

/// Get the offset into content for a given (line, character) position
size_t get_offset(const std::string& content, int line, int character) {
    int cur_line = 0;
    size_t i = 0;
    while (i < content.size() && cur_line < line) {
        if (content[i] == '\n') cur_line++;
        i++;
    }
    size_t offset = i + static_cast<size_t>(character);
    return std::min(offset, content.size());
}

/// Get the text of the line at the given line number
std::string get_line_text(const std::string& content, int line) {
    int cur_line = 0;
    size_t start = 0;
    for (size_t i = 0; i < content.size(); ++i) {
        if (cur_line == line) {
            start = i;
            size_t end = content.find('\n', i);
            if (end == std::string::npos) end = content.size();
            return content.substr(start, end - start);
        }
        if (content[i] == '\n') cur_line++;
    }
    return "";
}

/// Determine the brace nesting depth at a given offset (how many unclosed '{' before offset)
int get_brace_depth(const std::string& content, size_t offset) {
    int depth = 0;
    bool in_string = false;
    bool in_comment = false;
    for (size_t i = 0; i < offset && i < content.size(); ++i) {
        char c = content[i];
        if (c == '\n') { in_comment = false; in_string = false; continue; }
        if (in_comment) continue;
        if (!in_string && c == '/' && i + 1 < content.size() && content[i + 1] == '/') {
            in_comment = true; continue;
        }
        if (!in_string && c == '"') { in_string = true; continue; }
        if (in_string) {
            if (c == '\\' && i + 1 < content.size()) { i++; continue; }
            if (c == '"') in_string = false;
            continue;
        }
        if (c == '{') depth++;
        else if (c == '}') depth--;
    }
    return depth;
}

/// Check if cursor is in a type position (after ':', '->', or in type annotation context)
bool is_type_position(const std::string& line_text, int character) {
    std::string before = line_text.substr(0, std::min(static_cast<size_t>(character), line_text.size()));
    // Walk backwards past any partial identifier and whitespace to find the trigger char
    int pos = static_cast<int>(before.size()) - 1;
    // Skip trailing identifier characters (partial type being typed)
    while (pos >= 0 && (std::isalnum(static_cast<unsigned char>(before[pos])) || before[pos] == '_')) {
        pos--;
    }
    // Skip whitespace between trigger and identifier/cursor
    while (pos >= 0 && (before[pos] == ' ' || before[pos] == '\t')) {
        pos--;
    }
    if (pos < 0) return false;
    if (before[pos] == ':') return true;
    if (before[pos] == '>' && pos > 0 && before[pos - 1] == '-') return true;
    return false;
}

/// Check if cursor is after '@' (decorator/annotation context)
bool is_annotation_position(const std::string& line_text, int character) {
    std::string before = line_text.substr(0, std::min(static_cast<size_t>(character), line_text.size()));
    size_t end = before.find_last_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_");
    if (end == std::string::npos) return false;
    return before[end] == '@';
}

/// Check if cursor is after '.' (dot access context)
bool is_dot_position(const std::string& line_text, int character) {
    std::string before = line_text.substr(0, std::min(static_cast<size_t>(character), line_text.size()));
    size_t end = before.find_last_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_");
    if (end == std::string::npos) return false;
    return before[end] == '.';
}

/// Extract local variable names declared before the cursor offset
std::vector<std::string> extract_local_variables(const std::string& content, size_t offset) {
    std::vector<std::string> vars;
    std::regex var_re(R"(\b(?:let|var)\s+([a-zA-Z_][a-zA-Z0-9_]*))");
    std::string before = content.substr(0, offset);
    auto begin = std::sregex_iterator(before.begin(), before.end(), var_re);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        vars.push_back((*it)[1].str());
    }
    return vars;
}

/// Extract function names declared in the content
std::vector<std::string> extract_function_names(const std::string& content) {
    std::vector<std::string> funcs;
    std::regex fnc_re(R"(\bfnc\s+([a-zA-Z_][a-zA-Z0-9_]*))");
    auto begin = std::sregex_iterator(content.begin(), content.end(), fnc_re);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        funcs.push_back((*it)[1].str());
    }
    return funcs;
}

/// Extract user-defined type names from struct/enum/trait declarations
std::vector<std::pair<std::string, std::string>> extract_user_defined_types(const std::string& content) {
    std::vector<std::pair<std::string, std::string>> types;
    std::regex type_re(R"(\b(struct|enum|trait)\s+([a-zA-Z_][a-zA-Z0-9_]*))");
    auto begin = std::sregex_iterator(content.begin(), content.end(), type_re);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        std::string kind = (*it)[1].str();
        std::string name = (*it)[2].str();
        std::string desc;
        if (kind == "struct") desc = "User-defined struct";
        else if (kind == "enum") desc = "User-defined enum";
        else if (kind == "trait") desc = "User-defined trait";
        types.push_back({name, desc});
    }
    return types;
}

} // anonymous namespace

// ─── Completion provider ────────────────────────────────────────────────────

CompletionList LanguageService::get_completions(
    const std::string& /*uri*/, const std::string& content,
    int line, int character) {

    CompletionList result;
    if (content.empty()) {
        // Even for empty content, provide top-level keywords
        static const std::vector<std::pair<std::string, std::string>> top_keywords = {
            {"fnc", "Function declaration"},
            {"let", "Immutable binding"},
            {"var", "Mutable binding"},
            {"struct", "Struct definition"},
            {"enum", "Enum definition"},
            {"trait", "Trait definition"},
            {"effect", "Effect definition"},
            {"import", "Import statement"},
        };
        for (const auto& [kw, desc] : top_keywords) {
            result.items.push_back({kw, CompletionItemKind::Keyword, desc, "", kw});
        }
        return result;
    }

    size_t offset = get_offset(content, line, character);
    std::string line_text = get_line_text(content, line);
    int depth = get_brace_depth(content, offset);

    // 1. Annotation context: after '@'
    if (is_annotation_position(line_text, character)) {
        static const std::vector<std::pair<std::string, std::string>> annotations = {
            {"uses", "Effect annotation"},
            {"effect", "Effect declaration"},
            {"pure", "Pure function annotation"},
        };
        for (const auto& [name, desc] : annotations) {
            result.items.push_back({name, CompletionItemKind::Effect, desc, "", name});
        }
        return result;
    }

    // 2. Dot access context: after '.'
    if (is_dot_position(line_text, character)) {
        static const std::vector<std::pair<std::string, std::string>> members = {
            {"length", "Get length"},
            {"size", "Get size"},
            {"to_string", "Convert to string"},
            {"clone", "Clone value"},
        };
        for (const auto& [name, desc] : members) {
            result.items.push_back({name, CompletionItemKind::Property, desc, "", name});
        }
        return result;
    }

    // 3. Type position: after ':' or '->'
    if (is_type_position(line_text, character)) {
        static const std::vector<std::pair<std::string, std::string>> types = {
            {"Int", "Integer type"},
            {"Float", "Floating-point type"},
            {"String", "String type"},
            {"Bool", "Boolean type"},
            {"Void", "Void type"},
            {"List", "List type"},
            {"Map", "Map type"},
            {"Option", "Optional type"},
            {"Result", "Result type"},
        };
        for (const auto& [name, desc] : types) {
            result.items.push_back({name, CompletionItemKind::Type, desc, "", name});
        }

        // Add user-defined types from struct/enum/trait declarations
        auto user_types = extract_user_defined_types(content);
        for (const auto& [name, desc] : user_types) {
            result.items.push_back({name, CompletionItemKind::Type, desc, "", name});
        }

        return result;
    }

    // 4. Inside function body (depth > 0): body-level keywords + local variables
    if (depth > 0) {
        static const std::vector<std::pair<std::string, std::string>> body_keywords = {
            {"let", "Immutable binding"},
            {"var", "Mutable binding"},
            {"if", "Conditional"},
            {"else", "Else branch"},
            {"match", "Pattern match"},
            {"return", "Return statement"},
            {"for", "For loop"},
            {"while", "While loop"},
        };
        for (const auto& [kw, desc] : body_keywords) {
            result.items.push_back({kw, CompletionItemKind::Keyword, desc, "", kw});
        }

        // Add local variables declared before cursor
        auto locals = extract_local_variables(content, offset);
        for (const auto& var : locals) {
            result.items.push_back({var, CompletionItemKind::Variable, "Local variable", "", var});
        }

        // Add known function names
        auto funcs = extract_function_names(content);
        for (const auto& fn : funcs) {
            result.items.push_back({fn, CompletionItemKind::Function, "Function", "", fn});
        }

        return result;
    }

    // 5. Top-level context (depth == 0): top-level keywords
    static const std::vector<std::pair<std::string, std::string>> top_keywords = {
        {"fnc", "Function declaration"},
        {"let", "Immutable binding"},
        {"var", "Mutable binding"},
        {"struct", "Struct definition"},
        {"enum", "Enum definition"},
        {"trait", "Trait definition"},
        {"effect", "Effect definition"},
        {"import", "Import statement"},
    };
    for (const auto& [kw, desc] : top_keywords) {
        result.items.push_back({kw, CompletionItemKind::Keyword, desc, "", kw});
    }

    return result;
}

// ─── Signature help provider ────────────────────────────────────────────────

SignatureHelp LanguageService::get_signature_help(
    const std::string& /*uri*/, const std::string& content,
    int line, int character) {

    SignatureHelp result;
    if (content.empty()) return result;

    std::string line_text = get_line_text(content, line);
    std::string before = line_text.substr(0, std::min(static_cast<size_t>(character), line_text.size()));

    // Find the function name before the opening '('
    // Walk backwards to find '(' and count commas for active parameter
    int paren_depth = 0;
    int comma_count = 0;
    int paren_pos = -1;
    for (int i = static_cast<int>(before.size()) - 1; i >= 0; --i) {
        char c = before[i];
        if (c == ')') paren_depth++;
        else if (c == '(') {
            if (paren_depth == 0) {
                paren_pos = i;
                break;
            }
            paren_depth--;
        } else if (c == ',' && paren_depth == 0) {
            comma_count++;
        }
    }

    if (paren_pos < 0) return result;

    // Extract function name before '('
    int name_end = paren_pos;
    while (name_end > 0 && before[name_end - 1] == ' ') name_end--;
    int name_start = name_end;
    while (name_start > 0 && (std::isalnum(before[name_start - 1]) || before[name_start - 1] == '_')) {
        name_start--;
    }
    std::string func_name = before.substr(name_start, name_end - name_start);
    if (func_name.empty()) return result;

    // Search for the function declaration in content
    std::regex fnc_re(R"(\bfnc\s+)" + func_name + R"(\s*\(([^)]*)\))");
    std::smatch match;
    if (std::regex_search(content, match, fnc_re)) {
        SignatureInfo sig;
        sig.label = "fnc " + func_name + "(" + match[1].str() + ")";
        sig.documentation = "Function " + func_name;

        // Parse parameters
        std::string params_str = match[1].str();
        if (!params_str.empty()) {
            std::istringstream pstream(params_str);
            std::string param;
            while (std::getline(pstream, param, ',')) {
                // Trim whitespace
                size_t s = param.find_first_not_of(" \t");
                size_t e = param.find_last_not_of(" \t");
                if (s != std::string::npos) {
                    param = param.substr(s, e - s + 1);
                }
                sig.parameters.push_back({param, ""});
            }
        }

        result.signatures.push_back(std::move(sig));
        result.active_parameter = comma_count;
    }

    return result;
}

// ─── Combined diagnostics (syntax + type + undefined symbol) ────────────────

std::vector<Diagnostic> LanguageService::get_all_diagnostics(
    const std::string& uri, const std::string& content) {

    // Start with syntax diagnostics
    auto diagnostics = get_diagnostics(uri, content);

    // Add type errors
    auto type_result = analysis_engine_.check_types(uri, content);
    for (const auto& err : type_result.errors) {
        Diagnostic d;
        d.severity = DiagnosticSeverity::Error;
        d.line = err.line;
        d.character = err.character;
        d.end_line = err.end_line;
        d.end_character = err.end_character;
        d.message = err.message;
        if (!err.suggestion.empty()) {
            d.message += " (" + err.suggestion + ")";
        }
        d.source = "meld-type-checker";
        diagnostics.push_back(std::move(d));
    }

    // Add undefined symbol errors
    auto undef_result = analysis_engine_.detect_undefined_symbols(uri, content);
    for (const auto& err : undef_result.errors) {
        Diagnostic d;
        d.severity = DiagnosticSeverity::Error;
        d.line = err.line;
        d.character = err.character;
        d.end_line = err.end_line;
        d.end_character = err.end_character;
        d.message = err.message;
        d.source = "meld-symbol-resolver";
        diagnostics.push_back(std::move(d));
    }

    return diagnostics;
}

// ─── Navigation: Go to definition (Req 4.1) ────────────────────────────────

std::optional<analysis::Location> LanguageService::go_to_definition(
    const std::string& uri, const std::string& content,
    int line, int character) {

    // First ensure the document is analyzed
    analysis_engine_.analyze(uri, content);

    // Try to find the identifier at the cursor position
    // Get the line text and extract the word at the cursor
    std::string line_text;
    {
        int cur_line = 0;
        size_t start = 0;
        for (size_t i = 0; i < content.size(); ++i) {
            if (cur_line == line) {
                start = i;
                size_t end = content.find('\n', i);
                if (end == std::string::npos) end = content.size();
                line_text = content.substr(start, end - start);
                break;
            }
            if (content[i] == '\n') cur_line++;
        }
    }

    if (line_text.empty() || character < 0 ||
        character >= static_cast<int>(line_text.size())) {
        return std::nullopt;
    }

    // Find the identifier boundaries around the cursor
    int start = character;
    while (start > 0 && (std::isalnum(static_cast<unsigned char>(line_text[start - 1])) ||
                          line_text[start - 1] == '_')) {
        start--;
    }
    int end = character;
    while (end < static_cast<int>(line_text.size()) &&
           (std::isalnum(static_cast<unsigned char>(line_text[end])) || line_text[end] == '_')) {
        end++;
    }

    if (start == end) return std::nullopt;
    std::string symbol_name = line_text.substr(start, end - start);
    if (symbol_name.empty()) return std::nullopt;

    // Search for the symbol in the analysis cache
    auto symbols = analysis_engine_.get_document_symbols(uri);
    for (const auto& sym : symbols) {
        if (sym.name == symbol_name) {
            return sym.definition;
        }
    }

    return std::nullopt;
}

// ─── Navigation: Find references (Req 4.2) ─────────────────────────────────

std::vector<analysis::Location> LanguageService::find_references(
    const std::string& uri, const std::string& content,
    int line, int character) {

    // Ensure the document is analyzed
    analysis_engine_.analyze(uri, content);

    // Extract the identifier at cursor
    std::string line_text;
    {
        int cur_line = 0;
        for (size_t i = 0; i < content.size(); ++i) {
            if (cur_line == line) {
                size_t end = content.find('\n', i);
                if (end == std::string::npos) end = content.size();
                line_text = content.substr(i, end - i);
                break;
            }
            if (content[i] == '\n') cur_line++;
        }
    }

    if (line_text.empty() || character < 0 ||
        character >= static_cast<int>(line_text.size())) {
        return {};
    }

    int start = character;
    while (start > 0 && (std::isalnum(static_cast<unsigned char>(line_text[start - 1])) ||
                          line_text[start - 1] == '_')) {
        start--;
    }
    int end = character;
    while (end < static_cast<int>(line_text.size()) &&
           (std::isalnum(static_cast<unsigned char>(line_text[end])) || line_text[end] == '_')) {
        end++;
    }

    if (start == end) return {};
    std::string symbol_name = line_text.substr(start, end - start);

    return analysis_engine_.find_references(uri, content, symbol_name);
}

// ─── Navigation: Document symbols (Req 4.3) ────────────────────────────────

std::vector<analysis::SymbolInfo> LanguageService::get_document_symbols(
    const std::string& uri, const std::string& content) {

    // Analyze the document to populate the cache
    auto result = analysis_engine_.analyze(uri, content);
    return result.symbols;
}

// ─── Navigation: Workspace symbol search (Req 4.4) ─────────────────────────

std::vector<analysis::SymbolInfo> LanguageService::search_workspace_symbols(
    const std::string& query) {

    return analysis_engine_.search_symbols(query);
}

// ─── Hover information (Req 4.5) ────────────────────────────────────────────

namespace {

/// Convert a SymbolKind to a human-readable string
std::string symbol_kind_to_string(analysis::SymbolKind kind) {
    switch (kind) {
        case analysis::SymbolKind::Function:  return "function";
        case analysis::SymbolKind::Variable:  return "variable";
        case analysis::SymbolKind::Type:      return "type";
        case analysis::SymbolKind::Struct:    return "struct";
        case analysis::SymbolKind::Enum:      return "enum";
        case analysis::SymbolKind::Trait:     return "trait";
        case analysis::SymbolKind::Effect:    return "effect";
        case analysis::SymbolKind::Module:    return "module";
        case analysis::SymbolKind::Parameter: return "parameter";
        case analysis::SymbolKind::Property:  return "property";
        default:                              return "symbol";
    }
}

} // anonymous namespace

std::optional<LanguageService::HoverResult> LanguageService::get_hover(
    const std::string& uri, const std::string& content,
    int line, int character) {

    // Ensure the document is analyzed
    analysis_engine_.analyze(uri, content);

    // Extract the identifier at cursor position
    std::string line_text;
    {
        int cur_line = 0;
        for (size_t i = 0; i < content.size(); ++i) {
            if (cur_line == line) {
                size_t end = content.find('\n', i);
                if (end == std::string::npos) end = content.size();
                line_text = content.substr(i, end - i);
                break;
            }
            if (content[i] == '\n') cur_line++;
        }
    }

    if (line_text.empty() || character < 0 ||
        character >= static_cast<int>(line_text.size())) {
        return std::nullopt;
    }

    // Find identifier boundaries around cursor
    int start = character;
    while (start > 0 && (std::isalnum(static_cast<unsigned char>(line_text[start - 1])) ||
                          line_text[start - 1] == '_')) {
        start--;
    }
    int end = character;
    while (end < static_cast<int>(line_text.size()) &&
           (std::isalnum(static_cast<unsigned char>(line_text[end])) || line_text[end] == '_')) {
        end++;
    }

    if (start == end) return std::nullopt;
    std::string symbol_name = line_text.substr(start, end - start);
    if (symbol_name.empty()) return std::nullopt;

    // Resolve the symbol from the analysis cache
    auto symbols = analysis_engine_.get_document_symbols(uri);
    for (const auto& sym : symbols) {
        if (sym.name == symbol_name) {
            // Build markdown hover content
            std::ostringstream md;
            md << "```meld\n" << sym.type_signature << "\n```\n";
            md << "**(" << symbol_kind_to_string(sym.kind) << ")**\n\n";
            if (!sym.documentation.empty()) {
                md << sym.documentation << "\n";
            }

            HoverResult hover;
            hover.contents = md.str();
            hover.line = line;
            hover.character = start;
            hover.end_line = line;
            hover.end_character = end;
            return hover;
        }
    }

    return std::nullopt;
}

// ─── Formatting helpers ─────────────────────────────────────────────────────

std::string LanguageService::format_line(const std::string& line, int indent_depth) {
    // Trim leading and trailing whitespace
    size_t start = line.find_first_not_of(" \t");
    if (start == std::string::npos) {
        return "";  // blank line
    }
    size_t end = line.find_last_not_of(" \t\r");
    std::string trimmed = line.substr(start, end - start + 1);

    if (trimmed.empty()) return "";

    // Build indentation: 4 spaces per depth level
    if (indent_depth < 0) indent_depth = 0;
    std::string indent(static_cast<size_t>(indent_depth) * 4, ' ');

    return indent + trimmed;
}

std::string LanguageService::extract_identifier_at(const std::string& content, int line, int character) {
    // Find the line
    int cur_line = 0;
    size_t line_start = 0;
    for (size_t i = 0; i < content.size(); ++i) {
        if (cur_line == line) {
            line_start = i;
            break;
        }
        if (content[i] == '\n') cur_line++;
    }
    if (cur_line != line) return "";

    size_t line_end = content.find('\n', line_start);
    if (line_end == std::string::npos) line_end = content.size();
    std::string line_text = content.substr(line_start, line_end - line_start);

    if (character < 0 || character >= static_cast<int>(line_text.size())) return "";

    int start = character;
    while (start > 0 && (std::isalnum(static_cast<unsigned char>(line_text[start - 1])) ||
                          line_text[start - 1] == '_')) {
        start--;
    }
    int end = character;
    while (end < static_cast<int>(line_text.size()) &&
           (std::isalnum(static_cast<unsigned char>(line_text[end])) || line_text[end] == '_')) {
        end++;
    }

    if (start == end) return "";
    return line_text.substr(start, end - start);
}

// ─── Document formatting (Req 5.1) ─────────────────────────────────────────

std::vector<LanguageService::TextEdit> LanguageService::format_document(
    const std::string& /*uri*/, const std::string& content) {

    std::vector<TextEdit> edits;
    if (content.empty()) return edits;

    // Split content into lines
    std::vector<std::string> lines;
    {
        std::istringstream stream(content);
        std::string line;
        while (std::getline(stream, line)) {
            lines.push_back(line);
        }
        // Handle trailing newline: if content ends with \n, there's an implicit empty line
        if (!content.empty() && content.back() == '\n') {
            lines.push_back("");
        }
    }

    // Compute brace depth for each line and format
    int depth = 0;
    bool in_string = false;
    bool in_comment = false;
    int prev_blank_count = 0;

    std::vector<std::string> formatted_lines;

    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string& line = lines[i];

        // Check if line is blank
        bool is_blank = (line.find_first_not_of(" \t\r") == std::string::npos);

        if (is_blank) {
            prev_blank_count++;
            // Normalize: at most one blank line between top-level declarations
            if (prev_blank_count <= 1) {
                formatted_lines.push_back("");
            }
            continue;
        }
        prev_blank_count = 0;

        // Trim the line to analyze its content
        size_t first_non_ws = line.find_first_not_of(" \t");
        std::string trimmed = (first_non_ws != std::string::npos)
            ? line.substr(first_non_ws) : "";

        // If line starts with '}', decrease depth before formatting
        bool starts_with_close = (!trimmed.empty() && trimmed[0] == '}');
        if (starts_with_close) {
            depth--;
            if (depth < 0) depth = 0;
        }

        std::string formatted = format_line(line, depth);
        formatted_lines.push_back(formatted);

        // Count braces on this line to update depth for next line
        // (skip braces in strings and comments)
        in_string = false;
        in_comment = false;
        for (size_t j = 0; j < trimmed.size(); ++j) {
            char c = trimmed[j];
            if (in_comment) break;  // rest of line is comment
            if (!in_string && c == '/' && j + 1 < trimmed.size() && trimmed[j + 1] == '/') {
                break;  // line comment
            }
            if (!in_string && c == '"') {
                in_string = true;
                continue;
            }
            if (in_string) {
                if (c == '\\' && j + 1 < trimmed.size()) { j++; continue; }
                if (c == '"') in_string = false;
                continue;
            }
            if (c == '{') depth++;
            else if (c == '}' && !starts_with_close) {
                depth--;
                if (depth < 0) depth = 0;
            } else if (c == '}' && starts_with_close) {
                // Already handled the first '}', handle additional ones
                if (j > 0) {
                    depth--;
                    if (depth < 0) depth = 0;
                }
            }
        }
    }

    // Remove trailing blank lines
    while (!formatted_lines.empty() && formatted_lines.back().empty()) {
        formatted_lines.pop_back();
    }

    // Build the formatted content
    std::string formatted_content;
    for (size_t i = 0; i < formatted_lines.size(); ++i) {
        if (i > 0) formatted_content += "\n";
        formatted_content += formatted_lines[i];
    }
    if (!content.empty() && content.back() == '\n') {
        formatted_content += "\n";
    }

    // If nothing changed, return empty edits
    if (formatted_content == content) return edits;

    // Return a single edit replacing the entire document
    int end_line = static_cast<int>(lines.size()) - 1;
    if (end_line < 0) end_line = 0;
    int end_char = lines.empty() ? 0 : static_cast<int>(lines.back().size());

    TextEdit edit;
    edit.start_line = 0;
    edit.start_character = 0;
    edit.end_line = end_line;
    edit.end_character = end_char;
    edit.new_text = formatted_content;
    edits.push_back(std::move(edit));

    return edits;
}

// ─── Range formatting (Req 5.2) ────────────────────────────────────────────

std::vector<LanguageService::TextEdit> LanguageService::format_range(
    const std::string& /*uri*/, const std::string& content,
    int start_line, int end_line) {

    std::vector<TextEdit> edits;
    if (content.empty()) return edits;

    // Split content into lines
    std::vector<std::string> lines;
    {
        std::istringstream stream(content);
        std::string line;
        while (std::getline(stream, line)) {
            lines.push_back(line);
        }
    }

    int total_lines = static_cast<int>(lines.size());
    if (start_line < 0) start_line = 0;
    if (end_line >= total_lines) end_line = total_lines - 1;
    if (start_line > end_line) return edits;

    // Compute brace depth at the start of the range by scanning lines before it
    int depth = 0;
    bool in_string = false;
    for (int i = 0; i < start_line && i < total_lines; ++i) {
        const std::string& line = lines[i];
        in_string = false;
        for (size_t j = 0; j < line.size(); ++j) {
            char c = line[j];
            if (!in_string && c == '/' && j + 1 < line.size() && line[j + 1] == '/') break;
            if (!in_string && c == '"') { in_string = true; continue; }
            if (in_string) {
                if (c == '\\' && j + 1 < line.size()) { j++; continue; }
                if (c == '"') in_string = false;
                continue;
            }
            if (c == '{') depth++;
            else if (c == '}') { depth--; if (depth < 0) depth = 0; }
        }
    }

    // Format only lines in the range
    bool any_changed = false;
    for (int i = start_line; i <= end_line; ++i) {
        const std::string& line = lines[i];
        bool is_blank = (line.find_first_not_of(" \t\r") == std::string::npos);
        if (is_blank) continue;

        size_t first_non_ws = line.find_first_not_of(" \t");
        std::string trimmed = (first_non_ws != std::string::npos) ? line.substr(first_non_ws) : "";

        bool starts_with_close = (!trimmed.empty() && trimmed[0] == '}');
        int line_depth = depth;
        if (starts_with_close) {
            line_depth = depth - 1;
            if (line_depth < 0) line_depth = 0;
        }

        std::string formatted = format_line(line, line_depth);

        if (formatted != line) {
            TextEdit edit;
            edit.start_line = i;
            edit.start_character = 0;
            edit.end_line = i;
            edit.end_character = static_cast<int>(line.size());
            edit.new_text = formatted;
            edits.push_back(std::move(edit));
            any_changed = true;
        }

        // Update depth for next line
        in_string = false;
        for (size_t j = 0; j < trimmed.size(); ++j) {
            char c = trimmed[j];
            if (!in_string && c == '/' && j + 1 < trimmed.size() && trimmed[j + 1] == '/') break;
            if (!in_string && c == '"') { in_string = true; continue; }
            if (in_string) {
                if (c == '\\' && j + 1 < trimmed.size()) { j++; continue; }
                if (c == '"') in_string = false;
                continue;
            }
            if (c == '{') depth++;
            else if (c == '}') { depth--; if (depth < 0) depth = 0; }
        }
    }

    return edits;
}

// ─── Symbol renaming (Req 5.3, 5.4) ────────────────────────────────────────

LanguageService::RenameResult LanguageService::rename_symbol(
    const std::string& uri, const std::string& content,
    int line, int character, const std::string& new_name) {

    RenameResult result;

    // Validate new_name is a valid identifier
    if (new_name.empty()) {
        result.error_message = "New name cannot be empty";
        return result;
    }
    if (!std::isalpha(static_cast<unsigned char>(new_name[0])) && new_name[0] != '_') {
        result.error_message = "New name must start with a letter or underscore";
        return result;
    }
    for (char c : new_name) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
            result.error_message = "New name contains invalid character '" + std::string(1, c) + "'";
            return result;
        }
    }

    // Extract the symbol at the cursor position
    std::string old_name = extract_identifier_at(content, line, character);
    if (old_name.empty()) {
        result.error_message = "No symbol found at the specified position";
        return result;
    }

    if (old_name == new_name) {
        result.error_message = "New name is the same as the current name";
        return result;
    }

    // Analyze the document to get all symbols
    analysis_engine_.analyze(uri, content);

    // Check for naming conflicts: does new_name already exist as a symbol?
    auto symbols = analysis_engine_.get_document_symbols(uri);
    for (const auto& sym : symbols) {
        if (sym.name == new_name) {
            result.error_message = "Cannot rename to '" + new_name +
                                   "': a symbol with that name already exists";
            return result;
        }
    }

    // Find all references to the old symbol
    auto refs = analysis_engine_.find_references(uri, content, old_name);
    if (refs.empty()) {
        result.error_message = "No references found for symbol '" + old_name + "'";
        return result;
    }

    // Create edits for each reference (in reverse order to maintain positions)
    for (const auto& ref : refs) {
        TextEdit edit;
        edit.start_line = ref.line;
        edit.start_character = ref.character;
        edit.end_line = ref.end_line;
        edit.end_character = ref.end_character;
        edit.new_text = new_name;
        result.edits.push_back(std::move(edit));
    }

    result.success = true;
    return result;
}

} // namespace meld::lsp::services

// ---------------------------------------------------------------------------
// Cross-file resolution (Req 6.3)
// ---------------------------------------------------------------------------

namespace meld::lsp::services {

std::optional<analysis::SymbolInfo> LanguageService::resolve_cross_file_symbol(
    const std::string& uri, const std::string& content, int line, int character) {

    // Ensure the document is analyzed
    analysis_engine_.analyze(uri, content);

    // Extract the identifier at the cursor position
    std::string symbol = extract_identifier_at(content, line, character);
    if (symbol.empty()) return std::nullopt;

    // First try local resolution
    auto local = analysis_engine_.resolve_symbol(uri, line, character);
    if (local.has_value()) return local;

    // Try cross-file resolution via imports
    return analysis_engine_.resolve_import(uri, content, symbol);
}

std::vector<std::string> LanguageService::get_file_dependencies(
    const std::string& uri, const std::string& content) {
    return analysis_engine_.get_dependencies(uri, content);
}

} // namespace meld::lsp::services
