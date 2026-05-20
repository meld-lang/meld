#include <gtest/gtest.h>
#include "../../include/meld/types/anonymous_types.hpp"
#include "../../include/meld/meta/metatype.hpp"
#include "../../include/meld/kernel/primitives.hpp"

using namespace meld::types;
using namespace meld::meta;
using namespace meld::kernel;

class AnonymousTypesTest : public ::testing::Test {
protected:
    void SetUp() override {
        intType = std::make_shared<PrimitiveMetaType>("Int", 8);
        stringType = std::make_shared<PrimitiveMetaType>("String", 32);
        boolType = std::make_shared<PrimitiveMetaType>("Bool", 1);
    }
    
    std::shared_ptr<MetaType> intType;
    std::shared_ptr<MetaType> stringType;
    std::shared_ptr<MetaType> boolType;
};

// Test anonymous object creation and field access
TEST_F(AnonymousTypesTest, AnonymousObjectBasics) {
    std::vector<AnonymousObjectField> fields = {
        AnonymousObjectField("status", stringType),
        AnonymousObjectField("count", intType),
        AnonymousObjectField("active", boolType)
    };
    
    auto objType = std::make_shared<AnonymousObjectType>(fields);
    EXPECT_EQ(objType->name(), "{status: String, count: Int, active: Bool}");
    EXPECT_FALSE(objType->is_value_type());
    
    auto objValue = std::make_shared<AnonymousObjectValue>(objType);
    objValue->setField("status", std::string("ACTIVE"));
    objValue->setField("count", 42);
    objValue->setField("active", true);
    
    auto status = objValue->getField("status");
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(std::get<std::string>(*status), "ACTIVE");
    
    auto count = objValue->getField("count");
    ASSERT_TRUE(count.has_value());
    EXPECT_EQ(std::get<int>(*count), 42);
    
    auto active = objValue->getField("active");
    ASSERT_TRUE(active.has_value());
    EXPECT_TRUE(std::get<bool>(*active));
}

// Test anonymous object field type lookup
TEST_F(AnonymousTypesTest, AnonymousObjectFieldTypes) {
    std::vector<AnonymousObjectField> fields = {
        AnonymousObjectField("name", stringType),
        AnonymousObjectField("age", intType)
    };
    
    auto objType = std::make_shared<AnonymousObjectType>(fields);
    
    auto nameType = objType->getFieldType("name");
    ASSERT_TRUE(nameType.has_value());
    EXPECT_EQ(nameType.value()->name(), "String");
    
    auto ageType = objType->getFieldType("age");
    ASSERT_TRUE(ageType.has_value());
    EXPECT_EQ(ageType.value()->name(), "Int");
    
    auto unknownType = objType->getFieldType("unknown");
    EXPECT_FALSE(unknownType.has_value());
}

// Test structural typing compatibility
TEST_F(AnonymousTypesTest, StructuralTyping) {
    std::vector<AnonymousObjectField> fields1 = {
        AnonymousObjectField("status", stringType),
        AnonymousObjectField("count", intType),
        AnonymousObjectField("active", boolType)
    };
    auto objType1 = std::make_shared<AnonymousObjectType>(fields1);
    
    std::vector<AnonymousObjectField> fields2 = {
        AnonymousObjectField("status", stringType),
        AnonymousObjectField("active", boolType)
    };
    auto objType2 = std::make_shared<AnonymousObjectType>(fields2);
    
    // objType1 has all fields of objType2, so it's compatible
    EXPECT_TRUE(objType1->isStructurallyCompatibleWith(*objType2));
    
    // objType2 doesn't have all fields of objType1, so it's not compatible
    EXPECT_FALSE(objType2->isStructurallyCompatibleWith(*objType1));
}

// Test anonymous map creation and operations
TEST_F(AnonymousTypesTest, AnonymousMapBasics) {
    auto mapType = std::make_shared<AnonymousMapType>(boolType);
    EXPECT_EQ(mapType->name(), "Map<String, Bool>");
    EXPECT_FALSE(mapType->is_value_type());
    
    auto mapValue = std::make_shared<AnonymousMapValue>(mapType);
    mapValue->set("active", true);
    mapValue->set("checked", true);
    mapValue->set("enabled", false);
    
    auto active = mapValue->get("active");
    ASSERT_TRUE(active.has_value());
    EXPECT_TRUE(std::get<bool>(*active));
    
    auto enabled = mapValue->get("enabled");
    ASSERT_TRUE(enabled.has_value());
    EXPECT_FALSE(std::get<bool>(*enabled));
    
    auto unknown = mapValue->get("unknown");
    EXPECT_FALSE(unknown.has_value());
}

