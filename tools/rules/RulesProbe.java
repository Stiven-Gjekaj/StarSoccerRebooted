// RulesProbe: runs rules of the original game and prints their results,
// one line for each case. It runs on Android with dalvikvm, with the APK of
// the game on the class path. rulecheck prints the same lines from the
// rules of the remake, and scripts/rules-check.sh compares them.
//
// A float prints as the hex of its bits, so that the compare is exact.

import java.lang.reflect.Field;
import java.lang.reflect.Method;

public class RulesProbe {
    static final String PACKAGE = "com.newstargames.newstarsoccer.";

    static Class<?> type(String name) throws Exception {
        return Class.forName(PACKAGE + name);
    }

    static Method method(String type, String name, Class<?>... args) throws Exception {
        Method m = type(type).getDeclaredMethod(name, args);
        m.setAccessible(true);
        return m;
    }

    static Field field(String type, String name) throws Exception {
        Field f = type(type).getDeclaredField(name);
        f.setAccessible(true);
        return f;
    }

    static String bits(float value) {
        return String.format("%08x", Float.floatToRawIntBits(value));
    }

    // The random numbers of Monkey X (bb_random, bb_various).
    static void random() throws Exception {
        Field seed = field("bb_random", "g_Seed");
        Method rnd = method("bb_random", "g_Rnd");
        Method rnd2 = method("bb_random", "g_Rnd2", float.class, float.class);
        Method rnd3 = method("bb_random", "g_Rnd3", float.class);
        Method rand = method("bb_various", "g_Rand", int.class);
        Method rand2 = method("bb_various", "g_Rand2", int.class, int.class);
        Method randFloat = method("bb_various", "g_RandFloat", float.class, float.class);
        int[] seeds = {1234, 1, 99991, -5, 2147483647};
        int[] ns = {1, 2, 6, 100, 1000};
        int[][] ranges = {{0, 1}, {-3, 3}, {4, 25}, {10, 10}};
        for (int s : seeds) {
            seed.setInt(null, s);
            for (int i = 0; i < 10; i++) System.out.println("rnd " + s + " " + i + " = " + bits((Float) rnd.invoke(null)));
            for (int n : ns) System.out.println("rand " + s + " " + n + " = " + rand.invoke(null, n));
            for (int[] r : ranges) {
                System.out.println("rand2 " + s + " " + r[0] + " " + r[1] + " = " + rand2.invoke(null, r[0], r[1]));
            }
            System.out.println("rnd2 " + s + " = " + bits((Float) rnd2.invoke(null, -1.5f, 2.5f)));
            System.out.println("rnd3 " + s + " = " + bits((Float) rnd3.invoke(null, 7.0f)));
            System.out.println("randfloat " + s + " = " + bits((Float) randFloat.invoke(null, 0.5f, 2.0f)));
            System.out.println("seed " + s + " = " + seed.getInt(null));
        }
    }

    // TRouletteWheel.GetColour.
    static void roulette() throws Exception {
        Method colour = method("c_TRouletteWheel", "m_GetColour", int.class);
        for (int n = 0; n <= 37; n++) System.out.println("roulette " + n + " = " + colour.invoke(null, n));
    }

    // THorse.GetPrice2, GetCostToTrain, and GetOwnedHorsePrize.
    static void horses() throws Exception {
        java.lang.reflect.Constructor<?> make = type("c_THorse").getDeclaredConstructor();
        make.setAccessible(true);
        Object horse = make.newInstance();
        Field strength = field("c_THorse", "m_strength");
        Method price = method("c_THorse", "p_GetPrice2");
        Method train = method("c_THorse", "p_GetCostToTrain");
        Method prize = method("c_THorse", "m_GetOwnedHorsePrize", int.class);
        for (int s = 0; s <= 6; s++) {
            strength.setInt(horse, s);
            System.out.println("horseprice " + s + " = " + price.invoke(horse));
            System.out.println("horsetrain " + s + " = " + train.invoke(horse));
        }
        for (int p = 0; p <= 5; p++) System.out.println("horseprize " + p + " = " + prize.invoke(null, p));
    }

    // TPlayer.GetStarLevel of the star rating.
    static void starLevel() throws Exception {
        java.lang.reflect.Constructor<?> make = type("c_TPlayer").getDeclaredConstructor();
        make.setAccessible(true);
        Object player = make.newInstance();
        Field rating = field("c_TPlayer", "m_starrating");
        Method level = method("c_TPlayer", "p_GetStarLevel", int.class);
        int[] ratings = {0, 1, 99, 100, 250, 999, 1000, 4500};
        for (int r : ratings) {
            rating.setInt(player, r);
            System.out.println("starlevel " + r + " = " + level.invoke(player, 0));
        }
    }

    public static void main(String[] args) throws Exception {
        random();
        roulette();
        horses();
        starLevel();
    }
}
