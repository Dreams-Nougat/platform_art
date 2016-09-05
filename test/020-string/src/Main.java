/*
 * Copyright (C) 2007 The Android Open Source Project
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

import java.nio.charset.Charset;
import java.io.UnsupportedEncodingException;

/**
 * Simple string test.
 */
public class Main {
    public static void main(String args[]) {
        basicTest();
        indexTest();
        constructorTest();
        copyTest();

        int count = 100000;
        long noConflictTime = Other.benchmarkNoConflict(count);
        System.out.println(
            "benchmarkNoConflict: " + noConflictTime + "ms for " + count + " iterations.");
        long conflictTime = Other.benchmarkConflict(count);
        System.out.println(
            "benchmarkConflict: " + conflictTime + "ms for " + count + " iterations.");

        // Now, try to rerun the benchmarks without any memory.
        Object[] memory = wasteMemory();
        noConflictTime = Other.benchmarkNoConflict(count);
        if (memory == null) { throw new Error(); }  // Make sure memory is live.
        memory = null;  // Free memory to allow printing benchmark results.
        System.out.println(
            "benchmarkNoConflict: " + noConflictTime + "ms for " + count + " iterations.");

        memory = wasteMemory();
        conflictTime = Other.benchmarkConflict(count);
        if (memory == null) { throw new Error(); }  // Make sure memory is live.
        memory = null;  // Free memory to allow printing benchmark results.
        System.out.println(
            "benchmarkConflict: " + conflictTime + "ms for " + count + " iterations.");
    }

    public static Object[] wasteMemory() {
      // Waste all memory.
      Object[] memory = new Object[128 * 1024];
      int index = 0;
      for (int i = 1024 * 1024; i != 0; ) {
        try {
          memory[index] = new byte[i];
          ++index;
        } catch (OutOfMemoryError oome) {
          i /= 2;
        }
      }
      try {
        memory[index] = new Object();
        ++index;
      } catch (OutOfMemoryError oome) {
      }
      return memory;
    }

    public static void basicTest() {
        String baseStr = "*** This is a very nice string!!!";
        String testStr;
        int i;

        testStr = baseStr.substring(4, baseStr.length() - 3);
        System.out.println("testStr is '" + testStr + "'");

        /* sloppy for loop */
        for (i = 0; i < testStr.length(); i++)
            System.out.print(testStr.charAt(i));
        System.out.print("\n");

        String testStr2 = "This is a very nice strinG";
        if (testStr.length() != testStr2.length())
            System.out.println("WARNING: stringTest length mismatch");

        int compareResult = testStr.compareTo(testStr2);
        if (compareResult > 0) {
          System.out.println("Compare result is greater than zero");
        } else if (compareResult == 0) {
          System.out.println("Compare result is equal to zero");
        } else {
          System.out.println("Compare result is less than zero");
        }

        // expected: -65302
        String s1 = "\u0c6d\u0cb6\u0d00\u0000\u0080\u0080\u0080\u0000\u0002\u0002\u0002\u0000\u00e9\u00e9\u00e9";
        String s2 = "\u0c6d\u0cb6\u0d00\u0000\u0080\u0080\u0080\u0000\u0002\u0002\u0002\u0000\uffff\uffff\uffff\u00e9\u00e9\u00e9";
        System.out.println("Compare unicode: " + s1.compareTo(s2));

        try {
            testStr.charAt(500);
            System.out.println("GLITCH: expected exception");
        } catch (StringIndexOutOfBoundsException sioobe) {
            System.out.println("Got expected exception");
        }
    }

    public static void indexTest() {
        String baseStr = "The quick brown fox jumps over the lazy dog!";
        String subStr;

        subStr = baseStr.substring(5, baseStr.length() - 4);
        System.out.println("subStr is '" + subStr + "'");

        System.out.println("Indexes are: " +
            baseStr.indexOf('T') + ":" +
            subStr.indexOf('T') + ":" +
            subStr.indexOf('u') + ":" +
            baseStr.indexOf('!') + ":" +
            subStr.indexOf('y') + ":" +
            subStr.indexOf('d') + ":" +
            baseStr.indexOf('x') + ":" +
            subStr.indexOf('x', 0) + ":" +
            subStr.indexOf('x', -1) + ":" +
            subStr.indexOf('x', 200) + ":" +
            baseStr.indexOf('x', 17) + ":" +
            baseStr.indexOf('x', 18) + ":" +
            baseStr.indexOf('x', 19) + ":" +
            subStr.indexOf('x', 13) + ":" +
            subStr.indexOf('x', 14) + ":" +
            subStr.indexOf('&') + ":" +
            baseStr.indexOf(0x12341234));
    }

    public static void constructorTest() {
        byte[] byteArray = "byteArray".getBytes();
        char[] charArray = new char[] { 'c', 'h', 'a', 'r', 'A', 'r', 'r', 'a', 'y' };
        String charsetName = "US-ASCII";
        Charset charset = Charset.forName("UTF-8");
        String string = "string";
        StringBuffer stringBuffer = new StringBuffer("stringBuffer");
        int [] codePoints = new int[] { 65, 66, 67, 68, 69 };
        StringBuilder stringBuilder = new StringBuilder("stringBuilder");

        String s1 = new String();
        String s2 = new String(byteArray);
        String s3 = new String(byteArray, 1);
        String s4 = new String(byteArray, 0, 4);
        String s5 = new String(byteArray, 2, 4, 5);

        try {
            String s6 = new String(byteArray, 2, 4, charsetName);
            String s7 = new String(byteArray, charsetName);
        } catch (UnsupportedEncodingException e) {
            System.out.println("Got unexpected UnsupportedEncodingException");
        }
        String s8 = new String(byteArray, 3, 3, charset);
        String s9 = new String(byteArray, charset);
        String s10 = new String(charArray);
        String s11 = new String(charArray, 0, 4);
        String s12 = new String(string);
        String s13 = new String(stringBuffer);
        String s14 = new String(codePoints, 1, 3);
        String s15 = new String(stringBuilder);
    }

    public static void copyTest() {
        String src = new String("Hello Android");
        char[] dst = new char[7];
        char[] tmp = null;

        try {
            src.getChars(2, 9, tmp, 0);
            System.out.println("GLITCH: expected exception");
        } catch (NullPointerException npe) {
            System.out.println("Got expected exception");
        }

        try {
            src.getChars(-1, 9, dst, 0);
            System.out.println("GLITCH: expected exception");
        } catch (StringIndexOutOfBoundsException sioobe) {
            System.out.println("Got expected exception");
        }

        try {
            src.getChars(2, 19, dst, 0);
            System.out.println("GLITCH: expected exception");
        } catch (StringIndexOutOfBoundsException sioobe) {
            System.out.println("Got expected exception");
        }

        try {
            src.getChars(2, 1, dst, 0);
            System.out.println("GLITCH: expected exception");
        } catch (StringIndexOutOfBoundsException sioobe) {
            System.out.println("Got expected exception");
        }

        try {
            src.getChars(2, 10, dst, 0);
            System.out.println("GLITCH: expected exception");
        } catch (ArrayIndexOutOfBoundsException aioobe) {
            System.out.println("Got expected exception");
        }

        src.getChars(2, 9, dst, 0);
        System.out.println(new String(dst));
    }
}

