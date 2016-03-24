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

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;

public class Main {
  // Workaround for b/18051191.
  class Inner {}

  public static boolean field0;
  public static boolean field1;
  public static boolean field2;

  public static void assertTrue(boolean result) {
    if (!result) {
      throw new Error("Expected true");
    }
  }

  public static void assertFalse(boolean result) {
    if (result) {
      throw new Error("Expected false");
    }
  }

  public static void main(String[] args) throws Throwable {
    System.loadLibrary(args[0]);
    Class<?> c = Class.forName("TestCase");
    Method m = c.getMethod("testCase");

    try {
      field0 = true;
      field1 = false;
      assertTrue((Boolean) m.invoke(null, null));

      field0 = true;
      field1 = true;
      assertTrue((Boolean) m.invoke(null, null));

      field0 = false;
      field1 = false;
      assertFalse((Boolean) m.invoke(null, null));
    } catch (Exception e) {
      throw new Error(e);
    }
  }
}
