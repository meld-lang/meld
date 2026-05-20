#include "../../include/meld/std/numeric_wrappers.hpp"
#include "meld/testing/property_test.hpp"
#include "../../include/meld/kernel/primitives.hpp"
#include "../../include/meld/types/result.hpp"
#include "../../include/meld/types/refinement_runtime.hpp"
#include "../../include/meld/types/instance.hpp"
#include <gtest/gtest.h>
#include <cstdint>
#include <limits>

using namespace meld::stdx;
using namespace meld::testing;
using namespace meld::kernel;
using meld::types::StructInstance;
using namespace meld::types;

/**
 * Property-Based Tests for Unsigned Type Safety
 *
 * These tests validate that unsigned types correctly enforce non-negative
 * constraints and bit-width bounds, as implemented via refinement types
 * and struct wrappers.
 *
 * **Feature: meld-lang, Property 48: Unsigned Type Safety**
 * **Validates: Requirements 47.2, 47.9**
 */

// ---------------------------------------------------------------------------
// Property: uint refinement type rejects negative values
// ---------------------------------------------------------------------------

TEST(UnsignedTypeSafetyProperty, UintRejectsNegativeValues) {
    // Generate negative integers only
    auto neg_gen = Generators::integers(-10000, -1);

    bool property = PropertyTest::forall<int>(
        neg_gen,
        [](int value) {
            auto result = NumericWrappers::validate_uint(static_cast<int64_t>(value));
            return result.is_error();
        }
    );

    EXPECT_TRUE(property);
}

TEST(UnsignedTypeSafetyProperty, UintAcceptsNonNegativeValues) {
    // Generate non-negative integers
    auto pos_gen = Generators::integers(0, 10000);

    bool property = PropertyTest::forall<int>(
        pos_gen,
        [](int value) {
            auto result = NumericWrappers::validate_uint(static_cast<int64_t>(value));
            return result.is_success();
        }
    );

    EXPECT_TRUE(property);
}

// ---------------------------------------------------------------------------
// Property: u8 values stay within 0-255 range
// ---------------------------------------------------------------------------

TEST(UnsignedTypeSafetyProperty, U8RejectsOutOfRangeValues) {
    // Values below 0
    auto neg_gen = Generators::integers(-10000, -1);

    bool below_property = PropertyTest::forall<int>(
        neg_gen,
        [](int value) {
            auto v = NumericWrappers::create_u8(static_cast<int64_t>(value));
            // Out-of-range creation returns a String error value
            return v.is<String>();
        }
    );

    // Values above 255
    auto high_gen = Generators::integers(256, 10000);

    bool above_property = PropertyTest::forall<int>(
        high_gen,
        [](int value) {
            auto v = NumericWrappers::create_u8(static_cast<int64_t>(value));
            return v.is<String>();
        }
    );

    EXPECT_TRUE(below_property);
    EXPECT_TRUE(above_property);
}

TEST(UnsignedTypeSafetyProperty, U8AcceptsValidRange) {
    auto gen = Generators::integers(0, 255);

    bool property = PropertyTest::forall<int>(
        gen,
        [](int value) {
            auto v = NumericWrappers::create_u8(static_cast<int64_t>(value));
            return v.is<StructInstance>();
        }
    );

    EXPECT_TRUE(property);
}

// ---------------------------------------------------------------------------
// Property: u16 values stay within 0-65535 range
// ---------------------------------------------------------------------------

TEST(UnsignedTypeSafetyProperty, U16RejectsOutOfRangeValues) {
    auto neg_gen = Generators::integers(-10000, -1);

    bool below_property = PropertyTest::forall<int>(
        neg_gen,
        [](int value) {
            auto v = NumericWrappers::create_u16(static_cast<int64_t>(value));
            return v.is<String>();
        }
    );

    // Values above 65535 — use a custom generator for large values
    auto high_gen = []() -> int {
        static std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<int> dist(65536, 100000);
        return dist(rng);
    };

    bool above_property = PropertyTest::forall<int>(
        high_gen,
        [](int value) {
            auto v = NumericWrappers::create_u16(static_cast<int64_t>(value));
            return v.is<String>();
        }
    );

    EXPECT_TRUE(below_property);
    EXPECT_TRUE(above_property);
}

