#include <gtest/gtest.h>
#include "meld/daemon/lsp_server.hpp"
#include "meld/daemon/json_rpc.hpp"
#include "meld/daemon/language_service.hpp"
#include "meld/daemon/workspace_manager.hpp"
#include "meld/daemon/analysis_engine.hpp"
#include <sstream>
#include <string>
#include <vector>

namespace meld::lsp::integration_tests {

using namespace meld::lsp::protocol;
using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Helpers: mock LSP client utilities
// ---------------------------------------------------------------------------

/// Wrap a JSON body in an LSP Content-Length framed message
static std::string frame(const std::string& body) {
    return "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
}

/// Build a JSON-RPC request string
static std::string make_request(int id, const std::string& method,
                                const json& params = json::object()) {
    json req = {{"jsonrpc", "2.0"}, {"id", id}, {"method", method}, {"params", params}};
    return req.dump();
}

/// Build a JSON-RPC notification string
static std::string make_notification(const std::string& method,
                                     const json& params = json::object()) {
    json n = {{"jsonrpc", "2.0"}, {"method", method}, {"params", params}};
    return n.dump();
}

/// Parse a framed LSP response from an output stream, returning the JSON body.
/// Reads one Content-Length header + body pair.
static std::optional<json> read_response(std::istringstream& stream) {
    std::string line;
    int content_length = -1;

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) break;
        if (line.find("Content-Length:") == 0) {
            content_length = std::stoi(line.substr(15));
        }
    }
    if (content_length < 0) return std::nullopt;

    std::string body(content_length, '\0');
    stream.read(body.data(), content_length);
    if (stream.gcount() != content_length) return std::nullopt;

    return json::parse(body);
}

/// Collect ALL responses from an output stream into a vector of JSON objects.
static std::vector<json> read_all_responses(const std::string& raw_output) {
    std::vector<json> results;
    std::istringstream stream(raw_output);
    while (stream.good()) {
        auto resp = read_response(stream);
        if (resp) results.push_back(*resp);
        else break;
    }
    return results;
}

/// Helper class that simulates an LSP client talking to the server via run().
class MockLspClient {
public:
    /// Send a sequence of framed messages through the server and collect output.
    struct SessionResult {
        int exit_code;
        std::vector<json> responses;
    };

    static SessionResult run_session(const std::vector<std::string>& messages) {
        std::string input_data;
        for (const auto& msg : messages) {
            input_data += frame(msg);
        }

        LspServer server;
        std::istringstream input(input_data);
        std::ostringstream output;

        int exit_code = server.run(input, output);
        auto responses = read_all_responses(output.str());

        return {exit_code, std::move(responses)};
    }
};

// ---------------------------------------------------------------------------
// Test: Initialize / Initialized handshake
// ---------------------------------------------------------------------------

TEST(LspIntegrationTest, InitializeHandshake) {
    auto result = MockLspClient::run_session({
        make_request(1, "initialize", {
            {"processId", 1234},
            {"rootUri", "file:///workspace"},
            {"capabilities", json::object()}
        }),
        make_notification("initialized"),
        make_request(2, "shutdown"),
        make_notification("exit")
    });

    EXPECT_EQ(result.exit_code, 0);
    ASSERT_GE(result.responses.size(), 2u);

    // First response: initialize result
    auto& init_resp = result.responses[0];
    EXPECT_EQ(init_resp["id"], 1);
    EXPECT_TRUE(init_resp.contains("result"));
    EXPECT_TRUE(init_resp["result"].contains("capabilities"));
    EXPECT_TRUE(init_resp["result"].contains("serverInfo"));
    EXPECT_EQ(init_resp["result"]["serverInfo"]["name"], "meld-lsp-server");
    EXPECT_EQ(init_resp["result"]["serverInfo"]["version"], "0.1.0");

    // Capabilities should advertise key features
    auto& caps = init_resp["result"]["capabilities"];
    EXPECT_TRUE(caps.contains("completionProvider"));
    EXPECT_TRUE(caps.contains("hoverProvider"));
    EXPECT_TRUE(caps.contains("definitionProvider"));
    EXPECT_TRUE(caps.contains("referencesProvider"));
    EXPECT_TRUE(caps.contains("documentFormattingProvider"));
    EXPECT_TRUE(caps.contains("documentRangeFormattingProvider"));
    EXPECT_TRUE(caps.contains("renameProvider"));
    EXPECT_TRUE(caps.contains("semanticTokensProvider"));
    EXPECT_TRUE(caps.contains("textDocumentSync"));
}

