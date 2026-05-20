#pragma once

#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <regex>
#include <stdexcept>

namespace meld::stdlib {

// Forward declarations
class Match;
class RegexBuilder;

// Regex flags enum
enum class RegexFlags {
    NONE = 0,
    IGNORECASE = 1 << 0,    // Case-insensitive matching
    MULTILINE = 1 << 1,     // ^ and $ match line boundaries
    DOTALL = 1 << 2,        // . matches newlines
    EXTENDED = 1 << 3,      // Ignore whitespace and comments
    UNICODE = 1 << 4        // Unicode support
};

// Bitwise operators for flags
inline RegexFlags operator|(RegexFlags a, RegexFlags b) {
    return static_cast<RegexFlags>(static_cast<int>(a) | static_cast<int>(b));
}

inline RegexFlags operator&(RegexFlags a, RegexFlags b) {
    return static_cast<RegexFlags>(static_cast<int>(a) & static_cast<int>(b));
}

inline bool has_flag(RegexFlags flags, RegexFlags flag) {
    return (static_cast<int>(flags) & static_cast<int>(flag)) != 0;
}

// Match class - represents a regex match result
class Match {
public:
    Match(const std::smatch& match, const std::string& input)
        : match_(match), input_(input) {}
    
    // Get matched group by index (0 = full match, 1+ = capture groups)
    std::optional<std::string> group(size_t index) const {
        if (index >= match_.size()) {
            return std::nullopt;
        }
        if (!match_[index].matched) {
            return std::nullopt;
        }
        return match_[index].str();
    }
    
    // Get all groups as a vector
    std::vector<std::string> groups() const {
        std::vector<std::string> result;
        for (size_t i = 0; i < match_.size(); ++i) {
            if (match_[i].matched) {
                result.push_back(match_[i].str());
            } else {
                result.push_back("");
            }
        }
        return result;
    }
    
    // Get the full matched text
    std::string text() const {
        return match_[0].str();
    }
    
    // Get start position of match
    size_t start() const {
        return match_.position(0);
    }
    
    // Get end position of match
    size_t end() const {
        return match_.position(0) + match_.length(0);
    }
    
    // Get length of match
    size_t length() const {
        return match_.length(0);
    }
    
    // Number of groups (including full match)
    size_t size() const {
        return match_.size();
    }
    
    // Check if match is valid
    bool valid() const {
        return !match_.empty();
    }

private:
    std::smatch match_;
    std::string input_;
};

// Regex class - main regular expression class
class Regex {
public:
    // Constructors
    Regex() = default;
    
    explicit Regex(const std::string& pattern, RegexFlags flags = RegexFlags::NONE)
        : pattern_(pattern), flags_(flags) {
        compile();
    }
    
    explicit Regex(const std::string& pattern, const std::string& flag_string)
        : pattern_(pattern), flags_(parse_flags(flag_string)) {
        compile();
    }
    
    // Static factory method
    static Regex compile(const std::string& pattern, RegexFlags flags = RegexFlags::NONE) {
        return Regex(pattern, flags);
    }
    
    static Regex compile(const std::string& pattern, const std::string& flag_string) {
        return Regex(pattern, flag_string);
    }
    
    // Matching operations
    
    // Check if entire string matches pattern
    bool matches(const std::string& input) const {
        return std::regex_match(input, regex_);
    }
    
    // Find first match in string
    std::optional<Match> find(const std::string& input) const {
        std::smatch match;
        if (std::regex_search(input, match, regex_)) {
            return Match(match, input);
        }
        return std::nullopt;
    }
    
    // Find all matches in string
    std::vector<Match> findAll(const std::string& input) const {
        std::vector<Match> matches;
        std::string::const_iterator search_start(input.cbegin());
        std::smatch match;
        
        while (std::regex_search(search_start, input.cend(), match, regex_)) {
            matches.emplace_back(match, input);
            search_start = match.suffix().first;
        }
        
        return matches;
    }
    
    // Replacement operations
    
    // Replace first occurrence
    std::string replace(const std::string& input, const std::string& replacement) const {
        return std::regex_replace(input, regex_, replacement, 
                                 std::regex_constants::format_first_only);
    }
    
    // Replace all occurrences
    std::string replaceAll(const std::string& input, const std::string& replacement) const {
        return std::regex_replace(input, regex_, replacement);
    }
    
