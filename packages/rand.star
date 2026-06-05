// ─── rand 包 ──────────────────────────────────
// 随机数生成专用包 (基于 msvcrt.dll)
// 使用: start { use rand; }
// ──────────────────────────────────────────────

start {
    // 空 start 块
}

// 设置随机数种子（用当前时间自动种子）
thing seed() {
    int m = ffi_load("msvcrt.dll");
    int k = ffi_load("kernel32.dll");
    int t = ffi_call(k, "GetTickCount64", "i");
    ffi_call(m, "srand", "v", t);
    return 0;
}

// 自定义种子
thing seed_with(val) {
    int m = ffi_load("msvcrt.dll");
    ffi_call(m, "srand", "v", val);
    return 0;
}

// 随机整数 (0..32767)
thing next() {
    int m = ffi_load("msvcrt.dll");
    return ffi_call(m, "rand", "i");
}

// 指定范围随机整数 [min, max]
thing range(min, max) {
    int m = ffi_load("msvcrt.dll");
    int r = ffi_call(m, "rand", "i");
    int rng = max - min + 1;
    return min + (r % rng);
}

// 0.0..1.0 随机浮点数
thing unit() {
    int m = ffi_load("msvcrt.dll");
    int r = ffi_call(m, "rand", "i");
    return r / 32767;  // OP_DIV 始终返回 float
}
