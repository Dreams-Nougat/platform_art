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
    public static void main(String[] args) {
        // Reserve around 1/4 of the RAM for keeping objects live.
        long maxMem = Runtime.getRuntime().maxMemory();
        Object[] holder = new Object[(int)maxMem / 16];
        int count = 0;
        try {
            while (true) {
                holder[count++] = new byte[7000];
            }
        } catch (OutOfMemoryError e) {}
        for (int i = 0; i < count; ++i) {
            holder[i] = null;
        }
        // Make sure the heap can handle allocating large object array. This makes sure that free
        // pages are correctly coalesced together by the allocator.
        holder[0] = new Object[(int)maxMem / 8];
    }
}
