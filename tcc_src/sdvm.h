/*
 *  SDVM -- Star Dance Virtual Machine
 *  Version 2: function calls + anonymous functions
 */

#ifndef SDVM_H
#define SDVM_H

#ifdef _MSC_VER
#include <stdint.h>
#include <stddef.h>
#else
/* TCC minimal type definitions */
typedef signed char        int8_t;
typedef unsigned char      uint8_t;
typedef short              int16_t;
typedef unsigned short     uint16_t;
typedef int                int32_t;
typedef unsigned int       uint32_t;
typedef long long          int64_t;
typedef unsigned long long uint64_t;
typedef unsigned int       size_t;
typedef long               intptr_t;
typedef unsigned long      uintptr_t;
#ifndef NULL
#define NULL ((void*)0)
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ── Opcodes ────────────────────────────────── */
typedef enum {
    OP_NOP       = 0x00,
    OP_ICONST    = 0x01,
    OP_FCONST    = 0x02,
    OP_SCONST    = 0x03,
    OP_BCONST    = 0x04,
    OP_NULL      = 0x05,
    OP_DUP       = 0x06,
    OP_POP       = 0x07,
    OP_LOAD      = 0x10,
    OP_STORE     = 0x11,
    OP_ADD       = 0x20,
    OP_SUB       = 0x21,
    OP_MUL       = 0x22,
    OP_DIV       = 0x23,
    OP_MOD       = 0x24,
    OP_NEG       = 0x25,
    OP_EQ        = 0x30,
    OP_NE        = 0x31,
    OP_LT        = 0x32,
    OP_GT        = 0x33,
    OP_LE        = 0x34,
    OP_GE        = 0x35,
    OP_NOT       = 0x38,
    OP_JMP       = 0x40,
    OP_JIF       = 0x41,
    OP_BIF       = 0x42,
    OP_RET       = 0x43,
    OP_HALT      = 0x44,
    OP_CALL      = 0x50,
    OP_ANON      = 0x51,
    OP_CALLR     = 0x52,
    /* 对象操作 0x60-0x6F */
    OP_NEWOBJ    = 0x60,  /* +1: num_pairs, 从栈取 key-value 对创建对象 */
    OP_GETATTR   = 0x61,  /* +4: strpool_idx, 读取对象属性 */
    OP_SETATTR   = 0x62,  /* +4: strpool_idx, 设置对象属性 */
    OP_PRINT     = 0x70,
    OP_SCAN      = 0x71,
} OpCode;

/* ── Built-in Function Index ────────────────── */
typedef enum {
    BIF_SEE        = 0,
    BIF_INT        = 1,
    BIF_FLOAT      = 2,
    BIF_STR        = 3,
    BIF_BOOL       = 4,
    BIF_TYPE       = 5,
    BIF_ID         = 6,
    BIF_LEN        = 7,
    BIF_INSERT     = 8,
    BIF_NET_START   = 10,
    BIF_NET_ACCEPT  = 11,
    BIF_NET_READLINE= 12,
    BIF_NET_WRITE   = 13,
    BIF_NET_CLOSE   = 14,
    BIF_JSON_ENCODE = 16,   /* json_encode(val): 值→JSON 字符串 */
    BIF_JSON_DECODE = 17,   /* json_decode(str): JSON 字符串→值 */
    BIF_FILE_READ   = 18,   /* file_read(path): 读取整个文件→字符串 */
    BIF_FILE_WRITE  = 19,   /* file_write(path, content): 写入字符串到文件 */
    BIF_FILE_EXISTS = 20,   /* file_exists(path): 检查文件是否存在→bool */
    BIF_COUNT      = 21,
} BifIndex;

/* ── Value ──────────────────────────────────── */
typedef enum {
    VAL_INT, VAL_FLOAT, VAL_BOOL, VAL_STRING, VAL_NULL, VAL_FUNC, VAL_OBJECT
} ValueType;

typedef struct Value {
    ValueType type;
    union {
        int64_t     int_val;
        double      float_val;
        uint8_t     bool_val;
        const char* str_val;
        uint32_t    func_idx;
        void*       ptr_val;   /* for VAL_OBJECT → Object* */
    } data;
} Value;

/* ── 对象结构 ────────────────────────────────── */
typedef struct Value Value;

/* 对象键值对: key 是堆分配的 C 字符串 */
typedef struct {
    char* key;
    Value value;
} ObjectEntry;

/* 简单对象: 动态键值对数组 */
typedef struct {
    ObjectEntry* entries;
    int count;
    int capacity;
} Object;

/* ── Function Table & Call Stack ───────────── */
#define MAX_FUNCS      256
#define MAX_CALL_DEPTH 64
#define LOCALS_MAX     256

typedef struct {
    uint32_t name_idx;
    uint32_t arg_count;
    uint32_t local_count;
    uint32_t code_offset;
} FuncEntry;

typedef struct {
    uint32_t  return_ip;
    int       return_sp;
    Value     saved_locals[LOCALS_MAX];
    uint32_t  saved_local_count;
} CallFrame;

/* ── VM Instance ──────────────────────────── */
#define STACK_MAX   4096
#define STRPOOL_MAX 4096
#define CODE_MAX   (1024 * 64)

typedef struct {
    uint8_t*  code;
    uint32_t  code_size;
    char**    strpool;
    uint32_t  strpool_count;
    Value     stack[STACK_MAX];
    int       sp;
    Value     locals[LOCALS_MAX];
    int       local_count;
    uint32_t  ip;
    int       has_error;
    char      error_msg[256];
    int       verbose;
    char**    heap_strs;
    uint32_t  heap_str_count;
    uint32_t  heap_str_cap;
    FuncEntry func_table[MAX_FUNCS];
    uint32_t  func_count;
    CallFrame call_stack[MAX_CALL_DEPTH];
    int       call_sp;
    uintptr_t sockets[64];
    int       socket_count;
    Object*   objects[256];
    int       object_count;
} SDVM;

/* ── API ────────────────────────────────────── */
void       sdvm_init(SDVM* vm);
int        sdvm_load(SDVM* vm, const uint8_t* buffer, size_t size);
int        sdvm_run(SDVM* vm);
void       sdvm_free(SDVM* vm);
const char* sdvm_heap_strdup(SDVM* vm, const char* src);
uint8_t*   sdvm_read_file(const char* path, size_t* out_size);
void       sdvm_print_value(const Value* v);
const char* sdvm_value_type_str(const Value* v);
const char* sdvm_opname(uint8_t op);

#ifdef __cplusplus
}
#endif

#endif /* SDVM_H */
