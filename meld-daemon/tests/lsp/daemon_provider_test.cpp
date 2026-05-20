#include <gtest/gtest.h>
#include "meld/daemon/semantic_model.hpp"

namespace meld::daemon {

TEST(DaemonProviderTest, SemanticModelConstructible) {
    SemanticModel model;
    SUCCEED();
}

} // namespace meld::daemon
