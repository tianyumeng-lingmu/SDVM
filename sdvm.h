/*
 * ═══════════════════════════════════════════════
 *  SDVM — 星舞虚拟机 (Star Dance Virtual Machine)
 *  类 JVM 栈式虚拟机，执行 .dance 字节码
 *  版本 2: 支持函数调用 + 匿名函数
 * ═══════════════════════════════════════════════
 */

#ifndef SDVM_H
#define SDVM_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════
   指令集 (Opcodes)
   ┌─────────────┬──────┬──────────────────────────┐
   │ 指令        │ 操作数  │ 说明                     │
   ├─────────────┼──────┼──────────────────────────┤
   │ 栈操作      │      │                          │
   │ NOP         │ -    │ 空操作                    │
   │ ICONST i4   │ +4   │ 压入 int32 常量           │
   │ FCONST f8   │ +8   │ 压入 double 常量          │
   │ SCONST i4   │ +4   │ 压入字符串(常量池索引)     │
   │ BCONST b1   │ +1   │ 压入布尔值(0/1)           │
   │ NULL        │ -    │ 压入 null                 │
   │ DUP         │ -    │ 复制栈顶                  │
   │ POP         │ -    │ 弹出栈顶                  │
   │─────────────┼──────┼──────────────────────────┤
   │ 局部变量     │      │                          │
   │ LOAD b1     │ +1   │ 局部变量→栈               │
   │ STORE b1    │ +1   │ 栈→局部变量               │
   │─────────────┼──────┼──────────────────────────┤
   │ 算术运算     │      │ (pop b, pop a, push a◈b) │
   │ ADD / SUB    │ -    │ + - 运算                  │
   │ MUL / DIV    │ -    │ * / 运算                  │
   │ MOD / NEG    │ -    │ % 和取负                  │
   │─────────────┼──────┼──────────────────────────┤
   │ 比较运算     │      │ (pop b, pop a, push bool)│
   │ EQ NE LT     │ -    │ == != <                  │
   │ GT LE GE     │ -    │ > <= >=                  │
   │─────────────┼──────┼──────────────────────────┤
   │ 逻辑运算     │      │                          │
   │ NOT          │ -    │ 取反                      │
   │─────────────┼──────┼──────────────────────────┤
   │ 控制流       │      │                          │
   │ JMP i4      │ +4   │ 无条件跳转(相对偏移)       │
   │ JIF i4      │ +4   │ false 时跳转              │
   │ BIF b1 b1   │ +2   │ 调用内置函数(索引+参数数) │
   │ RET          │ -    │ 返回/函数返回             │
   │ HALT         │ -    │ 停止执行                  │
   │─────────────┼──────┼──────────────────────────┤
   │ 函数调用     │      │                          │
   │ CALL i4 b1  │ +5   │ 调用已命名的函数           │
   │ ANON i4     │ +4   │ 推送匿名函数引用           │
   │ CALLR b1    │ +1   │ 动态调用(栈顶为函数引用)  │
   │─────────────┼──────┼──────────────────────────┤
   │ I/O          │      │                          │
   │ PRINT        │ -    │ 弹出并打印                │
   │ SCAN         │ -    │ 读输入→压入字符串         │
   ╚═══════════════════════════════════════════════╝
   ═══════════════════════════════════════════════ */

