// 匿名函数测试：验证 anonymou 关键词
// 不含闭包捕获

start {
    int base = 10;
}

main {
    see("=== 匿名函数测试 ===\n");

    // 1. 匿名函数作为值（不含外部变量捕获）
    var double = anonymou(n) {
        return(n * 2);
    };

    see("double(21) = ");
    int d = double(21);
    see(d);
    see("\n");

    // 2. 匿名函数作为参数
    var triple = anonymou(n) {
        return(n * 3);
    };
    int r2 = triple(7);
    see("triple(7) = ");
    see(r2);
    see("\n");

    // 3. 直接使用匿名函数（内联）
    int r3 = anonymou(n) { return(n + 100); }(50);
    see("inline anon(50) = ");
    see(r3);
    see("\n");

    see("=== 测试完成 ===\n");
}
