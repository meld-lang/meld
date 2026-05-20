#include <gtest/gtest.h>
#include "meld/daemon/json_rpc.hpp"

namespace meld::lsp::protocol {

TEST(JsonRpcTest, ParseRequest) {
    std::string raw = R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})";
    auto msg = JsonRpc::parse(raw);
    ASSERT_TRUE(msg.has_value());
    EXPECT_EQ(msg->type, MessageType::Request);
    EXPECT_EQ(msg->id, 1);
    EXPECT_EQ(msg->method, "initialize");
}

TEST(JsonRpcTest, ParseNotification) {
    std::string raw = R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"uri":"file:///test.meld"}})";
    auto msg = JsonRpc::parse(raw);
    ASSERT_TRUE(msg.has_value());
    EXPECT_EQ(msg->type, MessageType::Notification);
    EXPECT_FALSE(msg->id.has_value());
    EXPECT_EQ(msg->method, "textDocument/didOpen");
}

TEST(JsonRpcTest, ParseInvalidJson) {
    auto msg = JsonRpc::parse("not json");
    EXPECT_FALSE(msg.has_value());
}

TEST(JsonRpcTest, MakeResponse) {
    auto response = JsonRpc::make_response(1, {{"capabilities", {}}});
    auto j = nlohmann::json::parse(response);
    EXPECT_EQ(j["jsonrpc"], "2.0");
    EXPECT_EQ(j["id"], 1);
    EXPECT_TRUE(j.contains("result"));
}

TEST(JsonRpcTest, MakeError) {
    auto response = JsonRpc::make_error(1, -32601, "Method not found");
    auto j = nlohmann::json::parse(response);
    EXPECT_EQ(j["error"]["code"], -32601);
    EXPECT_EQ(j["error"]["message"], "Method not found");
}

TEST(JsonRpcTest, MakeNotification) {
    auto notif = JsonRpc::make_notification("textDocument/publishDiagnostics", {{"uri", "file:///test.meld"}});
    auto j = nlohmann::json::parse(notif);
    EXPECT_EQ(j["method"], "textDocument/publishDiagnostics");
    EXPECT_FALSE(j.contains("id"));
}

TEST(MessageRouterTest, DispatchRequest) {
    MessageRouter router;
    router.register_request("test/method", [](const json& params) -> json {
        return {{"echo", params.value("input", "")}};
    });

    Message msg;
    msg.type = MessageType::Request;
    msg.id = 42;
    msg.method = "test/method";
    msg.params = {{"input", "hello"}};

    auto response = router.dispatch(msg);
    ASSERT_TRUE(response.has_value());
    auto j = nlohmann::json::parse(*response);
    EXPECT_EQ(j["id"], 42);
    EXPECT_EQ(j["result"]["echo"], "hello");
}

TEST(MessageRouterTest, DispatchNotification) {
    bool called = false;
    MessageRouter router;
    router.register_notification("test/notify", [&called](const json&) {
        called = true;
    });

    Message msg;
    msg.type = MessageType::Notification;
    msg.method = "test/notify";
    msg.params = {};

    auto response = router.dispatch(msg);
    EXPECT_FALSE(response.has_value());
    EXPECT_TRUE(called);
}

TEST(MessageRouterTest, UnknownMethodReturnsError) {
    MessageRouter router;

    Message msg;
    msg.type = MessageType::Request;
    msg.id = 1;
    msg.method = "unknown/method";
    msg.params = {};

    auto response = router.dispatch(msg);
    ASSERT_TRUE(response.has_value());
    auto j = nlohmann::json::parse(*response);
    EXPECT_TRUE(j.contains("error"));
    EXPECT_EQ(j["error"]["code"], -32601);
}

} // namespace meld::lsp::protocol
