/*
* Copyright (C) 2015 The Android Open Source Project
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

public class Main {

  public static void assertIntEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static boolean inlineTrue() {
    return true;
  }

  public static boolean inlineFalse() {
    return false;
  }
 
  // CHECK-START: int Main.testTrueBranch(int, int) dead_code_elimination_final (before)
  // CHECK-DAG:     [[ArgX:i\d+]]    ParameterValue
  // CHECK-DAG:     [[ArgY:i\d+]]    ParameterValue
  // CHECK-DAG:                      If
  // CHECK-DAG:     [[Add:i\d+]]     Add [ [[ArgX]] [[ArgY]] ]
  // CHECK-DAG:     [[Sub:i\d+]]     Sub [ [[ArgX]] [[ArgY]] ]
  // CHECK-DAG:     [[Phi:i\d+]]     Phi [ [[Add]] [[Sub]] ]
  // CHECK-DAG:                      Return [ [[Phi]] ]

  // CHECK-START: int Main.testTrueBranch(int, int) dead_code_elimination_final (after)
  // CHECK-DAG:     [[ArgX:i\d+]]    ParameterValue
  // CHECK-DAG:     [[ArgY:i\d+]]    ParameterValue
  // CHECK-DAG:     [[Add:i\d+]]     Add [ [[ArgX]] [[ArgY]] ]
  // CHECK-DAG:                      Return [ [[Add]] ]

  // CHECK-START: int Main.testTrueBranch(int, int) dead_code_elimination_final (after)
  // CHECK-NOT:                      If
  // CHECK-NOT:                      Sub
  // CHECK-NOT:                      Phi

  public static int testTrueBranch(int x, int y) {
    int z;
    if (inlineTrue()) {
      z = x + y;
    } else {
      z = x - y;
    }
    return z;
  }

  // CHECK-START: int Main.testFalseBranch(int, int) dead_code_elimination_final (before)
  // CHECK-DAG:     [[ArgX:i\d+]]    ParameterValue
  // CHECK-DAG:     [[ArgY:i\d+]]    ParameterValue
  // CHECK-DAG:                      If
  // CHECK-DAG:     [[Add:i\d+]]     Add [ [[ArgX]] [[ArgY]] ]
  // CHECK-DAG:     [[Sub:i\d+]]     Sub [ [[ArgX]] [[ArgY]] ]
  // CHECK-DAG:     [[Phi:i\d+]]     Phi [ [[Add]] [[Sub]] ]
  // CHECK-DAG:                      Return [ [[Phi]] ]

  // CHECK-START: int Main.testFalseBranch(int, int) dead_code_elimination_final (after)
  // CHECK-DAG:     [[ArgX:i\d+]]    ParameterValue
  // CHECK-DAG:     [[ArgY:i\d+]]    ParameterValue
  // CHECK-DAG:     [[Sub:i\d+]]     Sub [ [[ArgX]] [[ArgY]] ]
  // CHECK-DAG:                      Return [ [[Sub]] ]

  // CHECK-START: int Main.testFalseBranch(int, int) dead_code_elimination_final (after)
  // CHECK-NOT:                      If
  // CHECK-NOT:                      Add
  // CHECK-NOT:                      Phi

  public static int testFalseBranch(int x, int y) {
    int z;
    if (inlineFalse()) {
      z = x + y;
    } else {
      z = x - y;
    }
    return z;
  }

  // CHECK-START: int Main.testRemoveLoop(int) dead_code_elimination_final (before)
  // CHECK:                          Mul

  // CHECK-START: int Main.testRemoveLoop(int) dead_code_elimination_final (after)
  // CHECK-NOT:                      Mul

  public static int testRemoveLoop(int x) {
    if (inlineFalse()) {
      for (int i = 0; i < x; ++i) {
        x *= x;
      }
    }
    return x;
  }

  // CHECK-START: int Main.testInfiniteLoop(int) dead_code_elimination_final (before)
  // CHECK-DAG:                      Return
  // CHECK-DAG:                      Exit

  // CHECK-START: int Main.testInfiniteLoop(int) dead_code_elimination_final (after)
  // CHECK-NOT:                      Return
  // CHECK-NOT:                      Exit

  public static int testInfiniteLoop(int x) {
    while (inlineTrue()) {
      x++;
    }
    return x;
  }

  public static void main(String[] args) {
    assertIntEquals(7, testTrueBranch(4, 3));
    assertIntEquals(1, testFalseBranch(4, 3));
    assertIntEquals(42, testRemoveLoop(42));
  }
}