// ---------------------------------------------------------------------------
// Test: textDocument/didOpen notification
// ---------------------------------------------------------------------------

TEST(LspIntegrationTest, DidOpenNotification) {
    // didOpen is a notification — no response expected.
    // The server should accept it without error after initialization.
    auto result = MockLspClient::run_session({
        make_request(1, "initialize"),
        make_notification("initialized"),
        make_notification("textDocument/didOpen", {
            {"textDocument", {
                {"uri", "file:///test.meld"},
                {"languageId", "meld"},
                {"version", 1},
                {"text", "fnc main() { let x = 42; }"}
            }}
        }),
        make_request(2, "shutdown"),
        make_notification("exit")
    });

    EXPECT_EQ(result.exit_code, 0);
    // Should have initialize response + shutdown response = 2
    ASSERT_GE(result.responses.size(), 2u);

    // Shutdown response should be null result
    auto& shutdown_resp = result.responses[1];
    EXPECT_EQ(shutdown_resp["id"], 2);
    EXPECT_TRUE(shutdown_resp["result"].is_null());
}

// ---------------------------------------------------------------------------
// Test: textDocument/completion request/response
// ---------------------------------------------------------------------------

TEST(LspIntegrationTest, CompletionRequest) {
    LspServer server;

    // Initialize
    auto resp = server.process_message(make_request(1, "initialize"));
    ASSERT_TRUE(resp.has_value());
    server.process_message(make_notification("initialized"));

    // Register a completion handler that returns a simple completion list
    server.router().register_request("textDocument/completion",
        [](const json& params) -> json {
            // Simulate returning completions
            return {
                {"isIncomplete", false},
                {"items", json::array({
                    {{"label", "main"}, {"kind", 3}, {"detail", "fnc main()"}},
                    {{"label", "println"}, {"kind", 3}, {"detail", "fnc println(msg: String)"}}
                })}
            };
        });

    auto completion_resp = server.process_message(make_request(2, "textDocument/completion", {
        {"textDocument", {{"uri", "file:///test.meld"}}},
        {"position", {{"line", 0}, {"character", 5}}}
    }));

    ASSERT_TRUE(completion_resp.has_value());
    auto j = json::parse(*completion_resp);
    EXPECT_EQ(j["id"], 2);
    EXPECT_TRUE(j.contains("result"));
    EXPECT_TRUE(j["result"].contains("items"));
    EXPECT_FALSE(j["result"]["isIncomplete"].get<bool>());
    EXPECT_EQ(j["result"]["items"].size(), 2u);
    EXPECT_EQ(j["result"]["items"][0]["label"], "main");
}

// ---------------------------------------------------------------------------
// Test: textDocument/hover request/response
// ---------------------------------------------------------------------------

TEST(LspIntegrationTest, HoverRequest) {
    LspServer server;

    server.process_message(make_request(1, "initialize"));
    server.process_message(make_notification("initialized"));

    // Register a hover handler
    server.router().register_request("textDocument/hover",
        [](const json& params) -> json {
            return {
                {"contents", {
                    {"kind", "markdown"},
                    {"value", "```meld\nfnc main() -> Void\n```"}
                }},
                {"range", {
                    {"start", {{"line", 0}, {"character", 4}}},
                    {"end", {{"line", 0}, {"character", 8}}}
                }}
            };
        });

    auto hover_resp = server.process_message(make_request(2, "textDocument/hover", {
        {"textDocument", {{"uri", "file:///test.meld"}}},
        {"position", {{"line", 0}, {"character", 5}}}
    }));

    ASSERT_TRUE(hover_resp.has_value());
    auto j = json::parse(*hover_resp);
    EXPECT_EQ(j["id"], 2);
    EXPECT_TRUE(j["result"].contains("contents"));
    EXPECT_TRUE(j["result"].contains("range"));
    EXPECT_EQ(j["result"]["contents"]["kind"], "markdown");
}

