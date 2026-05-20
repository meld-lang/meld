#include "workspace_manager.hpp"
#include <algorithm>

namespace meld::lsp::workspace {

namespace fs = std::filesystem;

WorkspaceManager::WorkspaceManager() {
    index_state_.last_full_index = std::chrono::steady_clock::now();
}

WorkspaceManager::~WorkspaceManager() = default;

void WorkspaceManager::add_workspace_folder(const std::string& path) {
    if (std::find(workspace_folders_.begin(), workspace_folders_.end(), path) == workspace_folders_.end()) {
        workspace_folders_.push_back(path);
        // Discover and index files in the new folder
        auto files = discover_files(path);
        for (const auto& file : files) {
            index_state_.indexed_files.insert(file);
        }
    }
}

void WorkspaceManager::remove_workspace_folder(const std::string& path) {
    workspace_folders_.erase(
        std::remove(workspace_folders_.begin(), workspace_folders_.end(), path),
        workspace_folders_.end());
    // Remove indexed files that belonged to this folder
    for (auto it = index_state_.indexed_files.begin(); it != index_state_.indexed_files.end(); ) {
        if (it->find(path) != std::string::npos) {
            it = index_state_.indexed_files.erase(it);
        } else {
            ++it;
        }
    }
}

void WorkspaceManager::update_document(const std::string& uri, const std::string& content, int version) {
    auto it = documents_.find(uri);
    if (it != documents_.end()) {
        it->second->content = content;
        it->second->version = version;
        it->second->last_modified = std::chrono::steady_clock::now();
    } else {
        auto doc = std::make_shared<Document>();
        doc->uri = uri;
        doc->content = content;
        doc->version = version;
        doc->last_modified = std::chrono::steady_clock::now();
        documents_[uri] = doc;
    }
    // Track as changed for incremental indexing
    index_state_.changed_files.insert(uri);
}

void WorkspaceManager::close_document(const std::string& uri) {
    documents_.erase(uri);
}

std::shared_ptr<Document> WorkspaceManager::get_document(const std::string& uri) const {
    auto it = documents_.find(uri);
    return (it != documents_.end()) ? it->second : nullptr;
}

std::vector<std::string> WorkspaceManager::find_meld_files() const {
    std::vector<std::string> all_files;
    for (const auto& folder : workspace_folders_) {
        auto files = discover_files(folder);
        all_files.insert(all_files.end(), files.begin(), files.end());
    }
    return all_files;
}

std::vector<std::string> WorkspaceManager::workspace_folders() const {
    return workspace_folders_;
}

bool WorkspaceManager::is_in_workspace(const std::string& uri) const {
    for (const auto& folder : workspace_folders_) {
        if (uri.find(folder) != std::string::npos) {
            return true;
        }
    }
    return false;
}

void WorkspaceManager::notify_file_added(const std::string& uri) {
    index_state_.indexed_files.insert(uri);
    index_state_.changed_files.insert(uri);
}

void WorkspaceManager::notify_file_deleted(const std::string& uri) {
    index_state_.indexed_files.erase(uri);
    index_state_.changed_files.insert(uri);
    documents_.erase(uri);
}

std::unordered_set<std::string> WorkspaceManager::get_changed_files() const {
    return index_state_.changed_files;
}

void WorkspaceManager::clear_changed_files() {
    index_state_.changed_files.clear();
}

void WorkspaceManager::reload_configuration() {
    rebuild_index();
}

const IndexState& WorkspaceManager::index_state() const {
    return index_state_;
}

std::vector<std::string> WorkspaceManager::all_document_uris() const {
    std::vector<std::string> uris;
    uris.reserve(documents_.size());
    for (const auto& [uri, doc] : documents_) {
        uris.push_back(uri);
    }
    return uris;
}

std::vector<std::string> WorkspaceManager::analyze_all(
    std::function<void(const std::string& uri, const std::string& content)> analyzer) const {
    std::vector<std::string> analyzed;
    for (const auto& [uri, doc] : documents_) {
        if (doc && !doc->content.empty()) {
            analyzer(uri, doc->content);
            analyzed.push_back(uri);
        }
    }
    return analyzed;
}

void WorkspaceManager::rebuild_index() {
    index_state_.indexed_files.clear();
    index_state_.changed_files.clear();
    for (const auto& folder : workspace_folders_) {
        auto files = discover_files(folder);
        for (const auto& file : files) {
            index_state_.indexed_files.insert(file);
            index_state_.changed_files.insert(file);
        }
    }
    index_state_.last_full_index = std::chrono::steady_clock::now();
}

std::vector<std::string> WorkspaceManager::discover_files(const std::string& root) const {
    std::vector<std::string> files;
    std::error_code ec;

    if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) {
        return files;
    }

    for (const auto& entry : fs::recursive_directory_iterator(root, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".meld") {
            files.push_back(entry.path().string());
        }
    }

    return files;
}

} // namespace meld::lsp::workspace
