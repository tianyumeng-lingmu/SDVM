// 测试：函数必须有 return() 语句
// 所有模块级 thing（函数）必须有明确的 return()

thing calc_result(x) {
    int y = x + 1;
    return(y);
}

main {
    thing main() {
        int result = calc_result(41);
        see("calc_result(41) = ", result, "\n");
        if (result == 42) {
            see("[PASS] return 验证通过\n");
        } else {
            see("[FAIL] 返回值错误\n");
        }
    }
}