// ---------------------------------------------------------------------------
// Test: textDocument/formatting request/response
// ---------------------------------------------------------------------------

TEST(LspIntegrationTest, FormattingRequest) {
    LspServer server;

    server.process_message(make_request(1, "initialize"));
    server.process_message(make_notification("initialized"));

    // Register a formatting handler
    server.router().register_request("textDocument/formatting",
        [](const json& params) -> json {
            return json::array({
                {
                    {"range", {
                        {"start", {{"line", 0}, {"character", 0}}},
                        {"end", {{"line", 2}, {"character", 0}}}
                    }},
                    {"newText", "fnc main() {\n    let x = 42;\n}\n"}
                }
            });
        });

    auto fmt_resp = server.process_message(make_request(2, "textDocument/formatting", {
        {"textDocument", {{"uri", "file:///test.meld"}}},
        {"options", {{"tabSize", 4}, {"insertSpaces", true}}}
    }));

    ASSERT_TRUE(fmt_resp.has_value());
    auto j = json::parse(*fmt_resp);
    EXPECT_EQ(j["id"], 2);
    EXPECT_TRUE(j["result"].is_array());
    EXPECT_GE(j["result"].size(), 1u);
    EXPECT_TRUE(j["result"][0].contains("range"));
    EXPECT_TRUE(j["result"][0].contains("newText"));
}

// ---------------------------------------------------------------------------
// Test: Shutdown / Exit lifecycle
// ---------------------------------------------------------------------------

TEST(LspIntegrationTest, ShutdownExitLifecycle) {
    auto result = MockLspClient::run_session({
        make_request(1, "initialize"),
        make_notification("initialized"),
        make_request(2, "shutdown"),
        make_notification("exit")
    });

    EXPECT_EQ(result.exit_code, 0);
    ASSERT_GE(result.responses.size(), 2u);

    // Shutdown response: null result
    auto& shutdown_resp = result.responses[1];
    EXPECT_EQ(shutdown_resp["id"], 2);
    EXPECT_TRUE(shutdown_resp["result"].is_null());
}

TEST(LspIntegrationTest, ExitWithoutShutdownStopsServer) {
    auto result = MockLspClient::run_session({
        make_request(1, "initialize"),
        make_notification("initialized"),
        make_notification("exit")
    });

    // Server transitions to Stopped on exit regardless of shutdown.
    // The current implementation returns 0 because state becomes Stopped.
    EXPECT_EQ(result.exit_code, 0);
    ASSERT_GE(result.responses.size(), 1u);
}

// ---------------------------------------------------------------------------
// Test: Requests before initialization are rejected
// ---------------------------------------------------------------------------

TEST(LspIntegrationTest, RequestBeforeInitializeRejected) {
    LspServer server;

    auto resp = server.process_message(make_request(1, "textDocument/completion", {
        {"textDocument", {{"uri", "file:///test.meld"}}},
        {"position", {{"line", 0}, {"character", 0}}}
    }));

    ASSERT_TRUE(resp.has_value());
    auto j = json::parse(*resp);
    EXPECT_TRUE(j.contains("error"));
    EXPECT_EQ(j["error"]["code"], -32002); // Server not initialized
}

// ---------------------------------------------------------------------------
// Test: Requests after shutdown are rejected
// ---------------------------------------------------------------------------

