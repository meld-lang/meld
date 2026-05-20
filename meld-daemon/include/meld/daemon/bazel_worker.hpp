#pragma once

#include "meld/daemon/semantic_model.hpp"

#include <cstdint>
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace meld::daemon {

/// A Bazel WorkRequest (simplified protobuf representation)
struct WorkRequest {
    std::vector<std::string> arguments;
    std::vector<std::string> inputs;
    int32_t request_id{0};
};

/// A Bazel WorkResponse
struct WorkResponse {
    int32_t exit_code{0};
    std::string output;
    int32_t request_id{0};
};

/// Persistent Bazel worker that maintains an LLVM_Context_Pool across invocations.
/// Reads WorkRequest protobuf messages from stdin, compiles .meld sources to .bc
/// bitcode, and writes WorkResponse messages to stdout.
class BazelWorker {
public:
    explicit BazelWorker(SemanticModel& model,
                         std::istream& in = std::cin,
                         std::ostream& out = std::cout);
    ~BazelWorker();

    /// Enter the worker loop (blocking — reads WorkRequests until EOF)
    void run();

    /// Process a single work request
    WorkResponse process_request(const WorkRequest& request);

    /// Stop the worker loop
    void stop();

    /// Get the number of requests processed
    size_t requests_processed() const { return requests_processed_; }

private:
    /// Read a WorkRequest from the input stream (length-delimited protobuf)
    std::optional<WorkRequest> read_request();

    /// Write a WorkResponse to the output stream
    void write_response(const WorkResponse& response);

    /// Compile a set of .meld files to .bc bitcode
    WorkResponse compile_sources(const std::vector<std::string>& sources,
                                 const std::vector<std::string>& args,
                                 int32_t request_id);

    /// Reset the LLVM context after an unrecoverable error
    void reset_llvm_context();

    /// Track which files have changed since last request for a target
    std::unordered_map<std::string, std::unordered_set<std::string>> target_file_hashes_;

    SemanticModel& model_;
    std::istream& in_;
    std::ostream& out_;
    bool running_{false};
    size_t requests_processed_{0};
};

}  // namespace meld::daemon
