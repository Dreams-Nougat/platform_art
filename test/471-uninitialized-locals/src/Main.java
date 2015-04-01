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

import java.lang.reflect.Method;

public class Main {

  public static void main(String args[]) throws Exception {
    // Workaround for b/18051191.
    System.out.println("Test started");
    try {
      Class<?> c = Class.forName("Test");
      Method m = c.getMethod("ThrowException", (Class[]) null);
      m.invoke(null, (Object[]) null);
    } catch (VerifyError e) {
       // Compilation should go fine but we expect the runtime verification to fail.
      return;
    }

    throw new Error("Failed to preset verification error!");
  }

}
