#include <gtest/gtest.h>
#include "meld/kernel/type_utils.hpp"
#include "meld/meta/metatype.hpp"
#include <typeinfo>

using namespace meld::kernel;
using namespace meld::meta;

// Test fixture for RTTI tests
class RTTITest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create various MetaType instances for testing
        primitive_int = MetaType::create_primitive("Int", 8);
        primitive_bool = MetaType::create_primitive("Bool", 1);
        
        struct_point = MetaType::create_struct("Point", {
            Field("x", primitive_int),
            Field("y", primitive_int)
        });
        
        class_person = MetaType::create_class("Person", {
            Field("name", MetaType::create_primitive("String", 0)),
            Field("age", primitive_int)
        }, {});
    }
    
    std::shared_ptr<MetaType> primitive_int;
    std::shared_ptr<MetaType> primitive_bool;
    std::shared_ptr<MetaType> struct_point;
    std::shared_ptr<MetaType> class_person;
};

// Test TypeComparator::are_same
TEST_F(RTTITest, TypeComparator_AreSame) {
    const std::type_info& t1 = typeid(int);
    const std::type_info& t2 = typeid(int);
    const std::type_info& t3 = typeid(double);
    
    EXPECT_TRUE(TypeComparator::are_same(t1, t2));
    EXPECT_FALSE(TypeComparator::are_same(t1, t3));
}

// Test TypeComparator::is_derived_from
TEST_F(RTTITest, TypeComparator_IsDerivedFrom) {
    EXPECT_TRUE((TypeComparator::is_derived_from<MetaType, PrimitiveMetaType>()));
    EXPECT_TRUE((TypeComparator::is_derived_from<MetaType, StructMetaType>()));
    EXPECT_TRUE((TypeComparator::is_derived_from<MetaType, ClassMetaType>()));
    EXPECT_FALSE((TypeComparator::is_derived_from<PrimitiveMetaType, MetaType>()));
}

// Test TypeComparator::before
TEST_F(RTTITest, TypeComparator_Before) {
    const std::type_info& t1 = typeid(int);
    const std::type_info& t2 = typeid(double);
    
    // before() provides a consistent ordering
    bool result = TypeComparator::before(t1, t2);
    EXPECT_EQ(result, !TypeComparator::before(t2, t1) || TypeComparator::are_same(t1, t2));
}

// Test TypeComparator::hash
TEST_F(RTTITest, TypeComparator_Hash) {
    const std::type_info& t1 = typeid(int);
    const std::type_info& t2 = typeid(int);
    const std::type_info& t3 = typeid(double);
    
    EXPECT_EQ(TypeComparator::hash(t1), TypeComparator::hash(t2));
    // Different types should (usually) have different hashes
    // Note: hash collisions are possible but unlikely
}

// Test TypeComparator::get_name
TEST_F(RTTITest, TypeComparator_GetName) {
    const std::type_info& t = typeid(int);
    std::string name = TypeComparator::get_name(t);
    
    EXPECT_FALSE(name.empty());
    // Note: The actual name is implementation-defined and may be mangled
}

// Test TypeComparator::equals and not_equals
TEST_F(RTTITest, TypeComparator_EqualsNotEquals) {
    const std::type_info& t1 = typeid(int);
    const std::type_info& t2 = typeid(int);
    const std::type_info& t3 = typeid(double);
    
    EXPECT_TRUE(TypeComparator::equals(t1, t2));
    EXPECT_FALSE(TypeComparator::not_equals(t1, t2));
    
    EXPECT_FALSE(TypeComparator::equals(t1, t3));
    EXPECT_TRUE(TypeComparator::not_equals(t1, t3));
}

// Test MetaType::type_info()
TEST_F(RTTITest, MetaType_TypeInfo) {
    const std::type_info& info = primitive_int->type_info();
    
    EXPECT_EQ(info, typeid(*primitive_int));
    EXPECT_EQ(info, typeid(PrimitiveMetaType));
}

// Test MetaType::type_name()
TEST_F(RTTITest, MetaType_TypeName) {
    std::string name = primitive_int->type_name();
    
    EXPECT_FALSE(name.empty());
    // The name should be related to PrimitiveMetaType
}

// Test MetaType::is<T>() for correct type
TEST_F(RTTITest, MetaType_Is_CorrectType) {
    EXPECT_TRUE(primitive_int->is<PrimitiveMetaType>());
    EXPECT_TRUE(primitive_int->is<MetaType>());
    
    EXPECT_TRUE(struct_point->is<StructMetaType>());
    EXPECT_TRUE(struct_point->is<MetaType>());
    
    EXPECT_TRUE(class_person->is<ClassMetaType>());
    EXPECT_TRUE(class_person->is<MetaType>());
}

// Test MetaType::is<T>() for incorrect type
TEST_F(RTTITest, MetaType_Is_IncorrectType) {
    EXPECT_FALSE(primitive_int->is<StructMetaType>());
    EXPECT_FALSE(primitive_int->is<ClassMetaType>());
    
    EXPECT_FALSE(struct_point->is<PrimitiveMetaType>());
    EXPECT_FALSE(struct_point->is<ClassMetaType>());
    
    EXPECT_FALSE(class_person->is<PrimitiveMetaType>());
    EXPECT_FALSE(class_person->is<StructMetaType>());
}

