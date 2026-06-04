// 函数测试：模块级 thing 是函数，有 return，有命名参数
// 编译: python compiler.py test_func.star -o test_func.dance
// 运行: sdvm test_func.dance

start {
    int base = 10;
}

// 外部函数：接受命名参数
thing greet(name, count) {
    for (int i = 0; i < count; i++) {
        see("Hello, ", name, "!\n");
    }
    return(null);
}

// 有返回值的函数
thing add(a, b) {
    return(a + b);
}

// 嵌套调用：add 函数内部调用 greet
thing demo() {
    see("=== Positional args ===\n");
    greet("World", 3);

    see("=== Named args ===\n");
    greet(count=2, name="Alice");

    see("=== Return value ===\n");
    int result = add(15, 27);
    see("15 + 27 = ", result, "\n");

    see("=== Inner variable ===\n");
    int x = 100;
    int y = add(x, 50);
    see("100 + 50 = ", y, "\n");

    return(null);
}

main {
    see("Function Test Starting...\n");
    demo();
    see("Test Complete!\n");
    int end = base + 90;
    see("base + 90 = ", end, "\n");
}
