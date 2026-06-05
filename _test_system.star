// 测试 system 包
start {
    use system;
}

main {
    thing main() {
        // 进程信息
        int pid = system.GetCurrentProcessId();
        see("PID = "); see(pid); see("\n");
        int tid = system.GetCurrentThreadId();
        see("TID = "); see(tid); see("\n");
        // 系统运行时间
        int ticks = system.GetTickCount64();
        see("TickCount64 = "); see(ticks); see("\n");
        // 调试器检测
        int debug = system.IsDebuggerPresent();
        see("DebuggerPresent = "); see(debug); see("\n");
        // 错误码
        int err = system.GetLastError();
        see("LastError = "); see(err); see("\n");
        // 进程版本
        int ver = system.GetProcessVersion(pid);
        see("ProcessVersion = "); see(ver); see("\n");
        // 控制台
        int hOut = system.GetStdHandle(-11);
        see("StdOutHandle = "); see(hOut); see("\n");
        system.SetConsoleTitle("StarDance System Test");
        see("ConsoleTitle set\n");
        // 蜂鸣测试 (取消注释即可听到声音)
        // system.Beep(800, 200);
        see("\nsystem 包测试通过!\n");
    }
}
