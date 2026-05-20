#include "../../include/meld/std/numeric_wrappers.hpp"
#include "meld/testing/property_test.hpp"
#include "../../include/meld/kernel/primitives.hpp"
#include "../../include/meld/meta/metatype.hpp"
#include "../../include/meld/types/instance.hpp"
#include <gtest/gtest.h>
#include <cstdint>
#include <string>
#include <map>

using namespace meld::stdx;
using namespace meld::testing;
using namespace meld::kernel;
using meld::types::StructInstance;
using namespace meld::meta;

/**
 * Property-Based Tests for Numeric Wrapper Transpilation
 *
 * These tests validate that numeric wrapper structs carry correct
 * @transpile_as annotations mapping to native types in each target
 * language, and that @value annotations are present for copy semantics.
 *
 * **Feature: meld-lang, Property 49: Numeric Wrapper Transpilation**
 * **Validates: Requirements 47.4**
 */

// ---------------------------------------------------------------------------
// Helper: verify a wrapper struct has the expected transpile_as mappings
// ---------------------------------------------------------------------------

static void verify_transpile_as(
    const std::shared_ptr<StructMetaType>& type,
    const std::string& type_name,
    const std::string& expected_cpp,
    const std::string& expected_java,
    const std::string& expected_go
) {
    ASSERT_NE(type, nullptr) << type_name << " struct should not be null";
    EXPECT_TRUE(type->has_annotation("transpile_as"))
        << type_name << " should have @transpile_as annotation";

    auto ann = type->get_annotation("transpile_as");
    ASSERT_TRUE(ann.has_value()) << type_name << " @transpile_as lookup failed";

    auto& mappings = *ann;
    EXPECT_EQ(mappings.at("cpp"), expected_cpp)
        << type_name << " C++ mapping mismatch";
    EXPECT_EQ(mappings.at("java"), expected_java)
        << type_name << " Java mapping mismatch";
    EXPECT_EQ(mappings.at("go"), expected_go)
        << type_name << " Go mapping mismatch";
}

// ---------------------------------------------------------------------------
// Property: Every numeric wrapper struct has @transpile_as with correct native types
// ---------------------------------------------------------------------------

TEST(NumericWrapperTranspilationProperty, U8MapsToNativeTypes) {
    auto u8_type = NumericWrappers::create_u8_struct();
    verify_transpile_as(u8_type, "u8", "uint8_t", "byte", "uint8");
}

TEST(NumericWrapperTranspilationProperty, U16MapsToNativeTypes) {
    auto u16_type = NumericWrappers::create_u16_struct();
    verify_transpile_as(u16_type, "u16", "uint16_t", "short", "uint16");
}

TEST(NumericWrapperTranspilationProperty, U32MapsToNativeTypes) {
    auto u32_type = NumericWrappers::create_u32_struct();
    verify_transpile_as(u32_type, "u32", "uint32_t", "int", "uint32");
}

TEST(NumericWrapperTranspilationProperty, U64MapsToNativeTypes) {
    auto u64_type = NumericWrappers::create_u64_struct();
    verify_transpile_as(u64_type, "u64", "uint64_t", "long", "uint64");
}

TEST(NumericWrapperTranspilationProperty, I8MapsToNativeTypes) {
    auto i8_type = NumericWrappers::create_i8_struct();
    verify_transpile_as(i8_type, "i8", "int8_t", "byte", "int8");
}

TEST(NumericWrapperTranspilationProperty, I16MapsToNativeTypes) {
    auto i16_type = NumericWrappers::create_i16_struct();
    verify_transpile_as(i16_type, "i16", "int16_t", "short", "int16");
}

TEST(NumericWrapperTranspilationProperty, CharMapsToNativeTypes) {
    auto char_type = NumericWrappers::create_char_struct();
    verify_transpile_as(char_type, "char", "char32_t", "int", "rune");
}

TEST(NumericWrapperTranspilationProperty, BufferMapsToNativeByteArrays) {
    auto buffer_type = NumericWrappers::create_buffer_struct();
    verify_transpile_as(buffer_type, "Buffer",
                        "std::vector<uint8_t>", "byte[]", "[]byte");
}

// ---------------------------------------------------------------------------
// Property: Every numeric wrapper struct has @value annotation (copy semantics)
// ---------------------------------------------------------------------------

TEST(NumericWrapperTranspilationProperty, AllWrappersHaveValueAnnotation) {
    struct WrapperFactory {
        std::string name;
        std::shared_ptr<StructMetaType> (*create)();
    };

    // Use function pointers to avoid lambda-to-function-pointer issues
    auto factories = std::vector<std::pair<std::string, std::shared_ptr<StructMetaType>>>{
        {"u8",     NumericWrappers::create_u8_struct()},
        {"u16",    NumericWrappers::create_u16_struct()},
        {"u32",    NumericWrappers::create_u32_struct()},
        {"u64",    NumericWrappers::create_u64_struct()},
        {"i8",     NumericWrappers::create_i8_struct()},
        {"i16",    NumericWrappers::create_i16_struct()},
        {"char",   NumericWrappers::create_char_struct()},
        {"Buffer", NumericWrappers::create_buffer_struct()},
    };

    for (const auto& [name, type] : factories) {
        EXPECT_TRUE(type->has_annotation("value"))
            << name << " should have @value annotation for copy-by-value semantics";
    }
}

// ---------------------------------------------------------------------------
// Property: Created wrapper instances preserve their type identity
// ---------------------------------------------------------------------------

TEST(NumericWrapperTranspilationProperty, CreatedInstancesPreserveTypeIdentity) {
    // For each valid value in range, the created instance should be a StructInstance
    auto gen = Generators::integers(0, 255);

    bool property = PropertyTest::forall<int>(
        gen,
        [](int value) {
            auto u8_val = NumericWrappers::create_u8(static_cast<int64_t>(value));
            if (!u8_val.is<StructInstance>()) return false;

            auto instance = u8_val.as<StructInstance>();
            auto bits = instance->get_field("bits");
            if (!bits || !bits->is<Integer>()) return false;

            return bits->as<Integer>()->value() == static_cast<int64_t>(value);
        }
    );

    EXPECT_TRUE(property);
}

TEST(NumericWrapperTranspilationProperty, U16InstancesPreserveBits) {
    auto gen = Generators::integers(0, 65535);

    bool property = PropertyTest::forall<int>(
        gen,
        [](int value) {
            auto u16_val = NumericWrappers::create_u16(static_cast<int64_t>(value));
            if (!u16_val.is<StructInstance>()) return false;

            auto instance = u16_val.as<StructInstance>();
            auto bits = instance->get_field("bits");
            if (!bits || !bits->is<Integer>()) return false;

            return bits->as<Integer>()->value() == static_cast<int64_t>(value);
        }
    );

    EXPECT_TRUE(property);
}

// ---------------------------------------------------------------------------
// Property: Transpile mappings are consistent across repeated creation
// ---------------------------------------------------------------------------

TEST(NumericWrapperTranspilationProperty, TranspileMappingsAreIdempotent) {
    // Creating the same struct type twice should yield identical annotations
    auto u8_a = NumericWrappers::create_u8_struct();
    auto u8_b = NumericWrappers::create_u8_struct();

    auto ann_a = u8_a->get_annotation("transpile_as");
    auto ann_b = u8_b->get_annotation("transpile_as");

    ASSERT_TRUE(ann_a.has_value());
    ASSERT_TRUE(ann_b.has_value());
    EXPECT_EQ(*ann_a, *ann_b);
}

