// 测试包：依赖 system 包
start {
    use system;
}

// 包装一下 system 的函数，验证依赖传递
thing GetInfo() {
    int pid = system.GetCurrentProcessId();
    int tid = system.GetCurrentThreadId();
    int ticks = system.GetTickCount64();
    see("--- test_dep 包 ---\n");
    see("PID="); see(pid); see(" TID="); see(tid);
    see(" Ticks="); see(ticks); see("\n");
    return pid;
}

thing SysBeep(f, d) {
    system.Beep(f, d);
    return 0;
}
