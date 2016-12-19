/*
 * Copyright (C) 2016 The Android Open Source Project
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

/**
 * A few tests of Math.abs for floating-point data.
 */
public class Main {

  /// CHECK-START: float Main.absSP(float) intrinsics_recognition (after)
  /// CHECK-DAG: <<Result:f\d+>> InvokeStaticOrDirect intrinsic:MathFloatAbs
  /// CHECK-DAG:                 Return [<<Result>>]
  private static float absSP(float f) {
    return Math.abs(f);
  }

  /// CHECK-START: double Main.absDP(double) intrinsics_recognition (after)
  /// CHECK-DAG: <<Result:d\d+>> InvokeStaticOrDirect intrinsic:MathDoubleAbs
  /// CHECK-DAG:                 Return [<<Result>>]
  private static double absDP(double d) {
    return Math.abs(d);
  }

  public static void main(String args[]) {
    // A few obvious numbers.
    for (float f = -100.0f; f < 0.0f; f += 0.5f) {
      expectEqualsSP(-f, absSP(f));
    }
    for (float f = 0.0f; f <= 100.0f; f += 0.5f) {
      expectEqualsSP(f, absSP(f));
    }
    for (float f = -1.5f; f <= -1.499f; f = Math.nextAfter(f, Float.POSITIVE_INFINITY)) {
      expectEqualsSP(-f, absSP(f));
    }
    for (float f = 1.499f; f <= 1.5f; f = Math.nextAfter(f, Float.POSITIVE_INFINITY)) {
      expectEqualsSP(f, absSP(f));
    }

    // Zero
    expectEqualsSP(Float.intBitsToFloat(0), +0.0f);
    expectEqualsSP(Float.intBitsToFloat(0), -0.0f);

    // Inf.
    expectEqualsSP(Float.POSITIVE_INFINITY, absSP(Float.NEGATIVE_INFINITY));
    expectEqualsSP(Float.POSITIVE_INFINITY, absSP(Float.POSITIVE_INFINITY));

    // A few NaN numbers.
    //
    // Note, as a "quality of implementation" rather than pure "spec compliance" we require that
    // Math.abs() clears the sign bit (but nothing else) for all NaN numbers. This requirement
    // should hold regardless of whether Art uses the interpreter, library, or compiler.
    //
    int[] spnans = {
      0x7f800001,
      0x7fa00000,
      0x7fc00000,
      0x7fffffff,
      0xff800001,
      0xffa00000,
      0xffc00000,
      0xffffffff
    };
    for (int i = 0; i < spnans.length; i++) {
      float f = Float.intBitsToFloat(spnans[i]);
      expectEquals32(spnans[i] & 0x7fffffff, Float.floatToRawIntBits(absSP(f)));
    }

    // A few obvious numbers.
    for (double d = -100.0; d < 0.0; d += 0.5) {
      expectEqualsDP(-d, absDP(d));
    }
    for (double d = 0.0; d <= 100.0; d += 0.5) {
      expectEqualsDP(d, absDP(d));
    }
    for (double d = -1.5d; d <= -1.49999999999d; d = Math.nextAfter(d, Double.POSITIVE_INFINITY)) {
      expectEqualsDP(-d, absDP(d));
    }
    for (double d = 1.49999999999d; d <= 1.5; d = Math.nextAfter(d, Double.POSITIVE_INFINITY)) {
      expectEqualsDP(d, absDP(d));
    }

    // Zero
    expectEqualsDP(Double.longBitsToDouble(0), +0.0);
    expectEqualsDP(Double.longBitsToDouble(0), -0.0);

    // Inf.
    expectEqualsDP(Double.POSITIVE_INFINITY, absDP(Double.NEGATIVE_INFINITY));
    expectEqualsDP(Double.POSITIVE_INFINITY, absDP(Double.POSITIVE_INFINITY));

    // A few NaN numbers.
    //
    // Note, as a "quality of implementation" rather than pure "spec compliance" we require that
    // Math.abs() clears the sign bit (but nothing else) for all NaN numbers. This requirement
    // should hold regardless of whether Art uses the interpreter, library, or compiler.
    //
    long[] dpnans = {
      0x7ff0000000000001L,
      0x7ff4000000000000L,
      0x7ff8000000000000L,
      0x7fffffffffffffffL,
      0xfff0000000000001L,
      0xfff4000000000000L,
      0xfff8000000000000L,
      0xffffffffffffffffL
    };
    for (int i = 0; i < dpnans.length; i++) {
      double d = Double.longBitsToDouble(dpnans[i]);
      expectEquals64(
          dpnans[i] & 0x7fffffffffffffffL,
          Double.doubleToRawLongBits(absDP(d)));
    }

    System.out.println("passed");
  }

  private static void expectEquals32(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: 0x" + Integer.toHexString(expected)
          + ", found: 0x" + Integer.toHexString(result));
    }
  }

  private static void expectEquals64(long expected, long result) {
    if (expected != result) {
      throw new Error("Expected: 0x" + Long.toHexString(expected)
          + ", found: 0x" + Long.toHexString(result));
    }
  }

  private static void expectEqualsSP(float expected, float result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  private static void expectEqualsDP(double expected, double result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }
}