class Other {
    public static final String string_0000 = "TestString_0000";
    public static final String string_0001 = "TestString_0001";
    public static final String string_0002 = "TestString_0002";
    public static final String string_0003 = "TestString_0003";
    public static final String string_0004 = "TestString_0004";
    public static final String string_0005 = "TestString_0005";
    public static final String string_0006 = "TestString_0006";
    public static final String string_0007 = "TestString_0007";
    public static final String string_0008 = "TestString_0008";
    public static final String string_0009 = "TestString_0009";
    public static final String string_0010 = "TestString_0010";
    public static final String string_0011 = "TestString_0011";
    public static final String string_0012 = "TestString_0012";
    public static final String string_0013 = "TestString_0013";
    public static final String string_0014 = "TestString_0014";
    public static final String string_0015 = "TestString_0015";
    public static final String string_0016 = "TestString_0016";
    public static final String string_0017 = "TestString_0017";
    public static final String string_0018 = "TestString_0018";
    public static final String string_0019 = "TestString_0019";
    public static final String string_0020 = "TestString_0020";
    public static final String string_0021 = "TestString_0021";
    public static final String string_0022 = "TestString_0022";
    public static final String string_0023 = "TestString_0023";
    public static final String string_0024 = "TestString_0024";
    public static final String string_0025 = "TestString_0025";
    public static final String string_0026 = "TestString_0026";
    public static final String string_0027 = "TestString_0027";
    public static final String string_0028 = "TestString_0028";
    public static final String string_0029 = "TestString_0029";
    public static final String string_0030 = "TestString_0030";
    public static final String string_0031 = "TestString_0031";
    public static final String string_0032 = "TestString_0032";
    public static final String string_0033 = "TestString_0033";
    public static final String string_0034 = "TestString_0034";
    public static final String string_0035 = "TestString_0035";
    public static final String string_0036 = "TestString_0036";
    public static final String string_0037 = "TestString_0037";
    public static final String string_0038 = "TestString_0038";
    public static final String string_0039 = "TestString_0039";
    public static final String string_0040 = "TestString_0040";
    public static final String string_0041 = "TestString_0041";
    public static final String string_0042 = "TestString_0042";
    public static final String string_0043 = "TestString_0043";
    public static final String string_0044 = "TestString_0044";
    public static final String string_0045 = "TestString_0045";
    public static final String string_0046 = "TestString_0046";
    public static final String string_0047 = "TestString_0047";
    public static final String string_0048 = "TestString_0048";
    public static final String string_0049 = "TestString_0049";
    public static final String string_0050 = "TestString_0050";
    public static final String string_0051 = "TestString_0051";
    public static final String string_0052 = "TestString_0052";
    public static final String string_0053 = "TestString_0053";
    public static final String string_0054 = "TestString_0054";
    public static final String string_0055 = "TestString_0055";
    public static final String string_0056 = "TestString_0056";
    public static final String string_0057 = "TestString_0057";
    public static final String string_0058 = "TestString_0058";
    public static final String string_0059 = "TestString_0059";
    public static final String string_0060 = "TestString_0060";
    public static final String string_0061 = "TestString_0061";
    public static final String string_0062 = "TestString_0062";
    public static final String string_0063 = "TestString_0063";
    public static final String string_0064 = "TestString_0064";
    public static final String string_0065 = "TestString_0065";
    public static final String string_0066 = "TestString_0066";
    public static final String string_0067 = "TestString_0067";
    public static final String string_0068 = "TestString_0068";
    public static final String string_0069 = "TestString_0069";
    public static final String string_0070 = "TestString_0070";
    public static final String string_0071 = "TestString_0071";
    public static final String string_0072 = "TestString_0072";
    public static final String string_0073 = "TestString_0073";
    public static final String string_0074 = "TestString_0074";
    public static final String string_0075 = "TestString_0075";
    public static final String string_0076 = "TestString_0076";
    public static final String string_0077 = "TestString_0077";
    public static final String string_0078 = "TestString_0078";
    public static final String string_0079 = "TestString_0079";
    public static final String string_0080 = "TestString_0080";
    public static final String string_0081 = "TestString_0081";
    public static final String string_0082 = "TestString_0082";
    public static final String string_0083 = "TestString_0083";
    public static final String string_0084 = "TestString_0084";
    public static final String string_0085 = "TestString_0085";
    public static final String string_0086 = "TestString_0086";
    public static final String string_0087 = "TestString_0087";
    public static final String string_0088 = "TestString_0088";
    public static final String string_0089 = "TestString_0089";
    public static final String string_0090 = "TestString_0090";
    public static final String string_0091 = "TestString_0091";
    public static final String string_0092 = "TestString_0092";
    public static final String string_0093 = "TestString_0093";
    public static final String string_0094 = "TestString_0094";
    public static final String string_0095 = "TestString_0095";
    public static final String string_0096 = "TestString_0096";
    public static final String string_0097 = "TestString_0097";
    public static final String string_0098 = "TestString_0098";
    public static final String string_0099 = "TestString_0099";
    public static final String string_0100 = "TestString_0100";
    public static final String string_0101 = "TestString_0101";
    public static final String string_0102 = "TestString_0102";
    public static final String string_0103 = "TestString_0103";
    public static final String string_0104 = "TestString_0104";
    public static final String string_0105 = "TestString_0105";
    public static final String string_0106 = "TestString_0106";
    public static final String string_0107 = "TestString_0107";
    public static final String string_0108 = "TestString_0108";
    public static final String string_0109 = "TestString_0109";
    public static final String string_0110 = "TestString_0110";
    public static final String string_0111 = "TestString_0111";
    public static final String string_0112 = "TestString_0112";
    public static final String string_0113 = "TestString_0113";
    public static final String string_0114 = "TestString_0114";
    public static final String string_0115 = "TestString_0115";
    public static final String string_0116 = "TestString_0116";
    public static final String string_0117 = "TestString_0117";
    public static final String string_0118 = "TestString_0118";
    public static final String string_0119 = "TestString_0119";
    public static final String string_0120 = "TestString_0120";
    public static final String string_0121 = "TestString_0121";
    public static final String string_0122 = "TestString_0122";
    public static final String string_0123 = "TestString_0123";
    public static final String string_0124 = "TestString_0124";
    public static final String string_0125 = "TestString_0125";
    public static final String string_0126 = "TestString_0126";
    public static final String string_0127 = "TestString_0127";
    public static final String string_0128 = "TestString_0128";
    public static final String string_0129 = "TestString_0129";
    public static final String string_0130 = "TestString_0130";
    public static final String string_0131 = "TestString_0131";
    public static final String string_0132 = "TestString_0132";
    public static final String string_0133 = "TestString_0133";
    public static final String string_0134 = "TestString_0134";
    public static final String string_0135 = "TestString_0135";
    public static final String string_0136 = "TestString_0136";
    public static final String string_0137 = "TestString_0137";
    public static final String string_0138 = "TestString_0138";
    public static final String string_0139 = "TestString_0139";
    public static final String string_0140 = "TestString_0140";
    public static final String string_0141 = "TestString_0141";
    public static final String string_0142 = "TestString_0142";
    public static final String string_0143 = "TestString_0143";
    public static final String string_0144 = "TestString_0144";
    public static final String string_0145 = "TestString_0145";
    public static final String string_0146 = "TestString_0146";
    public static final String string_0147 = "TestString_0147";
    public static final String string_0148 = "TestString_0148";
    public static final String string_0149 = "TestString_0149";
    public static final String string_0150 = "TestString_0150";
    public static final String string_0151 = "TestString_0151";
    public static final String string_0152 = "TestString_0152";
    public static final String string_0153 = "TestString_0153";
    public static final String string_0154 = "TestString_0154";
    public static final String string_0155 = "TestString_0155";
    public static final String string_0156 = "TestString_0156";
    public static final String string_0157 = "TestString_0157";
    public static final String string_0158 = "TestString_0158";
    public static final String string_0159 = "TestString_0159";
    public static final String string_0160 = "TestString_0160";
    public static final String string_0161 = "TestString_0161";
    public static final String string_0162 = "TestString_0162";
    public static final String string_0163 = "TestString_0163";
    public static final String string_0164 = "TestString_0164";
    public static final String string_0165 = "TestString_0165";
    public static final String string_0166 = "TestString_0166";
    public static final String string_0167 = "TestString_0167";
    public static final String string_0168 = "TestString_0168";
    public static final String string_0169 = "TestString_0169";
    public static final String string_0170 = "TestString_0170";
    public static final String string_0171 = "TestString_0171";
    public static final String string_0172 = "TestString_0172";
    public static final String string_0173 = "TestString_0173";
    public static final String string_0174 = "TestString_0174";
    public static final String string_0175 = "TestString_0175";
    public static final String string_0176 = "TestString_0176";
    public static final String string_0177 = "TestString_0177";
    public static final String string_0178 = "TestString_0178";
    public static final String string_0179 = "TestString_0179";
    public static final String string_0180 = "TestString_0180";
    public static final String string_0181 = "TestString_0181";
    public static final String string_0182 = "TestString_0182";
    public static final String string_0183 = "TestString_0183";
    public static final String string_0184 = "TestString_0184";
    public static final String string_0185 = "TestString_0185";
    public static final String string_0186 = "TestString_0186";
    public static final String string_0187 = "TestString_0187";
    public static final String string_0188 = "TestString_0188";
    public static final String string_0189 = "TestString_0189";
    public static final String string_0190 = "TestString_0190";
    public static final String string_0191 = "TestString_0191";
    public static final String string_0192 = "TestString_0192";
    public static final String string_0193 = "TestString_0193";
    public static final String string_0194 = "TestString_0194";
    public static final String string_0195 = "TestString_0195";
    public static final String string_0196 = "TestString_0196";
    public static final String string_0197 = "TestString_0197";
    public static final String string_0198 = "TestString_0198";
    public static final String string_0199 = "TestString_0199";
    public static final String string_0200 = "TestString_0200";
    public static final String string_0201 = "TestString_0201";
    public static final String string_0202 = "TestString_0202";
    public static final String string_0203 = "TestString_0203";
    public static final String string_0204 = "TestString_0204";
    public static final String string_0205 = "TestString_0205";
    public static final String string_0206 = "TestString_0206";
    public static final String string_0207 = "TestString_0207";
    public static final String string_0208 = "TestString_0208";
    public static final String string_0209 = "TestString_0209";
    public static final String string_0210 = "TestString_0210";
    public static final String string_0211 = "TestString_0211";
    public static final String string_0212 = "TestString_0212";
    public static final String string_0213 = "TestString_0213";
    public static final String string_0214 = "TestString_0214";
    public static final String string_0215 = "TestString_0215";
    public static final String string_0216 = "TestString_0216";
    public static final String string_0217 = "TestString_0217";
    public static final String string_0218 = "TestString_0218";
    public static final String string_0219 = "TestString_0219";
    public static final String string_0220 = "TestString_0220";
    public static final String string_0221 = "TestString_0221";
    public static final String string_0222 = "TestString_0222";
    public static final String string_0223 = "TestString_0223";
    public static final String string_0224 = "TestString_0224";
    public static final String string_0225 = "TestString_0225";
    public static final String string_0226 = "TestString_0226";
    public static final String string_0227 = "TestString_0227";
    public static final String string_0228 = "TestString_0228";
    public static final String string_0229 = "TestString_0229";
    public static final String string_0230 = "TestString_0230";
    public static final String string_0231 = "TestString_0231";
    public static final String string_0232 = "TestString_0232";
    public static final String string_0233 = "TestString_0233";
    public static final String string_0234 = "TestString_0234";
    public static final String string_0235 = "TestString_0235";
    public static final String string_0236 = "TestString_0236";
    public static final String string_0237 = "TestString_0237";
    public static final String string_0238 = "TestString_0238";
    public static final String string_0239 = "TestString_0239";
    public static final String string_0240 = "TestString_0240";
    public static final String string_0241 = "TestString_0241";
    public static final String string_0242 = "TestString_0242";
    public static final String string_0243 = "TestString_0243";
    public static final String string_0244 = "TestString_0244";
    public static final String string_0245 = "TestString_0245";
    public static final String string_0246 = "TestString_0246";
    public static final String string_0247 = "TestString_0247";
    public static final String string_0248 = "TestString_0248";
    public static final String string_0249 = "TestString_0249";
    public static final String string_0250 = "TestString_0250";
    public static final String string_0251 = "TestString_0251";
    public static final String string_0252 = "TestString_0252";
    public static final String string_0253 = "TestString_0253";
    public static final String string_0254 = "TestString_0254";
    public static final String string_0255 = "TestString_0255";
    public static final String string_0256 = "TestString_0256";
    public static final String string_0257 = "TestString_0257";
    public static final String string_0258 = "TestString_0258";
    public static final String string_0259 = "TestString_0259";
    public static final String string_0260 = "TestString_0260";
    public static final String string_0261 = "TestString_0261";
    public static final String string_0262 = "TestString_0262";
    public static final String string_0263 = "TestString_0263";
    public static final String string_0264 = "TestString_0264";
    public static final String string_0265 = "TestString_0265";
    public static final String string_0266 = "TestString_0266";
    public static final String string_0267 = "TestString_0267";
    public static final String string_0268 = "TestString_0268";
    public static final String string_0269 = "TestString_0269";
    public static final String string_0270 = "TestString_0270";
    public static final String string_0271 = "TestString_0271";
    public static final String string_0272 = "TestString_0272";
    public static final String string_0273 = "TestString_0273";
    public static final String string_0274 = "TestString_0274";
    public static final String string_0275 = "TestString_0275";
    public static final String string_0276 = "TestString_0276";
    public static final String string_0277 = "TestString_0277";
    public static final String string_0278 = "TestString_0278";
    public static final String string_0279 = "TestString_0279";
    public static final String string_0280 = "TestString_0280";
    public static final String string_0281 = "TestString_0281";
    public static final String string_0282 = "TestString_0282";
    public static final String string_0283 = "TestString_0283";
    public static final String string_0284 = "TestString_0284";
    public static final String string_0285 = "TestString_0285";
    public static final String string_0286 = "TestString_0286";
    public static final String string_0287 = "TestString_0287";
    public static final String string_0288 = "TestString_0288";
    public static final String string_0289 = "TestString_0289";
    public static final String string_0290 = "TestString_0290";
    public static final String string_0291 = "TestString_0291";
    public static final String string_0292 = "TestString_0292";
    public static final String string_0293 = "TestString_0293";
    public static final String string_0294 = "TestString_0294";
    public static final String string_0295 = "TestString_0295";
    public static final String string_0296 = "TestString_0296";
    public static final String string_0297 = "TestString_0297";
    public static final String string_0298 = "TestString_0298";
    public static final String string_0299 = "TestString_0299";
    public static final String string_0300 = "TestString_0300";
    public static final String string_0301 = "TestString_0301";
    public static final String string_0302 = "TestString_0302";
    public static final String string_0303 = "TestString_0303";
    public static final String string_0304 = "TestString_0304";
    public static final String string_0305 = "TestString_0305";
    public static final String string_0306 = "TestString_0306";
    public static final String string_0307 = "TestString_0307";
    public static final String string_0308 = "TestString_0308";
    public static final String string_0309 = "TestString_0309";
    public static final String string_0310 = "TestString_0310";
    public static final String string_0311 = "TestString_0311";
    public static final String string_0312 = "TestString_0312";
    public static final String string_0313 = "TestString_0313";
    public static final String string_0314 = "TestString_0314";
    public static final String string_0315 = "TestString_0315";
    public static final String string_0316 = "TestString_0316";
    public static final String string_0317 = "TestString_0317";
    public static final String string_0318 = "TestString_0318";
    public static final String string_0319 = "TestString_0319";
    public static final String string_0320 = "TestString_0320";
    public static final String string_0321 = "TestString_0321";
    public static final String string_0322 = "TestString_0322";
    public static final String string_0323 = "TestString_0323";
    public static final String string_0324 = "TestString_0324";
    public static final String string_0325 = "TestString_0325";
    public static final String string_0326 = "TestString_0326";
    public static final String string_0327 = "TestString_0327";
    public static final String string_0328 = "TestString_0328";
    public static final String string_0329 = "TestString_0329";
    public static final String string_0330 = "TestString_0330";
    public static final String string_0331 = "TestString_0331";
    public static final String string_0332 = "TestString_0332";
    public static final String string_0333 = "TestString_0333";
    public static final String string_0334 = "TestString_0334";
    public static final String string_0335 = "TestString_0335";
    public static final String string_0336 = "TestString_0336";
    public static final String string_0337 = "TestString_0337";
    public static final String string_0338 = "TestString_0338";
    public static final String string_0339 = "TestString_0339";
    public static final String string_0340 = "TestString_0340";
    public static final String string_0341 = "TestString_0341";
    public static final String string_0342 = "TestString_0342";
    public static final String string_0343 = "TestString_0343";
    public static final String string_0344 = "TestString_0344";
    public static final String string_0345 = "TestString_0345";
    public static final String string_0346 = "TestString_0346";
    public static final String string_0347 = "TestString_0347";
    public static final String string_0348 = "TestString_0348";
    public static final String string_0349 = "TestString_0349";
    public static final String string_0350 = "TestString_0350";
    public static final String string_0351 = "TestString_0351";
    public static final String string_0352 = "TestString_0352";
    public static final String string_0353 = "TestString_0353";
    public static final String string_0354 = "TestString_0354";
    public static final String string_0355 = "TestString_0355";
    public static final String string_0356 = "TestString_0356";
    public static final String string_0357 = "TestString_0357";
    public static final String string_0358 = "TestString_0358";
    public static final String string_0359 = "TestString_0359";
    public static final String string_0360 = "TestString_0360";
    public static final String string_0361 = "TestString_0361";
    public static final String string_0362 = "TestString_0362";
    public static final String string_0363 = "TestString_0363";
    public static final String string_0364 = "TestString_0364";
    public static final String string_0365 = "TestString_0365";
    public static final String string_0366 = "TestString_0366";
    public static final String string_0367 = "TestString_0367";
    public static final String string_0368 = "TestString_0368";
    public static final String string_0369 = "TestString_0369";
    public static final String string_0370 = "TestString_0370";
    public static final String string_0371 = "TestString_0371";
    public static final String string_0372 = "TestString_0372";
    public static final String string_0373 = "TestString_0373";
    public static final String string_0374 = "TestString_0374";
    public static final String string_0375 = "TestString_0375";
    public static final String string_0376 = "TestString_0376";
    public static final String string_0377 = "TestString_0377";
    public static final String string_0378 = "TestString_0378";
    public static final String string_0379 = "TestString_0379";
    public static final String string_0380 = "TestString_0380";
    public static final String string_0381 = "TestString_0381";
    public static final String string_0382 = "TestString_0382";
    public static final String string_0383 = "TestString_0383";
    public static final String string_0384 = "TestString_0384";
    public static final String string_0385 = "TestString_0385";
    public static final String string_0386 = "TestString_0386";
    public static final String string_0387 = "TestString_0387";
    public static final String string_0388 = "TestString_0388";
    public static final String string_0389 = "TestString_0389";
    public static final String string_0390 = "TestString_0390";
    public static final String string_0391 = "TestString_0391";
    public static final String string_0392 = "TestString_0392";
    public static final String string_0393 = "TestString_0393";
    public static final String string_0394 = "TestString_0394";
    public static final String string_0395 = "TestString_0395";
    public static final String string_0396 = "TestString_0396";
    public static final String string_0397 = "TestString_0397";
    public static final String string_0398 = "TestString_0398";
    public static final String string_0399 = "TestString_0399";
    public static final String string_0400 = "TestString_0400";
    public static final String string_0401 = "TestString_0401";
    public static final String string_0402 = "TestString_0402";
    public static final String string_0403 = "TestString_0403";
    public static final String string_0404 = "TestString_0404";
    public static final String string_0405 = "TestString_0405";
    public static final String string_0406 = "TestString_0406";
    public static final String string_0407 = "TestString_0407";
    public static final String string_0408 = "TestString_0408";
    public static final String string_0409 = "TestString_0409";
    public static final String string_0410 = "TestString_0410";
    public static final String string_0411 = "TestString_0411";
    public static final String string_0412 = "TestString_0412";
    public static final String string_0413 = "TestString_0413";
    public static final String string_0414 = "TestString_0414";
    public static final String string_0415 = "TestString_0415";
    public static final String string_0416 = "TestString_0416";
    public static final String string_0417 = "TestString_0417";
    public static final String string_0418 = "TestString_0418";
    public static final String string_0419 = "TestString_0419";
    public static final String string_0420 = "TestString_0420";
    public static final String string_0421 = "TestString_0421";
    public static final String string_0422 = "TestString_0422";
    public static final String string_0423 = "TestString_0423";
    public static final String string_0424 = "TestString_0424";
    public static final String string_0425 = "TestString_0425";
    public static final String string_0426 = "TestString_0426";
    public static final String string_0427 = "TestString_0427";
    public static final String string_0428 = "TestString_0428";
    public static final String string_0429 = "TestString_0429";
    public static final String string_0430 = "TestString_0430";
    public static final String string_0431 = "TestString_0431";
    public static final String string_0432 = "TestString_0432";
    public static final String string_0433 = "TestString_0433";
    public static final String string_0434 = "TestString_0434";
    public static final String string_0435 = "TestString_0435";
    public static final String string_0436 = "TestString_0436";
    public static final String string_0437 = "TestString_0437";
    public static final String string_0438 = "TestString_0438";
    public static final String string_0439 = "TestString_0439";
    public static final String string_0440 = "TestString_0440";
    public static final String string_0441 = "TestString_0441";
    public static final String string_0442 = "TestString_0442";
    public static final String string_0443 = "TestString_0443";
    public static final String string_0444 = "TestString_0444";
    public static final String string_0445 = "TestString_0445";
    public static final String string_0446 = "TestString_0446";
    public static final String string_0447 = "TestString_0447";
    public static final String string_0448 = "TestString_0448";
    public static final String string_0449 = "TestString_0449";
    public static final String string_0450 = "TestString_0450";
    public static final String string_0451 = "TestString_0451";
    public static final String string_0452 = "TestString_0452";
    public static final String string_0453 = "TestString_0453";
    public static final String string_0454 = "TestString_0454";
    public static final String string_0455 = "TestString_0455";
    public static final String string_0456 = "TestString_0456";
    public static final String string_0457 = "TestString_0457";
    public static final String string_0458 = "TestString_0458";
    public static final String string_0459 = "TestString_0459";
    public static final String string_0460 = "TestString_0460";
    public static final String string_0461 = "TestString_0461";
    public static final String string_0462 = "TestString_0462";
    public static final String string_0463 = "TestString_0463";
    public static final String string_0464 = "TestString_0464";
    public static final String string_0465 = "TestString_0465";
    public static final String string_0466 = "TestString_0466";
    public static final String string_0467 = "TestString_0467";
    public static final String string_0468 = "TestString_0468";
    public static final String string_0469 = "TestString_0469";
    public static final String string_0470 = "TestString_0470";
    public static final String string_0471 = "TestString_0471";
    public static final String string_0472 = "TestString_0472";
    public static final String string_0473 = "TestString_0473";
    public static final String string_0474 = "TestString_0474";
    public static final String string_0475 = "TestString_0475";
    public static final String string_0476 = "TestString_0476";
    public static final String string_0477 = "TestString_0477";
    public static final String string_0478 = "TestString_0478";
    public static final String string_0479 = "TestString_0479";
    public static final String string_0480 = "TestString_0480";
    public static final String string_0481 = "TestString_0481";
    public static final String string_0482 = "TestString_0482";
    public static final String string_0483 = "TestString_0483";
    public static final String string_0484 = "TestString_0484";
    public static final String string_0485 = "TestString_0485";
    public static final String string_0486 = "TestString_0486";
    public static final String string_0487 = "TestString_0487";
    public static final String string_0488 = "TestString_0488";
    public static final String string_0489 = "TestString_0489";
    public static final String string_0490 = "TestString_0490";
    public static final String string_0491 = "TestString_0491";
    public static final String string_0492 = "TestString_0492";
    public static final String string_0493 = "TestString_0493";
    public static final String string_0494 = "TestString_0494";
    public static final String string_0495 = "TestString_0495";
    public static final String string_0496 = "TestString_0496";
    public static final String string_0497 = "TestString_0497";
    public static final String string_0498 = "TestString_0498";
    public static final String string_0499 = "TestString_0499";
    public static final String string_0500 = "TestString_0500";
    public static final String string_0501 = "TestString_0501";
    public static final String string_0502 = "TestString_0502";
    public static final String string_0503 = "TestString_0503";
    public static final String string_0504 = "TestString_0504";
    public static final String string_0505 = "TestString_0505";
    public static final String string_0506 = "TestString_0506";
    public static final String string_0507 = "TestString_0507";
    public static final String string_0508 = "TestString_0508";
    public static final String string_0509 = "TestString_0509";
    public static final String string_0510 = "TestString_0510";
    public static final String string_0511 = "TestString_0511";
    public static final String string_0512 = "TestString_0512";
    public static final String string_0513 = "TestString_0513";
    public static final String string_0514 = "TestString_0514";
    public static final String string_0515 = "TestString_0515";
    public static final String string_0516 = "TestString_0516";
    public static final String string_0517 = "TestString_0517";
    public static final String string_0518 = "TestString_0518";
    public static final String string_0519 = "TestString_0519";
    public static final String string_0520 = "TestString_0520";
    public static final String string_0521 = "TestString_0521";
    public static final String string_0522 = "TestString_0522";
    public static final String string_0523 = "TestString_0523";
    public static final String string_0524 = "TestString_0524";
    public static final String string_0525 = "TestString_0525";
    public static final String string_0526 = "TestString_0526";
    public static final String string_0527 = "TestString_0527";
    public static final String string_0528 = "TestString_0528";
    public static final String string_0529 = "TestString_0529";
    public static final String string_0530 = "TestString_0530";
    public static final String string_0531 = "TestString_0531";
    public static final String string_0532 = "TestString_0532";
    public static final String string_0533 = "TestString_0533";
    public static final String string_0534 = "TestString_0534";
    public static final String string_0535 = "TestString_0535";
    public static final String string_0536 = "TestString_0536";
    public static final String string_0537 = "TestString_0537";
    public static final String string_0538 = "TestString_0538";
    public static final String string_0539 = "TestString_0539";
    public static final String string_0540 = "TestString_0540";
    public static final String string_0541 = "TestString_0541";
    public static final String string_0542 = "TestString_0542";
    public static final String string_0543 = "TestString_0543";
    public static final String string_0544 = "TestString_0544";
    public static final String string_0545 = "TestString_0545";
    public static final String string_0546 = "TestString_0546";
    public static final String string_0547 = "TestString_0547";
    public static final String string_0548 = "TestString_0548";
    public static final String string_0549 = "TestString_0549";
    public static final String string_0550 = "TestString_0550";
    public static final String string_0551 = "TestString_0551";
    public static final String string_0552 = "TestString_0552";
    public static final String string_0553 = "TestString_0553";
    public static final String string_0554 = "TestString_0554";
    public static final String string_0555 = "TestString_0555";
    public static final String string_0556 = "TestString_0556";
    public static final String string_0557 = "TestString_0557";
    public static final String string_0558 = "TestString_0558";
    public static final String string_0559 = "TestString_0559";
    public static final String string_0560 = "TestString_0560";
    public static final String string_0561 = "TestString_0561";
    public static final String string_0562 = "TestString_0562";
    public static final String string_0563 = "TestString_0563";
    public static final String string_0564 = "TestString_0564";
    public static final String string_0565 = "TestString_0565";
    public static final String string_0566 = "TestString_0566";
    public static final String string_0567 = "TestString_0567";
    public static final String string_0568 = "TestString_0568";
    public static final String string_0569 = "TestString_0569";
    public static final String string_0570 = "TestString_0570";
    public static final String string_0571 = "TestString_0571";
    public static final String string_0572 = "TestString_0572";
    public static final String string_0573 = "TestString_0573";
    public static final String string_0574 = "TestString_0574";
    public static final String string_0575 = "TestString_0575";
    public static final String string_0576 = "TestString_0576";
    public static final String string_0577 = "TestString_0577";
    public static final String string_0578 = "TestString_0578";
    public static final String string_0579 = "TestString_0579";
    public static final String string_0580 = "TestString_0580";
    public static final String string_0581 = "TestString_0581";
    public static final String string_0582 = "TestString_0582";
    public static final String string_0583 = "TestString_0583";
    public static final String string_0584 = "TestString_0584";
    public static final String string_0585 = "TestString_0585";
    public static final String string_0586 = "TestString_0586";
    public static final String string_0587 = "TestString_0587";
    public static final String string_0588 = "TestString_0588";
    public static final String string_0589 = "TestString_0589";
    public static final String string_0590 = "TestString_0590";
    public static final String string_0591 = "TestString_0591";
    public static final String string_0592 = "TestString_0592";
    public static final String string_0593 = "TestString_0593";
    public static final String string_0594 = "TestString_0594";
    public static final String string_0595 = "TestString_0595";
    public static final String string_0596 = "TestString_0596";
    public static final String string_0597 = "TestString_0597";
    public static final String string_0598 = "TestString_0598";
    public static final String string_0599 = "TestString_0599";
    public static final String string_0600 = "TestString_0600";
    public static final String string_0601 = "TestString_0601";
    public static final String string_0602 = "TestString_0602";
    public static final String string_0603 = "TestString_0603";
    public static final String string_0604 = "TestString_0604";
    public static final String string_0605 = "TestString_0605";
    public static final String string_0606 = "TestString_0606";
    public static final String string_0607 = "TestString_0607";
    public static final String string_0608 = "TestString_0608";
    public static final String string_0609 = "TestString_0609";
    public static final String string_0610 = "TestString_0610";
    public static final String string_0611 = "TestString_0611";
    public static final String string_0612 = "TestString_0612";
    public static final String string_0613 = "TestString_0613";
    public static final String string_0614 = "TestString_0614";
    public static final String string_0615 = "TestString_0615";
    public static final String string_0616 = "TestString_0616";
    public static final String string_0617 = "TestString_0617";
    public static final String string_0618 = "TestString_0618";
    public static final String string_0619 = "TestString_0619";
    public static final String string_0620 = "TestString_0620";
    public static final String string_0621 = "TestString_0621";
    public static final String string_0622 = "TestString_0622";
    public static final String string_0623 = "TestString_0623";
    public static final String string_0624 = "TestString_0624";
    public static final String string_0625 = "TestString_0625";
    public static final String string_0626 = "TestString_0626";
    public static final String string_0627 = "TestString_0627";
    public static final String string_0628 = "TestString_0628";
    public static final String string_0629 = "TestString_0629";
    public static final String string_0630 = "TestString_0630";
    public static final String string_0631 = "TestString_0631";
    public static final String string_0632 = "TestString_0632";
    public static final String string_0633 = "TestString_0633";
    public static final String string_0634 = "TestString_0634";
    public static final String string_0635 = "TestString_0635";
    public static final String string_0636 = "TestString_0636";
    public static final String string_0637 = "TestString_0637";
    public static final String string_0638 = "TestString_0638";
    public static final String string_0639 = "TestString_0639";
    public static final String string_0640 = "TestString_0640";
    public static final String string_0641 = "TestString_0641";
    public static final String string_0642 = "TestString_0642";
    public static final String string_0643 = "TestString_0643";
    public static final String string_0644 = "TestString_0644";
    public static final String string_0645 = "TestString_0645";
    public static final String string_0646 = "TestString_0646";
    public static final String string_0647 = "TestString_0647";
    public static final String string_0648 = "TestString_0648";
    public static final String string_0649 = "TestString_0649";
    public static final String string_0650 = "TestString_0650";
    public static final String string_0651 = "TestString_0651";
    public static final String string_0652 = "TestString_0652";
    public static final String string_0653 = "TestString_0653";
    public static final String string_0654 = "TestString_0654";
    public static final String string_0655 = "TestString_0655";
    public static final String string_0656 = "TestString_0656";
    public static final String string_0657 = "TestString_0657";
    public static final String string_0658 = "TestString_0658";
    public static final String string_0659 = "TestString_0659";
    public static final String string_0660 = "TestString_0660";
    public static final String string_0661 = "TestString_0661";
    public static final String string_0662 = "TestString_0662";
    public static final String string_0663 = "TestString_0663";
    public static final String string_0664 = "TestString_0664";
    public static final String string_0665 = "TestString_0665";
    public static final String string_0666 = "TestString_0666";
    public static final String string_0667 = "TestString_0667";
    public static final String string_0668 = "TestString_0668";
    public static final String string_0669 = "TestString_0669";
    public static final String string_0670 = "TestString_0670";
    public static final String string_0671 = "TestString_0671";
    public static final String string_0672 = "TestString_0672";
    public static final String string_0673 = "TestString_0673";
    public static final String string_0674 = "TestString_0674";
    public static final String string_0675 = "TestString_0675";
    public static final String string_0676 = "TestString_0676";
    public static final String string_0677 = "TestString_0677";
    public static final String string_0678 = "TestString_0678";
    public static final String string_0679 = "TestString_0679";
    public static final String string_0680 = "TestString_0680";
    public static final String string_0681 = "TestString_0681";
    public static final String string_0682 = "TestString_0682";
    public static final String string_0683 = "TestString_0683";
    public static final String string_0684 = "TestString_0684";
    public static final String string_0685 = "TestString_0685";
    public static final String string_0686 = "TestString_0686";
    public static final String string_0687 = "TestString_0687";
    public static final String string_0688 = "TestString_0688";
    public static final String string_0689 = "TestString_0689";
    public static final String string_0690 = "TestString_0690";
    public static final String string_0691 = "TestString_0691";
    public static final String string_0692 = "TestString_0692";
    public static final String string_0693 = "TestString_0693";
    public static final String string_0694 = "TestString_0694";
    public static final String string_0695 = "TestString_0695";
    public static final String string_0696 = "TestString_0696";
    public static final String string_0697 = "TestString_0697";
    public static final String string_0698 = "TestString_0698";
    public static final String string_0699 = "TestString_0699";
    public static final String string_0700 = "TestString_0700";
    public static final String string_0701 = "TestString_0701";
    public static final String string_0702 = "TestString_0702";
    public static final String string_0703 = "TestString_0703";
    public static final String string_0704 = "TestString_0704";
    public static final String string_0705 = "TestString_0705";
    public static final String string_0706 = "TestString_0706";
    public static final String string_0707 = "TestString_0707";
    public static final String string_0708 = "TestString_0708";
    public static final String string_0709 = "TestString_0709";
    public static final String string_0710 = "TestString_0710";
    public static final String string_0711 = "TestString_0711";
    public static final String string_0712 = "TestString_0712";
    public static final String string_0713 = "TestString_0713";
    public static final String string_0714 = "TestString_0714";
    public static final String string_0715 = "TestString_0715";
    public static final String string_0716 = "TestString_0716";
    public static final String string_0717 = "TestString_0717";
    public static final String string_0718 = "TestString_0718";
    public static final String string_0719 = "TestString_0719";
    public static final String string_0720 = "TestString_0720";
    public static final String string_0721 = "TestString_0721";
    public static final String string_0722 = "TestString_0722";
    public static final String string_0723 = "TestString_0723";
    public static final String string_0724 = "TestString_0724";
    public static final String string_0725 = "TestString_0725";
    public static final String string_0726 = "TestString_0726";
    public static final String string_0727 = "TestString_0727";
    public static final String string_0728 = "TestString_0728";
    public static final String string_0729 = "TestString_0729";
    public static final String string_0730 = "TestString_0730";
    public static final String string_0731 = "TestString_0731";
    public static final String string_0732 = "TestString_0732";
    public static final String string_0733 = "TestString_0733";
    public static final String string_0734 = "TestString_0734";
    public static final String string_0735 = "TestString_0735";
    public static final String string_0736 = "TestString_0736";
    public static final String string_0737 = "TestString_0737";
    public static final String string_0738 = "TestString_0738";
    public static final String string_0739 = "TestString_0739";
    public static final String string_0740 = "TestString_0740";
    public static final String string_0741 = "TestString_0741";
    public static final String string_0742 = "TestString_0742";
    public static final String string_0743 = "TestString_0743";
    public static final String string_0744 = "TestString_0744";
    public static final String string_0745 = "TestString_0745";
    public static final String string_0746 = "TestString_0746";
    public static final String string_0747 = "TestString_0747";
    public static final String string_0748 = "TestString_0748";
    public static final String string_0749 = "TestString_0749";
    public static final String string_0750 = "TestString_0750";
    public static final String string_0751 = "TestString_0751";
    public static final String string_0752 = "TestString_0752";
    public static final String string_0753 = "TestString_0753";
    public static final String string_0754 = "TestString_0754";
    public static final String string_0755 = "TestString_0755";
    public static final String string_0756 = "TestString_0756";
    public static final String string_0757 = "TestString_0757";
    public static final String string_0758 = "TestString_0758";
    public static final String string_0759 = "TestString_0759";
    public static final String string_0760 = "TestString_0760";
    public static final String string_0761 = "TestString_0761";
    public static final String string_0762 = "TestString_0762";
    public static final String string_0763 = "TestString_0763";
    public static final String string_0764 = "TestString_0764";
    public static final String string_0765 = "TestString_0765";
    public static final String string_0766 = "TestString_0766";
    public static final String string_0767 = "TestString_0767";
    public static final String string_0768 = "TestString_0768";
    public static final String string_0769 = "TestString_0769";
    public static final String string_0770 = "TestString_0770";
    public static final String string_0771 = "TestString_0771";
    public static final String string_0772 = "TestString_0772";
    public static final String string_0773 = "TestString_0773";
    public static final String string_0774 = "TestString_0774";
    public static final String string_0775 = "TestString_0775";
    public static final String string_0776 = "TestString_0776";
    public static final String string_0777 = "TestString_0777";
    public static final String string_0778 = "TestString_0778";
    public static final String string_0779 = "TestString_0779";
    public static final String string_0780 = "TestString_0780";
    public static final String string_0781 = "TestString_0781";
    public static final String string_0782 = "TestString_0782";
    public static final String string_0783 = "TestString_0783";
    public static final String string_0784 = "TestString_0784";
    public static final String string_0785 = "TestString_0785";
    public static final String string_0786 = "TestString_0786";
    public static final String string_0787 = "TestString_0787";
    public static final String string_0788 = "TestString_0788";
    public static final String string_0789 = "TestString_0789";
    public static final String string_0790 = "TestString_0790";
    public static final String string_0791 = "TestString_0791";
    public static final String string_0792 = "TestString_0792";
    public static final String string_0793 = "TestString_0793";
    public static final String string_0794 = "TestString_0794";
    public static final String string_0795 = "TestString_0795";
    public static final String string_0796 = "TestString_0796";
    public static final String string_0797 = "TestString_0797";
    public static final String string_0798 = "TestString_0798";
    public static final String string_0799 = "TestString_0799";
    public static final String string_0800 = "TestString_0800";
    public static final String string_0801 = "TestString_0801";
    public static final String string_0802 = "TestString_0802";
    public static final String string_0803 = "TestString_0803";
    public static final String string_0804 = "TestString_0804";
    public static final String string_0805 = "TestString_0805";
    public static final String string_0806 = "TestString_0806";
    public static final String string_0807 = "TestString_0807";
    public static final String string_0808 = "TestString_0808";
    public static final String string_0809 = "TestString_0809";
    public static final String string_0810 = "TestString_0810";
    public static final String string_0811 = "TestString_0811";
    public static final String string_0812 = "TestString_0812";
    public static final String string_0813 = "TestString_0813";
    public static final String string_0814 = "TestString_0814";
    public static final String string_0815 = "TestString_0815";
    public static final String string_0816 = "TestString_0816";
    public static final String string_0817 = "TestString_0817";
    public static final String string_0818 = "TestString_0818";
    public static final String string_0819 = "TestString_0819";
    public static final String string_0820 = "TestString_0820";
    public static final String string_0821 = "TestString_0821";
    public static final String string_0822 = "TestString_0822";
    public static final String string_0823 = "TestString_0823";
    public static final String string_0824 = "TestString_0824";
    public static final String string_0825 = "TestString_0825";
    public static final String string_0826 = "TestString_0826";
    public static final String string_0827 = "TestString_0827";
    public static final String string_0828 = "TestString_0828";
    public static final String string_0829 = "TestString_0829";
    public static final String string_0830 = "TestString_0830";
    public static final String string_0831 = "TestString_0831";
    public static final String string_0832 = "TestString_0832";
    public static final String string_0833 = "TestString_0833";
    public static final String string_0834 = "TestString_0834";
    public static final String string_0835 = "TestString_0835";
    public static final String string_0836 = "TestString_0836";
    public static final String string_0837 = "TestString_0837";
    public static final String string_0838 = "TestString_0838";
    public static final String string_0839 = "TestString_0839";
    public static final String string_0840 = "TestString_0840";
    public static final String string_0841 = "TestString_0841";
    public static final String string_0842 = "TestString_0842";
    public static final String string_0843 = "TestString_0843";
    public static final String string_0844 = "TestString_0844";
    public static final String string_0845 = "TestString_0845";
    public static final String string_0846 = "TestString_0846";
    public static final String string_0847 = "TestString_0847";
    public static final String string_0848 = "TestString_0848";
    public static final String string_0849 = "TestString_0849";
    public static final String string_0850 = "TestString_0850";
    public static final String string_0851 = "TestString_0851";
    public static final String string_0852 = "TestString_0852";
    public static final String string_0853 = "TestString_0853";
    public static final String string_0854 = "TestString_0854";
    public static final String string_0855 = "TestString_0855";
    public static final String string_0856 = "TestString_0856";
    public static final String string_0857 = "TestString_0857";
    public static final String string_0858 = "TestString_0858";
    public static final String string_0859 = "TestString_0859";
    public static final String string_0860 = "TestString_0860";
    public static final String string_0861 = "TestString_0861";
    public static final String string_0862 = "TestString_0862";
    public static final String string_0863 = "TestString_0863";
    public static final String string_0864 = "TestString_0864";
    public static final String string_0865 = "TestString_0865";
    public static final String string_0866 = "TestString_0866";
    public static final String string_0867 = "TestString_0867";
    public static final String string_0868 = "TestString_0868";
    public static final String string_0869 = "TestString_0869";
    public static final String string_0870 = "TestString_0870";
    public static final String string_0871 = "TestString_0871";
    public static final String string_0872 = "TestString_0872";
    public static final String string_0873 = "TestString_0873";
    public static final String string_0874 = "TestString_0874";
    public static final String string_0875 = "TestString_0875";
    public static final String string_0876 = "TestString_0876";
    public static final String string_0877 = "TestString_0877";
    public static final String string_0878 = "TestString_0878";
    public static final String string_0879 = "TestString_0879";
    public static final String string_0880 = "TestString_0880";
    public static final String string_0881 = "TestString_0881";
    public static final String string_0882 = "TestString_0882";
    public static final String string_0883 = "TestString_0883";
    public static final String string_0884 = "TestString_0884";
    public static final String string_0885 = "TestString_0885";
    public static final String string_0886 = "TestString_0886";
    public static final String string_0887 = "TestString_0887";
    public static final String string_0888 = "TestString_0888";
    public static final String string_0889 = "TestString_0889";
    public static final String string_0890 = "TestString_0890";
    public static final String string_0891 = "TestString_0891";
    public static final String string_0892 = "TestString_0892";
    public static final String string_0893 = "TestString_0893";
    public static final String string_0894 = "TestString_0894";
    public static final String string_0895 = "TestString_0895";
    public static final String string_0896 = "TestString_0896";
    public static final String string_0897 = "TestString_0897";
    public static final String string_0898 = "TestString_0898";
    public static final String string_0899 = "TestString_0899";
    public static final String string_0900 = "TestString_0900";
    public static final String string_0901 = "TestString_0901";
    public static final String string_0902 = "TestString_0902";
    public static final String string_0903 = "TestString_0903";
    public static final String string_0904 = "TestString_0904";
    public static final String string_0905 = "TestString_0905";
    public static final String string_0906 = "TestString_0906";
    public static final String string_0907 = "TestString_0907";
    public static final String string_0908 = "TestString_0908";
    public static final String string_0909 = "TestString_0909";
    public static final String string_0910 = "TestString_0910";
    public static final String string_0911 = "TestString_0911";
    public static final String string_0912 = "TestString_0912";
    public static final String string_0913 = "TestString_0913";
    public static final String string_0914 = "TestString_0914";
    public static final String string_0915 = "TestString_0915";
    public static final String string_0916 = "TestString_0916";
    public static final String string_0917 = "TestString_0917";
    public static final String string_0918 = "TestString_0918";
    public static final String string_0919 = "TestString_0919";
    public static final String string_0920 = "TestString_0920";
    public static final String string_0921 = "TestString_0921";
    public static final String string_0922 = "TestString_0922";
    public static final String string_0923 = "TestString_0923";
    public static final String string_0924 = "TestString_0924";
    public static final String string_0925 = "TestString_0925";
    public static final String string_0926 = "TestString_0926";
    public static final String string_0927 = "TestString_0927";
    public static final String string_0928 = "TestString_0928";
    public static final String string_0929 = "TestString_0929";
    public static final String string_0930 = "TestString_0930";
    public static final String string_0931 = "TestString_0931";
    public static final String string_0932 = "TestString_0932";
    public static final String string_0933 = "TestString_0933";
    public static final String string_0934 = "TestString_0934";
    public static final String string_0935 = "TestString_0935";
    public static final String string_0936 = "TestString_0936";
    public static final String string_0937 = "TestString_0937";
    public static final String string_0938 = "TestString_0938";
    public static final String string_0939 = "TestString_0939";
    public static final String string_0940 = "TestString_0940";
    public static final String string_0941 = "TestString_0941";
    public static final String string_0942 = "TestString_0942";
    public static final String string_0943 = "TestString_0943";
    public static final String string_0944 = "TestString_0944";
    public static final String string_0945 = "TestString_0945";
    public static final String string_0946 = "TestString_0946";
    public static final String string_0947 = "TestString_0947";
    public static final String string_0948 = "TestString_0948";
    public static final String string_0949 = "TestString_0949";
    public static final String string_0950 = "TestString_0950";
    public static final String string_0951 = "TestString_0951";
    public static final String string_0952 = "TestString_0952";
    public static final String string_0953 = "TestString_0953";
    public static final String string_0954 = "TestString_0954";
    public static final String string_0955 = "TestString_0955";
    public static final String string_0956 = "TestString_0956";
    public static final String string_0957 = "TestString_0957";
    public static final String string_0958 = "TestString_0958";
    public static final String string_0959 = "TestString_0959";
    public static final String string_0960 = "TestString_0960";
    public static final String string_0961 = "TestString_0961";
    public static final String string_0962 = "TestString_0962";
    public static final String string_0963 = "TestString_0963";
    public static final String string_0964 = "TestString_0964";
    public static final String string_0965 = "TestString_0965";
    public static final String string_0966 = "TestString_0966";
    public static final String string_0967 = "TestString_0967";
    public static final String string_0968 = "TestString_0968";
    public static final String string_0969 = "TestString_0969";
    public static final String string_0970 = "TestString_0970";
    public static final String string_0971 = "TestString_0971";
    public static final String string_0972 = "TestString_0972";
    public static final String string_0973 = "TestString_0973";
    public static final String string_0974 = "TestString_0974";
    public static final String string_0975 = "TestString_0975";
    public static final String string_0976 = "TestString_0976";
    public static final String string_0977 = "TestString_0977";
    public static final String string_0978 = "TestString_0978";
    public static final String string_0979 = "TestString_0979";
    public static final String string_0980 = "TestString_0980";
    public static final String string_0981 = "TestString_0981";
    public static final String string_0982 = "TestString_0982";
    public static final String string_0983 = "TestString_0983";
    public static final String string_0984 = "TestString_0984";
    public static final String string_0985 = "TestString_0985";
    public static final String string_0986 = "TestString_0986";
    public static final String string_0987 = "TestString_0987";
    public static final String string_0988 = "TestString_0988";
    public static final String string_0989 = "TestString_0989";
    public static final String string_0990 = "TestString_0990";
    public static final String string_0991 = "TestString_0991";
    public static final String string_0992 = "TestString_0992";
    public static final String string_0993 = "TestString_0993";
    public static final String string_0994 = "TestString_0994";
    public static final String string_0995 = "TestString_0995";
    public static final String string_0996 = "TestString_0996";
    public static final String string_0997 = "TestString_0997";
    public static final String string_0998 = "TestString_0998";
    public static final String string_0999 = "TestString_0999";
    public static final String string_1000 = "TestString_1000";
    public static final String string_1001 = "TestString_1001";
    public static final String string_1002 = "TestString_1002";
    public static final String string_1003 = "TestString_1003";
    public static final String string_1004 = "TestString_1004";
    public static final String string_1005 = "TestString_1005";
    public static final String string_1006 = "TestString_1006";
    public static final String string_1007 = "TestString_1007";
    public static final String string_1008 = "TestString_1008";
    public static final String string_1009 = "TestString_1009";
    public static final String string_1010 = "TestString_1010";
    public static final String string_1011 = "TestString_1011";
    public static final String string_1012 = "TestString_1012";
    public static final String string_1013 = "TestString_1013";
    public static final String string_1014 = "TestString_1014";
    public static final String string_1015 = "TestString_1015";
    public static final String string_1016 = "TestString_1016";
    public static final String string_1017 = "TestString_1017";
    public static final String string_1018 = "TestString_1018";
    public static final String string_1019 = "TestString_1019";
    public static final String string_1020 = "TestString_1020";
    public static final String string_1021 = "TestString_1021";
    public static final String string_1022 = "TestString_1022";
    public static final String string_1023 = "TestString_1023";
    public static final String string_1024 = "TestString_1024";

    public static boolean doThrow = false;

    static long benchmarkConflict(int count) {
      long start = System.nanoTime();
      for (int i = 0; i < count; ++i) {
        $noinline$foo("TestString_0000");
        $noinline$foo("TestString_1024");
      }
      long end = System.nanoTime();
      return (end - start) / (1000 * 1000);
    }

    static long benchmarkNoConflict(int count) {
      long start = System.nanoTime();
      for (int i = 0; i < count; ++i) {
        $noinline$foo("TestString_0001");
        $noinline$foo("TestString_1023");
      }
      long end = System.nanoTime();
      return (end - start) / (1000 * 1000);
    }

    static void $noinline$foo(String s) {
      if (doThrow) { throw new Error(); }
    }
}