TEST(LspIntegrationTest, RequestAfterShutdownRejected) {
    LspServer server;

    server.process_message(make_request(1, "initialize"));
    server.process_message(make_notification("initialized"));
    server.process_message(make_request(2, "shutdown"));

    auto resp = server.process_message(make_request(3, "textDocument/hover", {
        {"textDocument", {{"uri", "file:///test.meld"}}},
        {"position", {{"line", 0}, {"character", 0}}}
    }));

    ASSERT_TRUE(resp.has_value());
    auto j = json::parse(*resp);
    EXPECT_TRUE(j.contains("error"));
}

// ---------------------------------------------------------------------------
// Test: Unknown method returns method-not-found error
// ---------------------------------------------------------------------------

TEST(LspIntegrationTest, UnknownMethodReturnsError) {
    LspServer server;

    server.process_message(make_request(1, "initialize"));
    server.process_message(make_notification("initialized"));

    auto resp = server.process_message(make_request(2, "nonexistent/method"));
    ASSERT_TRUE(resp.has_value());
    auto j = json::parse(*resp);
    EXPECT_TRUE(j.contains("error"));
    EXPECT_EQ(j["error"]["code"], -32601); // Method not found
}

// ---------------------------------------------------------------------------
// Test: Malformed JSON returns parse error
// ---------------------------------------------------------------------------

TEST(LspIntegrationTest, MalformedJsonReturnsParseError) {
    LspServer server;

    auto resp = server.process_message("{invalid json!!!");
    ASSERT_TRUE(resp.has_value());
    auto j = json::parse(*resp);
    EXPECT_TRUE(j.contains("error"));
    EXPECT_EQ(j["error"]["code"], -32700); // Parse error
}

// ---------------------------------------------------------------------------
// Test: Multiple requests in a single session
// ---------------------------------------------------------------------------

TEST(LspIntegrationTest, MultipleRequestsInSession) {
    LspServer server;

    server.process_message(make_request(1, "initialize"));
    server.process_message(make_notification("initialized"));

    // Register handlers for multiple methods
    server.router().register_request("textDocument/completion",
        [](const json&) -> json {
            return {{"isIncomplete", false}, {"items", json::array()}};
        });

    server.router().register_request("textDocument/hover",
        [](const json&) -> json {
            return {{"contents", {{"kind", "plaintext"}, {"value", "Int"}}}};
        });

    // Send completion request
    auto resp1 = server.process_message(make_request(2, "textDocument/completion", {
        {"textDocument", {{"uri", "file:///test.meld"}}},
        {"position", {{"line", 0}, {"character", 0}}}
    }));
    ASSERT_TRUE(resp1.has_value());
    auto j1 = json::parse(*resp1);
    EXPECT_EQ(j1["id"], 2);
    EXPECT_TRUE(j1["result"].contains("items"));

    // Send hover request
    auto resp2 = server.process_message(make_request(3, "textDocument/hover", {
        {"textDocument", {{"uri", "file:///test.meld"}}},
        {"position", {{"line", 0}, {"character", 0}}}
    }));
    ASSERT_TRUE(resp2.has_value());
    auto j2 = json::parse(*resp2);
    EXPECT_EQ(j2["id"], 3);
    EXPECT_TRUE(j2["result"].contains("contents"));
}

// ---------------------------------------------------------------------------
// Test: Full session via run() with multiple operations
// ---------------------------------------------------------------------------

