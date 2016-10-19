import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodHandles.Lookup;
import java.lang.invoke.MethodType;
import java.lang.invoke.WrongMethodTypeException;

public class Main {

    public static class ValueHolder {
        public boolean m_z = false;
        public byte m_b = 0;
        public char m_c = 'a';
        public short m_s = 0;
        public int m_i = 0;
        public float m_f = 0.0f;
        public double m_d = 0.0;
        public long m_j = 0;
        public String m_l = "a";

        public static boolean s_z;
        public static byte s_b;
        public static char s_c;
        public static short s_s;
        public static int s_i;
        public static float s_f;
        public static double s_d;
        public static long s_j;
        public static String s_l;

        public final int m_fi = 0xa5a5a5a5;
        public static final int s_fi = 0x5a5a5a5a;
    }

    public static class InvokeExactTester {
        private enum PrimitiveType {
            Boolean,
            Byte,
            Char,
            Short,
            Int,
            Long,
            Float,
            Double,
            String,
        }

        private enum AccessorType {
            IPUT,
            SPUT,
            IGET,
            SGET,
        }

        private static void assertActualAndExpectedMatch(boolean actual, boolean expected)
                throws AssertionError {
            if (actual != expected) {
                throw new AssertionError("Actual != Expected (" + actual + " != " + expected + ")");
            }
        }

        private static void assertTrue(boolean value) throws AssertionError {
            if (!value) {
                throw new AssertionError("Value is not true");
            }
        }

        static void setByte(MethodHandle m, ValueHolder v, byte value, boolean expectFailure)
                throws Throwable {
            boolean exceptionThrown = false;
            try {
                if (v == null) {
                    m.invokeExact(value);
                }
                else {
                    m.invokeExact(v, value);
                }
            }
            catch (WrongMethodTypeException e) {
                exceptionThrown = true;
            }
            assertActualAndExpectedMatch(exceptionThrown, expectFailure);
        }

        static void setByte(MethodHandle m, byte value, boolean expectFailure) throws Throwable {
            setByte(m, null, value, expectFailure);
        }

        static void getByte(MethodHandle m, ValueHolder v, byte value, boolean expectFailure)
                throws Throwable {
            boolean exceptionThrown = false;
            try {
                final byte got;
                if (v == null) {
                    got = (byte)m.invokeExact();
                } else {
                    got = (byte)m.invokeExact(v);
                }
                assertTrue(got == value);
            }
            catch (WrongMethodTypeException e) {
                exceptionThrown = true;
            }
            assertActualAndExpectedMatch(exceptionThrown, expectFailure);
        }

        static void getByte(MethodHandle m, byte value, boolean expectFailure) throws Throwable {
            getByte(m, null, value, expectFailure);
        }

        static void setChar(MethodHandle m, ValueHolder v, char value, boolean expectFailure)
                throws Throwable {
            boolean exceptionThrown = false;
            try {
                if (v == null) {
                    m.invokeExact(value);
                }
                else {
                    m.invokeExact(v, value);
                }
            }
            catch (WrongMethodTypeException e) {
                exceptionThrown = true;
            }
            assertActualAndExpectedMatch(exceptionThrown, expectFailure);
        }

        static void setChar(MethodHandle m, char value, boolean expectFailure) throws Throwable {
            setChar(m, null, value, expectFailure);
        }

        static void getChar(MethodHandle m, ValueHolder v, char value, boolean expectFailure)
                throws Throwable {
            boolean exceptionThrown = false;
            try {
                final char got;
                if (v == null) {
                    got = (char)m.invokeExact();
                } else {
                    got = (char)m.invokeExact(v);
                }
                assertTrue(got == value);
            }
            catch (WrongMethodTypeException e) {
                exceptionThrown = true;
            }
            assertActualAndExpectedMatch(exceptionThrown, expectFailure);
        }

        static void getChar(MethodHandle m, char value, boolean expectFailure) throws Throwable {
            getChar(m, null, value, expectFailure);
        }

        static void setShort(MethodHandle m, ValueHolder v, short value, boolean expectFailure)
                throws Throwable {
            boolean exceptionThrown = false;
            try {
                if (v == null) {
                    m.invokeExact(value);
                }
                else {
                    m.invokeExact(v, value);
                }
            }
            catch (WrongMethodTypeException e) {
                exceptionThrown = true;
            }
            assertActualAndExpectedMatch(exceptionThrown, expectFailure);
        }

