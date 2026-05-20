#include "meld/daemon/integrity_verifier.hpp"

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

namespace meld::daemon {
namespace {

class IntegrityVerifierTest : public ::testing::Test {
protected:
    IntegrityVerifier verifier{VerificationMode::Offline};
    std::filesystem::path tmp_dir;

    void SetUp() override {
        tmp_dir = std::filesystem::temp_directory_path() / "meld-iv-test";
        std::filesystem::create_directories(tmp_dir);
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(tmp_dir, ec);
    }

    void write_file(const std::filesystem::path& path, const std::string& content) {
        std::ofstream ofs(path);
        ofs << content;
    }
};

TEST_F(IntegrityVerifierTest, FailsWhenTombstoneNotFound) {
    auto binary = tmp_dir / "app";
    auto manifest = tmp_dir / "app.manifest";
    write_file(binary, "binary content");
    write_file(manifest, "code_hash=abc\nversion=1.0");

    auto result = verifier.verify(binary, manifest);
    EXPECT_FALSE(result.passed);
    EXPECT_EQ(result.failed_check, "tombstone_read");
}

TEST_F(IntegrityVerifierTest, FailsWhenManifestNotFound) {
    auto binary = tmp_dir / "app";
    auto tombstone = tmp_dir / "app.tombstone";
    write_file(binary, "binary content");
    write_file(tombstone, "signature_blob=sig123\nmanifest_hash=mh\ncode_hash=ch");

    auto result = verifier.verify(binary, tmp_dir / "nonexistent.manifest");
    EXPECT_FALSE(result.passed);
    EXPECT_EQ(result.failed_check, "manifest_read");
}

TEST_F(IntegrityVerifierTest, FailsOnCodeHashMismatch) {
    auto binary = tmp_dir / "app";
    auto tombstone = tmp_dir / "app.tombstone";
    auto manifest = tmp_dir / "app.manifest";

    write_file(binary, "binary content");
    write_file(tombstone, "signature_blob=sig123\nmanifest_hash=placeholder\ncode_hash=ch");
    write_file(manifest, "code_hash=wrong_hash\nversion=1.0");

    auto result = verifier.verify(binary, manifest);
    EXPECT_FALSE(result.passed);
    EXPECT_EQ(result.failed_check, "code_hash");
}

TEST_F(IntegrityVerifierTest, AuditLogRecordsFailures) {
    auto binary = tmp_dir / "app";
    auto manifest = tmp_dir / "app.manifest";
    write_file(binary, "binary content");
    write_file(manifest, "code_hash=abc");

    verifier.verify(binary, manifest);
    EXPECT_GE(verifier.audit_log().size(), 1u);
    EXPECT_EQ(verifier.audit_log()[0].binary_path, binary);
}

TEST_F(IntegrityVerifierTest, ModeIsConfigurable) {
    EXPECT_EQ(verifier.mode(), VerificationMode::Offline);
    IntegrityVerifier online_verifier(VerificationMode::Online);
    EXPECT_EQ(online_verifier.mode(), VerificationMode::Online);
}

}  // namespace
}  // namespace meld::daemon
