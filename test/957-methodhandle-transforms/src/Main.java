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

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodHandles.Lookup;
import java.lang.invoke.MethodType;
import java.lang.invoke.WrongMethodTypeException;

public class Main {
  public static void main(String[] args) throws Throwable {
    testThrowException();
    testDropArguments();
    testCatchException();
    testGuardWithTest();
    testArrayElementGetter();
    testArrayElementSetter();
    testIdentity();
    testConstant();
  }

  public static void testThrowException() throws Throwable {
    MethodHandle handle = MethodHandles.throwException(String.class,
        IllegalArgumentException.class);

    if (handle.type().returnType() != String.class) {
      System.out.println("Unexpected return type for handle: " + handle +
          " [ " + handle.type() + "]");
    }

    final IllegalArgumentException iae = new IllegalArgumentException("boo!");
    try {
      handle.invoke(iae);
      System.out.println("Expected an exception of type: java.lang.IllegalArgumentException");
    } catch (IllegalArgumentException expected) {
      if (expected != iae) {
        System.out.println("Wrong exception: expected " + iae + " but was " + expected);
      }
    }
  }

  public static void dropArguments_delegate(String message, long message2) {
    System.out.println("Message: " + message + ", Message2: " + message2);
  }

  public static void testDropArguments() throws Throwable {
    MethodHandle delegate = MethodHandles.lookup().findStatic(Main.class,
        "dropArguments_delegate",
        MethodType.methodType(void.class, new Class<?>[] { String.class, long.class }));

    MethodHandle transform = MethodHandles.dropArguments(delegate, 0, int.class, Object.class);

    // The transformer will accept two additional arguments at position zero.
    try {
      transform.invokeExact("foo", 42l);
      fail();
    } catch (WrongMethodTypeException expected) {
    }

    transform.invokeExact(45, new Object(), "foo", 42l);
    transform.invoke(45, new Object(), "foo", 42l);

    // Additional arguments at position 1.
    transform = MethodHandles.dropArguments(delegate, 1, int.class, Object.class);
    transform.invokeExact("foo", 45, new Object(), 42l);
    transform.invoke("foo", 45, new Object(), 42l);

    // Additional arguments at position 2.
    transform = MethodHandles.dropArguments(delegate, 2, int.class, Object.class);
    transform.invokeExact("foo", 42l, 45, new Object());
    transform.invoke("foo", 42l, 45, new Object());

    // Note that we still perform argument conversions even for the arguments that
    // are subsequently dropped.
    try {
      transform.invoke("foo", 42l, 45l, new Object());
      fail();
    } catch (WrongMethodTypeException expected) {
    } catch (IllegalArgumentException expected) {
      // TODO(narayan): We currently throw the wrong type of exception here,
      // it's IAE and should be WMTE instead.
    }

    // Check that asType works as expected.
    transform = MethodHandles.dropArguments(delegate, 0, int.class, Object.class);
    transform = transform.asType(MethodType.methodType(void.class,
          new Class<?>[] { short.class, Object.class, String.class, long.class }));
    transform.invokeExact((short) 45, new Object(), "foo", 42l);

    // Invalid argument location, should not be allowed.
    try {
      MethodHandles.dropArguments(delegate, -1, int.class, Object.class);
      fail();
    } catch (IllegalArgumentException expected) {
    }

    // Invalid argument location, should not be allowed.
    try {
      MethodHandles.dropArguments(delegate, 3, int.class, Object.class);
      fail();
    } catch (IllegalArgumentException expected) {
    }

    try {
      MethodHandles.dropArguments(delegate, 1, void.class);
      fail();
    } catch (IllegalArgumentException expected) {
    }
  }

  public static String testCatchException_target(String arg1, long arg2, String exceptionMessage)
      throws Throwable {
    if (exceptionMessage != null) {
      throw new IllegalArgumentException(exceptionMessage);
    }

    System.out.println("Target: Arg1: " + arg1 + ", Arg2: " + arg2);
    return "target";
  }

  public static String testCatchException_handler(IllegalArgumentException iae, String arg1, long arg2,
      String exMsg) {
    System.out.println("Handler: " + iae + ", Arg1: " + arg1 + ", Arg2: " + arg2 + ", ExMsg: " + exMsg);
    return "handler1";
  }

