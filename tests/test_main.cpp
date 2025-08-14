#include <gtest/gtest.h>

#include <iostream>

// Main function for the test executable
int main(int argc, char **argv) {
  std::cout << "Running Magic Folder Test Suite..." << std::endl;

  ::testing::InitGoogleTest(&argc, argv);

  // Fixtures reinitialize/clear the database per-test; no global setup required here
  int result = RUN_ALL_TESTS();

  if (result == 0) {
    std::cout << "All tests passed!" << std::endl;
  } else {
    std::cout << "Some tests failed. Check output above for details." << std::endl;
  }

  return result;
}