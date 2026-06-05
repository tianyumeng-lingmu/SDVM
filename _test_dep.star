// 测试包依赖包
start {
    use test_dep;
}

main {
    int pid = GetInfo();
    see("Main PID = "); see(pid); see("\n");

    see("包依赖测试通过!\n");
}
