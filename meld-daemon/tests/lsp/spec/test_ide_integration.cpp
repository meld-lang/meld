#include <gtest/gtest.h>
#include "meld/daemon/protocol.hpp"
#include <sstream>

using namespace meld::lsp;

class LSPIDEIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        lsp_protocol = std::make_unique<LSPProtocol>();
        lsp_protocol->initialize();
    }

    std::unique_ptr<LSPProtocol> lsp_protocol;
};

// TASK 35.9: Test inlay hints request handling
TEST_F(LSPIDEIntegrationTest, HandleInlayHintsRequest) {
    std::string document_uri = "file:///test.meld";
    
    // Handle inlay hints request
    std::string response = lsp_protocol->handle_inlay_hints_request(document_uri, 0, 100);
    
    // Should return valid JSON-RPC response
    EXPECT_NE(response.find("\"jsonrpc\":\"2.0\""), std::string::npos);
    EXPECT_NE(response.find("\"id\":1"), std::string::npos);
    EXPECT_NE(response.find("\"result\":"), std::string::npos);
}

// TASK 35.9: Test document change handling
TEST_F(LSPIDEIntegrationTest, HandleDocumentChange) {
    std::string document_uri = "file:///test.meld";
    std::string content = "fnc saveUser() { File.write(\"user.txt\", data) }";
    
    // Should not throw exception
    EXPECT_NO_THROW(lsp_protocol->handle_document_change(document_uri, content));
}

// TASK 35.9: Test LSP message parsing
TEST_F(LSPIDEIntegrationTest, HandleLSPMessages) {
    // Mock inlay hints request message
    std::string inlay_hints_message = R"({
        "jsonrpc": "2.0",
        "id": 1,
        "method": "textDocument/inlayHint",
        "params": {
            "textDocument": {
                "uri": "file:///test.meld"
            },
            "range": {
                "start": {"line": 0, "character": 0},
                "end": {"line": 100, "character": 0}
            }
        }
    })";
    
    // Should handle without throwing
    EXPECT_NO_THROW(lsp_protocol->handleMessage(inlay_hints_message));
    
    // Mock document change notification
    std::string doc_change_message = R"({
        "jsonrpc": "2.0",
        "method": "textDocument/didChange",
        "params": {
            "textDocument": {
                "uri": "file:///test.meld",
                "version": 2
            },
            "contentChanges": [{
                "text": "fnc newFunction() { println(\"hello\") }"
            }]
        }
    })";
    
    // Should handle without throwing
    EXPECT_NO_THROW(lsp_protocol->handleMessage(doc_change_message));
}

// TASK 35.9: Test JSON-RPC response creation
TEST_F(LSPIDEIntegrationTest, CreateJsonRpcResponse) {
    // This tests the internal helper method indirectly
    std::string response = lsp_protocol->handle_inlay_hints_request("file:///test.meld", 0, 10);
    
    // Should be valid JSON-RPC format
    EXPECT_NE(response.find("{"), std::string::npos);
    EXPECT_NE(response.find("}"), std::string::npos);
    EXPECT_NE(response.find("\"jsonrpc\""), std::string::npos);
    EXPECT_NE(response.find("\"id\""), std::string::npos);
    EXPECT_NE(response.find("\"result\""), std::string::npos);
}

// TASK 35.9: Test incremental change handling
TEST_F(LSPIDEIntegrationTest, HandleIncrementalChange) {
    std::string document_uri = "file:///test.meld";
    
    // Should handle incremental changes without throwing
    EXPECT_NO_THROW(lsp_protocol->handle_incremental_change(document_uri, 10, 15, "new code"));
}

// TASK 35.9: Test ghost annotation updates
TEST_F(LSPIDEIntegrationTest, SendGhostAnnotationUpdates) {
    std::string document_uri = "file:///test.meld";
    
    // Should handle ghost annotation updates without throwing
    EXPECT_NO_THROW(lsp_protocol->send_ghost_annotation_updates(document_uri));
}

// TASK 35.9: Test enhanced LSP message handling with incremental changes
TEST_F(LSPIDEIntegrationTest, HandleIncrementalLSPMessages) {
    // Mock incremental change message
    std::string incremental_change_message = R"({
        "jsonrpc": "2.0",
        "method": "textDocument/didChange",
        "params": {
            "textDocument": {
                "uri": "file:///test.meld",
                "version": 3
            },
            "contentChanges": [{
                "range": {
                    "start": {"line": 10, "character": 0},
                    "end": {"line": 15, "character": 0}
                },
                "text": "fnc newFunction() { println(\"hello\") }"
            }]
        }
    })";
    
    // Should handle incremental changes without throwing
    EXPECT_NO_THROW(lsp_protocol->handleMessage(incremental_change_message));
    
    // Mock full document change message (no range specified)
    std::string full_change_message = R"({
        "jsonrpc": "2.0",
        "method": "textDocument/didChange",
        "params": {
            "textDocument": {
                "uri": "file:///test.meld",
                "version": 4
            },
            "contentChanges": [{
                "text": "fnc completelyNewContent() { File.write(\"test.txt\", \"data\") }"
            }]
        }
    })";
    
    // Should handle full document changes without throwing
    EXPECT_NO_THROW(lsp_protocol->handleMessage(full_change_message));
}