#include <gtest/gtest.h>
#include "meld/compiler/codegen.hpp"
#include "meld/std/numeric_wrappers.hpp"
#include "meld/meta/metatype.hpp"

using namespace meld::compiler;
using namespace meld::stdx;
using namespace meld::meta;

class TranspilerMappingsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Register numeric wrapper types
        NumericWrappers::register_all_types();
    }
};

TEST_F(TranspilerMappingsTest, TestU8MappingToCpp) {
    CodeGenerator generator(CodeGenTarget::CPP);
    
    // Get u8 type from registry
    auto& registry = TypeRegistry::instance();
    auto u8_type_result = registry.get_type("u8");
    ASSERT_TRUE(u8_type_result.has_value());
    
    auto u8_type = u8_type_result.value();
    
    // Test that u8 maps to uint8_t in C++
    std::string cpp_type = generator.map_transpiler_annotation(u8_type, CodeGenTarget::CPP);
    EXPECT_EQ(cpp_type, "uint8_t");
}

TEST_F(TranspilerMappingsTest, TestU8MappingToJava) {
    CodeGenerator generator(CodeGenTarget::JVM);
    
    // Get u8 type from registry
    auto& registry = TypeRegistry::instance();
    auto u8_type_result = registry.get_type("u8");
    ASSERT_TRUE(u8_type_result.has_value());
    
    auto u8_type = u8_type_result.value();
    
    // Test that u8 maps to byte in Java
    std::string java_type = generator.map_transpiler_annotation(u8_type, CodeGenTarget::JVM);
    EXPECT_EQ(java_type, "byte");
}

TEST_F(TranspilerMappingsTest, TestU8MappingToGo) {
    CodeGenerator generator(CodeGenTarget::Go);
    
    // Get u8 type from registry
    auto& registry = TypeRegistry::instance();
    auto u8_type_result = registry.get_type("u8");
    ASSERT_TRUE(u8_type_result.has_value());
    
    auto u8_type = u8_type_result.value();
    
    // Test that u8 maps to uint8 in Go
    std::string go_type = generator.map_transpiler_annotation(u8_type, CodeGenTarget::Go);
    EXPECT_EQ(go_type, "uint8");
}

TEST_F(TranspilerMappingsTest, TestI8MappingToCpp) {
    CodeGenerator generator(CodeGenTarget::CPP);
    
    // Get i8 type from registry
    auto& registry = TypeRegistry::instance();
    auto i8_type_result = registry.get_type("i8");
    ASSERT_TRUE(i8_type_result.has_value());
    
    auto i8_type = i8_type_result.value();
    
    // Test that i8 maps to int8_t in C++
    std::string cpp_type = generator.map_transpiler_annotation(i8_type, CodeGenTarget::CPP);
    EXPECT_EQ(cpp_type, "int8_t");
}

TEST_F(TranspilerMappingsTest, TestI8MappingToJava) {
    CodeGenerator generator(CodeGenTarget::JVM);
    
    // Get i8 type from registry
    auto& registry = TypeRegistry::instance();
    auto i8_type_result = registry.get_type("i8");
    ASSERT_TRUE(i8_type_result.has_value());
    
    auto i8_type = i8_type_result.value();
    
    // Test that i8 maps to byte in Java
    std::string java_type = generator.map_transpiler_annotation(i8_type, CodeGenTarget::JVM);
    EXPECT_EQ(java_type, "byte");
}

TEST_F(TranspilerMappingsTest, TestI8MappingToGo) {
    CodeGenerator generator(CodeGenTarget::Go);
    
    // Get i8 type from registry
    auto& registry = TypeRegistry::instance();
    auto i8_type_result = registry.get_type("i8");
    ASSERT_TRUE(i8_type_result.has_value());
    
    auto i8_type = i8_type_result.value();
    
    // Test that i8 maps to int8 in Go
    std::string go_type = generator.map_transpiler_annotation(i8_type, CodeGenTarget::Go);
    EXPECT_EQ(go_type, "int8");
}

TEST_F(TranspilerMappingsTest, TestI16MappingToCpp) {
    CodeGenerator generator(CodeGenTarget::CPP);
    
    // Get i16 type from registry
    auto& registry = TypeRegistry::instance();
    auto i16_type_result = registry.get_type("i16");
    ASSERT_TRUE(i16_type_result.has_value());
    
    auto i16_type = i16_type_result.value();
    
    // Test that i16 maps to int16_t in C++
    std::string cpp_type = generator.map_transpiler_annotation(i16_type, CodeGenTarget::CPP);
    EXPECT_EQ(cpp_type, "int16_t");
}

