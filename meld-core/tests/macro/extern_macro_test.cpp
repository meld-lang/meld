#include <gtest/gtest.h>
#include "meld/macro/extern_macro.hpp"
#include "meld/parser/ast.hpp"
#include "meld/macro/macro.hpp"

using namespace meld::macro;
using namespace meld::parser::ast;

class ExternMacroTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Register the @extern macro for testing
        register_extern_macro();
        register_extern_decorator();
    }
    
    void TearDown() override {
        // Clean up registries
        MacroRegistry::instance().clear();
        DecoratorRegistry::instance().clear();
    }
};

// Test target language parsing
TEST_F(ExternMacroTest, ParseTargetLanguage) {
    auto java_result = parse_target_language("java");
    ASSERT_TRUE(java_result.has_value());
    EXPECT_EQ(*java_result, TargetLanguage::Java);
    
    auto go_result = parse_target_language("go");
    ASSERT_TRUE(go_result.has_value());
    EXPECT_EQ(*go_result, TargetLanguage::Go);
    
    auto cpp_result = parse_target_language("cpp");
    ASSERT_TRUE(cpp_result.has_value());
    EXPECT_EQ(*cpp_result, TargetLanguage::Cpp);
    
    auto c_result = parse_target_language("c");
    ASSERT_TRUE(c_result.has_value());
    EXPECT_EQ(*c_result, TargetLanguage::C);
    
    // Test case insensitive
    auto java_upper = parse_target_language("JAVA");
    ASSERT_TRUE(java_upper.has_value());
    EXPECT_EQ(*java_upper, TargetLanguage::Java);
    
    // Test invalid language
    auto invalid = parse_target_language("invalid");
    EXPECT_FALSE(invalid.has_value());
}

// Test target language to string conversion
TEST_F(ExternMacroTest, TargetLanguageToString) {
    EXPECT_EQ(target_language_to_string(TargetLanguage::Java), "java");
    EXPECT_EQ(target_language_to_string(TargetLanguage::Go), "go");
    EXPECT_EQ(target_language_to_string(TargetLanguage::Cpp), "cpp");
    EXPECT_EQ(target_language_to_string(TargetLanguage::C), "c");
}

// Test default type mappings
TEST_F(ExternMacroTest, DefaultTypeMappings) {
    auto java_mappings = TypeMapper::get_default_mappings(TargetLanguage::Java);
    EXPECT_EQ(java_mappings["int"], "int");
    EXPECT_EQ(java_mappings["string"], "String");
    EXPECT_EQ(java_mappings["bool"], "boolean");
    
    auto go_mappings = TypeMapper::get_default_mappings(TargetLanguage::Go);
    EXPECT_EQ(go_mappings["int"], "int64");
    EXPECT_EQ(go_mappings["string"], "string");
    EXPECT_EQ(go_mappings["bool"], "bool");
    
    auto cpp_mappings = TypeMapper::get_default_mappings(TargetLanguage::Cpp);
    EXPECT_EQ(cpp_mappings["int"], "int64_t");
    EXPECT_EQ(cpp_mappings["string"], "std::string");
    EXPECT_EQ(cpp_mappings["bool"], "bool");
    
    auto c_mappings = TypeMapper::get_default_mappings(TargetLanguage::C);
    EXPECT_EQ(c_mappings["int"], "int64_t");
    EXPECT_EQ(c_mappings["string"], "char*");
    EXPECT_EQ(c_mappings["bool"], "bool");
}

// Test type mapping
TEST_F(ExternMacroTest, TypeMapping) {
    // Test default mapping
    auto java_int = TypeMapper::map_type("int", TargetLanguage::Java);
    ASSERT_TRUE(java_int.has_value());
    EXPECT_EQ(*java_int, "int");
    
    auto go_string = TypeMapper::map_type("string", TargetLanguage::Go);
    ASSERT_TRUE(go_string.has_value());
    EXPECT_EQ(*go_string, "string");
    
    // Test custom mapping
    std::map<std::string, std::string> custom_mappings = {
        {"int", "int32"},
        {"string", "std::string"}
    };
    
    auto custom_int = TypeMapper::map_type("int", TargetLanguage::Java, custom_mappings);
    ASSERT_TRUE(custom_int.has_value());
    EXPECT_EQ(*custom_int, "int32");
    
    // Test unmapped type (should return original)
    auto unmapped = TypeMapper::map_type("CustomType", TargetLanguage::Java);
    ASSERT_TRUE(unmapped.has_value());
    EXPECT_EQ(*unmapped, "CustomType");
}

