// 等腰三角形判定（自动测试版）
// 三条路夹角120°，给出三人走的距离 a, b, c
// 判断三点连线是否构成等腰三角形

main {
    thing main() {
        // 测试用例：[3, 3, 5] → 等腰 (Yes)
        //          [3, 4, 5] → 不等腰 (No)
        //          [7, 5, 7] → 等腰 (Yes)
        object cases = [
            "3 3 5",
            "3 4 5",
            "7 5 7"
        ];
        object expected = [true, false, true];
        object labels = ["3 3 5", "3 4 5", "7 5 7"];

        int all_pass = 1;
        for (int i = 0; i < len(cases); i++) {
            str line = cases[i];
            object nums = str_split(line, " ");
            int a = int(nums[0]);
            int b = int(nums[1]);
            int c = int(nums[2]);

            if (a == b || b == c || a == c) {
                if (expected[i]) {
                    see("[PASS] ", labels[i], " → Yes\n");
                } else {
                    see("[FAIL] ", labels[i], " → Yes (应 No)\n");
                    all_pass = 0;
                }
            } else {
                if (!expected[i]) {
                    see("[PASS] ", labels[i], " → No\n");
                } else {
                    see("[FAIL] ", labels[i], " → No (应 Yes)\n");
                    all_pass = 0;
                }
            }
        }

        if (all_pass) {
            see("[PASS] 所有三角形测试通过\n");
        } else {
            see("[FAIL] 部分测试未通过\n");
        }
    }
}
