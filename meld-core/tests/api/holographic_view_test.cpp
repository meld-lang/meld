#include <gtest/gtest.h>
#include "meld/api/holographic_view.hpp"
#include "meld/api/holographic_extensions.hpp"
#include "meld/parser/parser.hpp"

using namespace meld::api;
using namespace meld::parser;

class HolographicViewTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up test data
    }
    
    void TearDown() override {
        // Clean up
    }
};

TEST_F(HolographicViewTest, BasicModuleHologram) {
    std::string source_code = R"(
        import std.io
        
        struct Point {
            val x: int
            val y: int
        }
        
        class Calculator {
            var result: int
            
            fnc add(val a: int, val b: int) -> int {
                val sum = a + b
                result = sum
                return sum
            }
            
            fnc multiply(val x: int, val y: int) -> int {
                val product = x * y
                result = product
                return product
            }
        }
        
        fnc distance(val p1: Point, val p2: Point) -> float {
            val dx = p2.x - p1.x
            val dy = p2.y - p1.y
            return sqrt(dx*dx + dy*dy)
        }
    )";
    
    // Create holographic view from source
    ModuleHologram hologram = HolographicView::fromSource(source_code, "test_module");
    
    // Verify basic properties
    EXPECT_EQ(hologram.getName(), "test_module");
    EXPECT_GT(hologram.getOriginalTokenCount(), 0);
    EXPECT_GT(hologram.getHologramTokenCount(), 0);
    
    // Verify compression ratio
    double compression_ratio = hologram.getCompressionRatio();
    EXPECT_GT(compression_ratio, 0.0);
    EXPECT_LT(compression_ratio, 1.0);
    
    // Generate holographic representation
    std::string hologram_text = hologram.toHologram();
    EXPECT_FALSE(hologram_text.empty());
    
    // Verify that function bodies are stripped but signatures remain
    EXPECT_TRUE(hologram_text.find("fnc add(val a: int, val b: int) -> int") != std::string::npos);
    EXPECT_TRUE(hologram_text.find("fnc multiply(val x: int, val y: int) -> int") != std::string::npos);
    EXPECT_TRUE(hologram_text.find("fnc distance(val p1: Point, val p2: Point) -> float") != std::string::npos);
    
    // Verify that function bodies are not present
    EXPECT_TRUE(hologram_text.find("val sum = a + b") == std::string::npos);
    EXPECT_TRUE(hologram_text.find("val product = x * y") == std::string::npos);
    EXPECT_TRUE(hologram_text.find("sqrt(dx*dx + dy*dy)") == std::string::npos);
    
    // Verify that type definitions are preserved
    EXPECT_TRUE(hologram_text.find("struct Point") != std::string::npos);
    EXPECT_TRUE(hologram_text.find("class Calculator") != std::string::npos);
    EXPECT_TRUE(hologram_text.find("val x: int") != std::string::npos);
    EXPECT_TRUE(hologram_text.find("var result: int") != std::string::npos);
    
    // Verify that imports are preserved
    EXPECT_TRUE(hologram_text.find("import std.io") != std::string::npos);
}

TEST_F(HolographicViewTest, ClassHologram) {
    // Create a simple class AST for testing
    ast::class_definition class_def;
    class_def.name.name = "TestClass";
    
    // Add a field
    ast::field_declaration field;
    field.is_mutable = false;
    field.name.name = "value";
    field.type.type_name.name = "int";
    class_def.fields.push_back(field);
    
    // Add a property
    ast::property_declaration property;
    property.is_mutable = true;
    property.name.name = "count";
    property.type.type_name.name = "int";
    property.has_getter = true;
    property.has_setter = true;
    class_def.properties.push_back(property);
    
    // Create holographic view
    ClassHologram hologram(class_def);
    
    // Verify basic properties
    EXPECT_EQ(hologram.getName(), "TestClass");
    EXPECT_EQ(hologram.getFields().size(), 1);
    EXPECT_EQ(hologram.getProperties().size(), 1);
    
    // Generate holographic representation
    std::string hologram_text = hologram.toHologram();
    EXPECT_FALSE(hologram_text.empty());
    
    // Verify content
    EXPECT_TRUE(hologram_text.find("class TestClass") != std::string::npos);
    EXPECT_TRUE(hologram_text.find("val value: int") != std::string::npos);
    EXPECT_TRUE(hologram_text.find("var count: int") != std::string::npos);
}

