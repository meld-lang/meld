#include "meld/daemon/vector_index.hpp"
#include "meld/daemon/passthrough_embedding_provider.hpp"
#include "meld/daemon/embedding_provider.hpp"

#include <gtest/gtest.h>
#include <cmath>
#include <filesystem>
#include <memory>

namespace meld::daemon {
namespace {

// A deterministic embedding provider for testing.
// Maps text to a simple hash-based vector so results are reproducible.
class TestEmbeddingProvider : public EmbeddingProvider {
public:
    explicit TestEmbeddingProvider(size_t dims = 8) : dims_(dims) {}

    std::vector<float> embed(const std::string& text) const override {
        std::vector<float> v(dims_, 0.0f);
        // Simple hash-scatter: spread characters across dimensions
        for (size_t i = 0; i < text.size(); ++i) {
            v[i % dims_] += static_cast<float>(text[i]) / 128.0f;
        }
        // Normalize
        float norm = 0.0f;
        for (float x : v) norm += x * x;
        norm = std::sqrt(norm);
        if (norm > 0.0f) {
            for (float& x : v) x /= norm;
        }
        return v;
    }

    size_t dimensions() const override { return dims_; }

private:
    size_t dims_;
};

// Helper: build a FileSemantics with a single exported function symbol.
FileSemantics make_file_semantics(const std::filesystem::path& path,
                                  const std::string& fn_name,
                                  const std::string& type_sig = "() -> Int",
                                  const std::vector<std::string>& effects = {}) {
    auto node = std::make_shared<ASTNode>();
    node->kind = "function_definition";
    node->name = fn_name;
    node->type_info = type_sig;
    node->location = {path, 1, 0};
    node->effects = effects;

    FileSemantics sem;
    sem.path = path;
    sem.ast = node;
    sem.exports = {fn_name};
    return sem;
}

// Helper: build FileSemantics with multiple symbols.
FileSemantics make_multi_symbol_file(const std::filesystem::path& path,
                                     const std::vector<std::pair<std::string, std::string>>& symbols) {
    auto root = std::make_shared<ASTNode>();
    root->kind = "module_definition";
    root->name = "root";
    root->location = {path, 0, 0};

    std::vector<std::string> exports;
    for (const auto& [name, type] : symbols) {
        auto child = std::make_shared<ASTNode>();
        child->kind = "function_definition";
        child->name = name;
        child->type_info = type;
        child->location = {path, static_cast<uint32_t>(exports.size() + 1), 0};
        root->children.push_back(child);
        exports.push_back(name);
    }

    FileSemantics sem;
    sem.path = path;
    sem.ast = root;
    sem.exports = exports;
    return sem;
}

class VectorIndexTest : public ::testing::Test {
protected:
    void SetUp() override {
        provider_ = std::make_shared<TestEmbeddingProvider>(8);
        index_ = std::make_unique<VectorIndex>(provider_);
    }

    std::shared_ptr<TestEmbeddingProvider> provider_;
    std::unique_ptr<VectorIndex> index_;
};

// --- Incremental re-indexing ---

TEST_F(VectorIndexTest, IndexFileAddsSymbols) {
    auto sem = make_file_semantics("src/math.meld", "add", "(Int, Int) -> Int");
    index_->index_file("src/math.meld", sem);
    EXPECT_EQ(index_->symbol_count(), 1u);
}

TEST_F(VectorIndexTest, IndexFileReplacesOnReindex) {
    auto sem1 = make_file_semantics("src/math.meld", "add", "(Int, Int) -> Int");
    index_->index_file("src/math.meld", sem1);
    EXPECT_EQ(index_->symbol_count(), 1u);

    // Re-index same file with different symbol
    auto sem2 = make_file_semantics("src/math.meld", "subtract", "(Int, Int) -> Int");
    index_->index_file("src/math.meld", sem2);
    EXPECT_EQ(index_->symbol_count(), 1u);

    // Verify the new symbol is searchable
    auto results = index_->keyword_search("subtract", 10);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].symbol.name, "subtract");
}

TEST_F(VectorIndexTest, RemoveFileRemovesSymbols) {
    auto sem = make_file_semantics("src/math.meld", "add");
    index_->index_file("src/math.meld", sem);
    EXPECT_EQ(index_->symbol_count(), 1u);

    index_->remove_file("src/math.meld");
    EXPECT_EQ(index_->symbol_count(), 0u);
}

TEST_F(VectorIndexTest, IncrementalReindexOnlyAffectsChangedFile) {
    auto sem_a = make_file_semantics("src/a.meld", "foo");
    auto sem_b = make_file_semantics("src/b.meld", "bar");
    index_->index_file("src/a.meld", sem_a);
    index_->index_file("src/b.meld", sem_b);
    EXPECT_EQ(index_->symbol_count(), 2u);

    // Re-index only file a with a new symbol
    auto sem_a2 = make_file_semantics("src/a.meld", "baz");
    index_->index_file("src/a.meld", sem_a2);
    EXPECT_EQ(index_->symbol_count(), 2u);

    // "bar" from file b should still be there
    auto results = index_->keyword_search("bar", 10);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].symbol.name, "bar");

    // "foo" should be gone, replaced by "baz"
    auto foo_results = index_->keyword_search("foo", 10);
    EXPECT_TRUE(foo_results.empty());
}

// --- Nearest-neighbor search ---

