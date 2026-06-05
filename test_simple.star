thing greet(name, count) {
    for (int i = 0; i < count; i++) {
        see(name);
        see("\n");
    }
    return(null);
}

main {
    thing main() {
        see("start\n");
        greet("world", 3);
        see("done\n");
    }
}