typedef enum {
    /* 栈操作 0x00-0x0F */
    OP_NOP       = 0x00,
    OP_ICONST    = 0x01,   /* +4: int32 LE */
    OP_FCONST    = 0x02,   /* +8: double IEEE 754 */
    OP_SCONST    = 0x03,   /* +4: string pool index */
    OP_BCONST    = 0x04,   /* +1: 0 or 1 */
    OP_NULL      = 0x05,
    OP_DUP       = 0x06,
    OP_POP       = 0x07,

    /* 局部变量 0x10-0x1F */
    OP_LOAD      = 0x10,   /* +1: local index */
    OP_STORE     = 0x11,   /* +1: local index */

    /* 算术运算 0x20-0x2F */
    OP_ADD       = 0x20,
    OP_SUB       = 0x21,
    OP_MUL       = 0x22,
    OP_DIV       = 0x23,
    OP_MOD       = 0x24,
    OP_NEG       = 0x25,

    /* 比较运算 0x30-0x37 */
    OP_EQ        = 0x30,
    OP_NE        = 0x31,
    OP_LT        = 0x32,
    OP_GT        = 0x33,
    OP_LE        = 0x34,
    OP_GE        = 0x35,

    /* 逻辑运算 0x38-0x3F */
    OP_NOT       = 0x38,

    /* 控制流 0x40-0x4F */
    OP_JMP       = 0x40,   /* +4: signed offset from next instruction */
    OP_JIF       = 0x41,   /* +4: jump if top of stack is false */
    OP_BIF       = 0x42,   /* +1: bif index, +1: arg count */
    OP_RET       = 0x43,
    OP_HALT      = 0x44,

    /* 函数调用 0x50-0x5F */
    OP_CALL      = 0x50,   /* +4: func_idx, +1: arg_count */
    OP_ANON      = 0x51,   /* +4: func_idx (push function reference) */
    OP_CALLR     = 0x52,   /* +1: arg_count (dynamic call via stack) */

    /* I/O 0x70-0x7F */
    OP_PRINT     = 0x70,
    OP_SCAN      = 0x71,

    /* 对象操作 0x60-0x6F */
    OP_NEWOBJ    = 0x60,   /* +1: num_pairs, 从栈取 key-value 对创建对象 */
    OP_GETATTR   = 0x61,   /* +4: strpool_idx, 读取对象属性 */
    OP_SETATTR   = 0x62,   /* +4: strpool_idx, 设置对象属性 */
    OP_GETINDEX  = 0x63,   /* - : 下标访问 expr[idx], 栈: obj idx → value */
    OP_SETINDEX  = 0x64,   /* - : 下标赋值 expr[idx]=val, 栈: obj idx val → */
} OpCode;

/* ═══════════════════════════════════════════════
   内置函数索引 (BIF)
   ═══════════════════════════════════════════════ */
typedef enum {
    BIF_SEE     = 0,   /* see(...): pop N args and print them */
    BIF_INT     = 1,   /* int(x): convert to int64 */
    BIF_FLOAT   = 2,   /* float(x): convert to double */
    BIF_STR     = 3,   /* str(x): convert to string */
    BIF_BOOL    = 4,   /* bool(x): convert to bool */
    BIF_TYPE    = 5,   /* type(x): return type description */
    BIF_ID      = 6,   /* ID(x): return unique id string */
    BIF_LEN     = 7,   /* len(x): get length */
    BIF_INSERT  = 8,   /* insert(prompt): read stdin string */
    BIF_NET_START = 10, /* net_start(port): start TCP listener */
    BIF_NET_ACCEPT = 11,/* net_accept(handle): accept connection */
    BIF_NET_READLINE = 12, /* net_readline(handle): read line */
    BIF_NET_WRITE = 13, /* net_write(handle, str): send data */
    BIF_NET_CLOSE = 14, /* net_close(handle): close handle */
    BIF_JSON_ENCODE = 16, /* json_encode(val): 值→JSON 字符串 */
    BIF_JSON_DECODE = 17, /* json_decode(str): JSON 字符串→值 */
    BIF_FILE_READ   = 18, /* file_read(path): 读取整个文件→字符串 */
    BIF_FILE_WRITE  = 19, /* file_write(path, content): 写入字符串到文件 */
    BIF_FILE_EXISTS = 20, /* file_exists(path): 检查文件是否存在→bool */
    BIF_STR_AT      = 21, /* str_at(s, idx): 取字符串第 idx 个字符 */
    BIF_STR_SUB     = 22, /* str_sub(s, start, end): 取子串 [start, end) */
    BIF_STR_FIND    = 23, /* str_find(s, pattern): 查找子串位置, -1=未找到 */
    BIF_STR_CONTAINS= 24, /* str_contains(s, pattern): 检查是否包含子串 */
    BIF_STR_TRIM    = 25, /* str_trim(s): 去除前后空白 */
    BIF_STR_UPPER   = 26, /* str_upper(s): 转大写 */
    BIF_STR_LOWER   = 27, /* str_lower(s): 转小写 */
    BIF_STR_SPLIT   = 28, /* str_split(s, delimiter): 分割字符串成列表 */
    BIF_FFI_LOAD    = 29, /* ffi_load(path): 加载动态链接库，返回句柄 */
    BIF_FFI_FREE    = 30, /* ffi_free(handle): 释放动态链接库 */
    BIF_FFI_CALL    = 31, /* ffi_call(handle, name, ret_type, ...): 调用 C 函数 */
    BIF_COUNT   = 32,
} BifIndex;

/* ═══════════════════════════════════════════════
   Value — 栈上的值
   类似 JVM 的变量类型，用 type + union 实现
   ═══════════════════════════════════════════════ */
