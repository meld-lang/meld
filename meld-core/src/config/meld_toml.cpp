// meld_toml.cpp — meld.toml Reader (Req 59)

#include "meld/config/meld_toml.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace meld::config {

// Minimal TOML parser for the subset meld.toml uses
static std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static std::string unquote(const std::string& s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
        return s.substr(1, s.size() - 2);
    return s;
}

static std::vector<std::string> parse_array(const std::string& s) {
    std::vector<std::string> result;
    auto trimmed = trim(s);
    if (trimmed.size() < 2 || trimmed.front() != '[' || trimmed.back() != ']')
        return result;
    auto inner = trimmed.substr(1, trimmed.size() - 2);
    std::istringstream iss(inner);
    std::string item;
    while (std::getline(iss, item, ',')) {
        auto t = trim(item);
        if (!t.empty()) result.push_back(unquote(t));
    }
    return result;
}

MeldToml MeldToml::defaults() {
    MeldToml config;
    config.project.name = "unnamed";
    config.project.version = "0.0.0";
    config.project.entry = "main.meld";
    config.project.targets = {"cpp"};
    return config;
}

std::expected<MeldToml, std::string> MeldToml::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return std::unexpected("Cannot open " + path);
    }

    MeldToml config = defaults();
    std::string current_section;
    std::string current_dep_name;
    std::string line;

    while (std::getline(file, line)) {
        auto trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#') continue;

        // Section header
        if (trimmed.front() == '[') {
            auto end = trimmed.find(']');
            if (end != std::string::npos) {
                current_section = trimmed.substr(1, end - 1);
                // Handle [dependencies.name] subsections
                if (current_section.starts_with("dependencies.")) {
                    current_dep_name = current_section.substr(13);
                    current_section = "dependencies";
                    config.dependencies[current_dep_name] = DependencyConfig{};
                    config.dependencies[current_dep_name].name = current_dep_name;
                }
            }
            continue;
        }

        // Key = value
        auto eq = trimmed.find('=');
        if (eq == std::string::npos) continue;
        auto key = trim(trimmed.substr(0, eq));
        auto val = trim(trimmed.substr(eq + 1));

        if (current_section == "project") {
            if (key == "name") config.project.name = unquote(val);
            else if (key == "version") config.project.version = unquote(val);
            else if (key == "entry") config.project.entry = unquote(val);
            else if (key == "targets") config.project.targets = parse_array(val);
        } else if (current_section == "dependencies" && !current_dep_name.empty()) {
            auto& dep = config.dependencies[current_dep_name];
            if (key == "version") dep.version = unquote(val);
            else if (key == "source") dep.source = unquote(val);
            else if (key == "allow") dep.allow = parse_array(val);
        } else if (current_section == "build") {
            if (key == "output") config.build.output_dir = unquote(val);
            else if (key == "verbose") config.build.verbose = (val == "true");
            else if (key == "trust-level") config.build.trust_level = unquote(val);
        }
    }

    return config;
}

} // namespace meld::config
