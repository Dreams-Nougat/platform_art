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

public class Main {

  Object field;

  /// CHECK-START: java.lang.Object Main.testGetField() builder (after)
  /// CHECK:       <<This:l\d+>>  ParameterValue
  /// CHECK:                      InstanceFieldGet [<<This>>] field_name:Main.field

  /// CHECK-START-ARM-IF(envTrue('ART_HEAP_POISONING') and envNotEquals('ART_READ_BARRIER_TYPE','TABLELOOKUP')): java.lang.Object Main.testGetField() disassembly (after)
  /// CHECK:       <<This:l\d+>>  ParameterValue
  /// CHECK:       <<Ref:l\d+>>   InstanceFieldGet [<<This>>] field_name:Main.field
  /// CHECK:                        ldr <<RefReg:r([0-8]|10|11)>>, [r1, #8]
  /// CHECK:                        rsbs <<RefReg>>, #0
  /// CHECK:                      Return [<<Ref>>]

  /// CHECK-START-ARM-IF(not envTrue('ART_HEAP_POISONING')): java.lang.Object Main.testGetField() disassembly (after)
  /// CHECK-NOT:                    rsbs {{r\d+}}, #0

  /// CHECK-START-ARM64-IF(envTrue('ART_HEAP_POISONING') and envNotEquals('ART_READ_BARRIER_TYPE','TABLELOOKUP')): java.lang.Object Main.testGetField() disassembly (after)
  /// CHECK:       <<This:l\d+>>  ParameterValue
  /// CHECK:       <<Ref:l\d+>>   InstanceFieldGet [<<This>>] field_name:Main.field
  /// CHECK:                        ldr w0, [x1, #8]
  /// CHECK:                        neg w0, w0
  /// CHECK:                      Return [<<Ref>>]
  /// CHECK:                        ret

  /// CHECK-START-ARM64-IF(not envTrue('ART_HEAP_POISONING')): java.lang.Object Main.testGetField() disassembly (after)
  /// CHECK-NOT:                    neg {{w\d+}}, {{w\d+}}

  /// CHECK-START-X86-IF(envTrue('ART_HEAP_POISONING') and envNotEquals('ART_READ_BARRIER_TYPE','TABLELOOKUP')): java.lang.Object Main.testGetField() disassembly (after)
  /// CHECK:       <<This:l\d+>>  ParameterValue
  /// CHECK:       <<Ref:l\d+>>   InstanceFieldGet [<<This>>] field_name:Main.field
  /// CHECK:                        mov eax, [ecx + 8]
  /// CHECK:                        neg eax
  /// CHECK:                      Return [<<Ref>>]
  /// CHECK:                        ret

  /// CHECK-START-X86-IF(not envTrue('ART_HEAP_POISONING')): java.lang.Object Main.testGetField() disassembly (after)
  /// CHECK-NOT:                    neg {{[a-z]+}}

  /// CHECK-START-X86_64-IF(envTrue('ART_HEAP_POISONING') and envNotEquals('ART_READ_BARRIER_TYPE','TABLELOOKUP')): java.lang.Object Main.testGetField() disassembly (after)
  /// CHECK:       <<This:l\d+>>  ParameterValue
  /// CHECK:       <<Ref:l\d+>>   InstanceFieldGet [<<This>>] field_name:Main.field
  /// CHECK:                        mov eax, [rsi + 8]
  /// CHECK:                        neg eax
  /// CHECK:                      Return [<<Ref>>]
  /// CHECK:                        ret

  /// CHECK-START-X86_64-IF(not envTrue('ART_HEAP_POISONING')): java.lang.Object Main.testGetField() disassembly (after)
  /// CHECK-NOT:                    neg {{[a-z]+}}

  Object testGetField() {
    return field;
  }

  /// CHECK-START: void Main.testSetField(java.lang.Object) builder (after)
  /// CHECK:       <<This:l\d+>>  ParameterValue
  /// CHECK:       <<Arg:l\d+>>   ParameterValue
  /// CHECK:                      InstanceFieldSet [<<This>>,<<Arg>>] field_name:Main.field