// Test MetaType::as<T>() for correct type
TEST_F(RTTITest, MetaType_As_CorrectType) {
    auto* prim = primitive_int->as<PrimitiveMetaType>();
    ASSERT_NE(prim, nullptr);
    EXPECT_EQ(prim->name(), "Int");
    EXPECT_EQ(prim->size(), 8);
    
    auto* struct_type = struct_point->as<StructMetaType>();
    ASSERT_NE(struct_type, nullptr);
    EXPECT_EQ(struct_type->name(), "Point");
    EXPECT_EQ(struct_type->fields().size(), 2);
    
    auto* class_type = class_person->as<ClassMetaType>();
    ASSERT_NE(class_type, nullptr);
    EXPECT_EQ(class_type->name(), "Person");
}

// Test MetaType::as<T>() for incorrect type
TEST_F(RTTITest, MetaType_As_IncorrectType) {
    auto* struct_type = primitive_int->as<StructMetaType>();
    EXPECT_EQ(struct_type, nullptr);
    
    auto* class_type = primitive_int->as<ClassMetaType>();
    EXPECT_EQ(class_type, nullptr);
    
    auto* prim = struct_point->as<PrimitiveMetaType>();
    EXPECT_EQ(prim, nullptr);
}

// Test MetaType::as<T>() const version
TEST_F(RTTITest, MetaType_As_Const) {
    const MetaType* const_ptr = primitive_int.get();
    
    const auto* prim = const_ptr->as<PrimitiveMetaType>();
    ASSERT_NE(prim, nullptr);
    EXPECT_EQ(prim->name(), "Int");
}

// Test safe downcasting in a polymorphic scenario
TEST_F(RTTITest, SafeDowncasting_Polymorphic) {
    // Store different types in a vector of base pointers
    std::vector<std::shared_ptr<MetaType>> types;
    types.push_back(primitive_int);
    types.push_back(struct_point);
    types.push_back(class_person);
    
    // Safely downcast and process each type
    for (const auto& type : types) {
        if (auto* prim = type->as<PrimitiveMetaType>()) {
            EXPECT_TRUE(prim->is_value_type());
        }
        else if (auto* struct_type = type->as<StructMetaType>()) {
            EXPECT_TRUE(struct_type->is_value_type());
            EXPECT_FALSE(struct_type->fields().empty());
        }
        else if (auto* class_type = type->as<ClassMetaType>()) {
            EXPECT_FALSE(class_type->is_value_type());
        }
        else {
            FAIL() << "Unknown type";
        }
    }
}

// Test type identification with typeid
TEST_F(RTTITest, TypeIdentification_WithTypeid) {
    EXPECT_EQ(typeid(*primitive_int), typeid(PrimitiveMetaType));
    EXPECT_EQ(typeid(*struct_point), typeid(StructMetaType));
    EXPECT_EQ(typeid(*class_person), typeid(ClassMetaType));
    
    EXPECT_NE(typeid(*primitive_int), typeid(*struct_point));
    EXPECT_NE(typeid(*struct_point), typeid(*class_person));
}

// Test RTTI with base class pointers
TEST_F(RTTITest, RTTI_WithBasePointers) {
    MetaType* base_ptr = primitive_int.get();
    
    // typeid works correctly with base pointers
    EXPECT_EQ(typeid(*base_ptr), typeid(PrimitiveMetaType));
    EXPECT_NE(typeid(*base_ptr), typeid(MetaType));
    
    // dynamic_cast works correctly
    auto* derived = dynamic_cast<PrimitiveMetaType*>(base_ptr);
    ASSERT_NE(derived, nullptr);
    EXPECT_EQ(derived->name(), "Int");
}

// Test type comparison across different instances
TEST_F(RTTITest, TypeComparison_DifferentInstances) {
    auto another_int = MetaType::create_primitive("Int", 8);
    
    // Same runtime type
    EXPECT_EQ(typeid(*primitive_int), typeid(*another_int));
    
    // Both are PrimitiveMetaType
    EXPECT_TRUE(primitive_int->is<PrimitiveMetaType>());
    EXPECT_TRUE(another_int->is<PrimitiveMetaType>());
}

// Test RTTI with const correctness
TEST_F(RTTITest, RTTI_ConstCorrectness) {
    const std::shared_ptr<MetaType>& const_ref = primitive_int;
    
    // const version of is<T>()
    EXPECT_TRUE(const_ref->is<PrimitiveMetaType>());
    
    // const version of as<T>()
    const auto* prim = const_ref->as<PrimitiveMetaType>();
    ASSERT_NE(prim, nullptr);
    EXPECT_EQ(prim->name(), "Int");
}

// Test RTTI performance (basic benchmark)
TEST_F(RTTITest, RTTI_Performance) {
    const int iterations = 10000;
    
    // Benchmark is<T>()
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        volatile bool result = primitive_int->is<PrimitiveMetaType>();
        (void)result;
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Should be very fast (< 1ms for 10000 iterations)
    EXPECT_LT(duration.count(), 1000);
}
