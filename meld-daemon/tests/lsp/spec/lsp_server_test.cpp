#include <gtest/gtest.h>
#include "meld/daemon/lsp_server.hpp"
#include <sstream>

namespace meld::lsp::protocol {

// Helper: wrap a JSON body in an LSP Content-Length framed message
static std::string frame(const std::string& body) {
    return "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
}

// Helper: build a JSON-RPC request string
static std::string make_request(int id, const std::string& method, const json& params = json::object()) {
    json req = {{"jsonrpc", "2.0"}, {"id", id}, {"method", method}, {"params", params}};
    return req.dump();
}

// Helper: build a JSON-RPC notification string
static std::string make_notif(const std::string& method, const json& params = json::object()) {
    json n = {{"jsonrpc", "2.0"}, {"method", method}, {"params", params}};
    return n.dump();
}

// --- Initialization ---

TEST(LspServerTest, StartsUninitialized) {
    LspServer server;
    EXPECT_EQ(server.state(), ServerState::Uninitialized);
}

TEST(LspServerTest, InitializeReturnsCapabilities) {
    LspServer server;
    auto resp = server.process_message(make_request(1, "initialize"));
    ASSERT_TRUE(resp.has_value());

    auto j = json::parse(*resp);
    EXPECT_EQ(j["id"], 1);
    EXPECT_TRUE(j["result"].contains("capabilities"));
    EXPECT_TRUE(j["result"].contains("serverInfo"));
    EXPECT_EQ(j["result"]["serverInfo"]["name"], "meld-lsp-server");
}

TEST(LspServerTest, InitializedTransitionsToRunning) {
    LspServer server;
    server.process_message(make_request(1, "initialize"));
    EXPECT_EQ(server.state(), ServerState::Initializing);

    server.process_message(make_notif("initialized"));
    EXPECT_EQ(server.state(), ServerState::Running);
}

// --- Capabilities ---

TEST(LspServerTest, CapabilitiesIncludeSemanticTokens) {
    auto caps = LspServer::server_capabilities();
    EXPECT_TRUE(caps.contains("semanticTokensProvider"));
    EXPECT_TRUE(caps["semanticTokensProvider"].contains("legend"));
}

TEST(LspServerTest, CapabilitiesIncludeCompletion) {
    auto caps = LspServer::server_capabilities();
    EXPECT_TRUE(caps.contains("completionProvider"));
}

TEST(LspServerTest, CapabilitiesIncludeHover) {
    auto caps = LspServer::server_capabilities();
    EXPECT_TRUE(caps.contains("hoverProvider"));
    EXPECT_EQ(caps["hoverProvider"], true);
}

TEST(LspServerTest, CapabilitiesIncludeDefinition) {
    auto caps = LspServer::server_capabilities();
    EXPECT_TRUE(caps.contains("definitionProvider"));
}

TEST(LspServerTest, CapabilitiesIncludeReferences) {
    auto caps = LspServer::server_capabilities();
    EXPECT_TRUE(caps.contains("referencesProvider"));
}

TEST(LspServerTest, CapabilitiesIncludeFormatting) {
    auto caps = LspServer::server_capabilities();
    EXPECT_TRUE(caps.contains("documentFormattingProvider"));
    EXPECT_TRUE(caps.contains("documentRangeFormattingProvider"));
}

TEST(LspServerTest, CapabilitiesIncludeRename) {
    auto caps = LspServer::server_capabilities();
    EXPECT_TRUE(caps.contains("renameProvider"));
}

TEST(LspServerTest, CapabilitiesIncludeDiagnostics) {
    auto caps = LspServer::server_capabilities();
    EXPECT_TRUE(caps.contains("diagnosticProvider"));
}

// --- Shutdown / Exit ---

TEST(LspServerTest, ShutdownTransitionsToShuttingDown) {
    LspServer server;
    server.process_message(make_request(1, "initialize"));
    server.process_message(make_notif("initialized"));

    auto resp = server.process_message(make_request(2, "shutdown"));
    ASSERT_TRUE(resp.has_value());
    auto j = json::parse(*resp);
    EXPECT_TRUE(j["result"].is_null());
    EXPECT_EQ(server.state(), ServerState::ShuttingDown);
}

TEST(LspServerTest, ExitAfterShutdownStops) {
    LspServer server;
    server.process_message(make_request(1, "initialize"));
    server.process_message(make_notif("initialized"));
    server.process_message(make_request(2, "shutdown"));

    server.process_message(make_notif("exit"));
    EXPECT_EQ(server.state(), ServerState::Stopped);
}

// --- Error handling ---

TEST(LspServerTest, RequestBeforeInitializeReturnsError) {
    LspServer server;
    auto resp = server.process_message(make_request(1, "textDocument/completion"));
    ASSERT_TRUE(resp.has_value());
    auto j = json::parse(*resp);
    EXPECT_TRUE(j.contains("error"));
    EXPECT_EQ(j["error"]["code"], -32002);
}

TEST(LspServerTest, RequestAfterShutdownReturnsError) {
    LspServer server;
    server.process_message(make_request(1, "initialize"));
    server.process_message(make_notif("initialized"));
    server.process_message(make_request(2, "shutdown"));

    auto resp = server.process_message(make_request(3, "textDocument/hover"));
    ASSERT_TRUE(resp.has_value());
    auto j = json::parse(*resp);
    EXPECT_TRUE(j.contains("error"));
}

// --- Full lifecycle via run() ---

TEST(LspServerTest, FullLifecycleViaRun) {
    LspServer server;

    std::string input_data;
    input_data += frame(make_request(1, "initialize"));
    input_data += frame(make_notif("initialized"));
    input_data += frame(make_request(2, "shutdown"));
    input_data += frame(make_notif("exit"));

    std::istringstream input(input_data);
    std::ostringstream output;

    int exit_code = server.run(input, output);
    EXPECT_EQ(exit_code, 0);
    EXPECT_EQ(server.state(), ServerState::Stopped);

    // Output should contain at least the initialize response
    EXPECT_FALSE(output.str().empty());
}

} // namespace meld::lsp::protocol
