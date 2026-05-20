#include <gtest/gtest.h>
#include "meld/cli/package_module.hpp"
#include "meld/cli/error_handler.hpp"
#include "meld/testing/property_test.hpp"
#include <memory>
#include <filesystem>
#include <random>
#include <algorithm>

/**
 * Feature: meld-cli, Property 29: Package Search Relevance
 * Validates: Requirements 9.3
 * 
 * Property: For any search query, returned packages should be relevant to the query terms 
 * and ranked by relevance
 */

namespace meld::cli::test {

class PackageSearchRelevancePropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        error_handler_ = std::make_shared<ErrorHandler>();
        package_module_ = std::make_unique<PackageModule>(error_handler_);
    }
    
    std::shared_ptr<ErrorHandler> error_handler_;
    std::unique_ptr<PackageModule> package_module_;
};

// Generator for search queries
class SearchQueryGenerator {
public:
    static std::string generate(std::mt19937& rng) {
        std::vector<std::string> common_terms = {
            "json", "http", "test", "log", "crypto", "string", "math", "network",
            "parser", "client", "server", "framework", "library", "utils", "tools",
            "database", "cache", "queue", "stream", "file", "io", "async", "sync"
        };
        
        std::uniform_int_distribution<size_t> term_dist(0, common_terms.size() - 1);
        std::uniform_int_distribution<int> num_terms_dist(1, 3);
        
        int num_terms = num_terms_dist(rng);
        std::string query;
        
        for (int i = 0; i < num_terms; ++i) {
            if (i > 0) query += " ";
            query += common_terms[term_dist(rng)];
        }
        
        return query;
    }
};

// Helper function to check if a result is relevant to a query
bool is_relevant_to_query(const PackageSearchResult& result, const std::string& query) {
    std::string query_lower = query;
    std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(), ::tolower);
    
    std::string name_lower = result.name;
    std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
    
    std::string desc_lower = result.description;
    std::transform(desc_lower.begin(), desc_lower.end(), desc_lower.begin(), ::tolower);
    
    // Check if query appears in name or description
    if (name_lower.find(query_lower) != std::string::npos) {
        return true;
    }
    
    if (desc_lower.find(query_lower) != std::string::npos) {
        return true;
    }
    
    // Check if query appears in keywords
    for (const auto& keyword : result.keywords) {
        std::string keyword_lower = keyword;
        std::transform(keyword_lower.begin(), keyword_lower.end(), keyword_lower.begin(), ::tolower);
        
        if (keyword_lower.find(query_lower) != std::string::npos) {
            return true;
        }
    }
    
    // Check if any word in query matches
    std::istringstream iss(query_lower);
    std::string word;
    while (iss >> word) {
        if (name_lower.find(word) != std::string::npos ||
            desc_lower.find(word) != std::string::npos) {
            return true;
        }
        
        for (const auto& keyword : result.keywords) {
            std::string keyword_lower = keyword;
            std::transform(keyword_lower.begin(), keyword_lower.end(), keyword_lower.begin(), ::tolower);
            
            if (keyword_lower.find(word) != std::string::npos) {
                return true;
            }
        }
    }
    
    return false;
}

// Property test: Search results relevance
TEST_F(PackageSearchRelevancePropertyTest, SearchResultsRelevance) {
    meld::testing::PropertyTest property_test;
    
    property_test.run_property_test(100, [this](std::mt19937& rng) {
        // Generate search query
        std::string query = SearchQueryGenerator::generate(rng);
        
        // Perform search
        auto results = package_module_->search_packages(query);
        
        // Property: All returned results should be relevant to the query
        for (const auto& result : results) {
            bool is_relevant = is_relevant_to_query(result, query);
            
            EXPECT_TRUE(is_relevant) 
                << "Search result '" << result.name << "' should be relevant to query '" << query << "'";
            
            // Property: Each result should have a relevance score
            EXPECT_GE(result.relevance_score, 0.0) 
                << "Relevance score should be non-negative";
        }
        
        return true;
    });
}

// Property test: Search results ranking
TEST_F(PackageSearchRelevancePropertyTest, SearchResultsRanking) {
    meld::testing::PropertyTest property_test;
    
    property_test.run_property_test(100, [this](std::mt19937& rng) {
        // Generate search query
        std::string query = SearchQueryGenerator::generate(rng);
        
        // Perform search
        auto results = package_module_->search_packages(query);
        
        if (results.size() > 1) {
            // Property: Results should be ranked by relevance (descending order)
            for (size_t i = 0; i < results.size() - 1; ++i) {
                EXPECT_GE(results[i].relevance_score, results[i + 1].relevance_score)
                    << "Results should be ranked by relevance score (descending) for query: " << query
                    << "\n  Result " << i << " (" << results[i].name << "): " << results[i].relevance_score
                    << "\n  Result " << i + 1 << " (" << results[i + 1].name << "): " << results[i + 1].relevance_score;
            }
        }
        
        return true;
    });
}

