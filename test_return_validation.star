// 测试 return 验证的边界情况

// 1. 函数中 return 在 if 内（应通过）
thing func_if_return(x) {
    if (x > 0) {
        return(x);
    }
    return(0);
}

// 2. 函数中 return 在循环内（应通过）
thing func_loop_return() {
    int i = 0;
    while (i < 10) {
        if (i == 5) {
            return(i);
        }
        i = i + 1;
    }
    return(null);
}

// 3. 函数中有 return(NULL)（应通过）
thing func_null_return() {
    return(null);
}

// 4. 命途内的 thing（方法）不应受 return 验证限制
life MyLife {
    thing my_method() {
        int x = 1 + 2;
    }
}

main {
    see("return validation test\n");
    int r1 = func_if_return(5);
    see(r1);
    see("\n");
    int r2 = func_loop_return();
    see(r2);
    see("\n");
    func_null_return();
    see("OK\n");
}