  /// CHECK-START-ARM-IF(envTrue('ART_HEAP_POISONING') and envNotEquals('ART_READ_BARRIER_TYPE','TABLELOOKUP')): void Main.testSetField(java.lang.Object) disassembly (after)
  /// CHECK:       <<This:l\d+>>  ParameterValue
  /// CHECK:       <<Arg:l\d+>>   ParameterValue
  /// CHECK:                      InstanceFieldSet [<<This>>,<<Arg>>] field_name:Main.field
  /// CHECK-NEXT:                   mov <<Temp:r([0-8]|10|11)>>, r2
  /// CHECK-NEXT:                   rsbs <<Temp>>, #0
  /// CHECK-NEXT:                   str <<Temp>>, [r1, #8]
  /// CHECK:                      ReturnVoid
  /// CHECK-NEXT:                   bx lr

  /// CHECK-START-ARM-IF(not envTrue('ART_HEAP_POISONING')): void Main.testSetField(java.lang.Object) disassembly (after)
  /// CHECK-NOT:                    rsbs {{r\d+}}, #0

  /// CHECK-START-ARM64-IF(envTrue('ART_HEAP_POISONING') and envNotEquals('ART_READ_BARRIER_TYPE','TABLELOOKUP')): void Main.testSetField(java.lang.Object) disassembly (after)
  /// CHECK:       <<This:l\d+>>  ParameterValue
  /// CHECK:       <<Arg:l\d+>>   ParameterValue
  /// CHECK:                      InstanceFieldSet [<<This>>,<<Arg>>] field_name:Main.field
  /// CHECK-NEXT:                   mov <<Temp:w1[67]>>, w2
  /// CHECK-NEXT:                   neg <<Temp>>, <<Temp>>
  /// CHECK-NEXT:                   str <<Temp>>, [x1, #8]
  /// CHECK:                      ReturnVoid
  /// CHECK-NEXT:                   ret

  /// CHECK-START-ARM64-IF(not envTrue('ART_HEAP_POISONING')): void Main.testSetField(java.lang.Object) disassembly (after)
  /// CHECK-NOT:                    neg {{w\d+}}, {{w\d+}}

  /// CHECK-START-X86-IF(envTrue('ART_HEAP_POISONING') and envNotEquals('ART_READ_BARRIER_TYPE','TABLELOOKUP')): void Main.testSetField(java.lang.Object) disassembly (after)
  /// CHECK:       <<This:l\d+>>  ParameterValue
  /// CHECK:       <<Arg:l\d+>>   ParameterValue
  /// CHECK:                      ParallelMove
  /// CHECK-NEXT:                   mov eax, ecx
  /// CHECK:                      InstanceFieldSet [<<This>>,<<Arg>>] field_name:Main.field
  /// CHECK-NEXT:                   mov <<Temp:e([acdb]x|bp|si|di)>>, edx
  /// CHECK-NEXT:                   neg <<Temp>>
  /// CHECK-NEXT:                   mov [eax + 8], <<Temp>>
  /// CHECK:                      ReturnVoid
  /// CHECK-NEXT:                   ret

  /// CHECK-START-X86-IF(not envTrue('ART_HEAP_POISONING')): void Main.testSetField(java.lang.Object) disassembly (after)
  /// CHECK-NOT:                    neg {{[a-z]+}}

  /// CHECK-START-X86_64-IF(envTrue('ART_HEAP_POISONING') and envNotEquals('ART_READ_BARRIER_TYPE','TABLELOOKUP')): void Main.testSetField(java.lang.Object) disassembly (after)
  /// CHECK:       <<This:l\d+>>  ParameterValue
  /// CHECK:       <<Arg:l\d+>>   ParameterValue
  /// CHECK:                      InstanceFieldSet [<<This>>,<<Arg>>] field_name:Main.field
  /// CHECK-NEXT:                   mov <<Temp:e([acdb]x|bp|si|di)>>, edx
  /// CHECK-NEXT:                   neg <<Temp>>
  /// CHECK-NEXT:                   mov [rsi + 8], <<Temp>>
  /// CHECK:                      ReturnVoid
  /// CHECK-NEXT:                   ret

  /// CHECK-START-X86_64-IF(not envTrue('ART_HEAP_POISONING')): void Main.testSetField(java.lang.Object) disassembly (after)
  /// CHECK-NOT:                    neg {{[a-z]+}}

  void testSetField(Object o) {
    field = o;
  }

  public static void main(String[] args) {
    Main m = new Main();
    Object o = m.testGetField();
    m.testSetField(o);
    System.out.println("passed");
  }
}