// Property test: Exact match ranking
TEST_F(PackageSearchRelevancePropertyTest, ExactMatchRanking) {
    meld::testing::PropertyTest property_test;
    
    property_test.run_property_test(50, [this](std::mt19937& rng) {
        // Use known package names for exact match testing
        std::vector<std::string> known_packages = {
            "json-parser", "http-client", "test-framework", "logging-lib"
        };
        
        std::uniform_int_distribution<size_t> pkg_dist(0, known_packages.size() - 1);
        std::string query = known_packages[pkg_dist(rng)];
        
        // Perform search
        auto results = package_module_->search_packages(query);
        
        if (!results.empty()) {
            // Property: Exact name match should have highest relevance score
            bool found_exact_match = false;
            double max_score = -1.0;
            
            for (const auto& result : results) {
                if (result.name == query) {
                    found_exact_match = true;
                    
                    // Property: Exact match should have highest score
                    EXPECT_GE(result.relevance_score, max_score)
                        << "Exact match '" << query << "' should have highest relevance score";
                }
                
                max_score = std::max(max_score, result.relevance_score);
            }
            
            // If exact match exists, it should be first in results
            if (found_exact_match) {
                EXPECT_EQ(results[0].name, query)
                    << "Exact match should be first result for query: " << query;
            }
        }
        
        return true;
    });
}

// Property test: Empty query handling
TEST_F(PackageSearchRelevancePropertyTest, EmptyQueryHandling) {
    // Test with empty query
    auto results = package_module_->search_packages("");
    
    // Property: Empty query should return results (or empty list, both are valid)
    // The key is that it should not crash or throw exceptions
    EXPECT_NO_THROW({
        auto results = package_module_->search_packages("");
    });
    
    // If results are returned, they should still be properly formatted
    for (const auto& result : results) {
        EXPECT_FALSE(result.name.empty()) << "Result name should not be empty";
        EXPECT_GE(result.relevance_score, 0.0) << "Relevance score should be non-negative";
    }
}

// Property test: Multi-word query relevance
TEST_F(PackageSearchRelevancePropertyTest, MultiWordQueryRelevance) {
    meld::testing::PropertyTest property_test;
    
    property_test.run_property_test(50, [this](std::mt19937& rng) {
        // Generate multi-word query
        std::vector<std::string> words = {"json", "http", "test", "parser", "client"};
        std::uniform_int_distribution<size_t> word_dist(0, words.size() - 1);
        
        std::string query = words[word_dist(rng)] + " " + words[word_dist(rng)];
        
        // Perform search
        auto results = package_module_->search_packages(query);
        
        // Property: Results matching multiple query terms should rank higher
        if (results.size() > 1) {
            // Count how many query terms each result matches
            std::vector<int> match_counts;
            
            for (const auto& result : results) {
                int match_count = 0;
                std::istringstream iss(query);
                std::string word;
                
                while (iss >> word) {
                    std::string name_lower = result.name;
                    std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
                    
                    std::string word_lower = word;
                    std::transform(word_lower.begin(), word_lower.end(), word_lower.begin(), ::tolower);
                    
                    if (name_lower.find(word_lower) != std::string::npos) {
                        match_count++;
                    }
                }
                
                match_counts.push_back(match_count);
            }
            
            // Property: Results with more matches should generally rank higher
            // (This is a soft property - there may be exceptions based on other factors)
            for (size_t i = 0; i < results.size() - 1; ++i) {
                if (match_counts[i] > match_counts[i + 1]) {
                    // If result i has more matches, it should have higher or equal score
                    EXPECT_GE(results[i].relevance_score, results[i + 1].relevance_score)
                        << "Result with more query term matches should rank higher or equal";
                }
            }
        }
        
        return true;
    });
}

// Property test: Search consistency
TEST_F(PackageSearchRelevancePropertyTest, SearchConsistency) {
    meld::testing::PropertyTest property_test;
    
    property_test.run_property_test(30, [this](std::mt19937& rng) {
        // Generate search query
        std::string query = SearchQueryGenerator::generate(rng);
        
        // Perform search twice
        auto results1 = package_module_->search_packages(query);
        auto results2 = package_module_->search_packages(query);
        
        // Property: Same query should return consistent results
        EXPECT_EQ(results1.size(), results2.size())
            << "Same query should return same number of results";
        
        if (results1.size() == results2.size()) {
            for (size_t i = 0; i < results1.size(); ++i) {
                EXPECT_EQ(results1[i].name, results2[i].name)
                    << "Same query should return results in same order";
                EXPECT_DOUBLE_EQ(results1[i].relevance_score, results2[i].relevance_score)
                    << "Same query should return same relevance scores";
            }
        }
        
        return true;
    });
}

// Property test: Case insensitivity
TEST_F(PackageSearchRelevancePropertyTest, CaseInsensitivity) {
    meld::testing::PropertyTest property_test;
    
    property_test.run_property_test(30, [this](std::mt19937& rng) {
        // Generate search query
        std::string query = SearchQueryGenerator::generate(rng);
        
        // Create variations with different cases
        std::string query_lower = query;
        std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(), ::tolower);
        
        std::string query_upper = query;
        std::transform(query_upper.begin(), query_upper.end(), query_upper.begin(), ::toupper);
        
        // Perform searches
        auto results_lower = package_module_->search_packages(query_lower);
        auto results_upper = package_module_->search_packages(query_upper);
        
        // Property: Search should be case-insensitive (return same results)
        EXPECT_EQ(results_lower.size(), results_upper.size())
            << "Case variations should return same number of results";
        
        if (results_lower.size() == results_upper.size()) {
            for (size_t i = 0; i < results_lower.size(); ++i) {
                EXPECT_EQ(results_lower[i].name, results_upper[i].name)
                    << "Case variations should return same results";
            }
        }
        
        return true;
    });
}

} // namespace meld::cli::test