#include <gtest/gtest.h>
#include "meld/daemon/analysis_engine.hpp"

namespace meld::lsp::analysis {

TEST(AnalysisEngineTest, AnalyzeEmptyContent) {
    AnalysisEngine engine;
    auto result = engine.analyze("file:///test.meld", "");
    EXPECT_EQ(result.uri, "file:///test.meld");
    EXPECT_TRUE(result.symbols.empty());
}

TEST(AnalysisEngineTest, AnalyzeReturnsUri) {
    AnalysisEngine engine;
    auto result = engine.analyze("file:///src/main.meld", "fnc main() {}");
    EXPECT_EQ(result.uri, "file:///src/main.meld");
}

TEST(AnalysisEngineTest, ResolveSymbolOnEmptyCache) {
    AnalysisEngine engine;
    auto sym = engine.resolve_symbol("file:///test.meld", 0, 0);
    EXPECT_FALSE(sym.has_value());
}

TEST(AnalysisEngineTest, GetDocumentSymbolsEmpty) {
    AnalysisEngine engine;
    auto symbols = engine.get_document_symbols("file:///test.meld");
    EXPECT_TRUE(symbols.empty());
}

TEST(AnalysisEngineTest, InvalidateClearsCache) {
    AnalysisEngine engine;
    engine.analyze("file:///test.meld", "fnc main() {}");
    engine.invalidate("file:///test.meld");
    auto symbols = engine.get_document_symbols("file:///test.meld");
    EXPECT_TRUE(symbols.empty());
}

TEST(AnalysisEngineTest, UpdateRefreshesCache) {
    AnalysisEngine engine;
    engine.update("file:///test.meld", "fnc main() {}");
    // After update, the URI should be cached
    auto result = engine.analyze("file:///test.meld", "");
    EXPECT_EQ(result.uri, "file:///test.meld");
}

// --- Content hash caching tests ---

TEST(AnalysisEngineTest, ContentHashSkipsReanalysis) {
    AnalysisEngine engine;
    std::string content = "fnc foo() {}\nfnc bar() {}";

    auto r1 = engine.analyze("file:///test.meld", content);
    EXPECT_EQ(r1.symbols.size(), 2u);

    // Same content should return cached result without re-parsing
    EXPECT_TRUE(engine.is_content_unchanged("file:///test.meld", content));

    auto r2 = engine.analyze("file:///test.meld", content);
    EXPECT_EQ(r2.symbols.size(), 2u);
    EXPECT_EQ(r2.uri, r1.uri);
}

TEST(AnalysisEngineTest, ContentHashDetectsChanges) {
    AnalysisEngine engine;
    engine.analyze("file:///test.meld", "fnc foo() {}");
    EXPECT_TRUE(engine.is_content_unchanged("file:///test.meld", "fnc foo() {}"));
    EXPECT_FALSE(engine.is_content_unchanged("file:///test.meld", "fnc bar() {}"));
}

TEST(AnalysisEngineTest, ContentHashUnknownUri) {
    AnalysisEngine engine;
    EXPECT_FALSE(engine.is_content_unchanged("file:///unknown.meld", "anything"));
}

// --- LRU cache eviction tests ---

TEST(AnalysisEngineTest, CacheSizeTracking) {
    AnalysisEngine engine;
    EXPECT_EQ(engine.cache_size(), 0u);
    engine.analyze("file:///a.meld", "fnc a() {}");
    EXPECT_EQ(engine.cache_size(), 1u);
    engine.analyze("file:///b.meld", "fnc b() {}");
    EXPECT_EQ(engine.cache_size(), 2u);
}

TEST(AnalysisEngineTest, DefaultMaxCacheSize) {
    AnalysisEngine engine;
    EXPECT_EQ(engine.max_cache_size(), 100u);
}

TEST(AnalysisEngineTest, LruEvictsOldestWhenFull) {
    AnalysisEngine engine;
    engine.set_max_cache_size(3);

    engine.analyze("file:///a.meld", "fnc a() {}");
    engine.analyze("file:///b.meld", "fnc b() {}");
    engine.analyze("file:///c.meld", "fnc c() {}");
    EXPECT_EQ(engine.cache_size(), 3u);

    // Adding a 4th should evict the least recently used (a)
    engine.analyze("file:///d.meld", "fnc d() {}");
    EXPECT_EQ(engine.cache_size(), 3u);

    // a should be evicted
    auto syms_a = engine.get_document_symbols("file:///a.meld");
    EXPECT_TRUE(syms_a.empty());

    // b, c, d should still be cached
    EXPECT_FALSE(engine.get_document_symbols("file:///b.meld").empty());
    EXPECT_FALSE(engine.get_document_symbols("file:///c.meld").empty());
    EXPECT_FALSE(engine.get_document_symbols("file:///d.meld").empty());
}

TEST(AnalysisEngineTest, LruAccessRefreshesOrder) {
    AnalysisEngine engine;
    engine.set_max_cache_size(3);

    engine.analyze("file:///a.meld", "fnc a() {}");
    engine.analyze("file:///b.meld", "fnc b() {}");
    engine.analyze("file:///c.meld", "fnc c() {}");

    // Access a to refresh it
    engine.get_document_symbols("file:///a.meld");

    // Now add d — b should be evicted (it's the LRU)
    engine.analyze("file:///d.meld", "fnc d() {}");
    EXPECT_EQ(engine.cache_size(), 3u);

    auto syms_b = engine.get_document_symbols("file:///b.meld");
    EXPECT_TRUE(syms_b.empty());

    // a should still be present
    EXPECT_FALSE(engine.get_document_symbols("file:///a.meld").empty());
}

TEST(AnalysisEngineTest, InvalidateCleansUpHashAndLru) {
    AnalysisEngine engine;
    engine.analyze("file:///test.meld", "fnc foo() {}");
    EXPECT_TRUE(engine.is_content_unchanged("file:///test.meld", "fnc foo() {}"));
    EXPECT_EQ(engine.cache_size(), 1u);

    engine.invalidate("file:///test.meld");
    EXPECT_EQ(engine.cache_size(), 0u);
    EXPECT_FALSE(engine.is_content_unchanged("file:///test.meld", "fnc foo() {}"));
    EXPECT_TRUE(engine.access_order().empty());
}

TEST(AnalysisEngineTest, AccessOrderTracking) {
    AnalysisEngine engine;
    engine.analyze("file:///a.meld", "fnc a() {}");
    engine.analyze("file:///b.meld", "fnc b() {}");
    engine.analyze("file:///c.meld", "fnc c() {}");

    auto order = engine.access_order();
    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], "file:///a.meld");
    EXPECT_EQ(order[2], "file:///c.meld");

    // Access a to move it to the back
    engine.resolve_symbol("file:///a.meld", 0, 4);
    order = engine.access_order();
    EXPECT_EQ(order[2], "file:///a.meld");
}

} // namespace meld::lsp::analysis
