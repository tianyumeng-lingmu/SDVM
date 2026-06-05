// _solve_bin_str.star
// 二进制连接字符串问题：0|1|10|11|100|101|...
// 求前 x 位中 1 的个数
//
// 算法：按 bit-length 分组
// k-bit 组: 2^(k-1) 个数字，每个 k 位
//   组内总位数 bg = k * 2^(k-1)
//   组内总 1 数 og = 2^(k-1) + (k-1) * 2^(k-2)
//
// 使用 << 代替乘2循环，使用 /^ 和 % 代替手写整除/取模

main {
    thing main() {
        see("solve(0)="); see(solve(0)); see("\n");
        see("solve(1)="); see(solve(1)); see("\n");
        see("solve(7)="); see(solve(7)); see("\n");
        see("solve(10)="); see(solve(10)); see("\n");
        see("solve(100)="); see(solve(100)); see("\n");
        see("solve(1000)="); see(solve(1000)); see("\n");
    }
}

// ─── 求 0..n 所有整数的二进制中 1 的总数 ──
// 对每个二进制位 i，每 2^(i+1) 为一个周期
// 前 2^i 个该位为 0，后 2^i 个为 1
thing popcount_sum(n) {
    if (n <= 0) return 0;
    count = 0;
    for (i = 0; ; i++) {
        pi = 1 << i;
        if (pi > n) break;
        block = pi << 1;        // 2^(i+1)
        full = (n + 1) /^ block;
        rem = (n + 1) % block;
        count = count + full * pi;
        if (rem > pi) {
            count = count + (rem - pi);
        }
    }
    return count;
}

// ─── 求 num 的前 bits 位中 1 的个数（从 MSB）─
thing popcount_prefix(num, bits) {
    count = 0;
    n = num;
    // 求 bit_len
    total = 0;
    tn = 1;
    while (tn <= num) { total = total + 1; tn = tn << 1; }
    // 逐位检查，从最高位开始
    for (i = 0; i < bits; i++) {
        bv = 1 << (total - 1 - i);
        if (n >= bv) {
            count = count + 1;
            n = n - bv;
        }
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
        nc = 1 << (m - 1);              // 2^(m-1): 组内数字个数
        bg = m * nc;                     // 组内总位数
        pm2 = 1 << (m - 2);              // 2^(m-2)
        og = nc + (m - 1) * pm2;         // 组内总 1 数
        if (bits_used + bg < x) {
            bits_used = bits_used + bg;
            total_ones = total_ones + og;
            m = m + 1;
        } else {
            rem = x - bits_used;
            fn = rem /^ m;                // 完整数字个数
            pb = rem % m;                 // 最后数字的部分位数
            k = fn - 1;
            if (k >= 0) {
                total_ones = total_ones + (k + 1);       // 每个数字的首位为 1
                total_ones = total_ones + popcount_sum(k); // 非首位的 1
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
