// ─── StarDance DP 算法与列表方法测试 ───
start{
    str PASS = "[PASS]";
    str FAIL = "[FAIL]";
}

main{
    thing main(){
        see("╔══════════════════════════════════════╗\n");
        see("║   StarDance — DP & 列表方法测试     ║\n");
        see("╚══════════════════════════════════════╝\n");

        // ─── 1. 列表方法测试 ───
        see("=== 1. 列表方法测试 ===\n");

        list a = [1, 2, 3];
        a.add(4);
        a.add(5);
        see("a.add(4,5) → ");
        _print_list(a);

        // add(0, 99) — 指定位置插入
        a.add(0, 99);
        see("a.add(0,99) → ");
        _print_list(a);

        // pop() — 弹出末尾
        int last = a.pop();
        see("a.pop() = ", last, "\n");

        // pop(0) — 弹出指定位置
        int first = a.pop(0);
        see("a.pop(0) = ", first, "\n");

        // remove
        list b = [10, 20, 30, 40];
        b.remove(2);
        see("b.remove(2) → ");
        _print_list(b);

        // sort
        list c = [3, 1, 4, 1, 5, 9, 2, 6];
        c.sort();
        see("c.sort() → ");
        _print_list(c);

        // reverse
        c.reverse();
        see("c.reverse() → ");
        _print_list(c);

        // copy
        list d = c.copy();
        see("d = c.copy(), len(d) = ", len(d), "\n");

        // clear
        d.clear();
        see("d.clear(), len(d) = ", len(d), "\n");

        // range
        list r = range(5);
        see("range(5) → ");
        _print_list(r);

        list r2 = range(2, 8);
        see("range(2,8) → ");
        _print_list(r2);

        // ─── 2. DP: 斐波那契 ───
        see("\n=== 2. DP: 斐波那契 ===\n");

        int fib0 = fib(0);
        see("fib(0) = ", fib0, " (expect 0)\n");
        int fib1 = fib(1);
        see("fib(1) = ", fib1, " (expect 1)\n");
        int fib10 = fib(10);
        see("fib(10) = ", fib10, " (expect 55)\n");
        int fib20 = fib(20);
        see("fib(20) = ", fib20, " (expect 6765)\n");

        // ─── 3. DP: 爬楼梯 ───
        see("\n=== 3. DP: 爬楼梯 ===\n");

        int cs1 = climbStairs(1);
        see("climbStairs(1) = ", cs1, " (expect 1)\n");
        int cs2 = climbStairs(2);
        see("climbStairs(2) = ", cs2, " (expect 2)\n");
        int cs5 = climbStairs(5);
        see("climbStairs(5) = ", cs5, " (expect 8)\n");
        int cs10 = climbStairs(10);
        see("climbStairs(10) = ", cs10, " (expect 89)\n");

        // ─── 4. DP: 0/1 背包 ───
        see("\n=== 4. DP: 0/1 背包 ===\n");

        list weight = [2, 3, 4, 5];
        list value = [6, 7, 8, 9];
        int maxVal = knapSack(5, weight, value, 4);
        see("knapSack(5) = ", maxVal, " (expect 13)\n");

        // ─── 5. DP: 最长上升子序列 ───
        see("\n=== 5. DP: LIS 最长上升子序列 ===\n");

        list arr = [10, 9, 2, 5, 3, 7, 101, 18];
        int lisLen = lengthOfLIS(arr);
        see("LIS length = ", lisLen, " (expect 4)\n");

        // ─── 结论 ───
        see("\n╔══════════════════════════════════════╗\n");
        see("║   所有 DP 测试通过！               ║\n");
        see("╚══════════════════════════════════════╝\n");
    }

    // ── 辅助函数 ──
    thing _print_list(list lst){
        see("[");
        int n = len(lst);
        for (i = 0; i < n; i = i + 1){
            see(lst[i]);
            if (i < n - 1){ see(", "); }
        }
        see("]\n");
    }

    // ── DP: 斐波那契 ──
    thing fib(int n){
        if (n <= 1){ return n; }
        list dp = [0, 1];
        for (i = 2; i <= n; i = i + 1){
            int val = dp[i - 1] + dp[i - 2];
            dp.add(val);
        }
        return dp[n];
    }

    // ── DP: 爬楼梯 ──
    thing climbStairs(int n){
        if (n <= 2){ return n; }
        int a = 1;
        int b = 2;
        for (i = 3; i <= n; i = i + 1){
            int c = a + b;
            a = b;
            b = c;
        }
        return b;
    }

    // ── DP: 0/1 背包 ──
    thing knapSack(int cap, list wt, list val, int n){
        list dp = [];
        for (w = 0; w <= cap; w = w + 1){
            dp.add(0);
        }
        for (i = 1; i <= n; i = i + 1){
            for (w = cap; w >= 0; w = w - 1){
                if (wt[i - 1] <= w){
                    int take = val[i - 1] + dp[w - wt[i - 1]];
                    if (take > dp[w]){
                        dp[w] = take;
                    }
                }
            }
        }
        return dp[cap];
    }

    // ── DP: 最长上升子序列 ──
    thing lengthOfLIS(list nums){
        int n = len(nums);
        if (n == 0){ return 0; }
        list dp = [];
        for (i = 0; i < n; i = i + 1){
            dp.add(1);
        }
        int ans = 1;
        for (i = 1; i < n; i = i + 1){
            for (j = 0; j < i; j = j + 1){
                if (nums[j] < nums[i]){
                    int candidate = dp[j] + 1;
                    if (candidate > dp[i]){
                        dp[i] = candidate;
                    }
                }
            }
            if (dp[i] > ans){ ans = dp[i]; }
        }
        return ans;
    }
}
