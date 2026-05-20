#include <gtest/gtest.h>
#include "meld/kernel/dynamic_method_invoker.hpp"
#include "meld/meta/type_registration.hpp"
#include "meld/kernel/primitives.hpp"
#ifndef __APPLE__
#include <rttr/type>
#endif

using namespace meld::kernel;
using namespace meld::meta;

class DynamicMethodTest : public ::testing::Test {
protected:
    void SetUp() override {
        initialize_meld_types();
    }
};

// Test invoke with valid method
TEST_F(DynamicMethodTest, Invoke_ValidMethod) {
    Symbol symbol("test");
    rttr::instance instance(symbol);
    
    auto result = DynamicMethodInvoker::invoke(instance, "to_string");
    ASSERT_TRUE(result.has_value());
    
    rttr::variant value = result.value();
    EXPECT_TRUE(value.is_valid());
    EXPECT_TRUE(value.is_type<std::string>());
    EXPECT_EQ(value.get_value<std::string>(), "test");
}

// Test invoke with invalid method
TEST_F(DynamicMethodTest, Invoke_InvalidMethod) {
    Symbol symbol("test");
    rttr::instance instance(symbol);
    
    auto result = DynamicMethodInvoker::invoke(instance, "nonexistent");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), MethodInvocationError::MethodNotFound);
}

// Test invoke with invalid instance
TEST_F(DynamicMethodTest, Invoke_InvalidInstance) {
    rttr::instance instance;
    
    auto result = DynamicMethodInvoker::invoke(instance, "to_string");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), MethodInvocationError::InvalidInstance);
}

// Test invoke with type validation
TEST_F(DynamicMethodTest, InvokeWithType_ValidType) {
    Symbol symbol("test");
    rttr::instance instance(symbol);
    
    auto result = DynamicMethodInvoker::invoke(instance, "Symbol", "to_string");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().get_value<std::string>(), "test");
}

// Test invoke with wrong type
TEST_F(DynamicMethodTest, InvokeWithType_WrongType) {
    Symbol symbol("test");
    rttr::instance instance(symbol);
    
    auto result = DynamicMethodInvoker::invoke(instance, "Integer", "to_string");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), MethodInvocationError::InvalidInstance);
}

// Test invoke with invalid type
TEST_F(DynamicMethodTest, InvokeWithType_InvalidType) {
    Symbol symbol("test");
    rttr::instance instance(symbol);
    
    auto result = DynamicMethodInvoker::invoke(instance, "NonExistent", "to_string");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), MethodInvocationError::TypeNotFound);
}

// Test has_method
TEST_F(DynamicMethodTest, HasMethod_ValidMethod) {
    Symbol symbol("test");
    rttr::instance instance(symbol);
    
    EXPECT_TRUE(DynamicMethodInvoker::has_method(instance, "to_string"));
    EXPECT_TRUE(DynamicMethodInvoker::has_method(instance, "hash"));
    EXPECT_FALSE(DynamicMethodInvoker::has_method(instance, "nonexistent"));
}

// Test has_method with invalid instance
TEST_F(DynamicMethodTest, HasMethod_InvalidInstance) {
    rttr::instance instance;
    
    EXPECT_FALSE(DynamicMethodInvoker::has_method(instance, "to_string"));
}

// Test get_return_type
TEST_F(DynamicMethodTest, GetReturnType_ValidMethod) {
    Symbol symbol("test");
    rttr::instance instance(symbol);
    
    auto return_type = DynamicMethodInvoker::get_return_type(instance, "to_string");
    ASSERT_TRUE(return_type.has_value());
    EXPECT_EQ(return_type->get_name().to_string(), "std::string");
}

// Test get_return_type with invalid method
TEST_F(DynamicMethodTest, GetReturnType_InvalidMethod) {
    Symbol symbol("test");
    rttr::instance instance(symbol);
    
    auto return_type = DynamicMethodInvoker::get_return_type(instance, "nonexistent");
    EXPECT_FALSE(return_type.has_value());
}

// Test get_overloads
TEST_F(DynamicMethodTest, GetOverloads_ValidMethod) {
    Symbol symbol("test");
    rttr::instance instance(symbol);
    
    auto overloads = DynamicMethodInvoker::get_overloads(instance, "to_string");
    EXPECT_FALSE(overloads.empty());
}

