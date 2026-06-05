thing greet(name, count) {
    int i = 0;
    while (i < count) {
        see("Hello, ", name, "!\n");
        i = i + 1;
    }
    return(null);
}
main {
    thing main() {
        see("=== Test positional args ===\n");
        greet("World", 3);
        see("=== Test named args ===\n");
        greet(count=2, name="Alice");
        see("Done!\n");
    }
}
