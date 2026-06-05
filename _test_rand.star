// 测试 rand 包 + system 随机相关函数

start {
    use rand;
    use system;
}

main {
    thing main() {
        see("=== 随机数包测试 ===\n");

        // 测试 rand 包
        see("--- rand 包 ---\n");
        
        // rand.seed()
        rand.seed();
        see("[OK] rand.seed()\n");

        // rand.next()
        int r1 = rand.next();
        int r2 = rand.next();
        see("rand.next(): ", r1, ", ", r2, "\n");
        if (r1 >= 0 && r1 <= 32767 && r2 >= 0 && r2 <= 32767) {
            see("[PASS] rand.next() 在范围内\n");
        } else {
            see("[FAIL] rand.next() 超出范围\n");
        }

        // 随机性验证
        if (r1 != r2) {
            see("[PASS] rand.next() 有随机性\n");
        } else {
            see("[FAIL] rand.next() 无随机性\n");
        }

        // rand.range()
        int r3 = rand.range(1, 6);
        if (r3 >= 1 && r3 <= 6) {
            see("[PASS] rand.range(1,6) = ", r3, "\n");
        } else {
            see("[FAIL] rand.range(1,6) = ", r3, "\n");
        }

        // rand.unit() — 0.0..1.0
        float u1 = rand.unit();
        if (u1 >= 0.0 && u1 <= 1.0) {
            see("[PASS] rand.unit() = ", u1, "\n");
        } else {
            see("[FAIL] rand.unit() = ", u1, "\n");
        }

        // 固定种子测试可重复性
        rand.seed_with(42);
        int s1 = rand.next();
        rand.seed_with(42);
        int s2 = rand.next();
        if (s1 == s2) {
            see("[PASS] rand.seed_with() 可重复: ", s1, "\n");
        } else {
            see("[FAIL] rand.seed_with() 不可重复: ", s1, " vs ", s2, "\n");
        }

        // 测试 system 随机函数
        see("--- system 随机函数 ---\n");
        
        system.srandom(99);
        int r4 = system.random();
        system.srandom(99);
        int r5 = system.random();
        if (r4 == r5) {
            see("[PASS] system.srandom()/random() 可重复: ", r4, "\n");
        } else {
            see("[FAIL] system.srandom()/random() 不可重复\n");
        }

        // system.random_range
        int r6 = system.random_range(10, 20);
        if (r6 >= 10 && r6 <= 20) {
            see("[PASS] system.random_range(10,20) = ", r6, "\n");
        } else {
            see("[FAIL] system.random_range(10,20) = ", r6, "\n");
        }

        // system.uptime()
        int up = system.uptime();
        see("uptime: ", up, " 秒\n");
        if (up >= 0) {
            see("[PASS] system.uptime()\n");
        } else {
            see("[FAIL] system.uptime() 负值\n");
        }

        see("=== 随机数测试完成 ===\n");
    }
}
