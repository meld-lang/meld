/// Property 2: Incremental Equivalence
/// For any file change, incremental re-analysis SHALL produce the same
/// diagnostics as a full re-analysis of the entire workspace.

#include "meld/daemon/incremental_analyzer.hpp"
#include "meld/daemon/semantic_model.hpp"

#include <gtest/gtest.h>
#include <exception>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include <filesystem>
#include <fstream>

namespace meld::daemon {
namespace {

class IncrementalEquivalenceFixture : public ::testing::Test {
protected:
    std::filesystem::path tmp_dir;

    void SetUp() override {
        tmp_dir = std::filesystem::temp_directory_path() / "meld-incr-prop-test";
        std::filesystem::create_directories(tmp_dir);
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(tmp_dir, ec);
    }

    void write_meld_file(const std::string& name, const std::string& content) {
        std::ofstream ofs(tmp_dir / name);
        ofs << content;
    }
};

RC_GTEST_FIXTURE_PROP(IncrementalEquivalenceFixture,
                       IncrementalMatchesFull, ()) {
    // Generate a small set of files
    auto file_count = *rc::gen::inRange(1, 5);
    std::vector<std::string> filenames;

    for (int i = 0; i < file_count; ++i) {
        std::string name = "module_" + std::to_string(i) + ".meld";
        std::string content = "// Module " + std::to_string(i) + "\n";
        if (i > 0) {
            content += "import module_" + std::to_string(i - 1) + "\n";
        }
        content += "val x_" + std::to_string(i) + " = " + std::to_string(i * 10) + "\n";
        write_meld_file(name, content);
        filenames.push_back(name);
    }

    // Full analysis
    SemanticModel full_model;
    IncrementalAnalyzer full_analyzer(full_model);
    full_analyzer.analyze_workspace(tmp_dir);

    // Incremental analysis: start from scratch, add files one by one
    SemanticModel incr_model;
    IncrementalAnalyzer incr_analyzer(incr_model);
    for (const auto& fname : filenames) {
        incr_analyzer.analyze_change(tmp_dir / fname);
    }

    // Both models should have the same files indexed
    RC_ASSERT(full_model.file_count() == incr_model.file_count());

    // Diagnostics should match for each file
    for (const auto& fname : filenames) {
        auto full_diags = full_model.get_diagnostics(tmp_dir / fname);
        auto incr_diags = incr_model.get_diagnostics(tmp_dir / fname);
        RC_ASSERT(full_diags.size() == incr_diags.size());
    }
}

RC_GTEST_FIXTURE_PROP(IncrementalEquivalenceFixture,
                       ModifyDoesNotAffectUnrelated, ()) {
    // Create two independent files
    write_meld_file("a.meld", "val a = 1\n");
    write_meld_file("b.meld", "val b = 2\n");

    SemanticModel model;
    IncrementalAnalyzer analyzer(model);
    analyzer.analyze_workspace(tmp_dir);

    auto b_diags_before = model.get_diagnostics(tmp_dir / "b.meld");

    // Modify file a
    write_meld_file("a.meld", "val a = 999\n");
    analyzer.analyze_change(tmp_dir / "a.meld");

    // File b's diagnostics should be unchanged
    auto b_diags_after = model.get_diagnostics(tmp_dir / "b.meld");
    RC_ASSERT(b_diags_before.size() == b_diags_after.size());
}

}  // namespace
}  // namespace meld::daemon