  public static String testCatchException_handler2(IllegalArgumentException iae, String arg1) {
    System.out.println("Handler: " + iae + ", Arg1: " + arg1);
    return "handler2";
  }

  public static void testCatchException() throws Throwable {
    MethodHandle target = MethodHandles.lookup().findStatic(Main.class,
        "testCatchException_target",
        MethodType.methodType(String.class, new Class<?>[] { String.class, long.class, String.class }));

    MethodHandle handler = MethodHandles.lookup().findStatic(Main.class,
        "testCatchException_handler",
        MethodType.methodType(String.class, new Class<?>[] { IllegalArgumentException.class,
            String.class, long.class, String.class }));

    MethodHandle adapter = MethodHandles.catchException(target, IllegalArgumentException.class,
        handler);

    String returnVal = null;

    // These two should end up calling the target always. We're passing a null exception
    // message here, which means the target will not throw.
    returnVal = (String) adapter.invoke("foo", 42, null);
    assertEquals("target", returnVal);
    returnVal = (String) adapter.invokeExact("foo", 42l, (String) null);
    assertEquals("target", returnVal);

    // We're passing a non-null exception message here, which means the target will throw,
    // which in turn means that the handler must be called for the next two invokes.
    returnVal = (String) adapter.invoke("foo", 42, "exceptionMessage");
    assertEquals("handler1", returnVal);
    returnVal = (String) adapter.invokeExact("foo", 42l, "exceptionMessage");
    assertEquals("handler1", returnVal);

    handler = MethodHandles.lookup().findStatic(Main.class,
        "testCatchException_handler2",
        MethodType.methodType(String.class, new Class<?>[] { IllegalArgumentException.class,
            String.class }));
    adapter = MethodHandles.catchException(target, IllegalArgumentException.class, handler);

    returnVal = (String) adapter.invoke("foo", 42, "exceptionMessage");
    assertEquals("handler2", returnVal);
    returnVal = (String) adapter.invokeExact("foo", 42l, "exceptionMessage");
    assertEquals("handler2", returnVal);

    // Test that the type of the invoke doesn't matter. Here we call
    // IllegalArgumentException.toString() on the exception that was thrown by
    // the target.
    handler = MethodHandles.lookup().findVirtual(IllegalArgumentException.class,
        "toString", MethodType.methodType(String.class));
    adapter = MethodHandles.catchException(target, IllegalArgumentException.class, handler);

    returnVal = (String) adapter.invoke("foo", 42, "exceptionMessage");
    assertEquals("java.lang.IllegalArgumentException: exceptionMessage", returnVal);
    returnVal = (String) adapter.invokeExact("foo", 42l, "exceptionMessage2");
    assertEquals("java.lang.IllegalArgumentException: exceptionMessage2", returnVal);

    // Check that asType works as expected.
    adapter = MethodHandles.catchException(target, IllegalArgumentException.class,
        handler);
    adapter = adapter.asType(MethodType.methodType(String.class,
          new Class<?>[] { String.class, int.class, String.class }));
    returnVal = (String) adapter.invokeExact("foo", 42, "exceptionMessage");
    assertEquals("java.lang.IllegalArgumentException: exceptionMessage", returnVal);
  }

  public static boolean testGuardWithTest_test(String arg1, long arg2) {
    return "target".equals(arg1) && 42 == arg2;
  }

  public static String testGuardWithTest_target(String arg1, long arg2, int arg3) {
    System.out.println("target: " + arg1 + ", " + arg2  + ", " + arg3);
    return "target";
  }

  public static String testGuardWithTest_fallback(String arg1, long arg2, int arg3) {
    System.out.println("fallback: " + arg1 + ", " + arg2  + ", " + arg3);
    return "fallback";
  }

