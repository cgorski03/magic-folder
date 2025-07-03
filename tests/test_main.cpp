#include <gtest/gtest.h>

#include <iostream>

// Main function for the test executable
// This will run all tests that are linked into this executable
int main(int argc, char **argv) {
  std::cout << "Running Magic Folder Test Suite..." << std::endl;

  // Initialize Google Test
  ::testing::InitGoogleTest(&argc, argv);

  // Run all tests
  int result = RUN_ALL_TESTS();

  if (result == 0) {
    std::cout << "All tests passed!" << std::endl;
  } else {
    std::cout << "Some tests failed. Check output above for details." << std::endl;
  }

  return result;
}