        static void setShort(MethodHandle m, short value, boolean expectFailure) throws Throwable {
            setShort(m, null, value, expectFailure);
        }

        static void getShort(MethodHandle m, ValueHolder v, short value, boolean expectFailure)
                throws Throwable {
            boolean exceptionThrown = false;
            try {
                final short got = (v == null) ? (short)m.invokeExact() : (short)m.invokeExact(v);
                assertTrue(got == value);
            }
            catch (WrongMethodTypeException e) {
                exceptionThrown = true;
            }
            assertActualAndExpectedMatch(exceptionThrown, expectFailure);
        }

        static void getShort(MethodHandle m, short value, boolean expectFailure) throws Throwable {
            getShort(m, null, value, expectFailure);
        }

        static void setInt(MethodHandle m, ValueHolder v, int value, boolean expectFailure)
                throws Throwable {
            boolean exceptionThrown = false;
            try {
                if (v == null) {
                    m.invokeExact(value);
                }
                else {
                    m.invokeExact(v, value);
                }
            }
            catch (WrongMethodTypeException e) {
                exceptionThrown = true;
            }
            assertActualAndExpectedMatch(exceptionThrown, expectFailure);
        }

        static void setInt(MethodHandle m, int value, boolean expectFailure) throws Throwable {
            setInt(m, null, value, expectFailure);
        }

        static void getInt(MethodHandle m, ValueHolder v, int value, boolean expectFailure)
                throws Throwable {
            boolean exceptionThrown = false;
            try {
                final int got = (v == null) ? (int)m.invokeExact() : (int)m.invokeExact(v);
                assertTrue(got == value);
            }
            catch (WrongMethodTypeException e) {
                exceptionThrown = true;
            }
            assertActualAndExpectedMatch(exceptionThrown, expectFailure);
        }

        static void getInt(MethodHandle m, int value, boolean expectFailure) throws Throwable {
            getInt(m, null, value, expectFailure);
        }

        static void setLong(MethodHandle m, ValueHolder v, long value, boolean expectFailure)
                throws Throwable {
            boolean exceptionThrown = false;
            try {
                if (v == null) {
                    m.invokeExact(value);
                }
                else {
                    m.invokeExact(v, value);
                }
            }
            catch (WrongMethodTypeException e) {
                exceptionThrown = true;
            }
            assertActualAndExpectedMatch(exceptionThrown, expectFailure);
        }

        static void setLong(MethodHandle m, long value, boolean expectFailure) throws Throwable {
            setLong(m, null, value, expectFailure);
        }

        static void getLong(MethodHandle m, ValueHolder v, long value, boolean expectFailure)
                throws Throwable {
            boolean exceptionThrown = false;
            try {
                final long got = (v == null) ? (long)m.invokeExact() : (long)m.invokeExact(v);
                assertTrue(got == value);
            }
            catch (WrongMethodTypeException e) {
                exceptionThrown = true;
            }
            assertActualAndExpectedMatch(exceptionThrown, expectFailure);
        }

        static void getLong(MethodHandle m, long value, boolean expectFailure) throws Throwable {
            getLong(m, null, value, expectFailure);
        }

        static void setFloat(MethodHandle m, ValueHolder v, float value, boolean expectFailure)
                throws Throwable {
            boolean exceptionThrown = false;
            try {
                if (v == null) {
                    m.invokeExact(value);
                }
                else {
                    m.invokeExact(v, value);
                }
            }
            catch (WrongMethodTypeException e) {
                exceptionThrown = true;
            }
            assertActualAndExpectedMatch(exceptionThrown, expectFailure);
        }

        static void setFloat(MethodHandle m, float value, boolean expectFailure) throws Throwable {
            setFloat(m, null, value, expectFailure);
        }

        static void getFloat(MethodHandle m, ValueHolder v, float value, boolean expectFailure)
                throws Throwable {
            boolean exceptionThrown = false;
            try {
                final float got = (v == null) ? (float)m.invokeExact() : (float)m.invokeExact(v);
                assertTrue(got == value);
            }
            catch (WrongMethodTypeException e) {
                exceptionThrown = true;
            }
            assertActualAndExpectedMatch(exceptionThrown, expectFailure);
        }