  public static void testGuardWithTest() throws Throwable {
    MethodHandle test = MethodHandles.lookup().findStatic(Main.class,
        "testGuardWithTest_test",
        MethodType.methodType(boolean.class, new Class<?>[] { String.class, long.class }));

    final MethodType type = MethodType.methodType(String.class,
        new Class<?>[] { String.class, long.class, int.class });

    final MethodHandle target = MethodHandles.lookup().findStatic(Main.class,
        "testGuardWithTest_target", type);
    final MethodHandle fallback = MethodHandles.lookup().findStatic(Main.class,
        "testGuardWithTest_fallback", type);

    MethodHandle adapter = MethodHandles.guardWithTest(test, target, fallback);

    String returnVal = null;

    returnVal = (String) adapter.invoke("target", 42, 56);
    assertEquals("target", returnVal);
    returnVal = (String) adapter.invokeExact("target", 42l, 56);
    assertEquals("target", returnVal);

    returnVal = (String) adapter.invoke("fallback", 42l, 56);
    assertEquals("fallback", returnVal);
    returnVal = (String) adapter.invokeExact("target", 42l, 56);
    assertEquals("target", returnVal);

    // Check that asType works as expected.
    adapter = adapter.asType(MethodType.methodType(String.class,
          new Class<?>[] { String.class, int.class, int.class }));
    returnVal = (String) adapter.invokeExact("target", 42, 56);
    assertEquals("target", returnVal);
  }

  public static void testArrayElementGetter() throws Throwable {
    MethodHandle getter = MethodHandles.arrayElementGetter(int[].class);

    {
      int[] array = new int[1];
      array[0] = 42;
      int value = (int) getter.invoke(array, 0);
      if (value != 42) {
        System.out.println("Unexpected value: " + value);
      }

      try {
        value = (int) getter.invoke(array, -1);
        fail();
      } catch (ArrayIndexOutOfBoundsException expected) {
      }

      try {
        value = (int) getter.invoke(null, -1);
        fail();
      } catch (NullPointerException expected) {
      }
    }

    {
      getter = MethodHandles.arrayElementGetter(long[].class);
      long[] array = new long[1];
      array[0] = 42;
      long value = (long) getter.invoke(array, 0);
      if (value != 42l) {
        System.out.println("Unexpected value: " + value);
      }
    }

    {
      getter = MethodHandles.arrayElementGetter(short[].class);
      short[] array = new short[1];
      array[0] = 42;
      short value = (short) getter.invoke(array, 0);
      if (value != 42l) {
        System.out.println("Unexpected value: " + value);
      }
    }

    {
      getter = MethodHandles.arrayElementGetter(char[].class);
      char[] array = new char[1];
      array[0] = 42;
      char value = (char) getter.invoke(array, 0);
      if (value != 42l) {
        System.out.println("Unexpected value: " + value);
      }
    }

    {
      getter = MethodHandles.arrayElementGetter(byte[].class);
      byte[] array = new byte[1];
      array[0] = (byte) 0x8;
      byte value = (byte) getter.invoke(array, 0);
      if (value != (byte) 0x8) {
        System.out.println("Unexpected value: " + value);
      }
    }

    {
      getter = MethodHandles.arrayElementGetter(boolean[].class);
      boolean[] array = new boolean[1];
      array[0] = true;
      boolean value = (boolean) getter.invoke(array, 0);
      if (!value) {
        System.out.println("Unexpected value: " + value);
      }
    }

    {
      getter = MethodHandles.arrayElementGetter(float[].class);
      float[] array = new float[1];
      array[0] = 42.0f;
      float value = (float) getter.invoke(array, 0);
      if (value != 42.0f) {
        System.out.println("Unexpected value: " + value);
      }
    }

    {
      getter = MethodHandles.arrayElementGetter(double[].class);
      double[] array = new double[1];
      array[0] = 42.0;
      double value = (double) getter.invoke(array, 0);
      if (value != 42.0) {
        System.out.println("Unexpected value: " + value);
      }
    }

    {
      getter = MethodHandles.arrayElementGetter(String[].class);
      String[] array = new String[3];
      array[0] = "42";
      array[1] = "48";
      array[2] = "54";
      String value = (String) getter.invoke(array, 0);
      assertEquals("42", value);
      value = (String) getter.invoke(array, 1);
      assertEquals("48", value);
      value = (String) getter.invoke(array, 1);
      assertEquals("54", value);
    }
  }