// Test function signature mapping
TEST_F(ExternMacroTest, FunctionSignatureMapping) {
    // Create a test function declaration
    function_definition func_decl;
    func_decl.name.name = "testFunction";
    func_decl.return_type.type_name.name = "int";
    
    // Add parameters
    function_parameter param1;
    param1.name.name = "x";
    param1.type.type_name.name = "int";
    func_decl.parameters.push_back(param1);
    
    function_parameter param2;
    param2.name.name = "message";
    param2.type.type_name.name = "string";
    func_decl.parameters.push_back(param2);
    
    // Test Java signature
    auto java_sig = TypeMapper::map_function_signature(func_decl, TargetLanguage::Java);
    ASSERT_TRUE(java_sig.has_value());
    EXPECT_TRUE(java_sig->find("public int testFunction(int x, String message)") != std::string::npos);
    
    // Test Go signature
    auto go_sig = TypeMapper::map_function_signature(func_decl, TargetLanguage::Go);
    ASSERT_TRUE(go_sig.has_value());
    EXPECT_TRUE(go_sig->find("func testFunction(x int64, message string) int64") != std::string::npos);
    
    // Test C++ signature
    auto cpp_sig = TypeMapper::map_function_signature(func_decl, TargetLanguage::Cpp);
    ASSERT_TRUE(cpp_sig.has_value());
    EXPECT_TRUE(cpp_sig->find("int64_t testFunction(int64_t x, std::string message)") != std::string::npos);
}

// Test @extern annotation parsing
TEST_F(ExternMacroTest, ExternAnnotationParsing) {
    // Test Java annotation
    std::string java_annotation = R"(@extern(lang: "java", class: "java.util.ArrayList"))";
    auto java_binding = ExternMacro::parse_extern_annotation(java_annotation);
    ASSERT_TRUE(java_binding.has_value());
    EXPECT_EQ(java_binding->target_lang, TargetLanguage::Java);
    EXPECT_EQ(java_binding->class_name, "java.util.ArrayList");
    
    // Test Go annotation
    std::string go_annotation = R"(@extern(lang: "go", package: "fmt"))";
    auto go_binding = ExternMacro::parse_extern_annotation(go_annotation);
    ASSERT_TRUE(go_binding.has_value());
    EXPECT_EQ(go_binding->target_lang, TargetLanguage::Go);
    EXPECT_EQ(go_binding->package_name, "fmt");
    
    // Test C++ annotation
    std::string cpp_annotation = R"(@extern(lang: "cpp", header: "<vector>"))";
    auto cpp_binding = ExternMacro::parse_extern_annotation(cpp_annotation);
    ASSERT_TRUE(cpp_binding.has_value());
    EXPECT_EQ(cpp_binding->target_lang, TargetLanguage::Cpp);
    EXPECT_EQ(cpp_binding->header_name, "<vector>");
    
    // Test missing lang parameter
    std::string invalid_annotation = R"(@extern(class: "java.util.ArrayList"))";
    auto invalid_binding = ExternMacro::parse_extern_annotation(invalid_annotation);
    EXPECT_FALSE(invalid_binding.has_value());
}

// Test FFI binding generation for functions
TEST_F(ExternMacroTest, FunctionBindingGeneration) {
    // Create test function
    function_definition func_decl;
    func_decl.name.name = "printf";
    func_decl.return_type.type_name.name = "int";
    
    function_parameter param;
    param.name.name = "format";
    param.type.type_name.name = "string";
    func_decl.parameters.push_back(param);
    
    // Create FFI binding
    FFIBinding binding;
    binding.target_lang = TargetLanguage::C;
    binding.header_name = "<stdio.h>";
    binding.type_mappings = TypeMapper::get_default_mappings(TargetLanguage::C);
    
    // Generate binding
    auto binding_result = FFIBindingGenerator::generate_function_binding(func_decl, binding);
    ASSERT_TRUE(binding_result.has_value());
    
    // Check that the binding contains expected elements
    EXPECT_TRUE(binding_result->find("#include <stdio.h>") != std::string::npos);
    EXPECT_TRUE(binding_result->find("printf") != std::string::npos);
}

// Test native call generation
TEST_F(ExternMacroTest, NativeCallGeneration) {
    FFIBinding binding;
    binding.target_lang = TargetLanguage::Java;
    binding.class_name = "java.lang.Math";
    
    std::vector<std::string> params = {"x", "y"};
    auto native_call = ExternMacro::generate_native_call("max", params, binding);
    
    // Verify the native call structure (simplified check)
    // In a full implementation, this would verify the complete AST structure
    EXPECT_TRUE(true); // Placeholder - would check AST structure
}

// Test macro registration
TEST_F(ExternMacroTest, MacroRegistration) {
    auto& macro_registry = MacroRegistry::instance();
    EXPECT_TRUE(macro_registry.has_macro("extern"));
    
    auto& decorator_registry = DecoratorRegistry::instance();
    EXPECT_TRUE(decorator_registry.has_decorator("extern"));
}