        static void getFloat(MethodHandle m, float value, boolean expectFailure) throws Throwable {
            getFloat(m, null, value, expectFailure);
        }

        static void setDouble(MethodHandle m, ValueHolder v, double value, boolean expectFailure)
                throws Throwable {
            boolean exceptionThrown = false;
            try {
                if (v == null) {
                    m.invokeExact(value);
                }
                else {
                    m.invokeExact(v, value);
                }
            }
            catch (WrongMethodTypeException e) {
                exceptionThrown = true;
            }
            assertActualAndExpectedMatch(exceptionThrown, expectFailure);
        }

        static void setDouble(MethodHandle m, double value, boolean expectFailure)
                throws Throwable {
            setDouble(m, null, value, expectFailure);
        }

        static void getDouble(MethodHandle m, ValueHolder v, double value, boolean expectFailure)
                throws Throwable {
            boolean exceptionThrown = false;
            try {
                final double got = (v == null) ? (double)m.invokeExact() : (double)m.invokeExact(v);
                assertTrue(got == value);
            }
            catch (WrongMethodTypeException e) {
                exceptionThrown = true;
            }
            assertActualAndExpectedMatch(exceptionThrown, expectFailure);
        }

        static void getDouble(MethodHandle m, double value, boolean expectFailure)
                throws Throwable {
            getDouble(m, null, value, expectFailure);
        }

        static void setString(MethodHandle m, ValueHolder v, String value, boolean expectFailure)
                throws Throwable {
            boolean exceptionThrown = false;
            try {
                if (v == null) {
                    m.invokeExact(value);
                }
                else {
                    m.invokeExact(v, value);
                }
            }
            catch (WrongMethodTypeException e) {
                exceptionThrown = true;
            }
            assertActualAndExpectedMatch(exceptionThrown, expectFailure);
        }

        static void setString(MethodHandle m, String value, boolean expectFailure)
                throws Throwable {
            setString(m, null, value, expectFailure);
        }

        static void getString(MethodHandle m, ValueHolder v, String value, boolean expectFailure)
                throws Throwable {
            boolean exceptionThrown = false;
            try {
                final String got = (v == null) ? (String)m.invokeExact() : (String)m.invokeExact(v);
                assertTrue(got.equals(value));
            }
            catch (WrongMethodTypeException e) {
                exceptionThrown = true;
            }
            assertActualAndExpectedMatch(exceptionThrown, expectFailure);
        }

        static void getString(MethodHandle m, String value, boolean expectFailure)
                throws Throwable {
            getString(m, null, value, expectFailure);
        }

        static void setBoolean(MethodHandle m, ValueHolder v, boolean value, boolean expectFailure)
                throws Throwable {
            boolean exceptionThrown = false;
            try {
                if (v == null) {
                    m.invokeExact(value);
                }
                else {
                    m.invokeExact(v, value);
                }
            }
            catch (WrongMethodTypeException e) {
                exceptionThrown = true;
            }
            assertActualAndExpectedMatch(exceptionThrown, expectFailure);
        }

        static void setBoolean(MethodHandle m, boolean value, boolean expectFailure)
                throws Throwable {
            setBoolean(m, null, value, expectFailure);
        }

        static void getBoolean(MethodHandle m, ValueHolder v, boolean value, boolean expectFailure)
                throws Throwable {
            boolean exceptionThrown = false;
            try {
                final boolean got =
                        (v == null) ? (boolean)m.invokeExact() : (boolean)m.invokeExact(v);
                assertTrue(got == value);
            }
            catch (WrongMethodTypeException e) {
                exceptionThrown = true;
            }
            assertActualAndExpectedMatch(exceptionThrown, expectFailure);
        }

        static void getBoolean(MethodHandle m, boolean value, boolean expectFailure)
                throws Throwable {
            getBoolean(m, null, value, expectFailure);
        }

        static boolean resultFor(PrimitiveType actualType, PrimitiveType expectedType,
                                 AccessorType actualAccessor,
                                 AccessorType expectedAccessor) {
            return (actualType != expectedType) || (actualAccessor != expectedAccessor);
        }

