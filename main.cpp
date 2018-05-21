/*
 * main.cpp
 *
 *  Created on: May 11, 2018
 *      Author: kevin
 */
#include "gtest/gtest.h"

int main(int argc, char **argv) {
  printf("Running main() from gtest_main.cc\n");
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}


