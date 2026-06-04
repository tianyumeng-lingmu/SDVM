// test_object.star - 测试对象/JSON/文件功能

start {
}

main {
    see("=== 创建对象 ===\n");

    // 测试特殊列表（字典/对象）
    obj = ['name': 'SDVM', 'version': 1, 'active': true];
    see("obj = "); see(obj); see("\n\n");

    // 测试属性访问
    see("=== 属性访问 ===\n");
    name = obj.name;
    see("name = "); see(name); see("\n");
    ver = obj.version;
    see("version = "); see(ver); see("\n");
    active = obj.active;
    see("active = "); see(active); see("\n\n");

    // 测试属性赋值
    see("=== 属性赋值 ===\n");
    obj.version = 2;
    ver2 = obj.version;
    see("version after set = "); see(ver2); see("\n\n");

    // 测试嵌套对象
    see("=== 嵌套对象 ===\n");
    inner = ['x': 10, 'y': 20];
    obj.inner = inner;
    see("inner = "); see(obj.inner); see("\n");
    x = obj.inner.x;
    see("obj.inner.x = "); see(x); see("\n\n");

    // 测试 JSON 编码
    see("=== JSON 编码 ===\n");
    json = json_encode(obj);
    see("json = "); see(json); see("\n\n");

    // 测试 JSON 解码
    see("=== JSON 解码 ===\n");
    decoded = json_decode(json);
    see("decoded = "); see(decoded); see("\n");
    decoded_name = decoded.name;
    see("decoded.name = "); see(decoded_name); see("\n\n");

    // 测试文件读写
    see("=== 文件读写 ===\n");
    file_write('test_output.txt', 'Hello from StarDance!');
    content = file_read('test_output.txt');
    see("file content = "); see(content); see("\n");

    // 测试文件存在
    exists = file_exists('test_output.txt');
    see("file exists = "); see(exists); see("\n");
    not_exists = file_exists('nonexistent.txt');
    see("nonexistent exists = "); see(not_exists); see("\n");

    see("\n=== 所有测试通过 ===\n");
}