// Test get_overloads with invalid method
TEST_F(DynamicMethodTest, GetOverloads_InvalidMethod) {
    Symbol symbol("test");
    rttr::instance instance(symbol);
    
    auto overloads = DynamicMethodInvoker::get_overloads(instance, "nonexistent");
    EXPECT_TRUE(overloads.empty());
}

// Test get_parameter_types
TEST_F(DynamicMethodTest, GetParameterTypes_ValidMethod) {
    Symbol symbol("test");
    rttr::instance instance(symbol);
    
    auto param_types = DynamicMethodInvoker::get_parameter_types(instance, "to_string");
    ASSERT_TRUE(param_types.has_value());
    // to_string() has no parameters
    EXPECT_TRUE(param_types->empty());
}

// Test get_parameter_types with invalid method
TEST_F(DynamicMethodTest, GetParameterTypes_InvalidMethod) {
    Symbol symbol("test");
    rttr::instance instance(symbol);
    
    auto param_types = DynamicMethodInvoker::get_parameter_types(instance, "nonexistent");
    EXPECT_FALSE(param_types.has_value());
}

// Test get_all_methods
TEST_F(DynamicMethodTest, GetAllMethods_ValidInstance) {
    Symbol symbol("test");
    rttr::instance instance(symbol);
    
    auto methods = DynamicMethodInvoker::get_all_methods(instance);
    EXPECT_FALSE(methods.empty());
    
    // Should have to_string and hash
    auto has_to_string = std::find(methods.begin(), methods.end(), "to_string") != methods.end();
    auto has_hash = std::find(methods.begin(), methods.end(), "hash") != methods.end();
    
    EXPECT_TRUE(has_to_string);
    EXPECT_TRUE(has_hash);
}

// Test get_all_methods with invalid instance
TEST_F(DynamicMethodTest, GetAllMethods_InvalidInstance) {
    rttr::instance instance;
    
    auto methods = DynamicMethodInvoker::get_all_methods(instance);
    EXPECT_TRUE(methods.empty());
}

// Test get_method_signature
TEST_F(DynamicMethodTest, GetMethodSignature) {
    Symbol symbol("test");
    rttr::instance instance(symbol);
    
    auto overloads = DynamicMethodInvoker::get_overloads(instance, "to_string");
    ASSERT_FALSE(overloads.empty());
    
    std::string signature = DynamicMethodInvoker::get_method_signature(overloads[0]);
    EXPECT_FALSE(signature.empty());
    EXPECT_NE(signature.find("to_string"), std::string::npos);
    EXPECT_NE(signature.find("->"), std::string::npos);
}

// Test arguments_match
TEST_F(DynamicMethodTest, ArgumentsMatch_ExactMatch) {
    std::vector<rttr::variant> args;
    args.push_back(rttr::variant(std::string("test")));
    
    std::vector<rttr::type> param_types;
    param_types.push_back(rttr::type::get<std::string>());
    
    EXPECT_TRUE(DynamicMethodInvoker::arguments_match(args, param_types));
}

// Test arguments_match with count mismatch
TEST_F(DynamicMethodTest, ArgumentsMatch_CountMismatch) {
    std::vector<rttr::variant> args;
    args.push_back(rttr::variant(std::string("test")));
    
    std::vector<rttr::type> param_types;
    param_types.push_back(rttr::type::get<std::string>());
    param_types.push_back(rttr::type::get<int>());
    
    EXPECT_FALSE(DynamicMethodInvoker::arguments_match(args, param_types));
}

// Test arguments_match with type mismatch
TEST_F(DynamicMethodTest, ArgumentsMatch_TypeMismatch) {
    std::vector<rttr::variant> args;
    args.push_back(rttr::variant(std::string("test")));
    
    std::vector<rttr::type> param_types;
    param_types.push_back(rttr::type::get<int>());
    
    // String cannot convert to int
    EXPECT_FALSE(DynamicMethodInvoker::arguments_match(args, param_types));
}

// Test calculate_match_score
TEST_F(DynamicMethodTest, CalculateMatchScore_ExactMatch) {
    std::vector<rttr::variant> args;
    args.push_back(rttr::variant(std::string("test")));
    
    std::vector<rttr::type> param_types;
    param_types.push_back(rttr::type::get<std::string>());
    
    int score = DynamicMethodInvoker::calculate_match_score(args, param_types);
    EXPECT_EQ(score, 100); // Exact match
}