  public static void testArrayElementSetter() throws Throwable {
    MethodHandle setter = MethodHandles.arrayElementSetter(int[].class);

    {
      int[] array = new int[2];
      setter.invoke(array, 0, 42);
      setter.invoke(array, 1, 43);

      if (array[0] != 42) {
        System.out.println("Unexpected value: " + array[0]);
      }
      if (array[1] != 43) {
        System.out.println("Unexpected value: " + array[1]);
      }

      try {
        setter.invoke(array, -1, 42);
        fail();
      } catch (ArrayIndexOutOfBoundsException expected) {
      }

      try {
        setter.invoke(null, 0, 42);
        fail();
      } catch (NullPointerException expected) {
      }
    }

    {
      setter = MethodHandles.arrayElementSetter(long[].class);
      long[] array = new long[1];
      setter.invoke(array, 0, 42l);
      if (array[0] != 42l) {
        System.out.println("Unexpected value: " + array[0]);
      }
    }

    {
      setter = MethodHandles.arrayElementSetter(short[].class);
      short[] array = new short[1];
      setter.invoke(array, 0, (short) 42);
      if (array[0] != 42l) {
        System.out.println("Unexpected value: " + array[0]);
      }
    }

    {
      setter = MethodHandles.arrayElementSetter(char[].class);
      char[] array = new char[1];
      setter.invoke(array, 0, (char) 42);
      if (array[0] != 42) {
        System.out.println("Unexpected value: " + array[0]);
      }
    }

    {
      setter = MethodHandles.arrayElementSetter(byte[].class);
      byte[] array = new byte[1];
      setter.invoke(array, 0, (byte) 0x8);
      if (array[0] != (byte) 0x8) {
        System.out.println("Unexpected value: " + array[0]);
      }
    }

    {
      setter = MethodHandles.arrayElementSetter(boolean[].class);
      boolean[] array = new boolean[1];
      setter.invoke(array, 0, true);
      if (!array[0]) {
        System.out.println("Unexpected value: " + array[0]);
      }
    }

    {
      setter = MethodHandles.arrayElementSetter(float[].class);
      float[] array = new float[1];
      setter.invoke(array, 0, 42.0f);
      if (array[0] != 42.0f) {
        System.out.println("Unexpected value: " + array[0]);
      }
    }

    {
      setter = MethodHandles.arrayElementSetter(double[].class);
      double[] array = new double[1];
      setter.invoke(array, 0, 42.0);
      if (array[0] != 42.0) {
        System.out.println("Unexpected value: " + array[0]);
      }
    }

    {
      setter = MethodHandles.arrayElementSetter(String[].class);
      String[] array = new String[3];
      setter.invoke(array, 0, "42");
      setter.invoke(array, 1, "48");
      setter.invoke(array, 2, "54");
      assertEquals("42", array[0]);
      assertEquals("48", array[1]);
      assertEquals("54", array[2]);
    }
  }

  public static void testIdentity() throws Throwable {
    {
      MethodHandle identity = MethodHandles.identity(boolean.class);
      boolean value = (boolean) identity.invoke(false);
      if (value) {
        System.out.println("Unexpected value: " + value);
      }
    }

    {
      MethodHandle identity = MethodHandles.identity(byte.class);
      byte value = (byte) identity.invoke((byte) 0x8);
      if (value != (byte) 0x8) {
        System.out.println("Unexpected value: " + value);
      }
    }

    {
      MethodHandle identity = MethodHandles.identity(char.class);
      char value = (char) identity.invoke((char) -56);
      if (value != (char) -56) {
        System.out.println("Unexpected value: " + value);
      }
    }

    {
      MethodHandle identity = MethodHandles.identity(short.class);
      short value = (short) identity.invoke((short) -59);
      if (value != (short) -59) {
        System.out.println("Unexpected value: " + value);
      }
    }

    {
      MethodHandle identity = MethodHandles.identity(int.class);
      int value = (int) identity.invoke(52);
      if (value != 52) {
        System.out.println("Unexpected value: " + value);
      }
    }

    {
      MethodHandle identity = MethodHandles.identity(long.class);
      long value = (long) identity.invoke(-76l);
      if (value != (long) -76) {
        System.out.println("Unexpected value: " + value);
      }
    }

    {
      MethodHandle identity = MethodHandles.identity(float.class);
      float value = (float) identity.invoke(56.0f);
      if (value != (float) 56.0f) {
        System.out.println("Unexpected value: " + value);
      }
    }

    {
      MethodHandle identity = MethodHandles.identity(double.class);
      double value = (double) identity.invoke((double) 72.0);
      if (value != (double) 72.0) {
        System.out.println("Unexpected value: " + value);
      }
    }

    {
      MethodHandle identity = MethodHandles.identity(String.class);
      String value = (String) identity.invoke("bazman");
      assertEquals("bazman", value);
    }
  }

