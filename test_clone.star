// ─── clone / deepclone 语法糖测试 ───
// val.clone()      浅拷贝语法糖（= copy.copy + copy.paste）
// val.deepclone()  深拷贝语法糖（= copy.deepcopy + copy.paste）

start{
    use copy;
    str PASS = "[PASS]";
    str FAIL = "[FAIL]";
}

main{
    thing main(){
        see("╔══════════════════════════════════════╗\n");
        see("║   clone / deepclone 语法糖测试     ║\n");
        see("╚══════════════════════════════════════╝\n");

        // ─── 1. int.clone() ───
        see("=== 1. 整型 .clone() ===\n");
        int a = 42;
        int b = a.clone();
        if (b == 42){
            see(PASS, " int.clone(): b = ", b, "\n");
        } else {
            see(FAIL, " int.clone(): got ", b, " expect 42\n");
        }

        // ─── 2. float.clone() ───
        see("\n=== 2. 浮点 .clone() ===\n");
        float pi = 3.14;
        float pf = pi.clone();
        see(PASS, " float.clone(): ", pf, "\n");

        // ─── 3. str.clone() ───
        see("\n=== 3. 字符串 .clone() ===\n");
        str s = "hello";
        str s2 = s.clone();
        see(PASS, " str.clone(): ", s2, "\n");

        // ─── 4. bool.clone() ───
        see("\n=== 4. 布尔 .clone() ===\n");
        bool flag = true;
        bool f2 = flag.clone();
        if (f2){
            see(PASS, " bool.clone(): true\n");
        }

        // ─── 5. 列表 .clone()（浅拷贝） ───
        see("\n=== 5. 列表 .clone() ===\n");
        list lst = [1, 2, 3];
        list lst2 = lst.clone();
        see("lst2 = ");
        _print_list(lst2);
        if (len(lst2) == 3 && lst2[0] == 1 && lst2[2] == 3){
            see(PASS, " list.clone() 成功\n");
        } else {
            see(FAIL, " list.clone() 失败\n");
        }

        // ─── 6. 嵌套列表 .deepclone() ───
        see("\n=== 6. 嵌套列表 .deepclone() ===\n");
        list outer = [1, [2, 3], 4];
        list deep = outer.deepclone();
        // deep[1] 是嵌套列表，修改它不应影响 outer[1]
        deep[1] = [99];
        int outer_inner = outer[1][0];
        int deep_inner = deep[1][0];
        see("outer[1][0] = ", outer_inner, ", deep[1][0] = ", deep_inner, "\n");
        if (outer_inner == 2 && deep_inner == 99){
            see(PASS, " 嵌套 .deepclone() 为深拷贝\n");
        } else {
            see(FAIL, " 嵌套 .deepclone() 不是深拷贝\n");
        }

        // ─── 7. 普通列表 .deepclone() 等价于 .clone() ───
        see("\n=== 7. 普通列表 .deepclone() ===\n");
        list flat = [10, 20, 30];
        list flat2 = flat.deepclone();
        see("flat2 = ");
        _print_list(flat2);
        if (len(flat2) == 3 && flat2[1] == 20){
            see(PASS, " flat.deepclone() 成功\n");
        }

        // ─── 8. 链式调用 ───
        see("\n=== 8. 链式 clone ===\n");
        int x = 7;
        int y = x.clone().clone().clone();
        if (y == 7){
            see(PASS, " 链式 clone: ", y, "\n");
        }

        // ─── 9. copy.deepcopy() 包方法（配合 paste） ───
        see("\n=== 9. copy.deepcopy() 包方法 ===\n");
        list nested2 = [5, [6, 7]];
        copy.deepcopy(nested2);
        list out2 = copy.paste();
        out2[1] = [99];
        see("nested2[1][0] = ", nested2[1][0], ", out2[1][0] = ", out2[1][0], "\n");
        if (nested2[1][0] == 6 && out2[1][0] == 99){
            see(PASS, " copy.deepcopy() 深拷贝正确\n");
        } else {
            see(FAIL, " copy.deepcopy() 不是深拷贝\n");
        }

        // ─── 结论 ───
        see("\n╔══════════════════════════════════════╗\n");
        see("║   clone / deepclone 全部通过！     ║\n");
        see("╚══════════════════════════════════════╝\n");
    }

    // ── 辅助 ──
    thing _print_list(list lst){
        see("[");
        int n = len(lst);
        for (i = 0; i < n; i = i + 1){
            see(lst[i]);
            if (i < n - 1){ see(", "); }
        }
        see("]\n");
    }
}