// Test calculate_match_score with no match
TEST_F(DynamicMethodTest, CalculateMatchScore_NoMatch) {
    std::vector<rttr::variant> args;
    args.push_back(rttr::variant(std::string("test")));
    
    std::vector<rttr::type> param_types;
    param_types.push_back(rttr::type::get<int>());
    
    int score = DynamicMethodInvoker::calculate_match_score(args, param_types);
    EXPECT_EQ(score, -1); // No match
}

// Test convert_arguments with exact types
TEST_F(DynamicMethodTest, ConvertArguments_ExactTypes) {
    std::vector<rttr::variant> args;
    args.push_back(rttr::variant(std::string("test")));
    
    std::vector<rttr::type> param_types;
    param_types.push_back(rttr::type::get<std::string>());
    
    auto result = DynamicMethodInvoker::convert_arguments(args, param_types);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 1);
}

// Test convert_arguments with count mismatch
TEST_F(DynamicMethodTest, ConvertArguments_CountMismatch) {
    std::vector<rttr::variant> args;
    args.push_back(rttr::variant(std::string("test")));
    
    std::vector<rttr::type> param_types;
    param_types.push_back(rttr::type::get<std::string>());
    param_types.push_back(rttr::type::get<int>());
    
    auto result = DynamicMethodInvoker::convert_arguments(args, param_types);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), MethodInvocationError::ArgumentCountMismatch);
}

// Test invoke_with_values
TEST_F(DynamicMethodTest, InvokeWithValues_NoArgs) {
    Symbol symbol("test");
    rttr::instance instance(symbol);
    
    auto result = DynamicMethodInvoker::invoke_with_values(instance, "to_string");
    ASSERT_TRUE(result.has_value());
    
    // Result should be a String
    EXPECT_TRUE(result.value().is<String>());
}

// Test invoke_with_values with invalid method
TEST_F(DynamicMethodTest, InvokeWithValues_InvalidMethod) {
    Symbol symbol("test");
    rttr::instance instance(symbol);
    
    auto result = DynamicMethodInvoker::invoke_with_values(instance, "nonexistent");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), MethodInvocationError::MethodNotFound);
}

// Test error message conversion
TEST_F(DynamicMethodTest, ErrorToString) {
    EXPECT_EQ(to_string(MethodInvocationError::TypeNotFound), "Type not found");
    EXPECT_EQ(to_string(MethodInvocationError::MethodNotFound), "Method not found");
    EXPECT_EQ(to_string(MethodInvocationError::InvalidInstance), "Invalid instance");
    EXPECT_EQ(to_string(MethodInvocationError::ArgumentCountMismatch), "Argument count mismatch");
    EXPECT_EQ(to_string(MethodInvocationError::ArgumentTypeMismatch), "Argument type mismatch");
    EXPECT_EQ(to_string(MethodInvocationError::ConversionFailed), "Conversion failed");
    EXPECT_EQ(to_string(MethodInvocationError::InvocationFailed), "Invocation failed");
    EXPECT_EQ(to_string(MethodInvocationError::AmbiguousOverload), "Ambiguous overload");
}

// Test Integer methods
TEST_F(DynamicMethodTest, Invoke_IntegerToString) {
    Integer integer(42);
    rttr::instance instance(integer);
    
    auto result = DynamicMethodInvoker::invoke(instance, "to_string");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().is_type<std::string>());
}

// Test Boolean methods
TEST_F(DynamicMethodTest, Invoke_BooleanToString) {
    Boolean boolean(Boolean::PrivateTag{}, true);
    rttr::instance instance(boolean);
    
    auto result = DynamicMethodInvoker::invoke(instance, "to_string");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().is_type<std::string>());
}

// Test String methods
TEST_F(DynamicMethodTest, Invoke_StringLength) {
    String str("hello");
    rttr::instance instance(str);
    
    auto result = DynamicMethodInvoker::invoke(instance, "length");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().is_type<size_t>());
    EXPECT_EQ(result.value().get_value<size_t>(), 5);
}

// Test Function methods
TEST_F(DynamicMethodTest, Invoke_FunctionArity) {
    std::vector<std::shared_ptr<Symbol>> params;
    params.push_back(std::make_shared<Symbol>("x"));
    params.push_back(std::make_shared<Symbol>("y"));
    
    Function func(params, Value());
    rttr::instance instance(func);
    
    auto result = DynamicMethodInvoker::invoke(instance, "arity");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().is_type<size_t>());
    EXPECT_EQ(result.value().get_value<size_t>(), 2);
}
