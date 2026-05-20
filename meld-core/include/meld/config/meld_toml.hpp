// meld_toml.hpp — meld.toml Reader (Req 59)
// Single source of truth for project configuration

#pragma once
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <expected>

namespace meld::config {

struct ProjectConfig {
    std::string name;
    std::string version;
    std::string entry;                    // Entry point file
    std::vector<std::string> targets;     // Backend targets: cpp, jvm, wasm, etc.
};

struct DependencyConfig {
    std::string name;
    std::string version;                  // SemVer range
    std::string source;                   // Git URL or registry name
    std::vector<std::string> allow;       // Allowed effects
};

struct BuildConfig {
    std::string output_dir = "./output";
    bool verbose = false;
    std::string trust_level = "0.0";
};

struct MeldToml {
    ProjectConfig project;
    std::map<std::string, DependencyConfig> dependencies;
    BuildConfig build;

    static std::expected<MeldToml, std::string> load(const std::string& path);
    static MeldToml defaults();
};

} // namespace meld::config
