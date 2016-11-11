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

import java.lang.ref.WeakReference;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public class Main {
    public static void main(String[] args) {
        System.loadLibrary(args[0]);
        try {
            testClearDexCache();
        } catch (Throwable t) {
            t.printStackTrace();
        }
        try {
            testMultiDex();
        } catch (Throwable t) {
            t.printStackTrace();
        }
        try {
            testRacyLoader();
        } catch (Throwable t) {
            t.printStackTrace();
        }
        try {
            testMisbehavingLoader();
        } catch (Throwable t) {
            t.printStackTrace();
        }
    }

    private static void testClearDexCache() throws Exception {
        DelegatingLoader delegating_loader = createDelegatingLoader();
        Class<?> helper = delegating_loader.loadClass("Helper1");

        WeakReference<Class<?>> weak_test1 = wrapHelperGet(helper);
        changeInner(delegating_loader);
        clearResolvedTypes(helper);
        Runtime.getRuntime().gc();
        WeakReference<Class<?>> weak_test2 = wrapHelperGet(helper);

        Class<?> test1 = weak_test1.get();
        if (test1 == null) {
            System.out.println("test1 disappeared");
        }
        Class<?> test2 = weak_test2.get();
        if (test2 == null) {
            System.out.println("test2 disappeared");
        }
        if (test1 != test2) {
            System.out.println("test1 != test2");
        }

        test1 = null;
        test2 = null;
        clearResolvedTypes(helper);
        helper = null;
        delegating_loader = null;
        Runtime.getRuntime().gc();
        if (weak_test1.get() != null) {
            System.out.println("weak_test1 still not null");
        }
        if (weak_test2.get() != null) {
            System.out.println("weak_test2 still not null");
        }
        System.out.println("testClearDexCache done");
    }

    private static void testMultiDex() throws Exception {
        DelegatingLoader delegating_loader = createDelegatingLoader();

        Class<?> helper1 = delegating_loader.loadClass("Helper1");
        WeakReference<Class<?>> weak_test1 = wrapHelperGet(helper1);

        changeInner(delegating_loader);

        Class<?> helper2 = delegating_loader.loadClass("Helper2");
        WeakReference<Class<?>> weak_test2 = wrapHelperGet(helper2);

        Class<?> test1 = weak_test1.get();
        if (test1 == null) {
            System.out.println("test1 disappeared");
        }
        Class<?> test2 = weak_test2.get();
        if (test2 == null) {
            System.out.println("test2 disappeared");
        }
        if (test1 != test2) {
            System.out.println("test1 != test2");
        }

        test1 = null;
        test2 = null;
        delegating_loader = null;
        helper1 = null;
        helper2 = null;
        Runtime.getRuntime().gc();
        if (weak_test1.get() != null) {
            System.out.println("weak_test1 still not null");
        }
        if (weak_test2.get() != null) {
            System.out.println("weak_test2 still not null");
        }
        System.out.println("testMultiDex done");
    }

    private static void testRacyLoader() throws Exception {
        final ClassLoader system_loader = ClassLoader.getSystemClassLoader();

        final Thread[] threads = new Thread[4];
        final Object[] results = new Object[threads.length];

        final RacyLoader racy_loader = new RacyLoader(system_loader, threads.length);
        final Class<?> helper1 = racy_loader.loadClass("Helper1");

        for (int i = 0; i != threads.length; ++i) {
            final int my_index = i;
            Thread t = new Thread() {
                public void run() {
                    try {
                        results[my_index] = Main.wrapHelperGet(helper1);
                    } catch (InvocationTargetException ite) {
                        results[my_index] = ite.getCause();
                    } catch (Throwable t) {
                        results[my_index] = t;
                    }
                }
            };
            t.start();
            threads[i] = t;
        }
        for (Thread t : threads) {
            t.join();
        }
        int throwables = 0;
        int class_weaks = 0;
        int unique_class_weaks = 0;
        for (int i = 0; i != results.length; ++i) {
            Object r = results[i];
            if (r instanceof Throwable) {
                ++throwables;
                System.out.println(((Throwable) r).getMessage());
            } else if (r instanceof WeakReference<?>) {
                Object ref = ((WeakReference<?>) r).get();
                if (ref instanceof Class<?>) {
                    ++class_weaks;
                    ++unique_class_weaks;
                    for (int j = 0; j != i; ++j) {
                        if ((results[j] instanceof WeakReference<?>) &&
                            (((WeakReference<?>) (results[j])).get() == ref)) {
                            --unique_class_weaks;
                            break;
                        }
                    }
                }
            }
        }
        System.out.println("total: " + results.length);
        System.out.println("  throwables: " + throwables);
        System.out.println("  class_weaks: " + class_weaks
            + " (" + unique_class_weaks + " unique)");

        System.out.println("testRacyLoader done");
    }

    private static void testMisbehavingLoader() throws Exception {
        ClassLoader system_loader = ClassLoader.getSystemClassLoader();
        DefiningLoader defining_loader = new DefiningLoader(system_loader);
        MisbehavingLoader misbehaving_loader =
            new MisbehavingLoader(system_loader, defining_loader);
        Class<?> helper = misbehaving_loader.loadClass("Helper1");
  
        try {
            WeakReference<Class<?>> weak_test = wrapHelperGet(helper);
        } catch (InvocationTargetException ite) {
            System.out.println(ite.getCause().getMessage());
        }
        System.out.println("testMisbehavingLoader done");
    }

    private static DelegatingLoader createDelegatingLoader() {
        ClassLoader system_loader = ClassLoader.getSystemClassLoader();
        DefiningLoader defining_loader = new DefiningLoader(system_loader);
        return new DelegatingLoader(system_loader, defining_loader);
    }

    private static void changeInner(DelegatingLoader delegating_loader) {
        ClassLoader system_loader = ClassLoader.getSystemClassLoader();
        DefiningLoader defining_loader = new DefiningLoader(system_loader);
        delegating_loader.resetDefiningLoader(defining_loader);
    }

    private static WeakReference<Class<?>> wrapHelperGet(Class<?> helper) throws Exception {
        Method get = helper.getDeclaredMethod("get");
        return new WeakReference<Class<?>>((Class<?>) get.invoke(null));
    }

    public static native void clearResolvedTypes(Class<?> c);
}