    // Splitting operations
    
    // Split string by pattern
    std::vector<std::string> split(const std::string& input) const {
        std::vector<std::string> result;
        std::sregex_token_iterator iter(input.begin(), input.end(), regex_, -1);
        std::sregex_token_iterator end;
        
        for (; iter != end; ++iter) {
            result.push_back(*iter);
        }
        
        return result;
    }
    
    // Split with limit
    std::vector<std::string> split(const std::string& input, size_t limit) const {
        std::vector<std::string> result;
        std::sregex_token_iterator iter(input.begin(), input.end(), regex_, -1);
        std::sregex_token_iterator end;
        
        size_t count = 0;
        for (; iter != end && count < limit; ++iter, ++count) {
            result.push_back(*iter);
        }
        
        // Add remaining string if limit reached
        if (iter != end) {
            std::string remaining;
            for (; iter != end; ++iter) {
                if (!remaining.empty()) {
                    remaining += *iter;
                } else {
                    remaining = *iter;
                }
            }
            if (!remaining.empty()) {
                result.push_back(remaining);
            }
        }
        
        return result;
    }
    
    // Pattern info
    std::string pattern() const { return pattern_; }
    RegexFlags flags() const { return flags_; }
    
    // Check if regex is valid
    bool valid() const { return compiled_; }

private:
    void compile() {
        try {
            std::regex::flag_type cpp_flags = std::regex::ECMAScript;
            
            if (has_flag(flags_, RegexFlags::IGNORECASE)) {
                cpp_flags |= std::regex::icase;
            }
            if (has_flag(flags_, RegexFlags::MULTILINE)) {
                // Note: C++ regex doesn't have direct multiline flag
                // This would need custom handling
            }
            if (has_flag(flags_, RegexFlags::EXTENDED)) {
                cpp_flags |= std::regex::extended;
            }
            
            regex_ = std::regex(pattern_, cpp_flags);
            compiled_ = true;
        } catch (const std::regex_error& e) {
            compiled_ = false;
            throw std::runtime_error("Invalid regex pattern: " + pattern_ + " - " + e.what());
        }
    }
    
    static RegexFlags parse_flags(const std::string& flag_string) {
        RegexFlags flags = RegexFlags::NONE;
        
        for (char c : flag_string) {
            switch (c) {
                case 'i': flags = flags | RegexFlags::IGNORECASE; break;
                case 'm': flags = flags | RegexFlags::MULTILINE; break;
                case 's': flags = flags | RegexFlags::DOTALL; break;
                case 'x': flags = flags | RegexFlags::EXTENDED; break;
                case 'u': flags = flags | RegexFlags::UNICODE; break;
                default:
                    throw std::runtime_error(std::string("Unknown regex flag: ") + c);
            }
        }
        
        return flags;
    }
    
    std::string pattern_;
    RegexFlags flags_ = RegexFlags::NONE;
    std::regex regex_;
    bool compiled_ = false;
};

// RegexBuilder - fluent builder for constructing regex patterns
class RegexBuilder {
public:
    RegexBuilder() = default;
    
    // Set pattern
    RegexBuilder& pattern(const std::string& p) {
        pattern_ = p;
        return *this;
    }
    
    // Add flags
    RegexBuilder& ignoreCase() {
        flags_ = flags_ | RegexFlags::IGNORECASE;
        return *this;
    }
    
    RegexBuilder& multiline() {
        flags_ = flags_ | RegexFlags::MULTILINE;
        return *this;
    }
    
    RegexBuilder& dotAll() {
        flags_ = flags_ | RegexFlags::DOTALL;
        return *this;
    }
    
    RegexBuilder& extended() {
        flags_ = flags_ | RegexFlags::EXTENDED;
        return *this;
    }
    
    RegexBuilder& unicode() {
        flags_ = flags_ | RegexFlags::UNICODE;
        return *this;
    }
    
    // Set flags directly
    RegexBuilder& flags(RegexFlags f) {
        flags_ = f;
        return *this;
    }
    
    // Build the regex
    Regex build() const {
        if (pattern_.empty()) {
            throw std::runtime_error("Cannot build regex with empty pattern");
        }
        return Regex(pattern_, flags_);
    }

private:
    std::string pattern_;
    RegexFlags flags_ = RegexFlags::NONE;
};

} // namespace meld::stdlib
