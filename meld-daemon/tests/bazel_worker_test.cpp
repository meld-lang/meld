#include "meld/daemon/bazel_worker.hpp"

#include <gtest/gtest.h>
#include <sstream>

namespace meld::daemon {
namespace {

class BazelWorkerTest : public ::testing::Test {
protected:
    SemanticModel model;
};

TEST_F(BazelWorkerTest, ProcessRequestWithMeldSources) {
    // Add a file to the model with no errors
    FileSemantics sem;
    sem.path = "main.meld";
    model.update_file("main.meld", std::move(sem));

    std::istringstream in;
    std::ostringstream out;
    BazelWorker worker(model, in, out);

    WorkRequest req;
    req.request_id = 1;
    req.arguments = {"--output=main.bc"};
    req.inputs = {"main.meld"};

    auto resp = worker.process_request(req);
    EXPECT_EQ(resp.exit_code, 0);
    EXPECT_EQ(resp.request_id, 1);
}

TEST_F(BazelWorkerTest, ProcessRequestWithNoMeldSources) {
    std::istringstream in;
    std::ostringstream out;
    BazelWorker worker(model, in, out);

    WorkRequest req;
    req.request_id = 2;
    req.inputs = {"helper.cpp"};

    auto resp = worker.process_request(req);
    EXPECT_NE(resp.exit_code, 0);
    EXPECT_NE(resp.output.find("No .meld source"), std::string::npos);
}

TEST_F(BazelWorkerTest, ProcessRequestWithDiagnostics) {
    FileSemantics sem;
    sem.path = "err.meld";
    Diagnostic d;
    d.location = {"err.meld", 5, 1};
    d.severity = DiagnosticSeverity::Error;
    d.message = "undefined symbol: foo";
    d.rule_id = "E0001";
    sem.diagnostics.push_back(d);
    model.update_file("err.meld", std::move(sem));

    std::istringstream in;
    std::ostringstream out;
    BazelWorker worker(model, in, out);

    WorkRequest req;
    req.request_id = 3;
    req.inputs = {"err.meld"};

    auto resp = worker.process_request(req);
    EXPECT_EQ(resp.exit_code, 1);
    EXPECT_NE(resp.output.find("undefined symbol"), std::string::npos);
}

TEST_F(BazelWorkerTest, RunReadsFromStream) {
    std::istringstream in("1|--output=a.bc|main.meld\n");
    std::ostringstream out;

    FileSemantics sem;
    sem.path = "main.meld";
    model.update_file("main.meld", std::move(sem));

    BazelWorker worker(model, in, out);
    worker.run();

    EXPECT_EQ(worker.requests_processed(), 1u);
    EXPECT_FALSE(out.str().empty());
}

TEST_F(BazelWorkerTest, RunHandlesEmptyStream) {
    std::istringstream in("");
    std::ostringstream out;
    BazelWorker worker(model, in, out);
    worker.run();
    EXPECT_EQ(worker.requests_processed(), 0u);
}

}  // namespace
}  // namespace meld::daemon
