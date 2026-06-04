// 测试：函数必须有 return() 语句

thing no_return_func(x) {
    int y = x + 1;
    // 没有 return 语句！应该编译失败
}

main {
    see("不应该运行到这里\n");
}