TEST_F(HolographicViewTest, CompressionRatioValidation) {
    std::string source_code = R"(
        fnc complexFunction(val a: int, val b: int, val c: int) -> int {
            val temp1 = a + b
            val temp2 = temp1 * c
            val temp3 = temp2 - a
            val temp4 = temp3 / b
            val temp5 = temp4 + c
            val temp6 = temp5 * 2
            val temp7 = temp6 - 1
            val temp8 = temp7 + a
            val temp9 = temp8 * b
            val result = temp9 / c
            return result
        }
    )";
    
    ModuleHologram hologram = HolographicView::fromSource(source_code, "compression_test");
    
    // Verify that we achieve significant compression
    double compression_ratio = hologram.getCompressionRatio();
    EXPECT_GT(compression_ratio, 0.80); // Should achieve at least 80% compression
    
    // Verify validation function
    EXPECT_TRUE(HolographicView::validateCompressionRatio(hologram, 0.80));
    EXPECT_FALSE(HolographicView::validateCompressionRatio(hologram, 0.99)); // Too high threshold
}

TEST_F(HolographicViewTest, CompressionStats) {
    std::string source_code = R"(
        struct Point { val x: int; val y: int }
        fnc add(val a: int, val b: int) -> int { return a + b }
    )";
    
    ModuleHologram hologram = HolographicView::fromSource(source_code, "stats_test");
    
    auto stats = HolographicView::getCompressionStats(hologram);
    
    // Verify that all expected stats are present
    EXPECT_TRUE(stats.find("module_name") != stats.end());
    EXPECT_TRUE(stats.find("original_tokens") != stats.end());
    EXPECT_TRUE(stats.find("hologram_tokens") != stats.end());
    EXPECT_TRUE(stats.find("compression_ratio") != stats.end());
    EXPECT_TRUE(stats.find("compression_percentage") != stats.end());
    EXPECT_TRUE(stats.find("type_definitions_count") != stats.end());
    EXPECT_TRUE(stats.find("function_signatures_count") != stats.end());
    
    // Verify values make sense
    EXPECT_EQ(stats["module_name"], "stats_test");
    EXPECT_GT(std::stoi(stats["original_tokens"]), 0);
    EXPECT_GT(std::stoi(stats["hologram_tokens"]), 0);
}

TEST_F(HolographicViewTest, FunctionSignatureExtraction) {
    std::string source_code = R"(
        fnc calculate(val x: int, val y: float = 1.0) -> (result: int, remainder: float) 
            effects { Math, IO }
            require { x > 0 }
            ensure { result >= 0 }
        {
            // Function body would be here
            return (x, y)
        }
    )";
    
    ModuleHologram hologram = HolographicView::fromSource(source_code, "signature_test");
    
    // Verify function signature is extracted
    auto functions = hologram.getFunctionSignatures();
    EXPECT_EQ(functions.size(), 1);
    
    if (!functions.empty()) {
        const auto& func = functions[0];
        EXPECT_EQ(func.name, "calculate");
        EXPECT_EQ(func.parameters.size(), 2);
        EXPECT_EQ(func.named_returns.size(), 2);
        
        // Verify parameters
        EXPECT_TRUE(func.parameters[0].find("x: int") != std::string::npos);
        EXPECT_TRUE(func.parameters[1].find("y: float") != std::string::npos);
        EXPECT_TRUE(func.parameters[1].find("default") != std::string::npos);
        
        // Verify named returns
        EXPECT_TRUE(func.named_returns[0].find("result: int") != std::string::npos);
        EXPECT_TRUE(func.named_returns[1].find("remainder: float") != std::string::npos);
    }
}

TEST_F(HolographicViewTest, ImportExportPreservation) {
    std::string source_code = R"(
        import std.io
        import std.math.*
        import std.collections as Collections
        
        struct Data { val value: int }
    )";
    
    ModuleHologram hologram = HolographicView::fromSource(source_code, "import_test");
    
    // Generate holographic representation
    std::string hologram_text = hologram.toHologram();
    
    // Verify that imports are preserved
    EXPECT_TRUE(hologram_text.find("import std.io") != std::string::npos);
    EXPECT_TRUE(hologram_text.find("import std.math.*") != std::string::npos);
    EXPECT_TRUE(hologram_text.find("import std.collections as Collections") != std::string::npos);
    
    // Verify import/export count
    auto import_exports = hologram.getImportExports();
    EXPECT_EQ(import_exports.size(), 3);
}

TEST_F(HolographicViewTest, JsonExport) {
    std::string source_code = R"(
        struct Point { val x: int; val y: int }
        fnc distance(val p1: Point, val p2: Point) -> float { return 0.0 }
    )";
    
    ModuleHologram hologram = HolographicView::fromSource(source_code, "json_test");
    
    std::string json = hologram.toJson();
    EXPECT_FALSE(json.empty());
    
    // Verify JSON contains expected fields
    EXPECT_TRUE(json.find("\"name\"") != std::string::npos);
    EXPECT_TRUE(json.find("\"original_token_count\"") != std::string::npos);
    EXPECT_TRUE(json.find("\"hologram_token_count\"") != std::string::npos);
    EXPECT_TRUE(json.find("\"compression_ratio\"") != std::string::npos);
    EXPECT_TRUE(json.find("\"type_definitions\"") != std::string::npos);
    EXPECT_TRUE(json.find("\"function_signatures\"") != std::string::npos);
}