TEST(UnsignedTypeSafetyProperty, U16AcceptsValidRange) {
    auto gen = Generators::integers(0, 65535);

    bool property = PropertyTest::forall<int>(
        gen,
        [](int value) {
            auto v = NumericWrappers::create_u16(static_cast<int64_t>(value));
            return v.is<StructInstance>();
        }
    );

    EXPECT_TRUE(property);
}

// ---------------------------------------------------------------------------
// Property: u32 values stay within 0-4294967295 range
// ---------------------------------------------------------------------------

TEST(UnsignedTypeSafetyProperty, U32RejectsNegativeValues) {
    auto neg_gen = Generators::integers(-10000, -1);

    bool property = PropertyTest::forall<int>(
        neg_gen,
        [](int value) {
            auto v = NumericWrappers::create_u32(static_cast<int64_t>(value));
            return v.is<String>();
        }
    );

    EXPECT_TRUE(property);
}

TEST(UnsignedTypeSafetyProperty, U32AcceptsValidRange) {
    // Test values within u32 range (use a custom generator for large values)
    auto gen = []() -> int {
        static std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<int> dist(0, 1000000);
        return dist(rng);
    };

    bool property = PropertyTest::forall<int>(
        gen,
        [](int value) {
            auto v = NumericWrappers::create_u32(static_cast<int64_t>(value));
            return v.is<StructInstance>();
        }
    );

    EXPECT_TRUE(property);
}

// ---------------------------------------------------------------------------
// Property: u64 values stay within valid range (non-negative)
// ---------------------------------------------------------------------------

TEST(UnsignedTypeSafetyProperty, U64RejectsNegativeValues) {
    auto neg_gen = Generators::integers(-10000, -1);

    bool property = PropertyTest::forall<int>(
        neg_gen,
        [](int value) {
            auto v = NumericWrappers::create_u64(static_cast<int64_t>(value));
            return v.is<String>();
        }
    );

    EXPECT_TRUE(property);
}

TEST(UnsignedTypeSafetyProperty, U64AcceptsNonNegativeValues) {
    auto gen = Generators::integers(0, 10000);

    bool property = PropertyTest::forall<int>(
        gen,
        [](int value) {
            auto v = NumericWrappers::create_u64(static_cast<int64_t>(value));
            return v.is<StructInstance>();
        }
    );

    EXPECT_TRUE(property);
}

// ---------------------------------------------------------------------------
// Property: Arithmetic on unsigned types maintains bounds
// Bitwise operations on valid unsigned values produce valid results
// ---------------------------------------------------------------------------

TEST(UnsignedTypeSafetyProperty, BitwiseAndPreservesBounds) {
    auto gen = Generators::integers(0, 255);

    bool property = PropertyTest::forall(
        gen, gen,
        [](int a, int b) {
            auto va = Value(std::make_shared<Integer>(static_cast<int64_t>(a)));
            auto vb = Value(std::make_shared<Integer>(static_cast<int64_t>(b)));
            auto result = NumericWrappers::bitwise_and(va, vb);

            if (!result.is<Integer>()) return false;
            int64_t r = result.as<Integer>()->value();
            // AND of two u8-range values must stay in u8 range
            return r >= 0 && r <= 255;
        }
    );

    EXPECT_TRUE(property);
}

TEST(UnsignedTypeSafetyProperty, BitwiseOrPreservesBounds) {
    auto gen = Generators::integers(0, 255);

    bool property = PropertyTest::forall(
        gen, gen,
        [](int a, int b) {
            auto va = Value(std::make_shared<Integer>(static_cast<int64_t>(a)));
            auto vb = Value(std::make_shared<Integer>(static_cast<int64_t>(b)));
            auto result = NumericWrappers::bitwise_or(va, vb);

            if (!result.is<Integer>()) return false;
            int64_t r = result.as<Integer>()->value();
            return r >= 0 && r <= 255;
        }
    );

    EXPECT_TRUE(property);
}

