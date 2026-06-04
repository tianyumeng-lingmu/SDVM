// 测试: 字符串索引 + 字符串 BIF
// 还有下标赋值

main {
    // 测试1: 字符串索引 s[x]
    s = "Hello, StarDance!";
    see("s = "); see(s); see("\n");

    c0 = s[0];
    see("s[0] = "); see(c0); see("\n");

    c7 = s[7];
    see("s[7] = "); see(c7); see("\n");

    // 测试2: str_at
    c = str_at(s, 4);
    see("str_at(s, 4) = "); see(c); see("\n");

    // 测试3: str_sub
    sub1 = str_sub(s, 7, 16);
    see("str_sub(s, 7, 16) = "); see(sub1); see("\n");

    sub2 = str_sub(s, 0, 5);
    see("str_sub(s, 0, 5) = "); see(sub2); see("\n");

    sub3 = str_sub(s, 7, -1);
    see("str_sub(s, 7, -1) = "); see(sub3); see("\n");

    // 测试4: str_find
    pos = str_find(s, "Star");
    see("str_find(s, 'Star') = "); see(pos); see("\n");

    pos2 = str_find(s, "xyz");
    see("str_find(s, 'xyz') = "); see(pos2); see("\n");

    // 测试5: str_contains
    has1 = str_contains(s, "Dance");
    see("str_contains(s, 'Dance') = "); see(has1); see("\n");

    has2 = str_contains(s, "Moon");
    see("str_contains(s, 'Moon') = "); see(has2); see("\n");

    // 测试6: str_trim
    messy = "  hello world  ";
    tidy = str_trim(messy);
    see("str_trim('  hello world  ') = '"); see(tidy); see("'\n");

    // 测试7: str_upper / str_lower
    up = str_upper("Hello");
    see("str_upper('Hello') = "); see(up); see("\n");

    low = str_lower("WORLD");
    see("str_lower('WORLD') = "); see(low); see("\n");

    // 测试8: 对象下标索引
    obj = ['name': 'SDVM', 'version': 2, 'lang': 'StarDance'];
    val = obj["name"];
    see("obj['name'] = "); see(val); see("\n");

    // 测试9: 下标赋值
    obj["extra"] = "new_field";
    extra = obj["extra"];
    see("obj['extra'] = "); see(extra); see("\n");

    // 测试10: 链式索引
    info = ['title': 'title_value', 'nested': ['a': 1, 'b': 2]];
    nested_a = info["nested"]["a"];
    see("info['nested']['a'] = "); see(nested_a); see("\n");

    // 测试11: str_split + obj[int] 下标
    parts = str_split("a,b,c", ",");
    see("split ',': "); see(parts[0]); see(parts[1]); see(parts[2]); see("\n");

    // 测试12: 解析一行输入
    line = "GET /index.html HTTP/1.1";
    fields = str_split(line, " ");
    see("split line: "); see(fields[0]); see(", "); see(fields[1]); see(", "); see(fields[2]); see("\n");

    // 测试13: 空分隔符（每字符分割）
    chars = str_split("ABC", "");
    see("split empty: "); see(chars[0]); see(chars[1]); see(chars[2]); see("\n");

    // 测试14: obj[str] + obj[int] 下标都可用
    see("parts[0](int)="); see(parts[0]); see(' parts["0"](str)='); see(parts["0"]); see("\n");

    see("\n=== 全部测试通过! ===\n");
}
