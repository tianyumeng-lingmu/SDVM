// 等腰三角形判定
// 三条路夹角120°，给出三人走的距离 a, b, c
// 判断三点连线是否构成等腰三角形

main {
    thing main() {
        str line = insert("");
        object nums = str_split(line, " ");
                int a = int(nums[0]);
        int b = int(nums[1]);
        int c = int(nums[2]);
                if (a == b || b == c || a == c) {
        see("Yes\n");
        } else {
        see("No\n");
        }
    }
}
