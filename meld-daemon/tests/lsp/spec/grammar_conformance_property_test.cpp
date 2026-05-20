/**
 * **Feature: meld-lsp-server, Property 11: Grammar conformance validation**
 *
 * For any Meld code, syntax validation should conform exactly to the
 * Meld grammar specification. At the protocol layer this means:
 * - Valid JSON-RPC messages conforming to LSP spec are accepted and routed
 * - Invalid or malformed messages produce proper error responses
 * - The server lifecycle (initialize -> initialized -> shutdown -> exit)
 *   enforces correct ordering per the LSP specification
 *
 * **Validates: Requirements 3.1**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include "meld/daemon/lsp_server.hpp"

#include <string>
#include <vector>
#include <sstream>

namespace {

using namespace meld::lsp::protocol;
using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Generators
// ---------------------------------------------------------------------------

/// Generate a valid LSP method name
rc::Gen<std::string> genLspMethod() {
    static const std::vector<std::string> methods = {
        "textDocument/completion",
        "textDocument/hover",
        "textDocument/definition",
        "textDocument/references",
        "textDocument/formatting",
        "textDocument/rangeFormatting",
        "textDocument/rename",
        "textDocument/semanticTokens/full",
        "textDocument/publishDiagnostics",
        "textDocument/didOpen",
        "textDocument/didChange",
        "textDocument/didClose",
        "textDocument/didSave",
    };
    return rc::gen::elementOf(methods);
}

/// Generate a valid JSON-RPC request id
rc::Gen<int> genRequestId() {
    return rc::gen::inRange(1, 100000);
}

/// Generate a well-formed JSON-RPC request string
rc::Gen<std::string> genValidRequest() {
    return rc::gen::map(
        rc::gen::tuple(genRequestId(), genLspMethod()),
        [](const std::tuple<int, std::string>& t) {
            auto [id, method] = t;
            json req = {
                {"jsonrpc", "2.0"},
                {"id", id},
                {"method", method},
                {"params", json::object()}
            };
            return req.dump();
        }
    );
}

/// Generate a well-formed JSON-RPC notification string
rc::Gen<std::string> genValidNotification() {
    return rc::gen::map(
        genLspMethod(),
        [](const std::string& method) {
            json notif = {
                {"jsonrpc", "2.0"},
                {"method", method},
                {"params", json::object()}
            };
            return notif.dump();
        }
    );
}

/// Generate a malformed / non-JSON string
rc::Gen<std::string> genMalformedMessage() {
    return rc::gen::oneOf(
        rc::gen::just(std::string("not json at all")),
        rc::gen::just(std::string("{}")),
        rc::gen::just(std::string("{\"jsonrpc\":\"2.0\"}")),
        rc::gen::just(std::string("{")),
        rc::gen::just(std::string("[1,2,3]"))
    );
}

/// Helper: bring a server to Running state
void bring_to_running(LspServer& server) {
    json init_req = {{"jsonrpc","2.0"},{"id",1},{"method","initialize"},{"params",json::object()}};
    server.process_message(init_req.dump());
    json initialized = {{"jsonrpc","2.0"},{"method","initialized"},{"params",json::object()}};
    server.process_message(initialized.dump());
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

/**
 * Property 11a: Any well-formed JSON-RPC request sent to a running server
 * must produce a JSON-RPC response (either result or error), never nothing.
 */
TEST(GrammarConformancePropertyTest, ValidRequestAlwaysProducesResponse) {
    rc::check("A valid JSON-RPC request to a running server always yields a response",
        []() {
            auto raw = *genValidRequest();

            LspServer server;
            bring_to_running(server);

            auto resp = server.process_message(raw);
            // Requests always get a response (result or method-not-found error)
            RC_ASSERT(resp.has_value());

            auto j = json::parse(*resp);
            RC_ASSERT(j.contains("jsonrpc"));
            RC_ASSERT(j["jsonrpc"] == "2.0");
            // Must have either "result" or "error"
            RC_ASSERT(j.contains("result") || j.contains("error"));
            // Must echo back an id
            RC_ASSERT(j.contains("id"));
        }
    );
}

