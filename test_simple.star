start {
    int base = 10;
}
thing say_hello() {
    see("Hello from function!\n");
    return(null);
}
main {
    see("Main start\n");
    say_hello();
    see("Back in main\n");
    see("base + 100 = ", base + 100, "\n");
}
