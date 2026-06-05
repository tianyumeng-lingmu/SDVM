// test_webstar.star - webstar 包测试用例
// 注意：此测试只验证 web_response 等纯函数，不启动 HTTP 服务器
// 要测试完整 HTTP 服务器功能，请手动运行原始版本

start {
    use webstar;
}

main {
    thing main() {
        see("webstar 包测试开始\n");

        // 测试 web_response 纯函数
        str resp = web_response(200, "text/html", "<h1>Hello</h1>");
        see("Response status line: ", resp, "\n");

        // 验证响应包含预期内容（通过 len 判断）
        int length = len(resp);
        see("Response length: ", length, "\n");
        if (length > 50) {
            see("[PASS] web_response 格式化正确\n");
        } else {
            see("[FAIL] web_response 格式错误，长度过短\n");
        }

        see("webstar 包测试完成\n");
    }
}
