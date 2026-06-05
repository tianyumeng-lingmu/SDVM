// _test_bin_str_v8.star
// 二进制连接字符串问题：0|1|10|11|100|101|...
// 求前 x 位中 1 的个数
//
// 使用 << 代替乘2循环，使用 /^ 和 % 代替手写整除/取模

main {
    thing main() {
        see("=== 测试 solve(x) ===\n");
        see("solve(0)="); see(solve(0)); see(" (期望 0)\n");
        see("solve(1)="); see(solve(1)); see(" (期望 0)\n");
        see("solve(2)="); see(solve(2)); see(" (期望 1)\n");
        see("solve(3)="); see(solve(3)); see(" (期望 2)\n");
        see("solve(4)="); see(solve(4)); see(" (期望 2)\n");
        see("solve(5)="); see(solve(5)); see(" (期望 3)\n");
        see("solve(6)="); see(solve(6)); see(" (期望 4)\n");
        see("solve(7)="); see(solve(7)); see(" (期望 5)\n");
        see("solve(8)="); see(solve(8)); see(" (期望 5)\n");
        see("solve(9)="); see(solve(9)); see(" (期望 5)\n");
        see("solve(10)="); see(solve(10)); see(" (期望 6)\n");
        see("solve(18)="); see(solve(18)); see(" (期望 12)\n");
        see("solve(50)="); see(solve(50)); see(" (期望 32)\n");
        see("solve(51)="); see(solve(51)); see(" (期望 33)\n");
        see("solve(55)="); see(solve(55)); see(" (期望 33)\n");
        see("solve(56)="); see(solve(56)); see(" (期望 34)\n");
        see("solve(60)="); see(solve(60)); see(" (期望 35)\n");
        see("solve(100)="); see(solve(100)); see(" (期望 57)\n");
        see("solve(1000)="); see(solve(1000)); see(" (期望 544)\n");
        see("solve(5000)="); see(solve(5000)); see(" (期望 2670)\n");
        see("solve(10000)="); see(solve(10000)); see(" (期望 5400)\n");
        see("solve(50000)="); see(solve(50000)); see(" (期望 26529)\n");
        see("solve(100000)="); see(solve(100000)); see(" (期望 53777)\n");
        see("\n=== 测试完成 ===\n");
    }
}

// ─── 求 0..n 所有整数的二进制中 1 的总数 ──
thing popcount_sum(n) {
    if (n <= 0) return 0;
    count = 0;
    i = 0;
    while (1) {
        pi = 1 << i;
        if (pi > n) break;
        block = pi << 1;
        full = (n + 1) /^ block;
        rem = (n + 1) % block;
        count = count + full * pi;
        if (rem > pi) {
            count = count + (rem - pi);
        }
        i = i + 1;
    }
    return count;
}

// ─── 求 num 的前 bits 位中 1 的个数（从 MSB）─
thing popcount_prefix(num, bits) {
    count = 0;
    n = num;
    total = 0;
    tn = 1;
    while (tn <= num) { total = total + 1; tn = tn << 1; }
    i = 0;
    while (i < bits) {
        bv = 1 << (total - 1 - i);
        if (n >= bv) {
            count = count + 1;
            n = n - bv;
        }
        i = i + 1;
    }
    return count;
}

// ─── 主求解函数 ─────────────────────────
thing solve(x) {
    if (x <= 1) return 0;
    total_ones = 0;
    bits_used = 1;
    if (bits_used + 1 >= x) { return x - bits_used; }
    bits_used = bits_used + 1;
    total_ones = total_ones + 1;
    m = 2;
    while (1) {
        nc = 1 << (m - 1);
        bg = m * nc;
        pm2 = 1 << (m - 2);
        og = nc + (m - 1) * pm2;
        if (bits_used + bg < x) {
            bits_used = bits_used + bg;
            total_ones = total_ones + og;
            m = m + 1;
        } else {
            rem = x - bits_used;
            fn = rem /^ m;
            pb = rem % m;
            k = fn - 1;
            if (k >= 0) {
                total_ones = total_ones + (k + 1);
                total_ones = total_ones + popcount_sum(k);
            }
            if (pb > 0) {
                num = (1 << (m - 1)) + fn;
                total_ones = total_ones + popcount_prefix(num, pb);
            }
            break;
        }
    }
    return total_ones;
}
