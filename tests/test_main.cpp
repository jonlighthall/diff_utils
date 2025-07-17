#include <gtest/gtest.h>

// Main function for running all tests
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);

    // Optional: Add custom test event listeners or configure test output
    std::cout << "Running uband_diff unit tests..." << std::endl;

    return RUN_ALL_TESTS();
}