TEST_F(TranspilerMappingsTest, TestI16MappingToJava) {
    CodeGenerator generator(CodeGenTarget::JVM);
    
    // Get i16 type from registry
    auto& registry = TypeRegistry::instance();
    auto i16_type_result = registry.get_type("i16");
    ASSERT_TRUE(i16_type_result.has_value());
    
    auto i16_type = i16_type_result.value();
    
    // Test that i16 maps to short in Java
    std::string java_type = generator.map_transpiler_annotation(i16_type, CodeGenTarget::JVM);
    EXPECT_EQ(java_type, "short");
}

TEST_F(TranspilerMappingsTest, TestI16MappingToGo) {
    CodeGenerator generator(CodeGenTarget::Go);
    
    // Get i16 type from registry
    auto& registry = TypeRegistry::instance();
    auto i16_type_result = registry.get_type("i16");
    ASSERT_TRUE(i16_type_result.has_value());
    
    auto i16_type = i16_type_result.value();
    
    // Test that i16 maps to int16 in Go
    std::string go_type = generator.map_transpiler_annotation(i16_type, CodeGenTarget::Go);
    EXPECT_EQ(go_type, "int16");
}

TEST_F(TranspilerMappingsTest, TestCharMappingToCpp) {
    CodeGenerator generator(CodeGenTarget::CPP);
    
    // Get char type from registry
    auto& registry = TypeRegistry::instance();
    auto char_type_result = registry.get_type("char");
    ASSERT_TRUE(char_type_result.has_value());
    
    auto char_type = char_type_result.value();
    
    // Test that char maps to char32_t in C++
    std::string cpp_type = generator.map_transpiler_annotation(char_type, CodeGenTarget::CPP);
    EXPECT_EQ(cpp_type, "char32_t");
}

TEST_F(TranspilerMappingsTest, TestCharMappingToJava) {
    CodeGenerator generator(CodeGenTarget::JVM);
    
    // Get char type from registry
    auto& registry = TypeRegistry::instance();
    auto char_type_result = registry.get_type("char");
    ASSERT_TRUE(char_type_result.has_value());
    
    auto char_type = char_type_result.value();
    
    // Test that char maps to int in Java (for Unicode code points)
    std::string java_type = generator.map_transpiler_annotation(char_type, CodeGenTarget::JVM);
    EXPECT_EQ(java_type, "int");
}

TEST_F(TranspilerMappingsTest, TestCharMappingToGo) {
    CodeGenerator generator(CodeGenTarget::Go);
    
    // Get char type from registry
    auto& registry = TypeRegistry::instance();
    auto char_type_result = registry.get_type("char");
    ASSERT_TRUE(char_type_result.has_value());
    
    auto char_type = char_type_result.value();
    
    // Test that char maps to rune in Go
    std::string go_type = generator.map_transpiler_annotation(char_type, CodeGenTarget::Go);
    EXPECT_EQ(go_type, "rune");
}

TEST_F(TranspilerMappingsTest, TestFallbackMappings) {
    CodeGenerator generator(CodeGenTarget::CPP);
    
    // Test fallback mappings for types without @transpile_as annotations
    EXPECT_EQ(generator.cpp_type_name_fallback("int"), "int64_t");
    EXPECT_EQ(generator.cpp_type_name_fallback("float"), "double");
    EXPECT_EQ(generator.cpp_type_name_fallback("bool"), "bool");
    EXPECT_EQ(generator.cpp_type_name_fallback("string"), "std::string");
    EXPECT_EQ(generator.cpp_type_name_fallback("void"), "void");
    
    // Test Java fallbacks
    EXPECT_EQ(generator.java_type_name_fallback("int"), "long");
    EXPECT_EQ(generator.java_type_name_fallback("float"), "double");
    EXPECT_EQ(generator.java_type_name_fallback("bool"), "boolean");
    EXPECT_EQ(generator.java_type_name_fallback("string"), "String");
    
    // Test Go fallbacks
    EXPECT_EQ(generator.go_type_name_fallback("int"), "int64");
    EXPECT_EQ(generator.go_type_name_fallback("float"), "float64");
    EXPECT_EQ(generator.go_type_name_fallback("bool"), "bool");
    EXPECT_EQ(generator.go_type_name_fallback("string"), "string");
}

TEST_F(TranspilerMappingsTest, TestZeroCostAbstractions) {
    // Test that wrapper types have the same size as their underlying types
    auto& registry = TypeRegistry::instance();
    
    auto u8_type_result = registry.get_type("u8");
    ASSERT_TRUE(u8_type_result.has_value());
    auto u8_type = u8_type_result.value();
    
    auto int_type = registry.get_int_type();
    
    // u8 should have the same size as int (since it wraps int)
    // In a full implementation, the transpiler would map this to native uint8_t
    // which would be smaller, but at the Meld level it's the same size
    EXPECT_EQ(u8_type->size(), int_type->size());
    
    // Verify that @transpile_as annotation exists
    EXPECT_TRUE(u8_type->has_annotation("transpile_as"));
    
    // Verify that @value annotation exists (for copy-by-value semantics)
    EXPECT_TRUE(u8_type->has_annotation("value"));
}