        static void tryAccessor(MethodHandle methodHandle,
                                ValueHolder valueHolder,
                                PrimitiveType primitive,
                                Object value,
                                AccessorType accessor) throws Throwable {
            boolean booleanValue =
                    value instanceof Boolean ? ((Boolean)value).booleanValue() : false;
            setBoolean(methodHandle, valueHolder, booleanValue,
                       resultFor(primitive, PrimitiveType.Boolean, accessor, AccessorType.IPUT));
            setBoolean(methodHandle, booleanValue,
                       resultFor(primitive, PrimitiveType.Boolean, accessor, AccessorType.SPUT));
            getBoolean(methodHandle, valueHolder, booleanValue,
                       resultFor(primitive, PrimitiveType.Boolean, accessor, AccessorType.IGET));
            getBoolean(methodHandle, booleanValue,
                       resultFor(primitive, PrimitiveType.Boolean, accessor, AccessorType.SGET));

            byte byteValue = value instanceof Byte ? ((Byte)value).byteValue() : (byte)0;
            setByte(methodHandle, valueHolder, byteValue,
                    resultFor(primitive, PrimitiveType.Byte, accessor, AccessorType.IPUT));
            setByte(methodHandle, byteValue,
                    resultFor(primitive, PrimitiveType.Byte, accessor, AccessorType.SPUT));
            getByte(methodHandle, valueHolder, byteValue,
                    resultFor(primitive, PrimitiveType.Byte, accessor, AccessorType.IGET));
            getByte(methodHandle, byteValue,
                    resultFor(primitive, PrimitiveType.Byte, accessor, AccessorType.SGET));

            char charValue = value instanceof Character ? ((Character)value).charValue() : 'z';
            setChar(methodHandle, valueHolder, charValue,
                    resultFor(primitive, PrimitiveType.Char, accessor, AccessorType.IPUT));
            setChar(methodHandle, charValue,
                    resultFor(primitive, PrimitiveType.Char, accessor, AccessorType.SPUT));
            getChar(methodHandle, valueHolder, charValue,
                    resultFor(primitive, PrimitiveType.Char, accessor, AccessorType.IGET));
            getChar(methodHandle, charValue,
                    resultFor(primitive, PrimitiveType.Char, accessor, AccessorType.SGET));

            short shortValue = value instanceof Short ? ((Short)value).shortValue() : (short)0;
            setShort(methodHandle, valueHolder, shortValue,
                     resultFor(primitive, PrimitiveType.Short, accessor, AccessorType.IPUT));
            setShort(methodHandle, shortValue,
                    resultFor(primitive, PrimitiveType.Short, accessor, AccessorType.SPUT));
            getShort(methodHandle, valueHolder, shortValue,
                     resultFor(primitive, PrimitiveType.Short, accessor, AccessorType.IGET));
            getShort(methodHandle, shortValue,
                    resultFor(primitive, PrimitiveType.Short, accessor, AccessorType.SGET));

            int intValue = value instanceof Integer ? ((Integer)value).intValue() : -1;
            setInt(methodHandle, valueHolder, intValue,
                   resultFor(primitive, PrimitiveType.Int, accessor, AccessorType.IPUT));
            setInt(methodHandle, intValue,
                   resultFor(primitive, PrimitiveType.Int, accessor, AccessorType.SPUT));
            getInt(methodHandle, valueHolder, intValue,
                   resultFor(primitive, PrimitiveType.Int, accessor, AccessorType.IGET));
            getInt(methodHandle, intValue,
                   resultFor(primitive, PrimitiveType.Int, accessor, AccessorType.SGET));

            long longValue = value instanceof Long ? ((Long)value).longValue() : (long)-1;
            setLong(methodHandle, valueHolder, longValue,
                    resultFor(primitive, PrimitiveType.Long, accessor, AccessorType.IPUT));
            setLong(methodHandle, longValue,
                    resultFor(primitive, PrimitiveType.Long, accessor, AccessorType.SPUT));
            getLong(methodHandle, valueHolder, longValue,
                    resultFor(primitive, PrimitiveType.Long, accessor, AccessorType.IGET));
            getLong(methodHandle, longValue,
                    resultFor(primitive, PrimitiveType.Long, accessor, AccessorType.SGET));

            float floatValue = value instanceof Float ? ((Float)value).floatValue() : -1.0f;
            setFloat(methodHandle, valueHolder, floatValue,
                    resultFor(primitive, PrimitiveType.Float, accessor, AccessorType.IPUT));
            setFloat(methodHandle, floatValue,
                    resultFor(primitive, PrimitiveType.Float, accessor, AccessorType.SPUT));
            getFloat(methodHandle, valueHolder, floatValue,
                    resultFor(primitive, PrimitiveType.Float, accessor, AccessorType.IGET));
            getFloat(methodHandle, floatValue,
                     resultFor(primitive, PrimitiveType.Float, accessor, AccessorType.SGET));

            double doubleValue = value instanceof Double ? ((Double)value).doubleValue() : -1.0;
            setDouble(methodHandle, valueHolder, doubleValue,
                      resultFor(primitive, PrimitiveType.Double, accessor, AccessorType.IPUT));
            setDouble(methodHandle, doubleValue,
                      resultFor(primitive, PrimitiveType.Double, accessor, AccessorType.SPUT));
            getDouble(methodHandle, valueHolder, doubleValue,
                      resultFor(primitive, PrimitiveType.Double, accessor, AccessorType.IGET));
            getDouble(methodHandle, doubleValue,
                      resultFor(primitive, PrimitiveType.Double, accessor, AccessorType.SGET));

            String stringValue = value instanceof String ? ((String) value) : "No Spock, no";
            setString(methodHandle, valueHolder, stringValue,
                      resultFor(primitive, PrimitiveType.String, accessor, AccessorType.IPUT));
            setString(methodHandle, stringValue,
                      resultFor(primitive, PrimitiveType.String, accessor, AccessorType.SPUT));
            getString(methodHandle, valueHolder, stringValue,
                      resultFor(primitive, PrimitiveType.String, accessor, AccessorType.IGET));
            getString(methodHandle, stringValue,
                      resultFor(primitive, PrimitiveType.String, accessor, AccessorType.SGET));
        }