// Test anonymous map keys and values
TEST_F(AnonymousTypesTest, AnonymousMapKeysValues) {
    auto mapType = std::make_shared<AnonymousMapType>(intType);
    auto mapValue = std::make_shared<AnonymousMapValue>(mapType);
    
    mapValue->set("first", 1);
    mapValue->set("second", 2);
    mapValue->set("third", 3);
    
    auto keys = mapValue->keys();
    EXPECT_EQ(keys.size(), 3);
    EXPECT_TRUE(std::find(keys.begin(), keys.end(), "first") != keys.end());
    EXPECT_TRUE(std::find(keys.begin(), keys.end(), "second") != keys.end());
    EXPECT_TRUE(std::find(keys.begin(), keys.end(), "third") != keys.end());
    
    auto values = mapValue->values();
    EXPECT_EQ(values.size(), 3);
}

// Test anonymous set creation and operations
TEST_F(AnonymousTypesTest, AnonymousSetBasics) {
    auto setType = std::make_shared<AnonymousSetType>(stringType);
    EXPECT_EQ(setType->name(), "Set<String>");
    EXPECT_FALSE(setType->is_value_type());
    
    auto setValue = std::make_shared<AnonymousSetValue>(setType);
    setValue->add(std::string("No"));
    setValue->add(std::string("Yes"));
    setValue->add(std::string("Maybe"));
    
    EXPECT_EQ(setValue->size(), 3);
    EXPECT_TRUE(setValue->contains(std::string("Yes")));
    EXPECT_FALSE(setValue->contains(std::string("Unknown")));
}

// Test anonymous set duplicate handling
TEST_F(AnonymousTypesTest, AnonymousSetDuplicates) {
    auto setType = std::make_shared<AnonymousSetType>(intType);
    auto setValue = std::make_shared<AnonymousSetValue>(setType);
    
    setValue->add(1);
    setValue->add(2);
    setValue->add(3);
    setValue->add(2);  // Duplicate
    setValue->add(1);  // Duplicate
    
    EXPECT_EQ(setValue->size(), 3);
}

// Test anonymous set remove operation
TEST_F(AnonymousTypesTest, AnonymousSetRemove) {
    auto setType = std::make_shared<AnonymousSetType>(intType);
    auto setValue = std::make_shared<AnonymousSetValue>(setType);
    
    setValue->add(1);
    setValue->add(2);
    setValue->add(3);
    
    auto newSet = setValue->remove(2);
    EXPECT_EQ(newSet->size(), 2);
    EXPECT_TRUE(newSet->contains(1));
    EXPECT_FALSE(newSet->contains(2));
    EXPECT_TRUE(newSet->contains(3));
    
    // Original set unchanged
    EXPECT_EQ(setValue->size(), 3);
    EXPECT_TRUE(setValue->contains(2));
}

// Test anonymous array creation and operations
TEST_F(AnonymousTypesTest, AnonymousArrayBasics) {
    auto arrayType = std::make_shared<AnonymousArrayType>(intType);
    EXPECT_EQ(arrayType->name(), "Array<Int>");
    EXPECT_FALSE(arrayType->is_value_type());
    
    auto arrayValue = std::make_shared<AnonymousArrayValue>(arrayType);
    arrayValue->push(1);
    arrayValue->push(2);
    arrayValue->push(3);
    arrayValue->push(4);
    
    EXPECT_EQ(arrayValue->size(), 4);
    
    auto elem0 = arrayValue->get(0);
    ASSERT_TRUE(elem0.has_value());
    EXPECT_EQ(std::get<int>(*elem0), 1);
    
    auto elem3 = arrayValue->get(3);
    ASSERT_TRUE(elem3.has_value());
    EXPECT_EQ(std::get<int>(*elem3), 4);
    
    auto elemOutOfBounds = arrayValue->get(10);
    EXPECT_FALSE(elemOutOfBounds.has_value());
}

