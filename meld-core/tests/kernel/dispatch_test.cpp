#include <gtest/gtest.h>
#include "../../include/meld/kernel/dispatch.hpp"
#include "../../include/meld/meta/metatype.hpp"

using namespace meld::kernel;
using namespace meld::meta;

class DispatchTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear registry before each test
        DispatchRegistry::instance().clear();
        
        // Create some test types
        int_type = TypeRegistry::instance().get_int_type();
        string_type = TypeRegistry::instance().get_string_type();
        bool_type = TypeRegistry::instance().get_bool_type();
        
        // Create a simple class hierarchy for testing
        // Animal (base)
        animal_type = MetaType::create_class("Animal", {}, {});
        TypeRegistry::instance().register_type("Animal", animal_type);
        
        // Dog : Animal
        dog_type = std::make_shared<ClassMetaType>(
            "Dog", 
            std::vector<Field>{}, 
            std::vector<Method>{},
            std::vector<Property>{},
            std::dynamic_pointer_cast<ClassMetaType>(animal_type)
        );
        TypeRegistry::instance().register_type("Dog", dog_type);
        
        // Cat : Animal
        cat_type = std::make_shared<ClassMetaType>(
            "Cat",
            std::vector<Field>{},
            std::vector<Method>{},
            std::vector<Property>{},
            std::dynamic_pointer_cast<ClassMetaType>(animal_type)
        );
        TypeRegistry::instance().register_type("Cat", cat_type);
        
        // Create dummy implementations
        dummy_impl = Value(std::make_shared<Integer>(42));
    }
    
    void TearDown() override {
        DispatchRegistry::instance().clear();
    }
    
    std::shared_ptr<MetaType> int_type;
    std::shared_ptr<MetaType> string_type;
    std::shared_ptr<MetaType> bool_type;
    std::shared_ptr<MetaType> animal_type;
    std::shared_ptr<MetaType> dog_type;
    std::shared_ptr<MetaType> cat_type;
    Value dummy_impl;
};

// Test basic function registration and lookup
TEST_F(DispatchTest, BasicRegistrationAndLookup) {
    auto sig = std::make_shared<FunctionSignature>(
        "add",
        std::vector<std::shared_ptr<MetaType>>{int_type, int_type},
        int_type,
        dummy_impl
    );
    
    DispatchRegistry::instance().register_function(sig);
    
    auto result = DispatchRegistry::instance().resolve(
        "add",
        std::vector<std::shared_ptr<MetaType>>{int_type, int_type}
    );
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value()->name, "add");
    EXPECT_EQ(result.value()->param_types.size(), 2);
}

// Test no matching function
TEST_F(DispatchTest, NoMatchingFunction) {
    auto sig = std::make_shared<FunctionSignature>(
        "add",
        std::vector<std::shared_ptr<MetaType>>{int_type, int_type},
        int_type,
        dummy_impl
    );
    
    DispatchRegistry::instance().register_function(sig);
    
    // Try to call with wrong types
    auto result = DispatchRegistry::instance().resolve(
        "add",
        std::vector<std::shared_ptr<MetaType>>{string_type, string_type}
    );
    
    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().find("No matching function") != std::string::npos);
}

// Test single dispatch (method resolution based on receiver type)
TEST_F(DispatchTest, SingleDispatch) {
    // Register: speak(Animal) -> String
    auto sig1 = std::make_shared<FunctionSignature>(
        "speak",
        std::vector<std::shared_ptr<MetaType>>{animal_type},
        string_type,
        dummy_impl
    );
    
    // Register: speak(Dog) -> String (more specific)
    auto sig2 = std::make_shared<FunctionSignature>(
        "speak",
        std::vector<std::shared_ptr<MetaType>>{dog_type},
        string_type,
        dummy_impl
    );
    
    DispatchRegistry::instance().register_function(sig1);
    DispatchRegistry::instance().register_function(sig2);
    
    // Call with Dog - should resolve to speak(Dog)
    auto result = DispatchRegistry::instance().resolve(
        "speak",
        std::vector<std::shared_ptr<MetaType>>{dog_type}
    );
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value()->param_types[0]->name(), "Dog");
}

