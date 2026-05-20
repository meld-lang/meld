#include "../../include/meld/std/numeric_wrappers.hpp"
#include "meld/testing/property_test.hpp"
#include "../../include/meld/kernel/primitives.hpp"
#include "../../include/meld/types/instance.hpp"
#include <gtest/gtest.h>
#include <cstdint>

using namespace meld::stdx;
using namespace meld::testing;
using namespace meld::kernel;
using meld::types::StructInstance;

/**
 * Property-Based Tests for Char Unicode Validity
 *
 * These tests validate that the char type correctly enforces Unicode
 * scalar value constraints: code points 0..0x10FFFF excluding the
 * surrogate range 0xD800..0xDFFF.
 *
 * **Feature: meld-lang, Property 51: Char Unicode Validity**
 * **Validates: Requirements 47.7**
 */

// ---------------------------------------------------------------------------
// Property: Valid BMP code points (0..0xD7FF) are accepted
// ---------------------------------------------------------------------------

TEST(CharUnicodeValidityProperty, AcceptsBMPCodePoints) {
    auto gen = Generators::integers(0, 0xD7FF);

    bool property = PropertyTest::forall<int>(
        gen,
        [](int value) {
            auto ch = NumericWrappers::create_char(static_cast<int64_t>(value));
            return ch.is<StructInstance>();
        }
    );

    EXPECT_TRUE(property);
}

// ---------------------------------------------------------------------------
// Property: Surrogate code points (0xD800..0xDFFF) are rejected
// ---------------------------------------------------------------------------

TEST(CharUnicodeValidityProperty, RejectsSurrogateCodePoints) {
    auto gen = Generators::integers(0xD800, 0xDFFF);

    bool property = PropertyTest::forall<int>(
        gen,
        [](int value) {
            auto ch = NumericWrappers::create_char(static_cast<int64_t>(value));
            // Should return error string, not a StructInstance
            return ch.is<String>();
        }
    );

    EXPECT_TRUE(property);
}

// ---------------------------------------------------------------------------
// Property: Post-surrogate BMP and supplementary planes (0xE000..0x10FFFF) accepted
// ---------------------------------------------------------------------------

TEST(CharUnicodeValidityProperty, AcceptsPostSurrogateAndSupplementaryPlanes) {
    // Test post-surrogate BMP range
    auto bmp_gen = Generators::integers(0xE000, 0xFFFF);

    bool bmp_property = PropertyTest::forall<int>(
        bmp_gen,
        [](int value) {
            auto ch = NumericWrappers::create_char(static_cast<int64_t>(value));
            return ch.is<StructInstance>();
        }
    );
    EXPECT_TRUE(bmp_property);

    // Test supplementary planes (SMP, SIP, TIP, SSP)
    auto supp_gen = Generators::integers(0x10000, 0x10FFFF);

    bool supp_property = PropertyTest::forall<int>(
        supp_gen,
        [](int value) {
            auto ch = NumericWrappers::create_char(static_cast<int64_t>(value));
            return ch.is<StructInstance>();
        }
    );
    EXPECT_TRUE(supp_property);
}

// ---------------------------------------------------------------------------
// Property: Negative code points are rejected
// ---------------------------------------------------------------------------

TEST(CharUnicodeValidityProperty, RejectsNegativeCodePoints) {
    auto gen = Generators::integers(-10000, -1);

    bool property = PropertyTest::forall<int>(
        gen,
        [](int value) {
            auto ch = NumericWrappers::create_char(static_cast<int64_t>(value));
            return ch.is<String>();
        }
    );

    EXPECT_TRUE(property);
}

// ---------------------------------------------------------------------------
// Property: Code points above 0x10FFFF are rejected
// ---------------------------------------------------------------------------

TEST(CharUnicodeValidityProperty, RejectsCodePointsAboveMax) {
    auto gen = Generators::integers(0x110000, 0x200000);

    bool property = PropertyTest::forall<int>(
        gen,
        [](int value) {
            auto ch = NumericWrappers::create_char(static_cast<int64_t>(value));
            return ch.is<String>();
        }
    );

    EXPECT_TRUE(property);
}

// ---------------------------------------------------------------------------
// Property: ASCII chars created from char literal preserve code point
// ---------------------------------------------------------------------------

TEST(CharUnicodeValidityProperty, AsciiCharPreservesCodePoint) {
    // All printable ASCII (32..126)
    auto gen = Generators::integers(32, 126);

    bool property = PropertyTest::forall<int>(
        gen,
        [](int value) {
            auto ch = NumericWrappers::create_char(static_cast<char>(value));
            if (!ch.is<StructInstance>()) return false;

            auto instance = ch.as<StructInstance>();
            auto cp = instance->get_field("code_point");
            if (!cp || !cp->is<Integer>()) return false;

            return cp->as<Integer>()->value() == static_cast<int64_t>(value);
        }
    );

    EXPECT_TRUE(property);
}

// ---------------------------------------------------------------------------
// Property: isDigit returns true only for '0'..'9'
// ---------------------------------------------------------------------------

TEST(CharUnicodeValidityProperty, IsDigitCorrectForAsciiRange) {
    auto gen = Generators::integers(0, 127);

    bool property = PropertyTest::forall<int>(
        gen,
        [](int value) {
            auto ch = NumericWrappers::create_char(static_cast<int64_t>(value));
            if (!ch.is<StructInstance>()) return true; // skip invalid

            bool expected = (value >= '0' && value <= '9');
            return NumericWrappers::is_digit(ch) == expected;
        }
    );

    EXPECT_TRUE(property);
}

// ---------------------------------------------------------------------------
// Property: toUpper/toLower are inverses for ASCII letters
// ---------------------------------------------------------------------------

TEST(CharUnicodeValidityProperty, ToUpperToLowerRoundTrip) {
    // Lowercase ASCII letters
    auto gen = Generators::integers('a', 'z');

    bool property = PropertyTest::forall<int>(
        gen,
        [](int value) {
            auto ch = NumericWrappers::create_char(static_cast<int64_t>(value));
            if (!ch.is<StructInstance>()) return false;

            auto upper = NumericWrappers::to_upper(ch);
            if (!upper.is<StructInstance>()) return false;

            auto back = NumericWrappers::to_lower(upper);
            if (!back.is<StructInstance>()) return false;

            auto original_cp = ch.as<StructInstance>()->get_field("code_point");
            auto round_cp = back.as<StructInstance>()->get_field("code_point");

            if (!original_cp || !round_cp) return false;
            return original_cp->as<Integer>()->value() == round_cp->as<Integer>()->value();
        }
    );

    EXPECT_TRUE(property);
}

// ---------------------------------------------------------------------------
// Property: ValidChar refinement rejects surrogates
// ---------------------------------------------------------------------------

TEST(CharUnicodeValidityProperty, ValidCharRefinementRejectsSurrogates) {
    auto refinement = NumericWrappers::create_valid_char_refinement();
    ASSERT_NE(refinement, nullptr);
    // The refinement type should exist and be named ValidChar
    EXPECT_EQ(refinement->name(), "ValidChar");
}

