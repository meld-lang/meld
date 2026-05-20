#include <gtest/gtest.h>
#include "meld/daemon/protocol.hpp"

namespace meld::lsp {

class LSPProtocolTest : public ::testing::Test {
protected:
    void SetUp() override {
        protocol = std::make_unique<LSPProtocol>();
    }
    
    std::unique_ptr<LSPProtocol> protocol;
};

TEST_F(LSPProtocolTest, InitializeProtocol) {
    // Test that protocol can be initialized without errors
    EXPECT_NO_THROW(protocol->initialize());
}

TEST_F(LSPProtocolTest, HandleMessage) {
    // Test that messages can be handled without errors
    std::string testMessage = R"({"jsonrpc":"2.0","method":"initialize","params":{}})";
    EXPECT_NO_THROW(protocol->handleMessage(testMessage));
}

TEST_F(LSPProtocolTest, SendResponse) {
    // Test that responses can be sent without errors
    std::string testResponse = R"({"jsonrpc":"2.0","result":{}})";
    EXPECT_NO_THROW(protocol->sendResponse(testResponse));
}

} // namespace meld::lsp