// Test multiple dispatch (resolution based on all argument types)
TEST_F(DispatchTest, MultipleDispatch) {
    // Register: collide(Animal, Animal) -> String
    auto sig1 = std::make_shared<FunctionSignature>(
        "collide",
        std::vector<std::shared_ptr<MetaType>>{animal_type, animal_type},
        string_type,
        dummy_impl
    );
    
    // Register: collide(Dog, Cat) -> String (more specific)
    auto sig2 = std::make_shared<FunctionSignature>(
        "collide",
        std::vector<std::shared_ptr<MetaType>>{dog_type, cat_type},
        string_type,
        dummy_impl
    );
    
    // Register: collide(Cat, Dog) -> String (more specific)
    auto sig3 = std::make_shared<FunctionSignature>(
        "collide",
        std::vector<std::shared_ptr<MetaType>>{cat_type, dog_type},
        string_type,
        dummy_impl
    );
    
    DispatchRegistry::instance().register_function(sig1);
    DispatchRegistry::instance().register_function(sig2);
    DispatchRegistry::instance().register_function(sig3);
    
    // Call with (Dog, Cat) - should resolve to collide(Dog, Cat)
    auto result1 = DispatchRegistry::instance().resolve(
        "collide",
        std::vector<std::shared_ptr<MetaType>>{dog_type, cat_type}
    );
    
    ASSERT_TRUE(result1.has_value());
    EXPECT_EQ(result1.value()->param_types[0]->name(), "Dog");
    EXPECT_EQ(result1.value()->param_types[1]->name(), "Cat");
    
    // Call with (Cat, Dog) - should resolve to collide(Cat, Dog)
    auto result2 = DispatchRegistry::instance().resolve(
        "collide",
        std::vector<std::shared_ptr<MetaType>>{cat_type, dog_type}
    );
    
    ASSERT_TRUE(result2.has_value());
    EXPECT_EQ(result2.value()->param_types[0]->name(), "Cat");
    EXPECT_EQ(result2.value()->param_types[1]->name(), "Dog");
    
    // Call with (Dog, Dog) - should resolve to collide(Animal, Animal)
    auto result3 = DispatchRegistry::instance().resolve(
        "collide",
        std::vector<std::shared_ptr<MetaType>>{dog_type, dog_type}
    );
    
    ASSERT_TRUE(result3.has_value());
    EXPECT_EQ(result3.value()->param_types[0]->name(), "Animal");
    EXPECT_EQ(result3.value()->param_types[1]->name(), "Animal");
}

// Test most specific match selection
TEST_F(DispatchTest, MostSpecificMatch) {
    // Register three signatures with different specificity levels
    auto sig1 = std::make_shared<FunctionSignature>(
        "process",
        std::vector<std::shared_ptr<MetaType>>{animal_type, animal_type},
        string_type,
        dummy_impl
    );
    
    auto sig2 = std::make_shared<FunctionSignature>(
        "process",
        std::vector<std::shared_ptr<MetaType>>{dog_type, animal_type},
        string_type,
        dummy_impl
    );
    
    auto sig3 = std::make_shared<FunctionSignature>(
        "process",
        std::vector<std::shared_ptr<MetaType>>{dog_type, cat_type},
        string_type,
        dummy_impl
    );
    
    DispatchRegistry::instance().register_function(sig1);
    DispatchRegistry::instance().register_function(sig2);
    DispatchRegistry::instance().register_function(sig3);
    
    // Call with (Dog, Cat) - should resolve to most specific: process(Dog, Cat)
    auto result = DispatchRegistry::instance().resolve(
        "process",
        std::vector<std::shared_ptr<MetaType>>{dog_type, cat_type}
    );
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value()->param_types[0]->name(), "Dog");
    EXPECT_EQ(result.value()->param_types[1]->name(), "Cat");
}

