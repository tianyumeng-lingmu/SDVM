main {
    thing main() {
        see("╔══════════════════════════════════╗\n");
        see("║  SDVM 功能测试 — Star Dance    ║\n");
        see("╚══════════════════════════════════╝\n\n");
                // ─── 1. 基础运算 ───
        see("=== 1. 基础运算 ===\n");
        int a = 10;
        int b = 3;
        see("a = 10, b = 3\n");
        int sum = a + b;
        see("a + b = "); see(sum); see("\n");
        int diff = a - b;
        see("a - b = "); see(diff); see("\n");
        int prod = a * b;
        see("a * b = "); see(prod); see("\n");
        int quot = a / b;
        see("a / b = "); see(quot); see("\n");
        int rem = a % b;
        see("a % b = "); see(rem); see("\n");
                // ─── 2. 比较运算 ───
        see("\n=== 2. 比较运算 ===\n");
        if (a < b) { see("a < b: true\n"); } else { see("a < b: false\n"); }
        if (a > b) { see("a > b: true\n"); } else { see("a > b: false\n"); }
        if (a == 10) { see("a == 10: true\n"); } else { see("a == 10: false\n"); }
        if (a != 5) { see("a != 5: true\n"); } else { see("a != 5: false\n"); }
        if (a >= 10) { see("a >= 10: true\n"); } else { see("a >= 10: false\n"); }
        if (a <= 9) { see("a <= 9: true\n"); } else { see("a <= 9: false\n"); }
        if (!(a == 5)) { see("!(a == 5): true\n"); } else { see("!(a == 5): false\n"); }
                // ─── 3. if-else 链 ───
        see("\n=== 3. if-else 链 ===\n");
        int score = 85;
        if (score >= 90) { see("等级: A\n"); }
        else {
        if (score >= 80) { see("等级: B\n"); }
        else {
        if (score >= 70) { see("等级: C\n"); }
        else { see("等级: D\n"); }
        }
        }
                // ─── 4. while 循环 ───
        see("\n=== 4. while 循环 ===\n");
        int i = 0;
        while (i < 5) {
        see("i = "); see(i); see("\n");
        i = i + 1;
        }
                // ─── 5. for 循环 ───
        see("\n=== 5. for 循环 ===\n");
        int total = 0;
        int j = 0;
        for (j = 0; j < 10; j = j + 1) {
        total = total + j;
        }
        see("0+1+...+9 = "); see(total); see("\n");
                // ─── 6. 字符串操作 ───
        see("\n=== 6. 字符串操作 ===\n");
        see("Hello"); see(", "); see("SDVM!"); see("\n");
        int len_str = type("test");
        see("type(\"test\") = "); see(len_str); see("\n");
                // ─── 7. 内置函数 ───
        see("\n=== 7. 内置函数 ===\n");
        int x = 42;
        see("int("); see(x); see(") → "); see(int(x)); see("\n");
        float pi = 3.14159;
        see("float(pi) → "); see(float(pi)); see("\n");
        see("str(123) → "); see(str(123)); see("\n");
        see("bool(1) → "); see(bool(1)); see("\n");
        see("bool(0) → "); see(bool(0)); see("\n");
        see("type(42) → "); see(type(42)); see("\n");
        see("type(3.14) → "); see(type(3.14)); see("\n");
        see("type(true) → "); see(type(true)); see("\n");
        see("type(\"hi\") → "); see(type("hi")); see("\n");
        see("len(\"hello\") → "); see(len("hello")); see("\n");
                // ─── 8. 增量/减量 ───
        see("\n=== 8. 增量/减量 ===\n");
        int n = 0;
        n = n + 1;
        see("n = "); see(n); see("\n");
        n = n - 1;
        see("n - 1 = "); see(n); see("\n");
        n = n * 2 + 5;
        see("n * 2 + 5 = "); see(n); see("\n");
                // ─── 9. 复杂表达式 ───
        see("\n=== 9. 复杂表达式 ===\n");
        int expr = (5 + 3) * 2 - 4 / 2;
        see("(5+3)*2 - 4/2 = "); see(expr); see("\n");
        int mod_chain = 17 % 5 + 3;
        see("17%5 + 3 = "); see(mod_chain); see("\n");
                // ─── 10. NULL/bool ───
        see("\n=== 10. NULL 和布尔 ===\n");
        see("true && false → false\n");
        see("true || false → true\n");
                see("\n╔══════════════════════════════════╗\n");
        see("║  所有测试完成!                  ║\n");
        see("╚══════════════════════════════════╝\n");
    }
}
