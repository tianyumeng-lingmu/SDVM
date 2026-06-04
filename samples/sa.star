main {
    see("╔══════════════════════════╗\n");
    see("║    ✦ 星舞计算器 ✦       ║\n");
    see("║   Star Dance Calculator  ║\n");
    see("╚══════════════════════════╝\n\n");

    see("支持的运算:\n");
    see("  +  加法          -  减法\n");
    see("  *  乘法          /  除法\n");
    see("  %% 取模(余数)    ^  幂运算\n\n");
    see("输入 .exit 退出计算器\n");
    see("━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n");

    bool running = true;

    while (running) {
        see("输入第一个数: ");
        str a_input = insert("");
        if (a_input == ".exit") {
            running = false;
            continue;
        }
        int a = int(a_input);

        see("输入运算符 (+, -, *, /, %%, ^): ");
        str op = insert("");
        if (op == ".exit") {
            running = false;
            continue;
        }

        see("输入第二个数: ");
        str b_input = insert("");
        if (b_input == ".exit") {
            running = false;
            continue;
        }
        int b = int(b_input);

        float result = 0;
        bool valid = true;

        if (op == "+") {
            result = a + b;
        } else if (op == "-") {
            result = a - b;
        } else if (op == "*") {
            result = a * b;
        } else if (op == "/") {
            if (b == 0) {
                see("错误：除数不能为零！\n\n");
                valid = false;
            } else {
                result = a / b;
            }
        } else if (op == "%%" || op == "%") {
            if (b == 0) {
                see("错误：取模不能为零！\n\n");
                valid = false;
            } else {
                result = a % b;
            }
        } else if (op == "^") {
            if (b == 0) {
                result = 1;
            } else {
                result = 1;
                int i = 0;
                if (b > 0) {
                    while (i < b) {
                        result = result * a;
                        i = i + 1;
                    }
                } else {
                    float abs_b = 0 - b;
                    while (i < abs_b) {
                        result = result * a;
                        i = i + 1;
                    }
                    if (result != 0) {
                        result = 1 / result;
                    } else {
                        see("错误：0 不能有负指数！\n\n");
                        valid = false;
                    }
                }
            }
        } else {
            see("错误：不支持的运算符 '\n\n");
            valid = false;
        }

        if (valid) {
            see("\n━━━━━━━━━━━━━━━━━━━━\n");
            see("  ", a, " ", op, " ", b, " = ", result, "\n");
            see("━━━━━━━━━━━━━━━━━━━━\n\n");
        }

        see("继续计算？(y/n): ");
        str cont = insert("");
        if (cont == "n" || cont == "N" || cont == ".exit") {
            running = false;
        }
        see("\n");
    }

    see("\n感谢使用星舞计算器，再见！\n");
}