// Test type specificity ranking
TEST_F(DispatchTest, TypeSpecificityRanking) {
    // Exact match should have distance 0
    int dist1 = dispatch_utils::type_distance(*int_type, *int_type);
    EXPECT_EQ(dist1, 0);
    
    // Subtype should have distance > 0
    int dist2 = dispatch_utils::type_distance(*animal_type, *dog_type);
    EXPECT_GT(dist2, 0);
    
    // Non-matching types should have distance -1
    int dist3 = dispatch_utils::type_distance(*int_type, *string_type);
    EXPECT_EQ(dist3, -1);
}

// Test is_more_specific utility
TEST_F(DispatchTest, IsMoreSpecific) {
    // Dog is more specific than Animal for a Dog argument
    EXPECT_TRUE(dispatch_utils::is_more_specific(*dog_type, *animal_type, *dog_type));
    EXPECT_FALSE(dispatch_utils::is_more_specific(*animal_type, *dog_type, *dog_type));
    
    // Same types are not more specific than each other
    EXPECT_FALSE(dispatch_utils::is_more_specific(*dog_type, *dog_type, *dog_type));
}

// Test function signature matching
TEST_F(DispatchTest, SignatureMatching) {
    auto sig = std::make_shared<FunctionSignature>(
        "test",
        std::vector<std::shared_ptr<MetaType>>{animal_type, int_type},
        string_type,
        dummy_impl
    );
    
    // Should match with exact types
    EXPECT_TRUE(sig->matches(std::vector<std::shared_ptr<MetaType>>{animal_type, int_type}));
    
    // Should match with subtype
    EXPECT_TRUE(sig->matches(std::vector<std::shared_ptr<MetaType>>{dog_type, int_type}));
    
    // Should not match with wrong arity
    EXPECT_FALSE(sig->matches(std::vector<std::shared_ptr<MetaType>>{animal_type}));
    
    // Should not match with incompatible types
    EXPECT_FALSE(sig->matches(std::vector<std::shared_ptr<MetaType>>{animal_type, string_type}));
}

// Test specificity scoring
TEST_F(DispatchTest, SpecificityScoring) {
    auto sig1 = std::make_shared<FunctionSignature>(
        "test",
        std::vector<std::shared_ptr<MetaType>>{animal_type},
        string_type,
        dummy_impl
    );
    
    auto sig2 = std::make_shared<FunctionSignature>(
        "test",
        std::vector<std::shared_ptr<MetaType>>{dog_type},
        string_type,
        dummy_impl
    );
    
    // sig2 should have higher specificity for Dog argument
    int score1 = sig1->total_specificity(std::vector<std::shared_ptr<MetaType>>{dog_type});
    int score2 = sig2->total_specificity(std::vector<std::shared_ptr<MetaType>>{dog_type});
    
    EXPECT_GT(score2, score1);
}

// Test get_signatures
TEST_F(DispatchTest, GetSignatures) {
    auto sig1 = std::make_shared<FunctionSignature>(
        "test",
        std::vector<std::shared_ptr<MetaType>>{int_type},
        int_type,
        dummy_impl
    );
    
    auto sig2 = std::make_shared<FunctionSignature>(
        "test",
        std::vector<std::shared_ptr<MetaType>>{string_type},
        string_type,
        dummy_impl
    );
    
    DispatchRegistry::instance().register_function(sig1);
    DispatchRegistry::instance().register_function(sig2);
    
    auto sigs = DispatchRegistry::instance().get_signatures("test");
    EXPECT_EQ(sigs.size(), 2);
    
    auto no_sigs = DispatchRegistry::instance().get_signatures("nonexistent");
    EXPECT_EQ(no_sigs.size(), 0);
}