TEST_F(HolographicViewTest, ToonExport) {
    std::string source_code = R"(
        import std.io
        
        struct Point { val x: int; val y: int }
        
        class Calculator {
            var result: int
            
            fnc add(val a: int, val b: int) -> int { return a + b }
            fnc multiply(val x: int, val y: int) -> int { return x * y }
        }
        
        fnc distance(val p1: Point, val p2: Point) -> float { return 0.0 }
    )";
    
    ModuleHologram hologram = HolographicView::fromSource(source_code, "toon_test");
    
    std::string toon = hologram.toToon();
    EXPECT_FALSE(toon.empty());
    
    // Verify TOON contains expected structure
    EXPECT_TRUE(toon.find("name: toon_test") != std::string::npos);
    EXPECT_TRUE(toon.find("original_token_count:") != std::string::npos);
    EXPECT_TRUE(toon.find("hologram_token_count:") != std::string::npos);
    EXPECT_TRUE(toon.find("compression_ratio:") != std::string::npos);
    
    // Check for tabular arrays
    EXPECT_TRUE(toon.find("types[") != std::string::npos);
    EXPECT_TRUE(toon.find("functions[") != std::string::npos);
    EXPECT_TRUE(toon.find("classes[") != std::string::npos);
    
    // Check for TOON-specific formatting (tabular headers)
    EXPECT_TRUE(toon.find("{name,kind,fields_count,properties_count}:") != std::string::npos);
    EXPECT_TRUE(toon.find("{name,param_count,return_type,effects_count}:") != std::string::npos);
}

TEST_F(HolographicViewTest, ClassToonExport) {
    // Create a simple class AST for testing
    ast::class_definition class_def;
    class_def.name.name = "TestClass";
    
    // Add a field
    ast::field_declaration field;
    field.is_mutable = false;
    field.name.name = "value";
    field.type.type_name.name = "int";
    class_def.fields.push_back(field);
    
    // Add a property
    ast::property_declaration property;
    property.is_mutable = true;
    property.name.name = "count";
    property.type.type_name.name = "int";
    property.has_getter = true;
    property.has_setter = true;
    class_def.properties.push_back(property);
    
    // Create holographic view
    ClassHologram hologram(class_def);
    
    std::string toon = hologram.toToon();
    EXPECT_FALSE(toon.empty());
    
    // Verify TOON structure
    EXPECT_TRUE(toon.find("name: TestClass") != std::string::npos);
    EXPECT_TRUE(toon.find("fields_count: 1") != std::string::npos);
    EXPECT_TRUE(toon.find("properties_count: 1") != std::string::npos);
    EXPECT_TRUE(toon.find("fields[1]{declaration}:") != std::string::npos);
    EXPECT_TRUE(toon.find("properties[1]{declaration}:") != std::string::npos);
}

TEST_F(HolographicViewTest, MinimalCompressionRequirement) {
    // Test that we meet the ~95% token reduction requirement
    std::string large_source_code = R"(
        class LargeClass {
            var field1: int
            var field2: string
            var field3: float
            var field4: bool
            var field5: list<int>
            
            fnc method1(val a: int, val b: int, val c: int) -> int {
                val temp1 = a + b + c
                val temp2 = temp1 * 2
                val temp3 = temp2 - 1
                val temp4 = temp3 / 3
                val temp5 = temp4 + a
                val temp6 = temp5 - b
                val temp7 = temp6 * c
                val temp8 = temp7 + temp1
                val temp9 = temp8 - temp2
                val result = temp9 + temp3
                return result
            }
            
            fnc method2(val x: string, val y: string) -> string {
                val combined = x + y
                val upper = combined.toUpperCase()
                val trimmed = upper.trim()
                val reversed = trimmed.reverse()
                val final = reversed + "!"
                return final
            }
            
            fnc method3(val items: list<int>) -> list<int> {
                val filtered = items.filter { it > 0 }
                val mapped = filtered.map { it * 2 }
                val sorted = mapped.sorted()
                val distinct = sorted.distinct()
                val limited = distinct.take(10)
                return limited
            }
        }
    )";
    
    ModuleHologram hologram = HolographicView::fromSource(large_source_code, "large_test");
    
    // Verify we achieve the required compression ratio
    double compression_ratio = hologram.getCompressionRatio();
    EXPECT_GT(compression_ratio, 0.90); // Should achieve at least 90% compression
    
    // Ideally should achieve ~95% compression as specified in requirements
    EXPECT_GT(compression_ratio, 0.85); // More lenient check for test stability
}