        public static void main() throws Throwable {
            ValueHolder valueHolder = new ValueHolder();
            MethodHandles.Lookup lookup = MethodHandles.lookup();

            boolean [] booleans = { false, true, false };
            for (boolean b : booleans) {
                Boolean boxed = new Boolean(b);
                tryAccessor(lookup.findSetter(ValueHolder.class, "m_z", boolean.class),
                            valueHolder, PrimitiveType.Boolean, boxed, AccessorType.IPUT);
                tryAccessor(lookup.findGetter(ValueHolder.class, "m_z", boolean.class),
                            valueHolder, PrimitiveType.Boolean, boxed, AccessorType.IGET);
                assertTrue(valueHolder.m_z == b);
                tryAccessor(lookup.findStaticSetter(ValueHolder.class, "s_z", boolean.class),
                            valueHolder, PrimitiveType.Boolean, boxed, AccessorType.SPUT);
                tryAccessor(lookup.findStaticGetter(ValueHolder.class, "s_z", boolean.class),
                            valueHolder, PrimitiveType.Boolean, boxed, AccessorType.SGET);
                assertTrue(valueHolder.s_z == b);
            }

            byte [] bytes = { (byte)0x73, (byte)0xfe };
            for (byte b : bytes) {
                Byte boxed = new Byte(b);
                tryAccessor(lookup.findSetter(ValueHolder.class, "m_b", byte.class),
                            valueHolder, PrimitiveType.Byte, boxed, AccessorType.IPUT);
                tryAccessor(lookup.findGetter(ValueHolder.class, "m_b", byte.class),
                            valueHolder, PrimitiveType.Byte, boxed, AccessorType.IGET);
                assertTrue(valueHolder.m_b == b);
                tryAccessor(lookup.findStaticSetter(ValueHolder.class, "s_b", byte.class),
                            valueHolder, PrimitiveType.Byte, boxed, AccessorType.SPUT);
                tryAccessor(lookup.findStaticGetter(ValueHolder.class, "s_b", byte.class),
                            valueHolder, PrimitiveType.Byte, boxed, AccessorType.SGET);
                assertTrue(valueHolder.s_b == b);
            }

            char [] chars = { 'a', 'b', 'c' };
            for (char c : chars) {
                Character boxed = new Character(c);
                tryAccessor(lookup.findSetter(ValueHolder.class, "m_c", char.class),
                            valueHolder, PrimitiveType.Char, boxed, AccessorType.IPUT);
                tryAccessor(lookup.findGetter(ValueHolder.class, "m_c", char.class),
                            valueHolder, PrimitiveType.Char, boxed, AccessorType.IGET);
                assertTrue(valueHolder.m_c == c);
                tryAccessor(lookup.findStaticSetter(ValueHolder.class, "s_c", char.class),
                            valueHolder, PrimitiveType.Char, boxed, AccessorType.SPUT);
                tryAccessor(lookup.findStaticGetter(ValueHolder.class, "s_c", char.class),
                            valueHolder, PrimitiveType.Char, boxed, AccessorType.SGET);
                assertTrue(valueHolder.s_c == c);
            }

            short [] shorts = { (short)0x1234, (short)0x4321 };
            for (short s : shorts) {
                Short boxed = new Short(s);
                tryAccessor(lookup.findSetter(ValueHolder.class, "m_s", short.class),
                            valueHolder, PrimitiveType.Short, boxed, AccessorType.IPUT);
                tryAccessor(lookup.findGetter(ValueHolder.class, "m_s", short.class),
                            valueHolder, PrimitiveType.Short, boxed, AccessorType.IGET);
                assertTrue(valueHolder.m_s == s);
                tryAccessor(lookup.findStaticSetter(ValueHolder.class, "s_s", short.class),
                            valueHolder, PrimitiveType.Short, boxed, AccessorType.SPUT);
                tryAccessor(lookup.findStaticGetter(ValueHolder.class, "s_s", short.class),
                            valueHolder, PrimitiveType.Short, boxed, AccessorType.SGET);
                assertTrue(valueHolder.s_s == s);
            }

            int [] ints = { -100000000, 10000000 };
            for (int i : ints) {
                Integer boxed = new Integer(i);
                tryAccessor(lookup.findSetter(ValueHolder.class, "m_i", int.class),
                            valueHolder, PrimitiveType.Int, boxed, AccessorType.IPUT);
                tryAccessor(lookup.findGetter(ValueHolder.class, "m_i", int.class),
                            valueHolder, PrimitiveType.Int, boxed, AccessorType.IGET);
                assertTrue(valueHolder.m_i == i);
                tryAccessor(lookup.findStaticSetter(ValueHolder.class, "s_i", int.class),
                            valueHolder, PrimitiveType.Int, boxed, AccessorType.SPUT);
                tryAccessor(lookup.findStaticGetter(ValueHolder.class, "s_i", int.class),
                            valueHolder, PrimitiveType.Int, boxed, AccessorType.SGET);
                assertTrue(valueHolder.s_i == i);
            }

            float [] floats = { 0.99f, -1.23e-17f };
            for (float f : floats) {
                Float boxed = new Float(f);
                tryAccessor(lookup.findSetter(ValueHolder.class, "m_f", float.class),
                            valueHolder, PrimitiveType.Float, boxed, AccessorType.IPUT);
                tryAccessor(lookup.findGetter(ValueHolder.class, "m_f", float.class),
                            valueHolder, PrimitiveType.Float, boxed, AccessorType.IGET);
                assertTrue(valueHolder.m_f == f);
                tryAccessor(lookup.findStaticSetter(ValueHolder.class, "s_f", float.class),
                            valueHolder, PrimitiveType.Float, boxed, AccessorType.SPUT);
                tryAccessor(lookup.findStaticGetter(ValueHolder.class, "s_f", float.class),
                            valueHolder, PrimitiveType.Float, boxed, AccessorType.SGET);
                assertTrue(valueHolder.s_f == f);
            }

            double [] doubles = { 0.44444444444e37, -0.555555555e-37 };
            for (double d : doubles) {
                Double boxed = new Double(d);
                tryAccessor(lookup.findSetter(ValueHolder.class, "m_d", double.class),
                            valueHolder, PrimitiveType.Double, boxed, AccessorType.IPUT);
                tryAccessor(lookup.findGetter(ValueHolder.class, "m_d", double.class),
                            valueHolder, PrimitiveType.Double, boxed, AccessorType.IGET);
                assertTrue(valueHolder.m_d == d);
                tryAccessor(lookup.findStaticSetter(ValueHolder.class, "s_d", double.class),
                            valueHolder, PrimitiveType.Double, boxed, AccessorType.SPUT);
                tryAccessor(lookup.findStaticGetter(ValueHolder.class, "s_d", double.class),
                            valueHolder, PrimitiveType.Double, boxed, AccessorType.SGET);
                assertTrue(valueHolder.s_d == d);
            }

            long [] longs = { 0x0123456789abcdefl, 0xfedcba9876543210l };
            for (long j : longs) {
                Long boxed = new Long(j);
                tryAccessor(lookup.findSetter(ValueHolder.class, "m_j", long.class),
                            valueHolder, PrimitiveType.Long, boxed, AccessorType.IPUT);
                tryAccessor(lookup.findGetter(ValueHolder.class, "m_j", long.class),
                            valueHolder, PrimitiveType.Long, boxed, AccessorType.IGET);
                assertTrue(valueHolder.m_j == j);
                tryAccessor(lookup.findStaticSetter(ValueHolder.class, "s_j", long.class),
                            valueHolder, PrimitiveType.Long, boxed, AccessorType.SPUT);
                tryAccessor(lookup.findStaticGetter(ValueHolder.class, "s_j", long.class),
                            valueHolder, PrimitiveType.Long, boxed, AccessorType.SGET);
                assertTrue(valueHolder.s_j == j);
            }

            String [] strings = { "octopus", "crab" };
            for (String s : strings) {
                tryAccessor(lookup.findSetter(ValueHolder.class, "m_l", String.class),
                            valueHolder, PrimitiveType.String, s, AccessorType.IPUT);
                tryAccessor(lookup.findGetter(ValueHolder.class, "m_l", String.class),
                            valueHolder, PrimitiveType.String, s, AccessorType.IGET);
                assertTrue(s.equals(valueHolder.m_l));
                tryAccessor(lookup.findStaticSetter(ValueHolder.class, "s_l", String.class),
                            valueHolder, PrimitiveType.String, s, AccessorType.SPUT);
                tryAccessor(lookup.findStaticGetter(ValueHolder.class, "s_l", String.class),
                            valueHolder, PrimitiveType.String, s, AccessorType.SGET);
                assertTrue(s.equals(valueHolder.s_l));
            }

            System.out.println("Passed InvokeExact tests for accessors.");
        }
    }

