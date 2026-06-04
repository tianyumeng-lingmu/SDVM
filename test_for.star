thing greet(name, count) {
    for (int i = 0; i < count; i++) {
        see("Hello, ", name, "!\n");
    }
    return(null);
}
main {
    see("=== For loop test ===\n");
    greet("World", 2);
    see("Done!\n");
}
