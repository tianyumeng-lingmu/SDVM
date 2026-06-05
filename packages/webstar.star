thing web_response(int status, str content_type, str body) {
    str res = "HTTP/1.1 ";
    res = res + str(status) + " OK\r\n";
    res = res + "Content-Type: " + content_type + "\r\n";
    res = res + "Content-Length: " + str(len(body)) + "\r\n";
    res = res + "Connection: close\r\n";
    res = res + "\r\n";
    res = res + body;
    return(res);
}

thing web_read_line(int conn) {
    return(net_readline(conn));
}

thing web_send(int conn, str data) {
    net_write(conn, data);
    return(null);
}

thing web_close(int conn) {
    net_close(conn);
    return(null);
}

thing web_start(int port) {
    return(net_start(port));
}

thing web_accept(int server) {
    return(net_accept(server));
}