TEST(LspIntegrationTest, FullSessionViaRun) {
    auto result = MockLspClient::run_session({
        make_request(1, "initialize", {
            {"processId", 42},
            {"rootUri", "file:///project"},
            {"capabilities", {
                {"textDocument", {
                    {"completion", {{"dynamicRegistration", false}}}
                }}
            }}
        }),
        make_notification("initialized"),
        make_notification("textDocument/didOpen", {
            {"textDocument", {
                {"uri", "file:///project/main.meld"},
                {"languageId", "meld"},
                {"version", 1},
                {"text", "fnc greet(name: String) -> String {\n    return \"Hello, \" + name;\n}\n"}
            }}
        }),
        make_request(2, "shutdown"),
        make_notification("exit")
    });

    EXPECT_EQ(result.exit_code, 0);
    ASSERT_GE(result.responses.size(), 2u);

    // Verify initialize response
    EXPECT_EQ(result.responses[0]["id"], 1);
    EXPECT_TRUE(result.responses[0]["result"].contains("capabilities"));

    // Verify shutdown response
    EXPECT_EQ(result.responses[1]["id"], 2);
    EXPECT_TRUE(result.responses[1]["result"].is_null());
}

// ---------------------------------------------------------------------------
// Test: LanguageService integration — semantic tokens
// ---------------------------------------------------------------------------

TEST(LspIntegrationTest, LanguageServiceSemanticTokens) {
    services::LanguageService service;

    std::string code = "fnc add(a: Int, b: Int) -> Int { return a + b; }";
    auto tokens = service.get_semantic_tokens("file:///test.meld", code);

    // Should produce at least some tokens for keywords, identifiers, types
    EXPECT_GT(tokens.size(), 0u);

    // Verify encoding produces valid delta-encoded array
    auto encoded = service.encode_semantic_tokens(tokens);
    // Delta encoding: 5 integers per token
    EXPECT_EQ(encoded.size() % 5, 0u);
}

// ---------------------------------------------------------------------------
// Test: LanguageService integration — diagnostics
// ---------------------------------------------------------------------------

TEST(LspIntegrationTest, LanguageServiceDiagnostics) {
    services::LanguageService service;

    // Valid code should produce no diagnostics
    std::string valid_code = "fnc main() { let x = 42; }";
    auto diags = service.get_diagnostics("file:///test.meld", valid_code);
    EXPECT_EQ(diags.size(), 0u);
}

// ---------------------------------------------------------------------------
// Test: LanguageService integration — completions
// ---------------------------------------------------------------------------

TEST(LspIntegrationTest, LanguageServiceCompletions) {
    services::LanguageService service;

    std::string code = "fnc main() {\n    let x = 10;\n    \n}";
    auto completions = service.get_completions("file:///test.meld", code, 2, 4);

    // Should return some completions (at minimum keywords)
    EXPECT_GT(completions.items.size(), 0u);
}

// ---------------------------------------------------------------------------
// Test: WorkspaceManager integration — document lifecycle
// ---------------------------------------------------------------------------

TEST(LspIntegrationTest, WorkspaceManagerDocumentLifecycle) {
    workspace::WorkspaceManager wm;

    // Open a document
    wm.update_document("file:///test.meld", "fnc main() {}", 1);
    auto doc = wm.get_document("file:///test.meld");
    ASSERT_NE(doc, nullptr);
    EXPECT_EQ(doc->uri, "file:///test.meld");
    EXPECT_EQ(doc->content, "fnc main() {}");
    EXPECT_EQ(doc->version, 1);

    // Update the document
    wm.update_document("file:///test.meld", "fnc main() { let x = 1; }", 2);
    doc = wm.get_document("file:///test.meld");
    ASSERT_NE(doc, nullptr);
    EXPECT_EQ(doc->version, 2);
    EXPECT_EQ(doc->content, "fnc main() { let x = 1; }");

    // Close the document
    wm.close_document("file:///test.meld");
    doc = wm.get_document("file:///test.meld");
    EXPECT_EQ(doc, nullptr);
}

// ---------------------------------------------------------------------------
// Test: AnalysisEngine integration — symbol analysis
// ---------------------------------------------------------------------------

TEST(LspIntegrationTest, AnalysisEngineSymbolAnalysis) {
    analysis::AnalysisEngine engine;

    std::string code = "fnc add(a: Int, b: Int) -> Int {\n    return a + b;\n}\n\nfnc main() {\n    let result = add(1, 2);\n}";
    auto result = engine.analyze("file:///test.meld", code);

    EXPECT_EQ(result.uri, "file:///test.meld");
    // Should find at least the function symbols
    EXPECT_GT(result.symbols.size(), 0u);
}