TEST_F(VectorIndexTest, SemanticSearchReturnsRelevantSymbols) {
    auto sem = make_multi_symbol_file("src/lib.meld", {
        {"calculate_sum", "(List[Int]) -> Int"},
        {"parse_json", "(String) -> Json"},
        {"render_html", "(Template) -> String"},
    });
    index_->index_file("src/lib.meld", sem);

    auto results = index_->find_by_intent("sum calculation", 2);
    ASSERT_FALSE(results.empty());
    // The top result should be calculate_sum since it's most similar
    EXPECT_EQ(results[0].symbol.name, "calculate_sum");
}

TEST_F(VectorIndexTest, SearchRespectsMaxResults) {
    auto sem = make_multi_symbol_file("src/lib.meld", {
        {"a", "() -> Int"}, {"b", "() -> Int"}, {"c", "() -> Int"},
        {"d", "() -> Int"}, {"e", "() -> Int"},
    });
    index_->index_file("src/lib.meld", sem);

    auto results = index_->find_by_intent("function", 3);
    EXPECT_LE(results.size(), 3u);
}

TEST_F(VectorIndexTest, SearchOnEmptyIndexReturnsEmpty) {
    auto results = index_->find_by_intent("anything", 10);
    EXPECT_TRUE(results.empty());
}

// --- @blueprint enriched embeddings ---

TEST_F(VectorIndexTest, BlueprintEnrichedEmbeddingProducesBetterMatch) {
    // Symbol with blueprint summary
    auto node_with_bp = std::make_shared<ASTNode>();
    node_with_bp->kind = "function_definition";
    node_with_bp->name = "process_data";
    node_with_bp->type_info = "(Data) -> Result";
    node_with_bp->location = {"src/proc.meld", 1, 0};

    FileSemantics sem_bp;
    sem_bp.path = "src/proc.meld";
    sem_bp.ast = node_with_bp;

    // Symbol without blueprint
    auto node_plain = std::make_shared<ASTNode>();
    node_plain->kind = "function_definition";
    node_plain->name = "handle_data";
    node_plain->type_info = "(Data) -> Result";
    node_plain->location = {"src/handle.meld", 1, 0};

    FileSemantics sem_plain;
    sem_plain.path = "src/handle.meld";
    sem_plain.ast = node_plain;

    index_->index_file("src/proc.meld", sem_bp);
    index_->index_file("src/handle.meld", sem_plain);

    // Both should be indexed
    EXPECT_EQ(index_->symbol_count(), 2u);
}

// --- Persistence round-trip ---

TEST_F(VectorIndexTest, PersistenceRoundTrip) {
    auto sem = make_multi_symbol_file("src/lib.meld", {
        {"alpha", "(Int) -> Bool"},
        {"beta", "(String) -> Int"},
    });
    index_->index_file("src/lib.meld", sem);

    auto tmp_dir = std::filesystem::temp_directory_path() / "meld_test_vidx";
    std::filesystem::create_directories(tmp_dir);

    // Save
    ASSERT_TRUE(index_->save(tmp_dir));

    // Load into a fresh index with same provider
    auto index2 = std::make_unique<VectorIndex>(provider_);
    ASSERT_TRUE(index2->load(tmp_dir));

    EXPECT_EQ(index2->symbol_count(), 2u);

    // Search should produce same results
    auto r1 = index_->keyword_search("alpha", 10);
    auto r2 = index2->keyword_search("alpha", 10);
    ASSERT_EQ(r1.size(), r2.size());
    EXPECT_EQ(r1[0].symbol.name, r2[0].symbol.name);

    std::filesystem::remove_all(tmp_dir);
}

TEST_F(VectorIndexTest, PersistenceInvalidatedOnModelChange) {
    auto sem = make_file_semantics("src/a.meld", "foo");
    index_->index_file("src/a.meld", sem);

    auto tmp_dir = std::filesystem::temp_directory_path() / "meld_test_vidx2";
    std::filesystem::create_directories(tmp_dir);

    ASSERT_TRUE(index_->save(tmp_dir));

    // Load with a different-dimension provider → fingerprint mismatch
    auto other_provider = std::make_shared<TestEmbeddingProvider>(16);
    auto index2 = std::make_unique<VectorIndex>(other_provider);
    EXPECT_FALSE(index2->load(tmp_dir));
    EXPECT_EQ(index2->symbol_count(), 0u);

    std::filesystem::remove_all(tmp_dir);
}

// --- PassthroughEmbeddingProvider fallback ---

TEST_F(VectorIndexTest, PassthroughFallsBackToKeywordSearch) {
    auto pt_provider = std::make_shared<PassthroughEmbeddingProvider>(0);
    VectorIndex pt_index(pt_provider);

    EXPECT_FALSE(pt_index.is_semantic());

    auto sem = make_multi_symbol_file("src/lib.meld", {
        {"calculate_sum", "(List[Int]) -> Int"},
        {"parse_json", "(String) -> Json"},
    });
    pt_index.index_file("src/lib.meld", sem);

    // find_by_intent should fall back to keyword search
    auto results = pt_index.find_by_intent("calculate", 10);
    ASSERT_FALSE(results.empty());
    EXPECT_EQ(results[0].symbol.name, "calculate_sum");
}

TEST_F(VectorIndexTest, IsSemanticReturnsTrueForRealProvider) {
    EXPECT_TRUE(index_->is_semantic());
}

TEST_F(VectorIndexTest, IndexFileWithNullAstRemovesFile) {
    auto sem = make_file_semantics("src/a.meld", "foo");
    index_->index_file("src/a.meld", sem);
    EXPECT_EQ(index_->symbol_count(), 1u);

    FileSemantics empty_sem;
    empty_sem.path = "src/a.meld";
    empty_sem.ast = nullptr;
    index_->index_file("src/a.meld", empty_sem);
    EXPECT_EQ(index_->symbol_count(), 0u);
}

}  // namespace
}  // namespace meld::daemon
