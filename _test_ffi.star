// 测试 FFI — 调用 kernel32.dll 函数
// 验证: 缓存机制、跨平台宏、返回类型(i/s/p)、字符串参数
main {
    thing main() {
        int kernel = ffi_load("kernel32.dll");

        // === 返回类型 "i" (int64) ===
        // 无参数函数
        int last_err = ffi_call(kernel, "GetLastError", "i");
        see("GetLastError = "); see(last_err); see("\n");
        int pid = ffi_call(kernel, "GetCurrentProcessId", "i");
        see("PID = "); see(pid); see("\n");
        int debug = ffi_call(kernel, "IsDebuggerPresent", "i");
        see("DebuggerPresent = "); see(debug); see("\n");

        // 1 个参数: GetProcessVersion(DWORD) → DWORD
        int ver = ffi_call(kernel, "GetProcessVersion", "i", pid);
        see("ProcessVersion = "); see(ver); see("\n");

        // === 返回类型 "s" (字符串) ===
        // GetCommandLineA() → char* → VM 内自动拷贝为字符串
        cmdline = ffi_call(kernel, "GetCommandLineA", "s");
        see("CmdLine = "); see(cmdline); see("\n");

        // === 返回类型 "p" (指针/句柄, 作为整数传递) ===
        int hStdOut = ffi_call(kernel, "GetStdHandle", "p", -11);
        see("StdOutHandle = "); see(hStdOut); see("\n");

        // === 字符串参数 ===
        // SetConsoleTitleA 接受 const char*
        ffi_call(kernel, "SetConsoleTitleA", "v", "FFI Test OK!");

        ffi_free(kernel);
        see("\nFFI test OK! (i/s/p all passed)\n");
    }
}
