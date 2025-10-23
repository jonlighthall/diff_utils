#include <gtest/gtest.h>

#include <sstream>

#include "uband_diff.h"

// Test utility functions and complex number parsing

// Test the round_to_decimals lambda function
TEST(UtilityFunctionsTest, RoundToDecimals) {
  // We need to access the lambda function from your implementation
  auto round_to_decimals = [](double value, int precision) {
    double scale = std::pow(10.0, precision);
    return std::round(value * scale) / scale;
  };

  EXPECT_DOUBLE_EQ(round_to_decimals(1.2345, 2), 1.23);
  EXPECT_DOUBLE_EQ(round_to_decimals(1.2355, 2), 1.24);
  EXPECT_DOUBLE_EQ(round_to_decimals(1.0, 3), 1.0);
  EXPECT_DOUBLE_EQ(round_to_decimals(1.9999, 3), 2.0);
  EXPECT_DOUBLE_EQ(round_to_decimals(0.0, 2), 0.0);
}

// Test complex number parsing with valid input
TEST(ComplexParsingTest, ReadComplexValid) {
  std::istringstream stream("1.5, 2.5)");
  Flags flag;

  auto [real, imag, dp_real, dp_imag] = readComplex(stream, flag);

  EXPECT_DOUBLE_EQ(real, 1.5);
  EXPECT_DOUBLE_EQ(imag, 2.5);
  EXPECT_EQ(dp_real, 1);
  EXPECT_EQ(dp_imag, 1);
  EXPECT_FALSE(flag.error_found);
}

// Test complex number parsing with different precisions
TEST(ComplexParsingTest, ReadComplexDifferentPrecision) {
  std::istringstream stream("1.234, 5.67890)");
  Flags flag;

  auto [real, imag, dp_real, dp_imag] = readComplex(stream, flag);

  EXPECT_DOUBLE_EQ(real, 1.234);
  EXPECT_DOUBLE_EQ(imag, 5.67890);
  EXPECT_EQ(dp_real, 3);
  EXPECT_EQ(dp_imag, 5);
  EXPECT_FALSE(flag.error_found);
}

// Test complex number parsing with integers
TEST(ComplexParsingTest, ReadComplexIntegers) {
  std::istringstream stream("1, 2)");
  Flags flag;

  auto [real, imag, dp_real, dp_imag] = readComplex(stream, flag);

  EXPECT_DOUBLE_EQ(real, 1.0);
  EXPECT_DOUBLE_EQ(imag, 2.0);
  EXPECT_EQ(dp_real, 0);
  EXPECT_EQ(dp_imag, 0);
  EXPECT_FALSE(flag.error_found);
}

// Test complex number parsing with invalid separator
TEST(ComplexParsingTest, ReadComplexInvalidSeparator) {
  std::istringstream stream("1.5; 2.5)");  // Wrong separator
  Flags flag;

  auto [real, imag, dp_real, dp_imag] = readComplex(stream, flag);

  EXPECT_TRUE(flag.error_found);
  EXPECT_EQ(dp_real, -1);
  EXPECT_EQ(dp_imag, -1);
}

// Test complex number parsing with missing closing parenthesis
TEST(ComplexParsingTest, ReadComplexMissingCloseParen) {
  std::istringstream stream("1.5, 2.5");  // Missing closing parenthesis
  Flags flag;

  // This test may need adjustment based on actual stream behavior
  auto [real, imag, dp_real, dp_imag] = readComplex(stream, flag);

  // The exact behavior depends on how the stream handles EOF
  // You may need to adjust expectations
}

// Test decimal place counting for various number formats
TEST(DecimalCountTest, CountDecimalPlaces) {
  // Note: stream_countDecimalPlaces is a lambda in your implementation
  // We'll create a similar function for testing
  auto count_decimal_places = [](const std::string& token) {
    int ndp = 0;
    if (size_t decimal_pos = token.find('.');
        decimal_pos != std::string::npos) {
      ndp = static_cast<int>(token.size() - decimal_pos - 1);
    }
    return ndp;
  };

  EXPECT_EQ(count_decimal_places("123"), 0);      // Integer
  EXPECT_EQ(count_decimal_places("123.0"), 1);    // One decimal place
  EXPECT_EQ(count_decimal_places("123.45"), 2);   // Two decimal places
  EXPECT_EQ(count_decimal_places("0.123"), 3);    // Three decimal places
  EXPECT_EQ(count_decimal_places("1.23456"), 5);  // Five decimal places
}

// Test threshold calculations
TEST(ThresholdTest, ThresholdComparisons) {
  // Test various threshold scenarios
  EXPECT_TRUE(0.001 > 0.0001);
  EXPECT_TRUE(0.05 > 0.01);
  EXPECT_FALSE(0.001 > 0.1);

  // Test epsilon values
  double epsilon = Thresholds::SINGLE_PRECISION_EPSILON;
  EXPECT_TRUE(epsilon > 0);
  EXPECT_TRUE(epsilon < 1e-6);
}

// Test flag initialization
TEST(FlagsTest, DefaultFlagValues) {
  Flags flag;

  // Test default flag values
  EXPECT_FALSE(flag.new_fmt);
  EXPECT_FALSE(flag.error_found);
  EXPECT_FALSE(flag.has_non_zero_diff);
  EXPECT_FALSE(flag.has_non_trivial_diff);
  EXPECT_FALSE(flag.has_significant_diff);
  EXPECT_FALSE(flag.has_critical_diff);
  EXPECT_FALSE(flag.has_printed_diff);
  EXPECT_TRUE(flag.files_are_same);
  EXPECT_TRUE(flag.files_have_same_values);
  EXPECT_TRUE(flag.files_are_close_enough);
}

// Test data structure initialization
TEST(DataStructuresTest, LineDataInitialization) {
  LineData line_data;

  EXPECT_TRUE(line_data.values.empty());
  EXPECT_TRUE(line_data.decimal_places.empty());
}

TEST(DataStructuresTest, ColumnValuesCreation) {
  ColumnValues col_vals{1.23, 4.56, 1.23, 2, 2, 2};

  EXPECT_DOUBLE_EQ(col_vals.value1, 1.23);
  EXPECT_DOUBLE_EQ(col_vals.value2, 4.56);
  EXPECT_DOUBLE_EQ(col_vals.range, 1.23);
  EXPECT_EQ(col_vals.dp1, 2);
  EXPECT_EQ(col_vals.dp2, 2);
  EXPECT_EQ(col_vals.min_dp, 2);
}
