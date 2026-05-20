#include "../../include/meld/compiler/cap.hpp"
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

using namespace meld::compiler::cap;

class CAPMeldBTest : public ::testing::Test {
protected:
    void SetUp() override {
        cap = std::make_unique<CompilerAgentProtocol>();
        cap->set_generate_meld_b(true);
        cap->set_include_effect_info(true);
        cap->set_enable_asg_queries(true);
        
        // Create test source file
        test_file = "test_source.meld";
        std::ofstream file(test_file);
        file << "fnc test_function() {\n";
        file << "    val x = 42\n";
        file << "    rtn x\n";
        file << "}\n";
        file.close();
    }
    
    void TearDown() override {
        // Clean up test files
        if (std::filesystem::exists(test_file)) {
            std::filesystem::remove(test_file);
        }
        
        std::string meld_b_file = test_file;
        meld_b_file.replace(meld_b_file.end() - 5, meld_b_file.end(), ".mldb");
        if (std::filesystem::exists(meld_b_file)) {
            std::filesystem::remove(meld_b_file);
        }
    }
    
    std::unique_ptr<CompilerAgentProtocol> cap;
    std::string test_file;
};

TEST_F(CAPMeldBTest, CompileToMeldBGeneratesFile) {
    auto result = cap->compile_to_meld_b(test_file);
    
    // Should have MELD-B file path in result
    EXPECT_TRUE(result.meld_b_file.has_value());
    
    if (result.meld_b_file) {
        EXPECT_TRUE(result.meld_b_file->ends_with(".mldb"));
    }
}

TEST_F(CAPMeldBTest, EffectInfoIncludedInResult) {
    auto result = cap->compile_file(test_file);
    
    // Should include effect information in compilation result
    EXPECT_FALSE(result.effect_information.empty());
    
    // Check effect info structure
    for (const auto& effect_info : result.effect_information) {
        EXPECT_FALSE(effect_info.function_name.empty());
        EXPECT_FALSE(effect_info.effect_annotation.empty());
    }
}

TEST_F(CAPMeldBTest, ASGQueriesWork) {
    // First compile to build the semantic graph
    auto result = cap->compile_file(test_file);
    
    // Test symbol query
    auto symbol_query = cap->query_asg("test_function", "symbol");
    EXPECT_EQ(symbol_query.query_type, "symbol");
    EXPECT_EQ(symbol_query.pattern, "test_function");
    EXPECT_GE(symbol_query.query_time.count(), 0);
    
    // Test usage query
    auto usage_query = cap->query_usage("test_function");
    EXPECT_EQ(usage_query.query_type, "usage");
    EXPECT_EQ(usage_query.pattern, "test_function");
    
    // Test effect query
    auto effect_query = cap->query_by_effect("FileSystem");
    EXPECT_EQ(effect_query.query_type, "effect");
    EXPECT_EQ(effect_query.pattern, "FileSystem");
}

TEST_F(CAPMeldBTest, BatchASGQueries) {
    // First compile to build the semantic graph
    auto result = cap->compile_file(test_file);
    
    std::vector<std::pair<std::string, std::string>> queries = {
        {"test_function", "symbol"},
        {"x", "variable"},
        {"function", "function"}
    };
    
    auto results = cap->batch_query_asg(queries);
    
    EXPECT_EQ(results.size(), 3);
    EXPECT_EQ(results[0].query_type, "symbol");
    EXPECT_EQ(results[1].query_type, "variable");
    EXPECT_EQ(results[2].query_type, "function");
}

TEST_F(CAPMeldBTest, JSONSerializationWorks) {
    auto result = cap->compile_file(test_file);
    
    // Test CompilationResult JSON serialization
    auto json_str = CompilerAgentProtocol::to_json_string(result, true);
    EXPECT_FALSE(json_str.empty());
    EXPECT_TRUE(json_str.find("effect_information") != std::string::npos);
    
    // Test ASGQueryResult JSON serialization
    auto query_result = cap->query_asg("test", "symbol");
    auto query_json = CompilerAgentProtocol::to_json_string(query_result, true);
    EXPECT_FALSE(query_json.empty());
    EXPECT_TRUE(query_json.find("query_type") != std::string::npos);
    
    // Test EffectInfo JSON serialization
    if (!result.effect_information.empty()) {
        auto effect_json = CompilerAgentProtocol::to_json_string(result.effect_information[0], true);
        EXPECT_FALSE(effect_json.empty());
        EXPECT_TRUE(effect_json.find("function_name") != std::string::npos);
    }
}

TEST_F(CAPMeldBTest, ConfigurationOptionsWork) {
    // Test configuration setters
    cap->set_generate_meld_b(false);
    cap->set_include_effect_info(false);
    cap->set_enable_asg_queries(false);
    
    auto result = cap->compile_file(test_file);
    
    // Should not have MELD-B file when disabled
    EXPECT_FALSE(result.meld_b_file.has_value());
    
    // Should not have effect information when disabled
    EXPECT_TRUE(result.effect_information.empty());
    
    // ASG queries should return empty results when disabled
    auto query_result = cap->query_asg("test", "symbol");
    EXPECT_TRUE(query_result.node_descriptions.size() > 0);
    EXPECT_TRUE(query_result.node_descriptions[0].find("not enabled") != std::string::npos);
}