// ---------------------------------------------------------------------------
// Test: Cross-component — workspace + analysis
// ---------------------------------------------------------------------------

TEST(LspIntegrationTest, WorkspaceWithAnalysis) {
    workspace::WorkspaceManager wm;
    analysis::AnalysisEngine engine;

    // Simulate opening multiple files
    std::string file1 = "fnc helper() -> Int { return 42; }";
    std::string file2 = "fnc main() { let x = helper(); }";

    wm.update_document("file:///helper.meld", file1, 1);
    wm.update_document("file:///main.meld", file2, 1);

    // Analyze both files
    auto result1 = engine.analyze("file:///helper.meld", file1);
    auto result2 = engine.analyze("file:///main.meld", file2);

    EXPECT_GT(result1.symbols.size(), 0u);
    EXPECT_GT(result2.symbols.size(), 0u);

    // Verify workspace tracks both documents
    auto uris = wm.all_document_uris();
    EXPECT_EQ(uris.size(), 2u);
}

// ---------------------------------------------------------------------------
// Test: Server capabilities match expected LSP features
// ---------------------------------------------------------------------------

TEST(LspIntegrationTest, ServerCapabilitiesComplete) {
    auto caps = LspServer::server_capabilities();

    // Text document sync
    EXPECT_TRUE(caps["textDocumentSync"]["openClose"].get<bool>());
    EXPECT_EQ(caps["textDocumentSync"]["change"].get<int>(), 2); // Incremental

    // Completion
    EXPECT_TRUE(caps["completionProvider"]["resolveProvider"].get<bool>());
    auto triggers = caps["completionProvider"]["triggerCharacters"];
    EXPECT_TRUE(triggers.is_array());
    EXPECT_GE(triggers.size(), 1u);

    // Semantic tokens
    EXPECT_TRUE(caps["semanticTokensProvider"]["full"].get<bool>());
    auto legend = caps["semanticTokensProvider"]["legend"];
    EXPECT_TRUE(legend.contains("tokenTypes"));
    EXPECT_TRUE(legend.contains("tokenModifiers"));
    EXPECT_GT(legend["tokenTypes"].size(), 0u);
    EXPECT_GT(legend["tokenModifiers"].size(), 0u);
}

// ---------------------------------------------------------------------------
// Test: Rapid successive messages don't corrupt state
// ---------------------------------------------------------------------------

TEST(LspIntegrationTest, RapidSuccessiveMessages) {
    LspServer server;

    server.process_message(make_request(1, "initialize"));
    server.process_message(make_notification("initialized"));
    EXPECT_EQ(server.state(), ServerState::Running);

    // Register a simple handler
    server.router().register_request("textDocument/hover",
        [](const json&) -> json {
            return {{"contents", {{"kind", "plaintext"}, {"value", "test"}}}};
        });

    // Send many requests rapidly
    for (int i = 10; i < 30; ++i) {
        auto resp = server.process_message(make_request(i, "textDocument/hover", {
            {"textDocument", {{"uri", "file:///test.meld"}}},
            {"position", {{"line", 0}, {"character", 0}}}
        }));
        ASSERT_TRUE(resp.has_value());
        auto j = json::parse(*resp);
        EXPECT_EQ(j["id"], i);
        EXPECT_TRUE(j.contains("result"));
    }

    // Server should still be in Running state
    EXPECT_EQ(server.state(), ServerState::Running);

    // Clean shutdown should still work
    auto shutdown = server.process_message(make_request(99, "shutdown"));
    ASSERT_TRUE(shutdown.has_value());
    EXPECT_EQ(server.state(), ServerState::ShuttingDown);

    server.process_message(make_notification("exit"));
    EXPECT_EQ(server.state(), ServerState::Stopped);
}

} // namespace meld::lsp::integration_tests
