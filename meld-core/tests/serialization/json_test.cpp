#include <gtest/gtest.h>
#include "meld/serialization/json_serializer.hpp"
#include "meld/meta/type_registration.hpp"
#include "meld/kernel/primitives.hpp"

using namespace meld::serialization;
using namespace meld::kernel;
using namespace meld::meta;

class JsonSerializationTest : public ::testing::Test {
protected:
    void SetUp() override {
        initialize_meld_types();
    }
};

// Test serialize Symbol
TEST_F(JsonSerializationTest, Serialize_Symbol) {
    Symbol symbol("test");
    rttr::instance instance(symbol);
    
    auto result = JsonSerializer::serialize(instance);
    ASSERT_TRUE(result.has_value());
    
    auto json = result.value();
    EXPECT_TRUE(json.is_object());
    EXPECT_TRUE(json.contains("__type__"));
    EXPECT_TRUE(json.contains("name"));
    EXPECT_EQ(json["name"], "test");
}

// Test serialize Integer
TEST_F(JsonSerializationTest, Serialize_Integer) {
    Integer integer(42);
    rttr::instance instance(integer);
    
    auto result = JsonSerializer::serialize(instance);
    ASSERT_TRUE(result.has_value());
    
    auto json = result.value();
    EXPECT_TRUE(json.contains("value"));
    EXPECT_EQ(json["value"], 42);
}

// Test serialize Boolean
TEST_F(JsonSerializationTest, Serialize_Boolean) {
    Boolean boolean(Boolean::PrivateTag{}, true);
    rttr::instance instance(boolean);
    
    auto result = JsonSerializer::serialize(instance);
    ASSERT_TRUE(result.has_value());
    
    auto json = result.value();
    EXPECT_TRUE(json.contains("value"));
    EXPECT_EQ(json["value"], true);
}

// Test serialize String
TEST_F(JsonSerializationTest, Serialize_String) {
    String str("hello");
    rttr::instance instance(str);
    
    auto result = JsonSerializer::serialize(instance);
    ASSERT_TRUE(result.has_value());
    
    auto json = result.value();
    EXPECT_TRUE(json.contains("value"));
    EXPECT_EQ(json["value"], "hello");
}

// Test to_json_string
TEST_F(JsonSerializationTest, ToJsonString) {
    Symbol symbol("test");
    rttr::instance instance(symbol);
    
    auto result = JsonSerializer::to_json_string(instance);
    ASSERT_TRUE(result.has_value());
    
    std::string json_str = result.value();
    EXPECT_FALSE(json_str.empty());
    EXPECT_NE(json_str.find("test"), std::string::npos);
}

// Test deserialize Symbol
TEST_F(JsonSerializationTest, Deserialize_Symbol) {
    nlohmann::json json = {
        {"__type__", "Symbol"},
        {"name", "test"}
    };
    
    rttr::type type = rttr::type::get_by_name("Symbol");
    auto result = JsonSerializer::deserialize(json, type);
    ASSERT_TRUE(result.has_value());
    
    EXPECT_TRUE(result.value().is_type<Symbol>());
}

// Test round trip Symbol
TEST_F(JsonSerializationTest, RoundTrip_Symbol) {
    Symbol symbol("test");
    rttr::instance instance(symbol);
    
    EXPECT_TRUE(JsonSerializer::test_round_trip(instance));
}

// Test round trip Integer
TEST_F(JsonSerializationTest, RoundTrip_Integer) {
    Integer integer(42);
    rttr::instance instance(integer);
    
    EXPECT_TRUE(JsonSerializer::test_round_trip(instance));
}

// Test is_serializable
TEST_F(JsonSerializationTest, IsSerializable) {
    EXPECT_TRUE(JsonSerializer::is_serializable(rttr::type::get<Symbol>()));
    EXPECT_TRUE(JsonSerializer::is_serializable(rttr::type::get<Integer>()));
    EXPECT_TRUE(JsonSerializer::is_serializable(rttr::type::get<std::string>()));
    EXPECT_TRUE(JsonSerializer::is_serializable(rttr::type::get<int>()));
}

// Test error to string
TEST_F(JsonSerializationTest, ErrorToString) {
    EXPECT_EQ(to_string(SerializationError::InvalidInstance), "Invalid instance");
    EXPECT_EQ(to_string(SerializationError::TypeNotRegistered), "Type not registered");
    EXPECT_EQ(to_string(SerializationError::UnsupportedType), "Unsupported type");
}
