#include <gtest/gtest.h>
#include "../src/protocol.hpp"

namespace meld::mcp {

class MCPProtocolTest : public ::testing::Test {
protected:
    void SetUp() override {
        protocol = std::make_unique<MCPProtocol>();
    }
    
    std::unique_ptr<MCPProtocol> protocol;
};

TEST_F(MCPProtocolTest, InitializeProtocol) {
    // Test that protocol can be initialized without errors
    EXPECT_NO_THROW(protocol->initialize());
}

TEST_F(MCPProtocolTest, HandleMessage) {
    // Test that messages can be handled without errors
    std::string testMessage = R"({"method":"analyze","params":{"code":"let x = 42"}})";
    EXPECT_NO_THROW(protocol->handleMessage(testMessage));
}

TEST_F(MCPProtocolTest, SendResponse) {
    // Test that responses can be sent without errors
    std::string testResponse = R"({"result":{"analysis":"success"}})";
    EXPECT_NO_THROW(protocol->sendResponse(testResponse));
}

TEST_F(MCPProtocolTest, GetCapabilities) {
    // Test that capabilities can be retrieved
    auto capabilities = protocol->getCapabilities();
    EXPECT_FALSE(capabilities.empty());
    EXPECT_GT(capabilities.size(), 0);
}

} // namespace meld::mcp