// Test anonymous array indexing
TEST_F(AnonymousTypesTest, AnonymousArrayIndexing) {
    auto arrayType = std::make_shared<AnonymousArrayType>(stringType);
    auto arrayValue = std::make_shared<AnonymousArrayValue>(arrayType);
    
    arrayValue->push(std::string("first"));
    arrayValue->push(std::string("second"));
    arrayValue->push(std::string("third"));
    
    for (size_t i = 0; i < arrayValue->size(); ++i) {
        auto elem = arrayValue->get(i);
        ASSERT_TRUE(elem.has_value());
    }
}

// Test nested anonymous objects
TEST_F(AnonymousTypesTest, NestedAnonymousObjects) {
    std::vector<AnonymousObjectField> innerFields = {
        AnonymousObjectField("host", stringType),
        AnonymousObjectField("port", intType)
    };
    auto innerType = std::make_shared<AnonymousObjectType>(innerFields);
    
    std::vector<AnonymousObjectField> outerFields = {
        AnonymousObjectField("server", innerType),
        AnonymousObjectField("debug", boolType)
    };
    auto outerType = std::make_shared<AnonymousObjectType>(outerFields);
    
    EXPECT_EQ(outerType->name(), "{server: {host: String, port: Int}, debug: Bool}");
}

// Test empty map creation
TEST_F(AnonymousTypesTest, EmptyMapCreation) {
    auto emptyMap = AnonymousMapValue::empty(intType);
    
    EXPECT_TRUE(emptyMap->isEmpty());
    EXPECT_EQ(emptyMap->keys().size(), 0);
    EXPECT_EQ(emptyMap->type()->name(), "Map<String, Int>");
}

// Test empty set creation
TEST_F(AnonymousTypesTest, EmptySetCreation) {
    auto emptySet = AnonymousSetValue::empty(stringType);
    
    EXPECT_TRUE(emptySet->isEmpty());
    EXPECT_EQ(emptySet->size(), 0);
    EXPECT_EQ(emptySet->type()->name(), "Set<String>");
}

// Test empty array creation
TEST_F(AnonymousTypesTest, EmptyArrayCreation) {
    auto emptyArray = AnonymousArrayValue::empty(boolType);
    
    EXPECT_TRUE(emptyArray->isEmpty());
    EXPECT_EQ(emptyArray->size(), 0);
    EXPECT_EQ(emptyArray->type()->name(), "Array<Bool>");
}

// Test empty map can be populated
TEST_F(AnonymousTypesTest, EmptyMapPopulation) {
    auto emptyMap = AnonymousMapValue::empty(intType);
    EXPECT_TRUE(emptyMap->isEmpty());
    
    emptyMap->set("first", 1);
    emptyMap->set("second", 2);
    
    EXPECT_FALSE(emptyMap->isEmpty());
    EXPECT_EQ(emptyMap->keys().size(), 2);
    
    auto first = emptyMap->get("first");
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(std::get<int>(*first), 1);
}

// Test empty set can be populated
TEST_F(AnonymousTypesTest, EmptySetPopulation) {
    auto emptySet = AnonymousSetValue::empty(stringType);
    EXPECT_TRUE(emptySet->isEmpty());
    
    emptySet->add(std::string("hello"));
    emptySet->add(std::string("world"));
    
    EXPECT_FALSE(emptySet->isEmpty());
    EXPECT_EQ(emptySet->size(), 2);
    EXPECT_TRUE(emptySet->contains(std::string("hello")));
}

// Test empty array can be populated
TEST_F(AnonymousTypesTest, EmptyArrayPopulation) {
    auto emptyArray = AnonymousArrayValue::empty(intType);
    EXPECT_TRUE(emptyArray->isEmpty());
    
    emptyArray->push(10);
    emptyArray->push(20);
    emptyArray->push(30);
    
    EXPECT_FALSE(emptyArray->isEmpty());
    EXPECT_EQ(emptyArray->size(), 3);
    
    auto elem = emptyArray->get(1);
    ASSERT_TRUE(elem.has_value());
    EXPECT_EQ(std::get<int>(*elem), 20);
}
