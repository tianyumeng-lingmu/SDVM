// 测试 FFI — 调用 kernel32.dll 函数
main {
    int kernel = ffi_load("kernel32.dll");

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

    ffi_free(kernel);
    see("\nFFI test OK!\n");
}
