// test_webstar.star - webstar 包测试用例
// 启动 HTTP 服务器，接受一个连接，返回 "Hello World"
// 在浏览器访问 http://localhost:18080/

start {
    use webstar;
}

main {
    thing main() {
        int port = 18080;
        see("webstar: 启动服务器在端口 ", port, "\n");
        int server = webstar.web_start(port);
        see("webstar: 服务器句柄 = ", server, "\n");
        int conn = webstar.web_accept(server);
        see("webstar: 收到连接，句柄 = ", conn, "\n");
        str line = webstar.web_read_line(conn);
        see("webstar: 收到请求行: ", line, "\n");
        // 读取剩余的请求头（直到空行）
        while (1) {
            str hdr = webstar.web_read_line(conn);
            if (hdr == "") {
                break;
            }
        }
        str html = "<html><body><h1>Hello from webstar!</h1><p>StarDance HTTP Server</p></body></html>";
        str resp = webstar.web_response(200, "text/html", html);
        webstar.web_send(conn, resp);
        see("webstar: 响应已发送\n");
        webstar.web_close(conn);
        webstar.web_close(server);
        see("webstar: 连接已关闭\n");
    }
}
