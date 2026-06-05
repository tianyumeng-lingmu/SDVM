// ─── copy 包测试 ───
// copy.copy(a)    将对象 a 存入临时区（不返回值）
// copy.paste()    粘贴返回临时区的对象
// copy.clean()    清除临时区的对象

start{
    use copy;
    str PASS = "[PASS]";
    str FAIL = "[FAIL]";
}

main{
    thing main(){
        see("╔══════════════════════════════════════╗\n");
        see("║   copy 包测试                       ║\n");
        see("╚══════════════════════════════════════╝\n");

        // ─── 1. 复制整型 ───
        see("=== 1. 复制整型 ===\n");
        int a = 42;
        copy.copy(a);
        int b = copy.paste();
        if (b == 42){
            see(PASS, " int copy: b = ", b, "\n");
        } else {
            see(FAIL, " int copy: got ", b, " expect 42\n");
        }

        // ─── 2. 复制浮点 ───
        see("\n=== 2. 复制浮点 ===\n");
        float pi = 3.14159;
        copy.copy(pi);
        float pf = copy.paste();
        see(PASS, " float copy: pf = ", pf, "\n");

        // ─── 3. 复制字符串 ───
        see("\n=== 3. 复制字符串 ===\n");
        str s = "hello, copy!";
        copy.copy(s);
        str s2 = copy.paste();
        see(PASS, " str copy: s2 = ", s2, "\n");

        // ─── 4. 复制布尔 ───
        see("\n=== 4. 复制布尔 ===\n");
        bool flag = true;
        copy.copy(flag);
        bool f2 = copy.paste();
        if (f2){
            see(PASS, " bool copy: f2 = true\n");
        } else {
            see(FAIL, " bool copy: got false\n");
        }

        // ─── 5. 复制列表 ───
        see("\n=== 5. 复制列表 ===\n");
        list lst = [10, 20, 30];
        copy.copy(lst);
        list lst2 = copy.paste();
        see("lst2 = ");
        _print_list(lst2);
        if (len(lst2) == 3 && lst2[0] == 10 && lst2[2] == 30){
            see(PASS, " list copy 成功\n");
        } else {
            see(FAIL, " list copy 失败\n");
        }

        // ─── 6. 多次复制覆盖 ───
        see("\n=== 6. 多次复制覆盖 ===\n");
        copy.copy(111);
        copy.copy(222);
        int val = copy.paste();
        if (val == 222){
            see(PASS, " 覆盖复制: val = ", val, "\n");
        } else {
            see(FAIL, " 覆盖复制: got ", val, " expect 222\n");
        }

        // ─── 7. clean 后 paste 得到 null ───
        see("\n=== 7. clean 测试 ===\n");
        copy.copy(999);
        copy.clean();
        int after_clean = copy.paste();
        see(PASS, " clean 后 paste: ", after_clean, "\n");

        // ─── 8. copy.clone(x) ───
        see("\n=== 8. copy.clone(x) ===\n");
        int c8 = 777;
        int c8b = copy.clone(c8);
        if (c8b == 777){
            see(PASS, " copy.clone int: ", c8b, "\n");
        }
        list c8l = [5, 6, 7];
        list c8l2 = copy.clone(c8l);
        if (len(c8l2) == 3 && c8l2[1] == 6){
            see(PASS, " copy.clone list: ");
            _print_list(c8l2);
        }

        // ─── 9. 对象.clone() 语法糖 ───
        see("\n=== 9. 对象.clone() ===\n");
        int d9 = 888;
        int d9b = d9.clone();
        if (d9b == 888){
            see(PASS, " int.clone(): ", d9b, "\n");
        }
        list d9l = [100, 200, 300];
        list d9l2 = d9l.clone();
        if (len(d9l2) == 3 && d9l2[2] == 300){
            see(PASS, " list.clone(): ");
            _print_list(d9l2);
        }

        // ─── 10. copy.deepclone(x) ───
        see("\n=== 10. copy.deepclone(x) ===\n");
        int e10 = 123;
        int e10b = copy.deepclone(e10);
        if (e10b == 123){
            see(PASS, " copy.deepclone int: ", e10b, "\n");
        }
        list e10l = [1, 2, 3];
        list e10l2 = copy.deepclone(e10l);
        if (len(e10l2) == 3 && e10l2[1] == 2){
            see(PASS, " copy.deepclone list: ");
            _print_list(e10l2);
        }

        // ─── 11. 对象.deepclone() 语法糖 ───
        see("\n=== 11. 对象.deepclone() ===\n");
        int f11 = 456;
        int f11b = f11.deepclone();
        if (f11b == 456){
            see(PASS, " int.deepclone(): ", f11b, "\n");
        }
        list f11l = [7, 8, 9];
        list f11l2 = f11l.deepclone();
        if (len(f11l2) == 3 && f11l2[0] == 7){
            see(PASS, " list.deepclone(): ");
            _print_list(f11l2);
        }

        // ─── 结论 ───
        see("\n╔══════════════════════════════════════╗\n");
        see("║   copy 包测试通过！              ║\n");
        see("╚══════════════════════════════════════╝\n");
    }

    // ── 辅助：打印列表 ──
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
