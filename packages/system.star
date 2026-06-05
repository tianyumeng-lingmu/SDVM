// ─── system 包 ─────────────────────────────────
// Windows 系统 API 封装 (基于 FFI 调用 kernel32.dll)
// 使用: start { use system; }
// ──────────────────────────────────────────────

start {
    // 空 start 块，便于未来增加包依赖
}

// ─── 进程/系统信息 ─────────────────────────────

// 获取系统运行时间（毫秒）
thing GetTickCount64() {
    int k = ffi_load("kernel32.dll");
    return ffi_call(k, "GetTickCount64", "i");
}

// 获取当前进程 ID
thing GetCurrentProcessId() {
    int k = ffi_load("kernel32.dll");
    return ffi_call(k, "GetCurrentProcessId", "i");
}

// 获取当前线程 ID
thing GetCurrentThreadId() {
    int k = ffi_load("kernel32.dll");
    return ffi_call(k, "GetCurrentThreadId", "i");
}

// 获取最后错误码
thing GetLastError() {
    int k = ffi_load("kernel32.dll");
    return ffi_call(k, "GetLastError", "i");
}

// 判断是否在调试器下运行
thing IsDebuggerPresent() {
    int k = ffi_load("kernel32.dll");
    return ffi_call(k, "IsDebuggerPresent", "i");
}

// 获取进程版本 (需要进程 ID)
thing GetProcessVersion(pid) {
    int k = ffi_load("kernel32.dll");
    return ffi_call(k, "GetProcessVersion", "i", pid);
}

// ─── 控制台 ──────────────────────────────────

// 设置控制台窗口标题
thing SetConsoleTitle(title) {
    int k = ffi_load("kernel32.dll");
    ffi_call(k, "SetConsoleTitleA", "v", title);
    return 0;
}

// 获取标准设备句柄 (-10=输入, -11=输出, -12=错误)
thing GetStdHandle(dev_type) {
    int k = ffi_load("kernel32.dll");
    return ffi_call(k, "GetStdHandle", "i", dev_type);
}

// ─── 时间/定时 ───────────────────────────────

// 休眠指定毫秒数
thing Sleep(ms) {
    int k = ffi_load("kernel32.dll");
    ffi_call(k, "Sleep", "v", ms);
    return 0;
}

// 获取本地时间 (返回 yyyy-mm-dd HH:MM:SS 字符串)
thing GetLocalTimeStr() {
    int k = ffi_load("kernel32.dll");
    int tz = ffi_call(k, "GetTimeZoneInformation", "i", 0);
    see("GetLocalTimeStr: 暂未实现完整版\n");
    return 0;
}

// ─── 蜂鸣/声音 ───────────────────────────────

// 蜂鸣 (频率 Hz, 持续毫秒)
thing Beep(freq, ms) {
    int k = ffi_load("kernel32.dll");
    ffi_call(k, "Beep", "v", freq, ms);
    return 0;
}

// ─── 进程控制 ────────────────────────────────

// 退出当前进程 (exit_code: 0=正常, 其他=错误码)
thing exit(exit_code) {
    int k = ffi_load("kernel32.dll");
    ffi_call(k, "ExitProcess", "v", exit_code);
    return(0);  // 实际不会执行，ExitProcess 不返回
}
