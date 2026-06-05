// 测试包依赖传递
start {
    use test_dep;
    use system;
}

main {
    thing main() {
        // 调用 test_dep 包的 GetInfo
        int pid = test_dep.GetInfo();
        see("GetInfo returned PID = "); see(pid); see("\n");
        // 测试 system 函数直接可用
        int my_pid = system.GetCurrentProcessId();
        see("Direct PID = "); see(my_pid); see("\n");
        // 验证值相同
        if (pid == my_pid) {
            see("包依赖测试通过!\n");
        } else {
            see("包依赖测试失败!\n");
        }
    }
}