TEST(UnsignedTypeSafetyProperty, BitwiseXorPreservesBounds) {
    auto gen = Generators::integers(0, 255);

    bool property = PropertyTest::forall(
        gen, gen,
        [](int a, int b) {
            auto va = Value(std::make_shared<Integer>(static_cast<int64_t>(a)));
            auto vb = Value(std::make_shared<Integer>(static_cast<int64_t>(b)));
            auto result = NumericWrappers::bitwise_xor(va, vb);

            if (!result.is<Integer>()) return false;
            int64_t r = result.as<Integer>()->value();
            return r >= 0 && r <= 255;
        }
    );

    EXPECT_TRUE(property);
}

TEST(UnsignedTypeSafetyProperty, LogicalRightShiftProducesNonNegative) {
    auto val_gen = Generators::integers(0, 10000);
    auto shift_gen = Generators::integers(0, 63);

    bool property = PropertyTest::forall(
        val_gen, shift_gen,
        [](int value, int shift) {
            auto v = Value(std::make_shared<Integer>(static_cast<int64_t>(value)));
            auto result = NumericWrappers::logical_right_shift(v, shift);

            if (!result.is<Integer>()) return false;
            int64_t r = result.as<Integer>()->value();
            // Logical right shift of a non-negative value is always non-negative
            return r >= 0;
        }
    );

    EXPECT_TRUE(property);
}

// ---------------------------------------------------------------------------
// Property: Type conversion safety between different unsigned widths
// A value valid for a narrower type is also valid for a wider type
// ---------------------------------------------------------------------------

TEST(UnsignedTypeSafetyProperty, U8ValidImpliesU16Valid) {
    auto gen = Generators::integers(0, 255);

    bool property = PropertyTest::forall<int>(
        gen,
        [](int value) {
            auto v8 = NumericWrappers::create_u8(static_cast<int64_t>(value));
            auto v16 = NumericWrappers::create_u16(static_cast<int64_t>(value));
            // If u8 accepts it, u16 must also accept it
            return v8.is<StructInstance>() && v16.is<StructInstance>();
        }
    );

    EXPECT_TRUE(property);
}

TEST(UnsignedTypeSafetyProperty, U16ValidImpliesU32Valid) {
    auto gen = Generators::integers(0, 65535);

    bool property = PropertyTest::forall<int>(
        gen,
        [](int value) {
            auto v16 = NumericWrappers::create_u16(static_cast<int64_t>(value));
            auto v32 = NumericWrappers::create_u32(static_cast<int64_t>(value));
            return v16.is<StructInstance>() && v32.is<StructInstance>();
        }
    );

    EXPECT_TRUE(property);
}

TEST(UnsignedTypeSafetyProperty, U32ValidImpliesU64Valid) {
    auto gen = Generators::integers(0, 1000000);

    bool property = PropertyTest::forall<int>(
        gen,
        [](int value) {
            auto v32 = NumericWrappers::create_u32(static_cast<int64_t>(value));
            auto v64 = NumericWrappers::create_u64(static_cast<int64_t>(value));
            return v32.is<StructInstance>() && v64.is<StructInstance>();
        }
    );

    EXPECT_TRUE(property);
}

TEST(UnsignedTypeSafetyProperty, AllUnsignedValidImpliesUintValid) {
    // Any value accepted by any unsigned type must also pass uint validation
    auto gen = Generators::integers(0, 10000);

    bool property = PropertyTest::forall<int>(
        gen,
        [](int value) {
            auto uint_result = NumericWrappers::validate_uint(static_cast<int64_t>(value));
            auto u64_val = NumericWrappers::create_u64(static_cast<int64_t>(value));
            // Both uint refinement and u64 wrapper must accept non-negative values
            return uint_result.is_success() && u64_val.is<StructInstance>();
        }
    );

    EXPECT_TRUE(property);
}