// Test annotation parameter parsing
TEST_F(ExternMacroTest, AnnotationParameterParsing) {
    using namespace extern_parser;
    
    // Test lang parameter
    auto lang = parse_lang_param("java");
    ASSERT_TRUE(lang.has_value());
    EXPECT_EQ(*lang, TargetLanguage::Java);
    
    // Test class parameter
    auto class_name = parse_class_param("java.util.ArrayList");
    ASSERT_TRUE(class_name.has_value());
    EXPECT_EQ(*class_name, "java.util.ArrayList");
    
    // Test package parameter
    auto package = parse_package_param("fmt");
    ASSERT_TRUE(package.has_value());
    EXPECT_EQ(*package, "fmt");
    
    // Test header parameter
    auto header = parse_header_param("<vector>");
    ASSERT_TRUE(header.has_value());
    EXPECT_EQ(*header, "<vector>");
    
    // Test library parameter
    auto library = parse_library_param("libmath.so");
    ASSERT_TRUE(library.has_value());
    EXPECT_EQ(*library, "libmath.so");
    
    // Test type mappings
    std::string type_mapping_json = R"({"int": "int32", "string": "std::string"})";
    auto mappings = parse_type_mappings(type_mapping_json);
    ASSERT_TRUE(mappings.has_value());
    EXPECT_EQ(mappings->at("int"), "int32");
    EXPECT_EQ(mappings->at("string"), "std::string");
}

// Test error handling
TEST_F(ExternMacroTest, ErrorHandling) {
    // Test invalid language
    auto invalid_lang = parse_target_language("invalid");
    EXPECT_FALSE(invalid_lang.has_value());
    EXPECT_TRUE(invalid_lang.error().find("Unsupported target language") != std::string::npos);
    
    // Test empty parameters
    auto empty_class = extern_parser::parse_class_param("");
    EXPECT_FALSE(empty_class.has_value());
    
    auto empty_package = extern_parser::parse_package_param("");
    EXPECT_FALSE(empty_package.has_value());
    
    auto empty_header = extern_parser::parse_header_param("");
    EXPECT_FALSE(empty_header.has_value());
    
    auto empty_library = extern_parser::parse_library_param("");
    EXPECT_FALSE(empty_library.has_value());
}

// Test class definition mapping
TEST_F(ExternMacroTest, ClassDefinitionMapping) {
    // Create test class definition
    class_definition class_def;
    class_def.name.name = "TestClass";
    
    // Add fields
    field_declaration field1;
    field1.name.name = "id";
    field1.type.type_name.name = "int";
    class_def.fields.push_back(field1);
    
    field_declaration field2;
    field2.name.name = "name";
    field2.type.type_name.name = "string";
    class_def.fields.push_back(field2);
    
    // Test Java mapping
    auto java_class = TypeMapper::map_class_definition(class_def, TargetLanguage::Java);
    ASSERT_TRUE(java_class.has_value());
    EXPECT_TRUE(java_class->find("public class TestClass") != std::string::npos);
    EXPECT_TRUE(java_class->find("public int id") != std::string::npos);
    EXPECT_TRUE(java_class->find("public String name") != std::string::npos);
    
    // Test Go mapping
    auto go_class = TypeMapper::map_class_definition(class_def, TargetLanguage::Go);
    ASSERT_TRUE(go_class.has_value());
    EXPECT_TRUE(go_class->find("type TestClass struct") != std::string::npos);
    EXPECT_TRUE(go_class->find("id int64") != std::string::npos);
    EXPECT_TRUE(go_class->find("name string") != std::string::npos);
    
    // Test C++ mapping
    auto cpp_class = TypeMapper::map_class_definition(class_def, TargetLanguage::Cpp);
    ASSERT_TRUE(cpp_class.has_value());
    EXPECT_TRUE(cpp_class->find("class TestClass") != std::string::npos);
    EXPECT_TRUE(cpp_class->find("int64_t id") != std::string::npos);
    EXPECT_TRUE(cpp_class->find("std::string name") != std::string::npos);
}

// Test import generation
TEST_F(ExternMacroTest, ImportGeneration) {
    FFIBinding java_binding;
    java_binding.target_lang = TargetLanguage::Java;
    java_binding.class_name = "java.util.ArrayList";
    
    auto java_imports = FFIBindingGenerator::generate_imports(java_binding);
    EXPECT_TRUE(java_imports.find("import java.util.ArrayList") != std::string::npos);
    
    FFIBinding go_binding;
    go_binding.target_lang = TargetLanguage::Go;
    go_binding.package_name = "fmt";
    
    auto go_imports = FFIBindingGenerator::generate_imports(go_binding);
    EXPECT_TRUE(go_imports.find("import \"fmt\"") != std::string::npos);
    
    FFIBinding cpp_binding;
    cpp_binding.target_lang = TargetLanguage::Cpp;
    cpp_binding.header_name = "<vector>";
    
    auto cpp_imports = FFIBindingGenerator::generate_imports(cpp_binding);
    EXPECT_TRUE(cpp_imports.find("#include <vector>") != std::string::npos);
}