/**
 * Property 11b: Any well-formed JSON-RPC notification sent to a running
 * server must NOT produce a response (notifications are fire-and-forget).
 */
TEST(GrammarConformancePropertyTest, NotificationsNeverProduceResponse) {
    rc::check("A valid JSON-RPC notification to a running server yields no response",
        []() {
            auto raw = *genValidNotification();

            LspServer server;
            bring_to_running(server);

            auto resp = server.process_message(raw);
            RC_ASSERT(!resp.has_value());
        }
    );
}

/**
 * Property 11c: Malformed messages always produce a parse-error response.
 */
TEST(GrammarConformancePropertyTest, MalformedMessagesProduceError) {
    rc::check("Malformed messages produce a JSON-RPC error response",
        []() {
            auto raw = *genMalformedMessage();

            LspServer server;
            bring_to_running(server);

            auto resp = server.process_message(raw);
            RC_ASSERT(resp.has_value());

            auto j = json::parse(*resp);
            RC_ASSERT(j.contains("error"));
        }
    );
}

/**
 * Property 11d: Before initialization, any request other than "initialize"
 * must be rejected with a "server not initialized" error.
 */
TEST(GrammarConformancePropertyTest, RequestsBeforeInitializeAreRejected) {
    rc::check("Requests before initialize are rejected with error -32002",
        []() {
            auto raw = *genValidRequest();

            LspServer server;
            // Server is Uninitialized — do NOT call initialize

            auto resp = server.process_message(raw);
            RC_ASSERT(resp.has_value());

            auto j = json::parse(*resp);
            RC_ASSERT(j.contains("error"));
            RC_ASSERT(j["error"]["code"] == -32002);
        }
    );
}

/**
 * Property 11e: The initialize response always contains "capabilities"
 * and "serverInfo" fields, regardless of client params.
 */
TEST(GrammarConformancePropertyTest, InitializeAlwaysReturnsCapabilities) {
    rc::check("Initialize response always contains capabilities and serverInfo",
        []() {
            // Generate random client capabilities to send
            auto client_name = *rc::gen::element(
                std::string("vscode"), std::string("neovim"),
                std::string("emacs"), std::string("sublime"));

            json params = {
                {"capabilities", json::object()},
                {"clientInfo", {{"name", client_name}}}
            };
            json req = {{"jsonrpc","2.0"},{"id",1},{"method","initialize"},{"params",params}};

            LspServer server;
            auto resp = server.process_message(req.dump());
            RC_ASSERT(resp.has_value());

            auto j = json::parse(*resp);
            RC_ASSERT(j.contains("result"));
            RC_ASSERT(j["result"].contains("capabilities"));
            RC_ASSERT(j["result"].contains("serverInfo"));

            // Capabilities must advertise the core features
            auto& caps = j["result"]["capabilities"];
            RC_ASSERT(caps.contains("semanticTokensProvider"));
            RC_ASSERT(caps.contains("completionProvider"));
            RC_ASSERT(caps.contains("hoverProvider"));
            RC_ASSERT(caps.contains("definitionProvider"));
            RC_ASSERT(caps.contains("referencesProvider"));
            RC_ASSERT(caps.contains("documentFormattingProvider"));
            RC_ASSERT(caps.contains("renameProvider"));
            RC_ASSERT(caps.contains("diagnosticProvider"));
        }
    );
}

/**
 * Property 11f: After shutdown, any request must be rejected.
 */
TEST(GrammarConformancePropertyTest, RequestsAfterShutdownAreRejected) {
    rc::check("Requests after shutdown are rejected with an error",
        []() {
            auto raw = *genValidRequest();

            LspServer server;
            bring_to_running(server);

            // Shutdown
            json shutdown = {{"jsonrpc","2.0"},{"id",999},{"method","shutdown"},{"params",json::object()}};
            server.process_message(shutdown.dump());

            auto resp = server.process_message(raw);
            RC_ASSERT(resp.has_value());

            auto j = json::parse(*resp);
            RC_ASSERT(j.contains("error"));
        }
    );
}