  public static void testConstant() throws Throwable {
    // int constants.
    {
      MethodHandle constant = MethodHandles.constant(int.class, 56);
      int value = (int) constant.invoke();
      if (value != 56) {
        System.out.println("Unexpected value: " + value);
      }

      // short constant values are converted to int.
      constant = MethodHandles.constant(int.class, (short) 52);
      value = (int) constant.invoke();
      if (value != 52) {
        System.out.println("Unexpected value: " + value);
      }

      // char constant values are converted to int.
      constant = MethodHandles.constant(int.class, (char) 'b');
      value = (int) constant.invoke();
      if (value != (int) 'b') {
        System.out.println("Unexpected value: " + value);
      }

      // int constant values are converted to int.
      constant = MethodHandles.constant(int.class, (byte) 0x1);
      value = (int) constant.invoke();
      if (value != 1) {
        System.out.println("Unexpected value: " + value);
      }

      // boolean, float, double and long primitive constants are not convertible
      // to int, so the handle creation must fail with a CCE.
      try {
        MethodHandles.constant(int.class, false);
        fail();
      } catch (ClassCastException expected) {
      }

      try {
        MethodHandles.constant(int.class, 0.1f);
        fail();
      } catch (ClassCastException expected) {
      }

      try {
        MethodHandles.constant(int.class, 0.2);
        fail();
      } catch (ClassCastException expected) {
      }

      try {
        MethodHandles.constant(int.class, 73l);
        fail();
      } catch (ClassCastException expected) {
      }
    }

    // long constants.
    {
      MethodHandle constant = MethodHandles.constant(long.class, 56l);
      long value = (long) constant.invoke();
      if (value != 56l) {
        System.out.println("Unexpected value: " + value);
      }

      constant = MethodHandles.constant(long.class, (int) 56);
      value = (long) constant.invoke();
      if (value != 56l) {
        System.out.println("Unexpected value: " + value);
      }
    }

    // byte constants.
    {
      MethodHandle constant = MethodHandles.constant(byte.class, (byte) 0x12);
      byte value = (byte) constant.invoke();
      if (value != (byte) 0x12) {
        System.out.println("Unexpected value: " + value);
      }
    }

    // boolean constants.
    {
      MethodHandle constant = MethodHandles.constant(boolean.class, true);
      boolean value = (boolean) constant.invoke();
      if (!value) {
        System.out.println("Unexpected value: " + value);
      }
    }

    // char constants.
    {
      MethodHandle constant = MethodHandles.constant(char.class, 'f');
      char value = (char) constant.invoke();
      if (value != 'f') {
        System.out.println("Unexpected value: " + value);
      }
    }

    // short constants.
    {
      MethodHandle constant = MethodHandles.constant(short.class, (short) 123);
      short value = (short) constant.invoke();
      if (value != (short) 123) {
        System.out.println("Unexpected value: " + value);
      }
    }

    // float constants.
    {
      MethodHandle constant = MethodHandles.constant(float.class, 56.0f);
      float value = (float) constant.invoke();
      if (value != 56.0f) {
        System.out.println("Unexpected value: " + value);
      }
    }

    // double constants.
    {
      MethodHandle constant = MethodHandles.constant(double.class, 256.0);
      double value = (double) constant.invoke();
      if (value != 256.0) {
        System.out.println("Unexpected value: " + value);
      }
    }

    // reference constants.
    {
      MethodHandle constant = MethodHandles.constant(String.class, "256.0");
      String value = (String) constant.invoke();
      assertEquals("256.0", value);
    }
  }

  public static void fail() {
    System.out.println("FAIL");
    Thread.dumpStack();
  }

  public static void assertEquals(String s1, String s2) {
    if (s1 == s2) {
      return;
    }

    if (s1 != null && s2 != null && s1.equals(s2)) {
      return;
    }

    throw new AssertionError("assertEquals s1: " + s1 + ", s2: " + s2);
  }
}


