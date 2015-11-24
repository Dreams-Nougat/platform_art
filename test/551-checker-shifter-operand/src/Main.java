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

  // A dummy value to defeat inlining of these routines.
  static boolean doThrow = false;

  public static void assertByteEquals(byte expected, byte result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void assertCharEquals(char expected, char result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void assertShortEquals(short expected, short result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void assertIntEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void assertLongEquals(long expected, long result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  // Non-inlinable type-casting helpers.
  static  char $noinline$byteToChar   (byte v) { if (doThrow) throw new Error(); return  (char)v; }
  static short $noinline$byteToShort  (byte v) { if (doThrow) throw new Error(); return (short)v; }
  static   int $noinline$byteToInt    (byte v) { if (doThrow) throw new Error(); return   (int)v; }
  static  long $noinline$byteToLong   (byte v) { if (doThrow) throw new Error(); return  (long)v; }
  static  byte $noinline$charToByte   (char v) { if (doThrow) throw new Error(); return  (byte)v; }
  static short $noinline$charToShort  (char v) { if (doThrow) throw new Error(); return (short)v; }
  static   int $noinline$charToInt    (char v) { if (doThrow) throw new Error(); return   (int)v; }
  static  long $noinline$charToLong   (char v) { if (doThrow) throw new Error(); return  (long)v; }
  static  byte $noinline$shortToByte (short v) { if (doThrow) throw new Error(); return  (byte)v; }
  static  char $noinline$shortToChar (short v) { if (doThrow) throw new Error(); return  (char)v; }
  static   int $noinline$shortToInt  (short v) { if (doThrow) throw new Error(); return   (int)v; }
  static  long $noinline$shortToLong (short v) { if (doThrow) throw new Error(); return  (long)v; }
  static  byte $noinline$intToByte     (int v) { if (doThrow) throw new Error(); return  (byte)v; }
  static  char $noinline$intToChar     (int v) { if (doThrow) throw new Error(); return  (char)v; }
  static short $noinline$intToShort    (int v) { if (doThrow) throw new Error(); return (short)v; }
  static  long $noinline$intToLong     (int v) { if (doThrow) throw new Error(); return  (long)v; }
  static  byte $noinline$longToByte   (long v) { if (doThrow) throw new Error(); return  (byte)v; }
  static  char $noinline$longToChar   (long v) { if (doThrow) throw new Error(); return  (char)v; }
  static short $noinline$longToShort  (long v) { if (doThrow) throw new Error(); return (short)v; }
  static   int $noinline$longToInt    (long v) { if (doThrow) throw new Error(); return   (int)v; }

  /**
   * Basic test merging a bitfield move operation (here a type conversion) into
   * the shifter operand.
   */

  /// CHECK-START-ARM64: long Main.$opt$noinline$translate(long, byte) instruction_simplifier_arm64 (before)
  /// CHECK-DAG:   <<l:j\d+>>           ParameterValue
  /// CHECK-DAG:   <<b:b\d+>>           ParameterValue
  /// CHECK:       <<tmp:j\d+>>         TypeConversion [<<b>>]
  /// CHECK:                            Sub [<<l>>,<<tmp>>]

  /// CHECK-START-ARM64: long Main.$opt$noinline$translate(long, byte) instruction_simplifier_arm64 (after)
  /// CHECK-DAG:   <<l:j\d+>>           ParameterValue
  /// CHECK-DAG:   <<b:b\d+>>           ParameterValue
  /// CHECK:                            Arm64DataProcWithShifterOp [<<l>>,<<b>>] kind:Sub+SXTB

  /// CHECK-START-ARM64: long Main.$opt$noinline$translate(long, byte) instruction_simplifier_arm64 (after)
  /// CHECK-NOT:                        TypeConversion
  /// CHECK-NOT:                        Sub

  /// CHECK-START-ARM64: long Main.$opt$noinline$translate(long, byte) disassembly (after)
  /// CHECK:                            sub x{{\d+}}, x{{\d+}}, w{{\d+}}, sxtb

  public static long $opt$noinline$translate(long l, byte b) {
    if (doThrow) throw new Error();
    long tmp = (long)b;
    return l - tmp;
  }


  /**
   * Test that we do not merge into the shifter operand when the left and right
   * inputs are the the IR.
   */

  /// CHECK-START-ARM64: int Main.$opt$noinline$sameInput(int) instruction_simplifier_arm64 (before)
  /// CHECK:       <<a:i\d+>>           ParameterValue
  /// CHECK:       <<Const2:i\d+>>      IntConstant 2
  /// CHECK:       <<tmp:i\d+>>         Shl [<<a>>,<<Const2>>]
  /// CHECK:                            Add [<<tmp>>,<<tmp>>]

  /// CHECK-START-ARM64: int Main.$opt$noinline$sameInput(int) instruction_simplifier_arm64 (after)
  /// CHECK-DAG:   <<a:i\d+>>           ParameterValue
  /// CHECK-DAG:   <<Const2:i\d+>>      IntConstant 2
  /// CHECK:       <<Shl:i\d+>>         Shl [<<a>>,<<Const2>>]
  /// CHECK:                            Add [<<Shl>>,<<Shl>>]

  /// CHECK-START-ARM64: int Main.$opt$noinline$sameInput(int) instruction_simplifier_arm64 (after)
  /// CHECK-NOT:                        Arm64DataProcWithShifterOp

  public static int $opt$noinline$sameInput(int a) {
    if (doThrow) throw new Error();
    int tmp = a << 2;
    return tmp + tmp;
  }

  /**
   * Check that we perform the merge for multiple uses.
   */

  /// CHECK-START-ARM64: int Main.$opt$noinline$multipleUses(int) instruction_simplifier_arm64 (before)
  /// CHECK:       <<arg:i\d+>>         ParameterValue
  /// CHECK:       <<Const23:i\d+>>     IntConstant 23
  /// CHECK:       <<tmp:i\d+>>         Shl [<<arg>>,<<Const23>>]
  /// CHECK:                            Add [<<tmp>>,{{i\d+}}]
  /// CHECK:                            Add [<<tmp>>,{{i\d+}}]
  /// CHECK:                            Add [<<tmp>>,{{i\d+}}]
  /// CHECK:                            Add [<<tmp>>,{{i\d+}}]
  /// CHECK:                            Add [<<tmp>>,{{i\d+}}]

  /// CHECK-START-ARM64: int Main.$opt$noinline$multipleUses(int) instruction_simplifier_arm64 (after)
  /// CHECK:       <<arg:i\d+>>         ParameterValue
  /// CHECK:                            Arm64DataProcWithShifterOp [{{i\d+}},<<arg>>] kind:Add+LSL shift:23
  /// CHECK:                            Arm64DataProcWithShifterOp [{{i\d+}},<<arg>>] kind:Add+LSL shift:23
  /// CHECK:                            Arm64DataProcWithShifterOp [{{i\d+}},<<arg>>] kind:Add+LSL shift:23
  /// CHECK:                            Arm64DataProcWithShifterOp [{{i\d+}},<<arg>>] kind:Add+LSL shift:23
  /// CHECK:                            Arm64DataProcWithShifterOp [{{i\d+}},<<arg>>] kind:Add+LSL shift:23

  /// CHECK-START-ARM64: int Main.$opt$noinline$multipleUses(int) instruction_simplifier_arm64 (after)
  /// CHECK-NOT:                        Shl
  /// CHECK-NOT:                        Add

  public static int $opt$noinline$multipleUses(int arg) {
    if (doThrow) throw new Error();
    int tmp = arg << 23;
    switch (arg) {
      case 1:  return (arg | 1) + tmp;
      case 2:  return (arg | 2) + tmp;
      case 3:  return (arg | 3) + tmp;
      case 4:  return (arg | 4) + tmp;
      case (1 << 20):  return (arg | 5) + tmp;
      default: return 0;
    }
  }

  /**
   * Logical instructions cannot take 'extend' operations into the shift
   * operand, so test that only the shifts are merged.
   */

  /// CHECK-START-ARM64: void Main.$opt$noinline$testAnd(long, long) instruction_simplifier_arm64 (after)
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK-NOT:                        Arm64DataProcWithShifterOp

  /// CHECK-START-ARM64: void Main.$opt$noinline$testAnd(long, long) disassembly (after)
  /// CHECK:                            and lsl
  /// CHECK:                            sxtb
  /// CHECK:                            and

  static void $opt$noinline$testAnd(long a, long b) {
    if (doThrow) throw new Error();
    assertLongEquals((a & $noinline$LongShl(b, 5)) | (a & $noinline$longToByte(b)),
                     (a & (b << 5)) | (a & (byte)b));
  }

  /// CHECK-START-ARM64: void Main.$opt$noinline$testOr(int, int) instruction_simplifier_arm64 (after)
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK-NOT:                        Arm64DataProcWithShifterOp

  /// CHECK-START-ARM64: void Main.$opt$noinline$testOr(int, int) disassembly (after)
  /// CHECK:                            orr asr
  /// CHECK:                            uxth
  /// CHECK:                            orr

  static void $opt$noinline$testOr(int a, int b) {
    if (doThrow) throw new Error();
    assertIntEquals((a | $noinline$IntShr(b, 6)) | (a | $noinline$intToChar(b)),
                    (a | (b >> 6)) | (a | (char)b));
  }

  /// CHECK-START-ARM64: void Main.$opt$noinline$testXor(long, long) instruction_simplifier_arm64 (after)
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK-NOT:                        Arm64DataProcWithShifterOp

  /// CHECK-START-ARM64: void Main.$opt$noinline$testXor(long, long) disassembly (after)
  /// CHECK:                            eor lsr
  /// CHECK:                            sxtw
  /// CHECK:                            eor

  static void $opt$noinline$testXor(long a, long b) {
    if (doThrow) throw new Error();
    assertLongEquals((a ^ $noinline$LongUshr(b, 7)) | (a ^ $noinline$longToInt(b)),
                     (a ^ (b >>> 7)) | (a ^ (int)b));
  }

  /// CHECK-START-ARM64: void Main.$opt$noinline$testNeg(int) instruction_simplifier_arm64 (after)
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK-NOT:                        Arm64DataProcWithShifterOp

  /// CHECK-START-ARM64: void Main.$opt$noinline$testNeg(int) disassembly (after)
  /// CHECK:                            neg lsl
  /// CHECK:                            sxth
  /// CHECK:                            neg

  static void $opt$noinline$testNeg(int a) {
    if (doThrow) throw new Error();
    assertIntEquals(-$noinline$IntShl(a, 8) | -$noinline$intToShort(a),
                    (-(a << 8)) | (-(short)a));
  }

  /**
   * The functions below are used to compare the result of optimized operations
   * to non-optimized operations.
   * On the left-hand side we use a non-inlined function call to ensure the
   * optimization does not occur. The checker tests ensure that the optimization
   * does occur on the right-hand.
   * Some conversions to `int` are optimized away so are not merged into the shifter
   * operand. Some conversions to `long` have environment uses in the graph,
   * preventing the merge of the instructions. The checker tests can be updated
   * if that changes.
   */

  /// CHECK-START-ARM64: void Main.$opt$validateExtendByte(byte, byte) instruction_simplifier_arm64 (after)
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK-NOT:                        Arm64DataProcWithShifterOp
  /// CHECK-NOT:                        Arm64DataProcWithShifterOp

  public static void $opt$validateExtendByte(byte a, byte b) {
    if (doThrow) throw new Error();
    assertIntEquals(a + $noinline$byteToChar (b), a +  (char)b);
    assertIntEquals(a + $noinline$byteToShort(b), a + (short)b);
    assertIntEquals(a + $noinline$byteToInt  (b), a +   (int)b);
    assertIntEquals((int)(a + $noinline$byteToLong (b)), (int)(a +  (long)b));
  }

  /// CHECK-START-ARM64: void Main.$opt$validateExtendChar(char, char) instruction_simplifier_arm64 (after)
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK-NOT:                        Arm64DataProcWithShifterOp
  /// CHECK-NOT:                        Arm64DataProcWithShifterOp

  public static void $opt$validateExtendChar(char a, char b) {
    if (doThrow) throw new Error();
    assertIntEquals (a + $noinline$charToByte (b), a +  (byte)b);
    assertIntEquals (a + $noinline$charToShort(b), a + (short)b);
    assertIntEquals (a + $noinline$charToInt  (b), a +   (int)b);
    assertLongEquals(a + $noinline$charToLong (b), a +  (long)b);
  }

  /// CHECK-START-ARM64: void Main.$opt$validateExtendShort(short, short) instruction_simplifier_arm64 (after)
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK-NOT:                        Arm64DataProcWithShifterOp
  /// CHECK-NOT:                        Arm64DataProcWithShifterOp

  public static void $opt$validateExtendShort(short a, short b) {
    if (doThrow) throw new Error();
    assertIntEquals (a + $noinline$shortToByte (b), a +  (byte)b);
    assertIntEquals (a + $noinline$shortToChar (b), a +  (char)b);
    assertIntEquals (a + $noinline$shortToInt  (b), a +   (int)b);
    assertLongEquals(a + $noinline$shortToLong (b), a +  (long)b);
  }

  /// CHECK-START-ARM64: void Main.$opt$validateExtendInt(int, int) instruction_simplifier_arm64 (after)
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK-NOT:                        Arm64DataProcWithShifterOp

  public static void $opt$validateExtendInt(int a, int b) {
    if (doThrow) throw new Error();
    assertIntEquals (a + $noinline$intToByte (b), a +  (byte)b);
    assertIntEquals (a + $noinline$intToChar (b), a +  (char)b);
    assertIntEquals (a + $noinline$intToShort(b), a + (short)b);
    assertLongEquals(a + $noinline$intToLong (b), a +  (long)b);
  }

  /// CHECK-START-ARM64: void Main.$opt$validateExtendLong(long, long) instruction_simplifier_arm64 (after)
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp

  public static void $opt$validateExtendLong(long a, long b) {
    if (doThrow) throw new Error();
    assertLongEquals(a + $noinline$longToByte (b), a +  (byte)b);
    assertLongEquals(a + $noinline$longToChar (b), a +  (char)b);
    assertLongEquals(a + $noinline$longToShort(b), a + (short)b);
    assertLongEquals(a + $noinline$longToInt  (b), a +   (int)b);
  }


  static int $noinline$IntShl(int b, int c) {
    if (doThrow) throw new Error();
    return b << c;
  }
  static int $noinline$IntShr(int b, int c) {
    if (doThrow) throw new Error();
    return b >> c;
  }
  static int $noinline$IntUshr(int b, int c) {
    if (doThrow) throw new Error();
    return b >>> c;
  }


  // Each test line below should see one merge.
  /// CHECK-START-ARM64: void Main.$opt$validateShiftInt(int, int) instruction_simplifier_arm64 (after)
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp

  public static void $opt$validateShiftInt(int a, int b) {
    if (doThrow) throw new Error();

    assertIntEquals(a + $noinline$IntShl(b, 1),   a + (b <<  1));
    assertIntEquals(a + $noinline$IntShl(b, 6),   a + (b <<  6));
    assertIntEquals(a + $noinline$IntShl(b, 7),   a + (b <<  7));
    assertIntEquals(a + $noinline$IntShl(b, 8),   a + (b <<  8));
    assertIntEquals(a + $noinline$IntShl(b, 14),  a + (b << 14));
    assertIntEquals(a + $noinline$IntShl(b, 15),  a + (b << 15));
    assertIntEquals(a + $noinline$IntShl(b, 16),  a + (b << 16));
    assertIntEquals(a + $noinline$IntShl(b, 30),  a + (b << 30));
    assertIntEquals(a + $noinline$IntShl(b, 31),  a + (b << 31));
    assertIntEquals(a + $noinline$IntShl(b, 32),  a + (b << 32));
    assertIntEquals(a + $noinline$IntShl(b, 62),  a + (b << 62));
    assertIntEquals(a + $noinline$IntShl(b, 63),  a + (b << 63));

    assertIntEquals(a - $noinline$IntShr(b, 1),   a - (b >>  1));
    assertIntEquals(a - $noinline$IntShr(b, 6),   a - (b >>  6));
    assertIntEquals(a - $noinline$IntShr(b, 7),   a - (b >>  7));
    assertIntEquals(a - $noinline$IntShr(b, 8),   a - (b >>  8));
    assertIntEquals(a - $noinline$IntShr(b, 14),  a - (b >> 14));
    assertIntEquals(a - $noinline$IntShr(b, 15),  a - (b >> 15));
    assertIntEquals(a - $noinline$IntShr(b, 16),  a - (b >> 16));
    assertIntEquals(a - $noinline$IntShr(b, 30),  a - (b >> 30));
    assertIntEquals(a - $noinline$IntShr(b, 31),  a - (b >> 31));
    assertIntEquals(a - $noinline$IntShr(b, 32),  a - (b >> 32));
    assertIntEquals(a - $noinline$IntShr(b, 62),  a - (b >> 62));
    assertIntEquals(a - $noinline$IntShr(b, 63),  a - (b >> 63));

    assertIntEquals(a ^ $noinline$IntUshr(b, 1),   a ^ (b >>>  1));
    assertIntEquals(a ^ $noinline$IntUshr(b, 6),   a ^ (b >>>  6));
    assertIntEquals(a ^ $noinline$IntUshr(b, 7),   a ^ (b >>>  7));
    assertIntEquals(a ^ $noinline$IntUshr(b, 8),   a ^ (b >>>  8));
    assertIntEquals(a ^ $noinline$IntUshr(b, 14),  a ^ (b >>> 14));
    assertIntEquals(a ^ $noinline$IntUshr(b, 15),  a ^ (b >>> 15));
    assertIntEquals(a ^ $noinline$IntUshr(b, 16),  a ^ (b >>> 16));
    assertIntEquals(a ^ $noinline$IntUshr(b, 30),  a ^ (b >>> 30));
    assertIntEquals(a ^ $noinline$IntUshr(b, 31),  a ^ (b >>> 31));
    assertIntEquals(a ^ $noinline$IntUshr(b, 32),  a ^ (b >>> 32));
    assertIntEquals(a ^ $noinline$IntUshr(b, 62),  a ^ (b >>> 62));
    assertIntEquals(a ^ $noinline$IntUshr(b, 63),  a ^ (b >>> 63));
  }


  static long $noinline$LongShl(long b, long c) {
    if (doThrow) throw new Error();
    return b << c;
  }
  static long $noinline$LongShr(long b, long c) {
    if (doThrow) throw new Error();
    return b >> c;
  }
  static long $noinline$LongUshr(long b, long c) {
    if (doThrow) throw new Error();
    return b >>> c;
  }

  // Each test line below should see one merge.
  /// CHECK-START-ARM64: void Main.$opt$validateShiftLong(long, long) instruction_simplifier_arm64 (after)
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp
  /// CHECK:                            Arm64DataProcWithShifterOp

  public static void $opt$validateShiftLong(long a, long b) {
    if (doThrow) throw new Error();

    assertLongEquals(a + $noinline$LongShl(b, 1),   a + (b <<  1));
    assertLongEquals(a + $noinline$LongShl(b, 6),   a + (b <<  6));
    assertLongEquals(a + $noinline$LongShl(b, 7),   a + (b <<  7));
    assertLongEquals(a + $noinline$LongShl(b, 8),   a + (b <<  8));
    assertLongEquals(a + $noinline$LongShl(b, 14),  a + (b << 14));
    assertLongEquals(a + $noinline$LongShl(b, 15),  a + (b << 15));
    assertLongEquals(a + $noinline$LongShl(b, 16),  a + (b << 16));
    assertLongEquals(a + $noinline$LongShl(b, 30),  a + (b << 30));
    assertLongEquals(a + $noinline$LongShl(b, 31),  a + (b << 31));
    assertLongEquals(a + $noinline$LongShl(b, 32),  a + (b << 32));
    assertLongEquals(a + $noinline$LongShl(b, 62),  a + (b << 62));
    assertLongEquals(a + $noinline$LongShl(b, 63),  a + (b << 63));

    assertLongEquals(a - $noinline$LongShr(b, 1),   a - (b >>  1));
    assertLongEquals(a - $noinline$LongShr(b, 6),   a - (b >>  6));
    assertLongEquals(a - $noinline$LongShr(b, 7),   a - (b >>  7));
    assertLongEquals(a - $noinline$LongShr(b, 8),   a - (b >>  8));
    assertLongEquals(a - $noinline$LongShr(b, 14),  a - (b >> 14));
    assertLongEquals(a - $noinline$LongShr(b, 15),  a - (b >> 15));
    assertLongEquals(a - $noinline$LongShr(b, 16),  a - (b >> 16));
    assertLongEquals(a - $noinline$LongShr(b, 30),  a - (b >> 30));
    assertLongEquals(a - $noinline$LongShr(b, 31),  a - (b >> 31));
    assertLongEquals(a - $noinline$LongShr(b, 32),  a - (b >> 32));
    assertLongEquals(a - $noinline$LongShr(b, 62),  a - (b >> 62));
    assertLongEquals(a - $noinline$LongShr(b, 63),  a - (b >> 63));

    assertLongEquals(a ^ $noinline$LongUshr(b, 1),   a ^ (b >>>  1));
    assertLongEquals(a ^ $noinline$LongUshr(b, 6),   a ^ (b >>>  6));
    assertLongEquals(a ^ $noinline$LongUshr(b, 7),   a ^ (b >>>  7));
    assertLongEquals(a ^ $noinline$LongUshr(b, 8),   a ^ (b >>>  8));
    assertLongEquals(a ^ $noinline$LongUshr(b, 14),  a ^ (b >>> 14));
    assertLongEquals(a ^ $noinline$LongUshr(b, 15),  a ^ (b >>> 15));
    assertLongEquals(a ^ $noinline$LongUshr(b, 16),  a ^ (b >>> 16));
    assertLongEquals(a ^ $noinline$LongUshr(b, 30),  a ^ (b >>> 30));
    assertLongEquals(a ^ $noinline$LongUshr(b, 31),  a ^ (b >>> 31));
    assertLongEquals(a ^ $noinline$LongUshr(b, 32),  a ^ (b >>> 32));
    assertLongEquals(a ^ $noinline$LongUshr(b, 62),  a ^ (b >>> 62));
    assertLongEquals(a ^ $noinline$LongUshr(b, 63),  a ^ (b >>> 63));
  }


  public static void main(String[] args) {
    assertLongEquals(10000 - 3, $opt$noinline$translate((long)10000, (byte)3));
    assertLongEquals(-10000 - -3, $opt$noinline$translate((long)-10000, (byte)-3));

    assertIntEquals(4096, $opt$noinline$sameInput(512));
    assertIntEquals(-8192, $opt$noinline$sameInput(-1024));

    assertIntEquals(((1 << 23) | 1), $opt$noinline$multipleUses(1));
    assertIntEquals(((1 << 20) | 5), $opt$noinline$multipleUses(1 << 20));

    long inputs[] = {
      -((1 <<  7) - 1), -((1 <<  7)), -((1 <<  7) + 1),
      -((1 << 15) - 1), -((1 << 15)), -((1 << 15) + 1),
      -((1 << 16) - 1), -((1 << 16)), -((1 << 16) + 1),
      -((1 << 31) - 1), -((1 << 31)), -((1 << 31) + 1),
      -((1 << 32) - 1), -((1 << 32)), -((1 << 32) + 1),
      -((1 << 63) - 1), -((1 << 63)), -((1 << 63) + 1),
      -42, -314, -2718281828L, -0x123456789L, -0x987654321L,
      -1, -20, -300, -4000, -50000, -600000, -7000000, -80000000,
      0,
      1, 20, 300, 4000, 50000, 600000, 7000000, 80000000,
      42,  314,  2718281828L,  0x123456789L,  0x987654321L,
      (1 <<  7) - 1, (1 <<  7), (1 <<  7) + 1,
      (1 <<  8) - 1, (1 <<  8), (1 <<  8) + 1,
      (1 << 15) - 1, (1 << 15), (1 << 15) + 1,
      (1 << 16) - 1, (1 << 16), (1 << 16) + 1,
      (1 << 31) - 1, (1 << 31), (1 << 31) + 1,
      (1 << 32) - 1, (1 << 32), (1 << 32) + 1,
      (1 << 63) - 1, (1 << 63), (1 << 63) + 1,
      Long.MIN_VALUE, Long.MAX_VALUE
    };
    for (int i = 0; i < inputs.length; i++) {
      $opt$noinline$testNeg((int)inputs[i]);
      for (int j = 0; j < inputs.length; j++) {
        $opt$noinline$testAnd(inputs[i], inputs[j]);
        $opt$noinline$testOr((int)inputs[i], (int)inputs[j]);
        $opt$noinline$testXor(inputs[i], inputs[j]);

        $opt$validateExtendByte((byte)inputs[i], (byte)inputs[j]);
        $opt$validateExtendChar((char)inputs[i], (char)inputs[j]);
        $opt$validateExtendShort((short)inputs[i], (short)inputs[j]);
        $opt$validateExtendInt((int)inputs[i], (int)inputs[j]);
        $opt$validateExtendLong(inputs[i], inputs[j]);

        $opt$validateShiftInt((int)inputs[i], (int)inputs[j]);
        $opt$validateShiftLong(inputs[i], inputs[j]);
      }
    }

  }
}
