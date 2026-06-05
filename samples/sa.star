// 星舞计算器（自动测试版）
// 使用预设的测试用例，不依赖用户输入

main {
    thing main() {
        see("╔══════════════════════════╗\n");
        see("║    ✦ 星舞计算器 ✦       ║\n");
        see("╚══════════════════════════╝\n\n");

        int all_pass = 1;

        // 测试用例1: 10 + 5 = 15
        see("10 + 5 = ");
        float result = 10 + 5;
        see(result, "\n");
        if (result == 15) {
            see("  [PASS]\n");
        } else {
            see("  [FAIL] 预期 15\n");
            all_pass = 0;
        }

        // 测试用例2: 10 - 3 = 7
        see("10 - 3 = ");
        result = 10 - 3;
        see(result, "\n");
        if (result == 7) {
            see("  [PASS]\n");
        } else {
            see("  [FAIL] 预期 7\n");
            all_pass = 0;
        }

        // 测试用例3: 4 * 5 = 20
        see("4 * 5 = ");
        result = 4 * 5;
        see(result, "\n");
        if (result == 20) {
            see("  [PASS]\n");
        } else {
            see("  [FAIL] 预期 20\n");
            all_pass = 0;
        }

        // 测试用例4: 20 / 4 = 5
        see("20 / 4 = ");
        result = 20 / 4;
        see(result, "\n");
        if (result == 5) {
            see("  [PASS]\n");
        } else {
            see("  [FAIL] 预期 5\n");
            all_pass = 0;
        }

        // 测试用例5: 17 %% 5 = 2
        see("17 % 5 = ");
        result = 17 % 5;
        see(result, "\n");
        if (result == 2) {
            see("  [PASS]\n");
        } else {
            see("  [FAIL] 预期 2\n");
            all_pass = 0;
        }

        // 测试用例6: 2 ^ 10 = 1024
        see("2 ^ 10 = ");
        result = 1;
        int j = 0;
        while (j < 10) {
            result = result * 2;
            j = j + 1;
        }
        see(result, "\n");
        if (result == 1024) {
            see("  [PASS]\n");
        } else {
            see("  [FAIL] 预期 1024\n");
            all_pass = 0;
        }

        if (all_pass) {
            see("\n[PASS] 所有计算器测试通过\n");
        } else {
            see("\n[FAIL] 部分测试未通过\n");
        }
    }
}
