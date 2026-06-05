// 测试 rand 包 + system 随机相关函数

start {
    use rand;
    use system;
}

main {
    thing main() {
        see("=== 随机数包测试 ===\n");

        // 测试 rand 包（直接调用，不用前缀）
        see("--- rand 包 ---\n");
        
        // 种子
        seed();
        see("[OK] seed()\n");

        // next()
        int r1 = next();
        int r2 = next();
        see("next(): ", r1, ", ", r2, "\n");
        if (r1 >= 0 && r1 <= 32767 && r2 >= 0 && r2 <= 32767) {
            see("[PASS] next() 在范围内\n");
        } else {
            see("[FAIL] next() 超出范围\n");
        }

        // 随机性验证
        if (r1 != r2) {
            see("[PASS] next() 有随机性\n");
        } else {
            see("[FAIL] next() 无随机性\n");
        }

        // range()
        int r3 = range(1, 6);
        if (r3 >= 1 && r3 <= 6) {
            see("[PASS] range(1,6) = ", r3, "\n");
        } else {
            see("[FAIL] range(1,6) = ", r3, "\n");
        }

        // unit() — 0.0..1.0
        float u1 = unit();
        if (u1 >= 0.0 && u1 <= 1.0) {
            see("[PASS] unit() = ", u1, "\n");
        } else {
            see("[FAIL] unit() = ", u1, "\n");
        }

        // 固定种子测试可重复性
        seed_with(42);
        int s1 = next();
        seed_with(42);
        int s2 = next();
        if (s1 == s2) {
            see("[PASS] seed_with() 可重复: ", s1, "\n");
        } else {
            see("[FAIL] seed_with() 不可重复: ", s1, " vs ", s2, "\n");
        }

        // 测试 system 随机函数
        see("--- system 随机函数 ---\n");
        
        srandom(99);
        int r4 = random();
        srandom(99);
        int r5 = random();
        if (r4 == r5) {
            see("[PASS] srandom()/random() 可重复: ", r4, "\n");
        } else {
            see("[FAIL] srandom()/random() 不可重复\n");
        }

        // random_range
        int r6 = random_range(10, 20);
        if (r6 >= 10 && r6 <= 20) {
            see("[PASS] random_range(10,20) = ", r6, "\n");
        } else {
            see("[FAIL] random_range(10,20) = ", r6, "\n");
        }

        // uptime
        int up = uptime();
        see("uptime: ", up, " 秒\n");
        if (up >= 0) {
            see("[PASS] uptime()\n");
        } else {
            see("[FAIL] uptime() 负值\n");
        }

        see("=== 随机数测试完成 ===\n");
    }
}