// Test signature to_string
TEST_F(DispatchTest, SignatureToString) {
    auto sig = std::make_shared<FunctionSignature>(
        "add",
        std::vector<std::shared_ptr<MetaType>>{int_type, int_type},
        int_type,
        dummy_impl
    );
    
    std::string str = sig->to_string();
    EXPECT_TRUE(str.find("add") != std::string::npos);
    EXPECT_TRUE(str.find("Int") != std::string::npos);
}

// ===== Ambiguity Detection Tests (Task 15.2) =====

// Test ambiguous call detection - two equally specific signatures
TEST_F(DispatchTest, AmbiguousCallTwoSignatures) {
    // Register: process(Dog, Animal) -> String
    auto sig1 = std::make_shared<FunctionSignature>(
        "process",
        std::vector<std::shared_ptr<MetaType>>{dog_type, animal_type},
        string_type,
        dummy_impl
    );
    
    // Register: process(Animal, Dog) -> String
    auto sig2 = std::make_shared<FunctionSignature>(
        "process",
        std::vector<std::shared_ptr<MetaType>>{animal_type, dog_type},
        string_type,
        dummy_impl
    );
    
    DispatchRegistry::instance().register_function(sig1);
    DispatchRegistry::instance().register_function(sig2);
    
    // Call with (Dog, Dog) - ambiguous! Both signatures match but neither is more specific
    auto result = DispatchRegistry::instance().resolve(
        "process",
        std::vector<std::shared_ptr<MetaType>>{dog_type, dog_type}
    );
    
    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().find("Ambiguous") != std::string::npos);
}

// Test ambiguous call with three signatures
TEST_F(DispatchTest, AmbiguousCallThreeSignatures) {
    // Register: mix(Dog, Cat, Animal) -> String
    auto sig1 = std::make_shared<FunctionSignature>(
        "mix",
        std::vector<std::shared_ptr<MetaType>>{dog_type, cat_type, animal_type},
        string_type,
        dummy_impl
    );
    
    // Register: mix(Dog, Animal, Cat) -> String
    auto sig2 = std::make_shared<FunctionSignature>(
        "mix",
        std::vector<std::shared_ptr<MetaType>>{dog_type, animal_type, cat_type},
        string_type,
        dummy_impl
    );
    
    // Register: mix(Animal, Cat, Cat) -> String
    auto sig3 = std::make_shared<FunctionSignature>(
        "mix",
        std::vector<std::shared_ptr<MetaType>>{animal_type, cat_type, cat_type},
        string_type,
        dummy_impl
    );
    
    DispatchRegistry::instance().register_function(sig1);
    DispatchRegistry::instance().register_function(sig2);
    DispatchRegistry::instance().register_function(sig3);
    
    // Call with (Dog, Cat, Cat) - ambiguous between sig1 and sig2
    auto result = DispatchRegistry::instance().resolve(
        "mix",
        std::vector<std::shared_ptr<MetaType>>{dog_type, cat_type, cat_type}
    );
    
    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().find("Ambiguous") != std::string::npos);
}

// Test is_ambiguous helper method
TEST_F(DispatchTest, IsAmbiguousHelper) {
    // Register two ambiguous signatures
    auto sig1 = std::make_shared<FunctionSignature>(
        "test",
        std::vector<std::shared_ptr<MetaType>>{dog_type, animal_type},
        string_type,
        dummy_impl
    );
    
    auto sig2 = std::make_shared<FunctionSignature>(
        "test",
        std::vector<std::shared_ptr<MetaType>>{animal_type, dog_type},
        string_type,
        dummy_impl
    );
    
    DispatchRegistry::instance().register_function(sig1);
    DispatchRegistry::instance().register_function(sig2);
    
    // Should detect ambiguity
    EXPECT_TRUE(DispatchRegistry::instance().is_ambiguous(
        "test",
        std::vector<std::shared_ptr<MetaType>>{dog_type, dog_type}
    ));
    
    // Should not be ambiguous with different args
    EXPECT_FALSE(DispatchRegistry::instance().is_ambiguous(
        "test",
        std::vector<std::shared_ptr<MetaType>>{dog_type, cat_type}
    ));
}

