#include "meld/daemon/bazel_worker.hpp"

#include <algorithm>
#include <sstream>

namespace meld::daemon {

BazelWorker::BazelWorker(SemanticModel& model, std::istream& in, std::ostream& out)
    : model_(model), in_(in), out_(out) {}

BazelWorker::~BazelWorker() {
    stop();
}

void BazelWorker::run() {
    running_ = true;
    while (running_) {
        auto request = read_request();
        if (!request) break;  // EOF or parse error
        auto response = process_request(*request);
        write_response(response);
        ++requests_processed_;
    }
}

WorkResponse BazelWorker::process_request(const WorkRequest& request) {
    // Extract .meld source files from inputs
    std::vector<std::string> meld_sources;
    for (const auto& input : request.inputs) {
        if (input.size() >= 5 && input.substr(input.size() - 5) == ".meld") {
            meld_sources.push_back(input);
        }
    }

    if (meld_sources.empty()) {
        return WorkResponse{1, "No .meld source files in inputs", request.request_id};
    }

    try {
        return compile_sources(meld_sources, request.arguments, request.request_id);
    } catch (const std::exception& e) {
        // Req 3.6: Unrecoverable error — reset LLVM context, don't terminate
        reset_llvm_context();
        return WorkResponse{1, std::string("Internal error: ") + e.what(), request.request_id};
    }
}

void BazelWorker::stop() {
    running_ = false;
}

std::optional<WorkRequest> BazelWorker::read_request() {
    // Simplified protobuf reading: length-delimited format
    // In production, use proper protobuf deserialization
    // For now, read newline-delimited JSON as a stand-in

    std::string line;
    if (!std::getline(in_, line)) return std::nullopt;
    if (line.empty()) return std::nullopt;

    // Parse as simple format: "request_id|arg1,arg2,...|input1,input2,..."
    WorkRequest req;
    std::istringstream iss(line);
    std::string segment;

    // request_id
    if (std::getline(iss, segment, '|')) {
        try { req.request_id = std::stoi(segment); } catch (...) { req.request_id = 0; }
    }

    // arguments
    if (std::getline(iss, segment, '|')) {
        std::istringstream args_stream(segment);
        std::string arg;
        while (std::getline(args_stream, arg, ',')) {
            if (!arg.empty()) req.arguments.push_back(arg);
        }
    }

    // inputs
    if (std::getline(iss, segment, '|')) {
        std::istringstream inputs_stream(segment);
        std::string input;
        while (std::getline(inputs_stream, input, ',')) {
            if (!input.empty()) req.inputs.push_back(input);
        }
    }

    return req;
}

void BazelWorker::write_response(const WorkResponse& response) {
    // Simplified: write as "request_id|exit_code|output\n"
    out_ << response.request_id << "|" << response.exit_code << "|" << response.output << "\n";
    out_.flush();
}

WorkResponse BazelWorker::compile_sources(const std::vector<std::string>& sources,
                                          const std::vector<std::string>& /*args*/,
                                          int32_t request_id) {
    std::ostringstream output;
    int errors = 0;

    for (const auto& src : sources) {
        std::filesystem::path src_path(src);

        // Check if file changed since last compilation for this target
        // (Req 3.4: incremental AST invalidation)
        auto& file_hashes = target_file_hashes_[src];
        // In production: compute file hash, compare with cached, skip if unchanged

        // Get diagnostics from the SemanticModel
        auto diags = model_.get_diagnostics(src_path);
        for (const auto& d : diags) {
            if (d.severity == DiagnosticSeverity::Error) {
                ++errors;
            }
            output << d.location.file.string() << ":" << d.location.line
                   << ":" << d.location.column << ": " << d.message << "\n";
        }

        // In production: invoke LLVM to compile to .bc bitcode
        // using the persistent LLVM_Context_Pool
    }

    return WorkResponse{errors > 0 ? 1 : 0, output.str(), request_id};
}

void BazelWorker::reset_llvm_context() {
    // In production: destroy and recreate the LLVM context and module cache
    // Clear cached file hashes to force full recompilation
    target_file_hashes_.clear();
}

}  // namespace meld::daemon
