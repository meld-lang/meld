#include "meld/daemon/lsp_channel.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <sstream>

namespace meld::daemon {
namespace {

class LspChannelTest : public ::testing::Test {
protected:
    SemanticModel model;

    std::string make_lsp_message(const nlohmann::json& msg) {
        std::string body = msg.dump();
        return "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
    }
};

TEST_F(LspChannelTest, InitializeReturnsCapabilities) {
    nlohmann::json init_msg;
    init_msg["jsonrpc"] = "2.0";
    init_msg["id"] = 1;
    init_msg["method"] = "initialize";
    init_msg["params"] = {{"capabilities", {}}};

    nlohmann::json shutdown_msg;
    shutdown_msg["jsonrpc"] = "2.0";
    shutdown_msg["id"] = 2;
    shutdown_msg["method"] = "shutdown";

    std::istringstream in(make_lsp_message(init_msg) + make_lsp_message(shutdown_msg));
    std::ostringstream out;

    LspChannel channel(model, in, out);
    channel.start();

    // Parse the response
    auto output = out.str();
    auto body_start = output.find("{");
    ASSERT_NE(body_start, std::string::npos);
    auto body_end = output.find("Content-Length:", body_start);
    std::string first_response;
    if (body_end != std::string::npos) {
        first_response = output.substr(body_start, body_end - body_start);
    } else {
        first_response = output.substr(body_start);
    }

    auto j = nlohmann::json::parse(first_response, nullptr, false);
    ASSERT_FALSE(j.is_discarded());
    EXPECT_TRUE(j.contains("result"));
    EXPECT_TRUE(j["result"].contains("capabilities"));
    EXPECT_EQ(j["result"]["serverInfo"]["name"], "meldd");
}

TEST_F(LspChannelTest, ShutdownSetsFlag) {
    nlohmann::json shutdown_msg;
    shutdown_msg["jsonrpc"] = "2.0";
    shutdown_msg["id"] = 1;
    shutdown_msg["method"] = "shutdown";

    std::istringstream in(make_lsp_message(shutdown_msg));
    std::ostringstream out;

    LspChannel channel(model, in, out);
    channel.start();

    EXPECT_TRUE(channel.shutdown_requested());
}

TEST_F(LspChannelTest, PublishDiagnosticsFormatsCorrectly) {
    std::istringstream in;
    std::ostringstream out;
    LspChannel channel(model, in, out);

    Diagnostic d;
    d.location = {"test.meld", 5, 3};
    d.severity = DiagnosticSeverity::Error;
    d.message = "type error";
    d.rule_id = "E0042";
    d.ast_selector = "$.module";

    channel.publish_diagnostics("test.meld", {d});

    auto output = out.str();
    EXPECT_NE(output.find("publishDiagnostics"), std::string::npos);
    EXPECT_NE(output.find("test.meld"), std::string::npos);
}

}  // namespace
}  // namespace meld::daemon