// Test non-ambiguous case with clear winner
TEST_F(DispatchTest, NonAmbiguousClearWinner) {
    // Register: process(Animal, Animal) -> String
    auto sig1 = std::make_shared<FunctionSignature>(
        "process",
        std::vector<std::shared_ptr<MetaType>>{animal_type, animal_type},
        string_type,
        dummy_impl
    );
    
    // Register: process(Dog, Dog) -> String (more specific in both positions)
    auto sig2 = std::make_shared<FunctionSignature>(
        "process",
        std::vector<std::shared_ptr<MetaType>>{dog_type, dog_type},
        string_type,
        dummy_impl
    );
    
    DispatchRegistry::instance().register_function(sig1);
    DispatchRegistry::instance().register_function(sig2);
    
    // Call with (Dog, Dog) - should resolve to sig2 (not ambiguous)
    auto result = DispatchRegistry::instance().resolve(
        "process",
        std::vector<std::shared_ptr<MetaType>>{dog_type, dog_type}
    );
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value()->param_types[0]->name(), "Dog");
    EXPECT_EQ(result.value()->param_types[1]->name(), "Dog");
}

// Test ambiguity error message includes all candidates
TEST_F(DispatchTest, AmbiguityErrorMessage) {
    auto sig1 = std::make_shared<FunctionSignature>(
        "test",
        std::vector<std::shared_ptr<MetaType>>{dog_type, animal_type},
        string_type,
        dummy_impl
    );
    
    auto sig2 = std::make_shared<FunctionSignature>(
        "test",
        std::vector<std::shared_ptr<MetaType>>{animal_type, dog_type},
        string_type,
        dummy_impl
    );
    
    DispatchRegistry::instance().register_function(sig1);
    DispatchRegistry::instance().register_function(sig2);
    
    auto result = DispatchRegistry::instance().resolve(
        "test",
        std::vector<std::shared_ptr<MetaType>>{dog_type, dog_type}
    );
    
    ASSERT_FALSE(result.has_value());
    std::string error = result.error();
    
    // Error should mention both signatures
    EXPECT_TRUE(error.find("Dog") != std::string::npos);
    EXPECT_TRUE(error.find("Animal") != std::string::npos);
    EXPECT_TRUE(error.find("Ambiguous") != std::string::npos);
}

// Test compile-time ambiguity detection scenario
TEST_F(DispatchTest, CompileTimeAmbiguityDetection) {
    // This simulates what a compiler would check at compile-time
    
    // Register two potentially conflicting signatures
    auto sig1 = std::make_shared<FunctionSignature>(
        "overlap",
        std::vector<std::shared_ptr<MetaType>>{dog_type, cat_type},
        string_type,
        dummy_impl
    );
    
    auto sig2 = std::make_shared<FunctionSignature>(
        "overlap",
        std::vector<std::shared_ptr<MetaType>>{cat_type, dog_type},
        string_type,
        dummy_impl
    );
    
    DispatchRegistry::instance().register_function(sig1);
    DispatchRegistry::instance().register_function(sig2);
    
    // These calls should NOT be ambiguous (different argument orders)
    auto result1 = DispatchRegistry::instance().resolve(
        "overlap",
        std::vector<std::shared_ptr<MetaType>>{dog_type, cat_type}
    );
    ASSERT_TRUE(result1.has_value());
    
    auto result2 = DispatchRegistry::instance().resolve(
        "overlap",
        std::vector<std::shared_ptr<MetaType>>{cat_type, dog_type}
    );
    ASSERT_TRUE(result2.has_value());
}