    public static class FindAccessorTester {
        public static void main() throws Throwable {
            ValueHolder valueHolder = new ValueHolder();
            MethodHandles.Lookup lookup = MethodHandles.lookup();

            lookup.findStaticGetter(ValueHolder.class, "s_fi", int.class);
            try {
                lookup.findStaticGetter(ValueHolder.class, "s_fi", byte.class);
                unreachable();
            } catch (NoSuchFieldException e) {}
            try {
                lookup.findGetter(ValueHolder.class, "s_fi", byte.class);
                unreachable();
            } catch (NoSuchFieldException e) {}
            try {
                lookup.findStaticSetter(ValueHolder.class, "s_fi", int.class);
                unreachable();
            } catch (IllegalAccessException e) {}

            lookup.findGetter(ValueHolder.class, "m_fi", int.class);
            try {
                lookup.findGetter(ValueHolder.class, "m_fi", byte.class);
                unreachable();
            } catch (NoSuchFieldException e) {}
            try {
                lookup.findStaticGetter(ValueHolder.class, "m_fi", byte.class);
                unreachable();
            } catch (NoSuchFieldException e) {}
            try {
                lookup.findSetter(ValueHolder.class, "m_fi", int.class);
                unreachable();
            } catch (IllegalAccessException e) {}
        }

        public static void unreachable() throws Throwable{
            throw new Error("unreachable");
        }
    }

    public static void main(String[] args) throws Throwable {
        FindAccessorTester.main();
        InvokeExactTester.main();
    }
}
