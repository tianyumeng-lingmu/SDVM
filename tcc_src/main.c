/*
 * ═══════════════════════════════════════════════
 *  SDVM — 星舞虚拟机 CLI 入口
 *  用法: sdvm <文件.dance> [-v]
 *   -v      启用指令跟踪调试
 * ═══════════════════════════════════════════════
 */

#include "sdvm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif
/* 提供 TCC 缺少的常量 */
#ifndef CP_UTF8
#define CP_UTF8 65001
#endif

static void print_usage(void) {
    printf("SDVM v1.0 - Star Dance Virtual Machine\n");
    printf("Usage: sdvm <file.dance> [-v]\n");
    printf("  -v    enable verbose instruction trace\n");
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    if (argc < 2) {
        print_usage();
        return 1;
    }

    const char* file_path = NULL;
    int verbose = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        } else if (argv[i][0] != '-') {
            file_path = argv[i];
        }
    }

    if (!file_path) {
        fprintf(stderr, "Error: no .dance file specified\n");
        print_usage();
        return 1;
    }

    const char* ext = strrchr(file_path, '.');
    if (!ext || (strcmp(ext, ".dance") != 0 && strcmp(ext, ".DANCE") != 0)) {
        fprintf(stderr, "Warning: '%s' does not have .dance extension\n", file_path);
    }

    size_t file_size = 0;
    uint8_t* buffer = sdvm_read_file(file_path, &file_size);
    if (!buffer) {
        fprintf(stderr, "Error: cannot read '%s'\n", file_path);
        return 1;
    }

    SDVM vm;
    sdvm_init(&vm);
    vm.verbose = verbose;

    int ret = sdvm_load(&vm, buffer, file_size);
    free(buffer);

    if (ret != 0) {
        fprintf(stderr, "Load error: %s\n", vm.error_msg);
        sdvm_free(&vm);
        return 1;
    }

    if (verbose) {
        printf("; Loaded: code_size=%u, strpool_count=%u\n",
               vm.code_size, vm.strpool_count);
    }

    ret = sdvm_run(&vm);
    if (ret != 0) {
        fprintf(stderr, "Runtime error: %s\n", vm.error_msg);
        sdvm_free(&vm);
        return 1;
    }

    fflush(stdout);  /* 确保所有输出被刷新 */
    sdvm_free(&vm);
    return 0;
}
