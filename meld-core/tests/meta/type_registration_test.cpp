#include <gtest/gtest.h>
#include "meld/meta/type_registration.hpp"
#include "meld/kernel/primitives.hpp"
#include "meld/meta/metatype.hpp"

using namespace meld::meta;
using namespace meld::kernel;

class TypeRegistrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        initialize_meld_types();
    }
};

TEST_F(TypeRegistrationTest, TypeRegistry_Singleton) {
    TypeRegistry& registry1 = TypeRegistry::instance();
    TypeRegistry& registry2 = TypeRegistry::instance();
    EXPECT_EQ(&registry1, &registry2);
}

TEST_F(TypeRegistrationTest, PrimitiveTypes_Registered) {
    TypeRegistry& registry = TypeRegistry::instance();
    EXPECT_TRUE(registry.is_registered("Symbol"));
    EXPECT_TRUE(registry.is_registered("Integer"));
    EXPECT_TRUE(registry.is_registered("Boolean"));
    EXPECT_TRUE(registry.is_registered("String"));
    EXPECT_TRUE(registry.is_registered("Function"));
}

TEST_F(TypeRegistrationTest, MetaTypeHierarchy_Registered) {
    TypeRegistry& registry = TypeRegistry::instance();
    EXPECT_TRUE(registry.is_registered("MetaType"));
    EXPECT_TRUE(registry.is_registered("PrimitiveMetaType"));
    EXPECT_TRUE(registry.is_registered("StructMetaType"));
    EXPECT_TRUE(registry.is_registered("ClassMetaType"));
    EXPECT_TRUE(registry.is_registered("TraitMetaType"));
    EXPECT_TRUE(registry.is_registered("UnionMetaType"));
    EXPECT_TRUE(registry.is_registered("IntersectionMetaType"));
}

TEST_F(TypeRegistrationTest, HelperTypes_Registered) {
    TypeRegistry& registry = TypeRegistry::instance();
    EXPECT_TRUE(registry.is_registered("Field"));
    EXPECT_TRUE(registry.is_registered("Method"));
}

TEST_F(TypeRegistrationTest, GetType_ValidTypes) {
    TypeRegistry& registry = TypeRegistry::instance();

    auto symbol_result = registry.get_type("Symbol");
    ASSERT_TRUE(symbol_result.has_value());
    EXPECT_EQ(symbol_result.value()->name(), "Symbol");

    auto integer_result = registry.get_type("Integer");
    ASSERT_TRUE(integer_result.has_value());
    EXPECT_EQ(integer_result.value()->name(), "Integer");
}

TEST_F(TypeRegistrationTest, GetType_InvalidType) {
    TypeRegistry& registry = TypeRegistry::instance();
    auto result = registry.get_type("NonExistentType");
    EXPECT_FALSE(result.has_value());
}

TEST_F(TypeRegistrationTest, GetAllTypeNames) {
    TypeRegistry& registry = TypeRegistry::instance();
    auto names = registry.get_all_type_names();
    EXPECT_FALSE(names.empty());

    auto has_type = [&names](const std::string& type_name) {
        return std::find(names.begin(), names.end(), type_name) != names.end();
    };

    EXPECT_TRUE(has_type("Symbol"));
    EXPECT_TRUE(has_type("Integer"));
    EXPECT_TRUE(has_type("MetaType"));
    EXPECT_TRUE(has_type("PrimitiveMetaType"));
}

TEST_F(TypeRegistrationTest, InitializeMeldTypes_Function) {
    EXPECT_NO_THROW(initialize_meld_types());
}