typedef enum {
    VAL_INT,
    VAL_FLOAT,
    VAL_BOOL,
    VAL_STRING,    /* owned by const pool OR dynamically allocated */
    VAL_NULL,
    VAL_FUNC,      /* 函数引用 (用于匿名字面量) */
    VAL_OBJECT,    /* 对象 */
} ValueType;

typedef struct {
    ValueType type;
    union {
        int64_t     int_val;
        double      float_val;
        uint8_t     bool_val;
        const char* str_val;
        uint32_t    func_idx;  /* 用于 VAL_FUNC */
        void*       ptr_val;   /* 用于 VAL_OBJECT */
    } data;
} Value;

/* ═══════════════════════════════════════════════
   对象 (动态键值对)
   ═══════════════════════════════════════════════ */
/* 对象键值对 */
typedef struct {
    char* key;     /* 堆分配的 key 字符串 */
    Value  value;
} ObjectEntry;

/* 简单对象 (动态键值对数组) */
typedef struct {
    ObjectEntry* entries;
    int count;
    int capacity;
} Object;

/* ═══════════════════════════════════════════════
   函数表 & 调用栈
   ═══════════════════════════════════════════════ */
#define MAX_FUNCS      256
#define MAX_CALL_DEPTH 64
#define LOCALS_MAX     256

typedef struct {
    uint32_t name_idx;      /* 字符串池索引，0xFFFFFFFF = 匿名 */
    uint32_t arg_count;     /* 参数个数 */
    uint32_t local_count;   /* 局部变量数 (含参数) */
    uint32_t code_offset;   /* 在 code 中的偏移量 */
} FuncEntry;

typedef struct {
    uint32_t  return_ip;    /* 返回后继续执行的 IP */
    int       return_sp;    /* 返回后恢复的 SP */
    Value     saved_locals[LOCALS_MAX];  /* 调用者的局部变量备份 */
    uint32_t  saved_local_count;        /* 调用者的 local_count */
} CallFrame;

/* ═══════════════════════════════════════════════
   SDVM 虚拟机实例
   ═══════════════════════════════════════════════ */
#define STACK_MAX   4096
#define STRPOOL_MAX 4096
#define CODE_MAX   (1024 * 64)    /* 最大字节码 64KB */

typedef struct {
    uint8_t*  code;           /* 字节码缓冲区 (主代码 + 所有函数代码) */
    uint32_t  code_size;      /* 字节码大小 */

    char**    strpool;        /* 字符串常量池 */
    uint32_t  strpool_count;  /* 字符串数量 */

    Value     stack[STACK_MAX];
    int       sp;             /* 栈顶指针 (-1 = 空) */

    Value     locals[LOCALS_MAX];
    int       local_count;    /* 局部变量数 */

    uint32_t  ip;             /* 指令指针 */

    int       has_error;
    char      error_msg[256];
    int       verbose;        /* 调试输出 */

    /* 动态分配的字符串堆 (用于 insert/scan 等运行时字符串) */
    char**    heap_strs;
    uint32_t  heap_str_count;
    uint32_t  heap_str_cap;

    /* 函数表 */
    FuncEntry func_table[MAX_FUNCS];
    uint32_t  func_count;

    /* 调用栈 */
    CallFrame call_stack[MAX_CALL_DEPTH];
    int       call_sp;        /* -1 = 无活动调用 */

    /* 网络套接字表 */
    uintptr_t sockets[64];    /* 套接字句柄，0 = 空闲 */
    int       socket_count;   /* 已分配套接字数 */

    Object*   objects[256];   /* 跟踪分配的对象，用于清理 */
    int       object_count;
} SDVM;

/* ═══════════════════════════════════════════════
   API
   ═══════════════════════════════════════════════ */

/* 初始化 */
void       sdvm_init(SDVM* vm);

/* 加载 .dance 字节码 (返回 0=成功, -1=失败) */
int        sdvm_load(SDVM* vm, const uint8_t* buffer, size_t size);

/* 执行 (返回 0=成功, -1=运行时错误) */
int        sdvm_run(SDVM* vm);

/* 清理 */
void       sdvm_free(SDVM* vm);

/* 动态字符串分配 */
const char* sdvm_heap_strdup(SDVM* vm, const char* src);

/* 文件工具 */
uint8_t*   sdvm_read_file(const char* path, size_t* out_size);

/* 值工具 */
void       sdvm_print_value(const Value* v);
const char* sdvm_value_type_str(const Value* v);

/* 反汇编调试 */
const char* sdvm_opname(uint8_t op);

#ifdef __cplusplus
}
#endif

#endif /* SDVM_H */
