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

static void print_usage(void) {
    printf("SDVM — 星舞虚拟机 (Star Dance Virtual Machine) v1.0\n");
    printf("用法: sdvm <文件.dance> [-v]\n");
    printf("  -v    启用指令追踪调试\n");
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    /* 设置控制台为 UTF-8 编码，确保中文正常显示 */
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    if (argc < 2) {
        print_usage();
        return 1;
    }

    /* 解析参数 */
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
        fprintf(stderr, "错误: 未指定 .dance 文件\n");
        print_usage();
        return 1;
    }

    /* 检查文件扩展名 */
    const char* ext = strrchr(file_path, '.');
    if (!ext || (strcmp(ext, ".dance") != 0 && strcmp(ext, ".DANCE") != 0)) {
        fprintf(stderr, "警告: 文件 '%s' 不是 .dance 后缀\n", file_path);
    }

    /* 读取文件 */
    size_t file_size = 0;
    uint8_t* buffer = sdvm_read_file(file_path, &file_size);
    if (!buffer) {
        fprintf(stderr, "错误: 无法读取文件 '%s'\n", file_path);
        return 1;
    }

    /* 初始化虚拟机 */
    SDVM vm;
    sdvm_init(&vm);
    vm.verbose = verbose;

    /* 加载字节码 */
    int ret = sdvm_load(&vm, buffer, file_size);
    free(buffer);  /* 加载完后释放文件缓冲区 */

    if (ret != 0) {
        fprintf(stderr, "加载错误: %s\n", vm.error_msg);
        sdvm_free(&vm);
        return 1;
    }

    if (verbose) {
        printf("; SDVM 加载成功: code_size=%u, strpool_count=%u\n",
               vm.code_size, vm.strpool_count);
    }

    /* 执行 */
    ret = sdvm_run(&vm);
    if (ret != 0) {
        fprintf(stderr, "运行时错误: %s\n", vm.error_msg);
        sdvm_free(&vm);
        return 1;
    }

    sdvm_free(&vm);
    return 0;
}
