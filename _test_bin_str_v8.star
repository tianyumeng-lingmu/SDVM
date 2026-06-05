// _test_bin_str_v8.star
// 二进制连接字符串问题：0|1|10|11|100|101|...
// 求前 x 位中 1 的个数
// 全部使用乘法实现（避免 SDVM 的浮点除法问题）
//
// 算法：按 bit-length 分组
// k-bit 组: 2^(k-1) 个数字，每个 k 位
//   组内总位数 bg = k * 2^(k-1)
//   组内总 1 数 og = 2^(k-1) + (k-1) * 2^(k-2)

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

// ─── 整数除法（乘法实现） ──────────────────
thing idiv(n, d) {
    if (n < d) return 0;
    k = 0;
    while (k < 60) {
        pk1 = 1; j = 0; while (j < k+1) { pk1 = pk1 * 2; j = j + 1; }
        if (pk1 * d > n) break;
        k = k + 1;
    }
    q = 1; j = 0; while (j < k) { q = q * 2; j = j + 1; }
    r = n - q * d;
    while (k > 0) {
        k = k - 1;
        pk = 1; j = 0; while (j < k) { pk = pk * 2; j = j + 1; }
        b = pk * d;
        if (r >= b) { r = r - b; q = q + pk; }
    }
    return q;
}

thing imod(n, d) {
    q = idiv(n, d);
    return n - q * d;
}

// ─── 求 0..n 所有整数的二进制中 1 的总数 ──
thing popcount_sum(n) {
    if (n <= 0) return 0;
    count = 0;
    i = 0;
    while (1) {
        pi = 1; j = 0; while (j < i) { pi = pi * 2; j = j + 1; }
        if (pi > n) break;
        pi1 = 1; j = 0; while (j < i+1) { pi1 = pi1 * 2; j = j + 1; }
        block = pi1;
        full = idiv(n + 1, block);
        rem = imod(n + 1, block);
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
    // bit_len（乘法实现）
    total = 0;
    tn = 1;
    if (num == 0) { total = 1; }
    else { while (tn <= num) { total = total + 1; tn = tn * 2; } }
    // bv = 2^(total-1)
    bv = 1;
    p = total - 1;
    while (p > 0) { bv = bv * 2; p = p - 1; }
    // 逐位检查（不用除法，每次重算 bv=2^shift）
    shift = total - 1;
    i = 0;
    while (i < bits) {
        if (n >= bv) {
            count = count + 1;
            n = n - bv;
        }
        shift = shift - 1;
        bv = 1; j = 0; while (j < shift) { bv = bv * 2; j = j + 1; }
        i = i + 1;
    }
    return count;
}

// ─── 主求解函数 ─────────────────────────
thing solve(x) {
    if (x <= 1) return 0;
    total_ones = 0;
    bits_used = 1;
    // 处理 0（1位，值为0）
    if (bits_used + 1 >= x) { return x - bits_used; }
    bits_used = bits_used + 1;  // 已处理: [0]
    total_ones = total_ones + 1; // 0 的二进制有 0 个 1，但第 1 步已经是第 2 位（数字1）
    // 处理 1（1位，值为1）
    // 实际上 0 占 1 位(0)，1 占 1 位(1)
    // bits_used=1 对应第1位(数字0的首位), 
    // bits_used=2 对应第2位(数字1的首位)
    m = 2;
    while (1) {
        // nc = 2^(m-1): m-bit 组中的数字个数
        nc = 1; j = 0; while (j < m-1) { nc = nc * 2; j = j + 1; }
        bg = m * nc;  // 组内总位数
        pm2 = 1; j = 0; while (j < m-2) { pm2 = pm2 * 2; j = j + 1; }
        og = nc + (m - 1) * pm2;  // 组内总 1 数
        if (bits_used + bg < x) {
            bits_used = bits_used + bg;
            total_ones = total_ones + og;
            m = m + 1;
        } else {
            rem = x - bits_used;
            fn = idiv(rem, m);  // 完整数字个数
            pb = imod(rem, m);  // 最后数字的部分位数
            k = fn - 1;
            if (k >= 0) {
                total_ones = total_ones + (k + 1);      // 每个数字的首位为1
                total_ones = total_ones + popcount_sum(k); // 非首位的1
            }
            if (pb > 0) {
                pm1 = 1; j = 0; while (j < m-1) { pm1 = pm1 * 2; j = j + 1; }
                num = pm1 + fn;
                total_ones = total_ones + popcount_prefix(num, pb);
            }
            break;
        }
    }
    return total_ones;
}