// Test ambiguity with identical signatures (should be caught)
TEST_F(DispatchTest, IdenticalSignaturesAmbiguity) {
    // Register two identical signatures
    auto sig1 = std::make_shared<FunctionSignature>(
        "duplicate",
        std::vector<std::shared_ptr<MetaType>>{int_type, string_type},
        bool_type,
        dummy_impl
    );
    
    auto sig2 = std::make_shared<FunctionSignature>(
        "duplicate",
        std::vector<std::shared_ptr<MetaType>>{int_type, string_type},
        bool_type,
        dummy_impl
    );
    
    DispatchRegistry::instance().register_function(sig1);
    DispatchRegistry::instance().register_function(sig2);
    
    // Should be ambiguous
    auto result = DispatchRegistry::instance().resolve(
        "duplicate",
        std::vector<std::shared_ptr<MetaType>>{int_type, string_type}
    );
    
    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().find("Ambiguous") != std::string::npos);
}

// Test partial ordering - one signature more specific in some positions
TEST_F(DispatchTest, PartialOrderingAmbiguity) {
    // sig1: more specific in first position
    auto sig1 = std::make_shared<FunctionSignature>(
        "partial",
        std::vector<std::shared_ptr<MetaType>>{dog_type, animal_type, animal_type},
        string_type,
        dummy_impl
    );
    
    // sig2: more specific in second position
    auto sig2 = std::make_shared<FunctionSignature>(
        "partial",
        std::vector<std::shared_ptr<MetaType>>{animal_type, cat_type, animal_type},
        string_type,
        dummy_impl
    );
    
    // sig3: more specific in third position
    auto sig3 = std::make_shared<FunctionSignature>(
        "partial",
        std::vector<std::shared_ptr<MetaType>>{animal_type, animal_type, dog_type},
        string_type,
        dummy_impl
    );
    
    DispatchRegistry::instance().register_function(sig1);
    DispatchRegistry::instance().register_function(sig2);
    DispatchRegistry::instance().register_function(sig3);
    
    // Call with (Dog, Cat, Dog) - all three match, but none is most specific
    auto result = DispatchRegistry::instance().resolve(
        "partial",
        std::vector<std::shared_ptr<MetaType>>{dog_type, cat_type, dog_type}
    );
    
    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().find("Ambiguous") != std::string::npos);
}

// Test that unambiguous calls still work with many signatures
TEST_F(DispatchTest, UnambiguousWithManySignatures) {
    // Register many signatures
    auto sig1 = std::make_shared<FunctionSignature>(
        "many",
        std::vector<std::shared_ptr<MetaType>>{animal_type, animal_type},
        string_type,
        dummy_impl
    );
    
    auto sig2 = std::make_shared<FunctionSignature>(
        "many",
        std::vector<std::shared_ptr<MetaType>>{dog_type, animal_type},
        string_type,
        dummy_impl
    );
    
    auto sig3 = std::make_shared<FunctionSignature>(
        "many",
        std::vector<std::shared_ptr<MetaType>>{animal_type, cat_type},
        string_type,
        dummy_impl
    );
    
    auto sig4 = std::make_shared<FunctionSignature>(
        "many",
        std::vector<std::shared_ptr<MetaType>>{dog_type, cat_type},
        string_type,
        dummy_impl
    );
    
    DispatchRegistry::instance().register_function(sig1);
    DispatchRegistry::instance().register_function(sig2);
    DispatchRegistry::instance().register_function(sig3);
    DispatchRegistry::instance().register_function(sig4);
    
    // Call with (Dog, Cat) - should resolve to sig4 (most specific)
    auto result = DispatchRegistry::instance().resolve(
        "many",
        std::vector<std::shared_ptr<MetaType>>{dog_type, cat_type}
    );
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value()->param_types[0]->name(), "Dog");
    EXPECT_EQ(result.value()->param_types[1]->name(), "Cat");
}
