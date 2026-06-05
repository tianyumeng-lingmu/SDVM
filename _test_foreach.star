main {
    thing main() {
        see("=== foreach 测试 ===");
        
        see("-- 遍历列表 --");
        foreach x in [10, 20, 30, 40, 50] {
            see(x);
        }
        
        see("-- 求和 --");
        sum = 0;
        foreach n in [1, 2, 3, 4, 5, 6, 7, 8, 9, 10] {
            sum = sum + n;
        }
        see(sum);
        
        see("-- 偶数过滤 --");
        foreach v in [1, 2, 3, 4, 5, 6, 7, 8] {
            if (v % 2 == 0) {
                see(v);
            }
        }
        
        see("-- 嵌套 foreach --");
        foreach a in [1, 2, 3] {
            foreach b in [10, 20] {
                see(a + b);
            }
        }
        
        see("=== 完成 ===");
        return(null);
    }
}
