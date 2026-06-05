/*
 * ═══════════════════════════════════════════════
 *  SDVM — 执行引擎
 *  栈式虚拟机，指令循环 + 内置函数 + 函数调用
 * ═══════════════════════════════════════════════
 */

#include "sdvm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <ctype.h>

/* ─── DEBUG: 状态打印 ────────────────────────── */
static void dump_value(const Value* v) {
    switch (v->type) {
    case VAL_INT:    printf("int(%lld)", (long long)v->data.int_val); break;
    case VAL_FLOAT:  printf("float(%g)", v->data.float_val); break;
    case VAL_BOOL:   printf("bool(%s)", v->data.bool_val ? "true" : "false"); break;
    case VAL_STRING: printf("str(\"%s\")", v->data.str_val ? v->data.str_val : ""); break;
    case VAL_NULL:   printf("null"); break;
    case VAL_FUNC:   printf("func(%u)", v->data.func_idx); break;
    default:         printf("unknown(%d)", v->type); break;
    }
}

static void debug_state(SDVM* vm, const char* label) {
    printf("  [DBG %s] sp=%d ip=%u local_count=%d\n", label, vm->sp, vm->ip, vm->local_count);
    if (vm->sp >= 0) {
        printf("  [DBG] stack[0..%d]:", vm->sp);
        for (int i = 0; i <= vm->sp && i < 20; i++) {
            printf(" ");
            dump_value(&vm->stack[i]);
        }
        printf("\n");
    } else {
        printf("  [DBG] stack: empty\n");
    }
    printf("  [DBG] locals[0..%d]:", vm->local_count > 0 ? vm->local_count - 1 : 0);
    for (int i = 0; i < vm->local_count && i < 10; i++) {
        printf(" r%d=", i);
        dump_value(&vm->locals[i]);
    }
    printf("\n");
}

#define LOG_OP(vm, fmt, ...) do { \
    printf("; [%04u] ", (vm)->ip - 1); \
    printf(fmt, ##__VA_ARGS__); \
    printf("\n"); \
} while(0)

/* ─── 字节序工具 (小端) ──────────────────────── */
static uint32_t read_u32(const uint8_t* p) {
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}
static int32_t read_i32(const uint8_t* p) {
    return (int32_t)read_u32(p);
}
static double read_f64(const uint8_t* p) {
    double v;
    uint64_t bits = (uint64_t)p[0]
                  | ((uint64_t)p[1] << 8)
                  | ((uint64_t)p[2] << 16)
                  | ((uint64_t)p[3] << 24)
                  | ((uint64_t)p[4] << 32)
                  | ((uint64_t)p[5] << 40)
                  | ((uint64_t)p[6] << 48)
                  | ((uint64_t)p[7] << 56);
    memcpy(&v, &bits, 8);
    return v;
}

/* ─── 栈操作宏 ───────────────────────────────── */
#define PUSH(v) do { \
    if (vm->sp >= STACK_MAX - 1) { \
        snprintf(vm->error_msg, sizeof(vm->error_msg), "栈溢出"); \
        vm->has_error = 1; return -1; \
    } \
    vm->stack[++vm->sp] = (v); \
} while(0)

#define POP(v) do { \
    if (vm->sp < 0) { \
        snprintf(vm->error_msg, sizeof(vm->error_msg), "栈下溢"); \
        vm->has_error = 1; return -1; \
    } \
    (v) = vm->stack[vm->sp--]; \
} while(0)

#define PEEK() vm->stack[vm->sp]

/* ─── 值工具函数 ─────────────────────────────── */

void sdvm_print_value(const Value* v) {
    switch (v->type) {
    case VAL_INT:    printf("%lld", (long long)v->data.int_val); break;
    case VAL_FLOAT:
        if (v->data.float_val == (double)(int64_t)v->data.float_val)
            printf("%.1f", v->data.float_val);
        else
            printf("%g", v->data.float_val);
        break;
    case VAL_BOOL:   printf(v->data.bool_val ? "true" : "false"); break;
    case VAL_STRING: printf("%s", v->data.str_val ? v->data.str_val : ""); break;
    case VAL_NULL:   printf("null"); break;
    case VAL_FUNC:   printf("<func:%u>", v->data.func_idx); break;
    }
}

const char* sdvm_value_type_str(const Value* v) {
    switch (v->type) {
    case VAL_INT:    return "int";
    case VAL_FLOAT:  return "float";
    case VAL_BOOL:   return "bool";
    case VAL_STRING: return "str";
    case VAL_NULL:   return "null";
    case VAL_FUNC:   return "func";
    default:         return "unknown";
    }
}

const char* sdvm_opname(uint8_t op) {
    switch (op) {
    case OP_NOP:    return "NOP";
    case OP_ICONST: return "ICONST";
    case OP_FCONST: return "FCONST";
    case OP_SCONST: return "SCONST";
    case OP_BCONST: return "BCONST";
    case OP_NULL:   return "NULL";
    case OP_DUP:    return "DUP";
    case OP_POP:    return "POP";
    case OP_LOAD:   return "LOAD";
    case OP_STORE:  return "STORE";
    case OP_ADD:    return "ADD";
    case OP_SUB:    return "SUB";
    case OP_MUL:    return "MUL";
    case OP_DIV:    return "DIV";
    case OP_MOD:    return "MOD";
    case OP_NEG:    return "NEG";
    case OP_EQ:     return "EQ";
    case OP_NE:     return "NE";
    case OP_LT:     return "LT";
    case OP_GT:     return "GT";
    case OP_LE:     return "LE";
    case OP_GE:     return "GE";
    case OP_NOT:    return "NOT";
    case OP_JMP:    return "JMP";
    case OP_JIF:    return "JIF";
    case OP_BIF:    return "BIF";
    case OP_RET:    return "RET";
    case OP_HALT:   return "HALT";
    case OP_CALL:   return "CALL";
    case OP_ANON:   return "ANON";
    case OP_CALLR:  return "CALLR";
    case OP_PRINT:  return "PRINT";
    case OP_SCAN:   return "SCAN";
    default:        return "???";
    }
}

/* ─── 类型转换辅助 ───────────────────────────── */
static double val_to_double(const Value* v) {
    switch (v->type) {
    case VAL_INT:    return (double)v->data.int_val;
    case VAL_FLOAT:  return v->data.float_val;
    case VAL_BOOL:   return v->data.bool_val ? 1.0 : 0.0;
    default:         return 0.0;
    }
}
static int64_t val_to_int64(const Value* v) {
    switch (v->type) {
    case VAL_INT:    return v->data.int_val;
    case VAL_FLOAT:  return (int64_t)v->data.float_val;
    case VAL_BOOL:   return v->data.bool_val ? 1 : 0;
    default:         return 0;
    }
}
static int val_is_number(const Value* v) {
    return v->type == VAL_INT || v->type == VAL_FLOAT;
}

/* ─── 取数辅助：从 ip 读取操作数 ──────────────── */
#define FETCH_U32()   read_u32(vm->code + vm->ip)
#define FETCH_I32()   read_i32(vm->code + vm->ip)
#define FETCH_F64()   read_f64(vm->code + vm->ip)
#define FETCH_U8()    vm->code[vm->ip]
#define ADVANCE(n)    vm->ip += (n)

/* ═══════════════════════════════════════════════
   API 实现
   ═══════════════════════════════════════════════ */

void sdvm_init(SDVM* vm) {
    memset(vm, 0, sizeof(SDVM));
    vm->sp = -1;
    vm->call_sp = -1;
}

/* ─── 加载 .dance 文件 (支持 v1 和 v2) ───────── */
int sdvm_load(SDVM* vm, const uint8_t* buffer, size_t size) {
    uint32_t off = 0;

    if (size < 12) {
        snprintf(vm->error_msg, sizeof(vm->error_msg),
                 ".dance 文件过短 (%zu bytes)", size);
        return -1;
    }

    /* 魔数: "SDNC" */
    if (buffer[0] != 'S' || buffer[1] != 'D' ||
        buffer[2] != 'N' || buffer[3] != 'C') {
        snprintf(vm->error_msg, sizeof(vm->error_msg),
                 "无效的 .dance 文件: 魔数错误 (expected 'SDNC')");
        return -1;
    }
    off += 4;

    /* 版本 */
    uint32_t version = read_u32(buffer + off);
    if (version != 1 && version != 2) {
        snprintf(vm->error_msg, sizeof(vm->error_msg),
                 "不支持的 .dance 版本: %u (支持: 1, 2)", version);
        return -1;
    }
    off += 4;

    /* 字符串常量池 (v1 和 v2 共用) */
    if (off + 4 > size) goto truncated;
    vm->strpool_count = read_u32(buffer + off);
    off += 4;

    if (vm->strpool_count > STRPOOL_MAX) {
        snprintf(vm->error_msg, sizeof(vm->error_msg),
                 "字符串常量池过大: %u (上限 %d)",
                 vm->strpool_count, STRPOOL_MAX);
        return -1;
    }

    vm->strpool = (char**)calloc(vm->strpool_count, sizeof(char*));
    if (!vm->strpool) goto oom;

    for (uint32_t i = 0; i < vm->strpool_count; i++) {
        if (off + 4 > size) goto truncated;
        uint32_t slen = read_u32(buffer + off);
        off += 4;
        if (off + slen > size) goto truncated;

        vm->strpool[i] = (char*)malloc(slen + 1);
        if (!vm->strpool[i]) goto oom;
        memcpy(vm->strpool[i], buffer + off, slen);
        vm->strpool[i][slen] = '\0';
        off += slen;
    }

    if (version == 2) {
        /* ── v2 格式: FuncTable ───────────────── */
        if (off + 4 > size) goto truncated;
        vm->func_count = read_u32(buffer + off);
        off += 4;

        if (vm->func_count > MAX_FUNCS) {
            snprintf(vm->error_msg, sizeof(vm->error_msg),
                     "函数表过大: %u (上限 %d)", vm->func_count, MAX_FUNCS);
            return -1;
        }

        for (uint32_t i = 0; i < vm->func_count; i++) {
            if (off + 16 > size) goto truncated;
            vm->func_table[i].name_idx    = read_u32(buffer + off); off += 4;
            vm->func_table[i].arg_count   = read_u32(buffer + off); off += 4;
            vm->func_table[i].local_count = read_u32(buffer + off); off += 4;
            vm->func_table[i].code_offset = read_u32(buffer + off); off += 4;
        }
    } else {
        /* v1 格式: 没有函数表, 只有一个隐式 main */
        vm->func_count = 1;
        vm->func_table[0].name_idx    = 0xFFFFFFFF;
        vm->func_table[0].arg_count   = 0;
        vm->func_table[0].local_count = 0;
        vm->func_table[0].code_offset = 0;
    }

    /* 字节码 */
    if (off + 4 > size) goto truncated;
    vm->code_size = read_u32(buffer + off);
    off += 4;

    if (vm->code_size > CODE_MAX) {
        snprintf(vm->error_msg, sizeof(vm->error_msg),
                 "字节码过大: %u (上限 %d)", vm->code_size, CODE_MAX);
        return -1;
    }

    if (off + vm->code_size > size) goto truncated;
    vm->code = (uint8_t*)malloc(vm->code_size);
    if (!vm->code) goto oom;
    memcpy(vm->code, buffer + off, vm->code_size);

    return 0;

truncated:
    snprintf(vm->error_msg, sizeof(vm->error_msg),
             ".dance 文件截断");
    return -1;
oom:
    snprintf(vm->error_msg, sizeof(vm->error_msg),
             "内存不足");
    return -1;
}

/* ─── BIF_SEE 辅助 ──────────────────────────── */
static void bif_see(SDVM* vm, int argc) {
    Value* args = (Value*)malloc(argc * sizeof(Value));
    if (!args) return;

    for (int i = argc - 1; i >= 0; i--) {
        if (vm->sp < 0) { free(args); return; }
        args[i] = vm->stack[vm->sp--];
    }
    for (int i = 0; i < argc; i++) {
        sdvm_print_value(&args[i]);
    }
    free(args);
}

/* ─── 内置函数转发 ──────────────────────────── */
static int dispatch_bif(SDVM* vm, int bif_idx, int argc) {
    switch (bif_idx) {
    case BIF_SEE: {
        bif_see(vm, argc);
        break;
    }
    case BIF_INT: {
        if (vm->sp < 0) { snprintf(vm->error_msg, sizeof(vm->error_msg), "BIF_INT: 栈空"); vm->has_error = 1; return -1; }
        Value v = vm->stack[vm->sp--];
        Value r;
        r.type = VAL_INT;
        r.data.int_val = 0;
        int ok = 1;
        if (v.type == VAL_NULL) ok = 0;
        else if (v.type == VAL_STRING) {
            const char* s = v.data.str_val ? v.data.str_val : "";
            if ((unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF) {
                s += 3;
            }
            char* end = NULL;
            r.data.int_val = strtol(s, &end, 10);
            if (end == s) ok = 0;
        } else {
            r.data.int_val = val_to_int64(&v);
        }
        if (!ok) {
            snprintf(vm->error_msg, sizeof(vm->error_msg),
                     "类型错误：无法将 '%s' 转换为整数",
                     v.data.str_val ? v.data.str_val : "null");
            vm->has_error = 1;
            return -1;
        }
        PUSH(r);
        break;
    }
    case BIF_FLOAT: {
        if (vm->sp < 0) { snprintf(vm->error_msg, sizeof(vm->error_msg), "BIF_FLOAT: 栈空"); vm->has_error = 1; return -1; }
        Value v = vm->stack[vm->sp--];
        Value r;
        r.type = VAL_FLOAT;
        r.data.float_val = 0.0;
        int ok = 1;
        if (v.type == VAL_NULL) ok = 0;
        else if (v.type == VAL_STRING) {
            const char* s = v.data.str_val ? v.data.str_val : "";
            if ((unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF) {
                s += 3;
            }
            char* end = NULL;
            r.data.float_val = strtod(s, &end);
            if (end == s) ok = 0;
        } else {
            r.data.float_val = val_to_double(&v);
        }
        if (!ok) {
            snprintf(vm->error_msg, sizeof(vm->error_msg),
                     "类型错误：无法将 '%s' 转换为浮点数",
                     v.data.str_val ? v.data.str_val : "null");
            vm->has_error = 1;
            return -1;
        }
        PUSH(r);
        break;
    }
    case BIF_STR: {
        if (vm->sp < 0) { snprintf(vm->error_msg, sizeof(vm->error_msg), "BIF_STR: 栈空"); vm->has_error = 1; return -1; }
        Value v = vm->stack[vm->sp--];
        Value r;
        r.type = VAL_STRING;
        static char buf[128];
        switch (v.type) {
        case VAL_INT:    snprintf(buf, sizeof(buf), "%lld", (long long)v.data.int_val); break;
        case VAL_FLOAT:
            if (v.data.float_val == (double)(int64_t)v.data.float_val)
                snprintf(buf, sizeof(buf), "%.1f", v.data.float_val);
            else
                snprintf(buf, sizeof(buf), "%g", v.data.float_val);
            break;
        case VAL_BOOL:   snprintf(buf, sizeof(buf), "%s", v.data.bool_val ? "true" : "false"); break;
        case VAL_STRING: r = v; PUSH(r); return 0;
        case VAL_NULL:   snprintf(buf, sizeof(buf), "null"); break;
        }
        r.data.str_val = buf;
        PUSH(r);
        break;
    }
    case BIF_BOOL: {
        if (vm->sp < 0) { snprintf(vm->error_msg, sizeof(vm->error_msg), "BIF_BOOL: 栈空"); vm->has_error = 1; return -1; }
        Value v = vm->stack[vm->sp--];
        Value r;
        r.type = VAL_BOOL;
        switch (v.type) {
        case VAL_INT:    r.data.bool_val = v.data.int_val != 0; break;
        case VAL_FLOAT:  r.data.bool_val = v.data.float_val != 0.0; break;
        case VAL_BOOL:   r.data.bool_val = v.data.bool_val; break;
        case VAL_STRING: r.data.bool_val = v.data.str_val && v.data.str_val[0] != '\0'; break;
        case VAL_NULL:   r.data.bool_val = 0; break;
        }
        PUSH(r);
        break;
    }
    case BIF_TYPE: {
        if (vm->sp < 0) { snprintf(vm->error_msg, sizeof(vm->error_msg), "BIF_TYPE: 栈空"); vm->has_error = 1; return -1; }
        Value v = vm->stack[vm->sp--];
        Value r;
        r.type = VAL_STRING;
        static char tbuf[64];
        switch (v.type) {
        case VAL_INT:    snprintf(tbuf, sizeof(tbuf), "<class:int>"); break;
        case VAL_FLOAT:  snprintf(tbuf, sizeof(tbuf), "<class:float>"); break;
        case VAL_BOOL:   snprintf(tbuf, sizeof(tbuf), "<class:bool>"); break;
        case VAL_STRING: snprintf(tbuf, sizeof(tbuf), "<class:str>"); break;
        case VAL_NULL:   snprintf(tbuf, sizeof(tbuf), "<class:null>"); break;
        case VAL_FUNC:   snprintf(tbuf, sizeof(tbuf), "<class:func>"); break;
        }
        r.data.str_val = tbuf;
        PUSH(r);
        break;
    }
    case BIF_ID: {
        if (vm->sp < 0) { snprintf(vm->error_msg, sizeof(vm->error_msg), "BIF_ID: 栈空"); vm->has_error = 1; return -1; }
        Value v = vm->stack[vm->sp--];
        Value r;
        r.type = VAL_STRING;
        static char ibuf[64];
        snprintf(ibuf, sizeof(ibuf), "<ID:%p>", (void*)(uintptr_t)v.data.int_val);
        r.data.str_val = ibuf;
        PUSH(r);
        break;
    }
    case BIF_LEN: {
        if (vm->sp < 0) { snprintf(vm->error_msg, sizeof(vm->error_msg), "BIF_LEN: 栈空"); vm->has_error = 1; return -1; }
        Value v = vm->stack[vm->sp--];
        Value r;
        r.type = VAL_INT;
        switch (v.type) {
        case VAL_STRING: r.data.int_val = v.data.str_val ? (int64_t)strlen(v.data.str_val) : 0; break;
        default:         r.data.int_val = 0; break;
        }
        PUSH(r);
        break;
    }
    case BIF_INSERT: {
        if (argc > 0 && vm->sp >= 0) {
            vm->sp--;
        }
        Value r;
        r.type = VAL_STRING;
        char inbuf[4096];
        if (!fgets(inbuf, sizeof(inbuf), stdin)) {
            inbuf[0] = '\0';
        } else {
            size_t len = strlen(inbuf);
            while (len > 0 && (inbuf[len-1] == '\n' || inbuf[len-1] == '\r'))
                inbuf[--len] = '\0';
            if (len >= 3 && (unsigned char)inbuf[0] == 0xEF
                        && (unsigned char)inbuf[1] == 0xBB
                        && (unsigned char)inbuf[2] == 0xBF) {
                memmove(inbuf, inbuf + 3, len - 2);
            }
        }
        r.data.str_val = sdvm_heap_strdup(vm, inbuf);
        PUSH(r);
        break;
    }
    default:
        snprintf(vm->error_msg, sizeof(vm->error_msg),
                 "未知的内置函数索引: %d", bif_idx);
        vm->has_error = 1;
        break;
    }
    return 0;
}

/* ═══════════════════════════════════════════════
   主执行循环
   ═══════════════════════════════════════════════ */

int sdvm_run(SDVM* vm) {
    vm->sp = -1;
    vm->ip = 0;
    vm->has_error = 0;
    vm->call_sp = -1;

    /* 初始化主函数的局部变量 */
    if (vm->func_count > 0) {
        /* 使用 func 0 (main) 的 local_count 初始化 */
        int main_local_count = (int)vm->func_table[0].local_count;
        for (int i = 0; i < main_local_count && i < LOCALS_MAX; i++) {
            vm->locals[i].type = VAL_NULL;
        }
        vm->local_count = main_local_count;
    }

    /* 指令数限制，防止死循环 */
    int64_t max_insn = 1000000;
    int64_t insn_count = 0;

    while (vm->ip < vm->code_size) {
        if (++insn_count > max_insn) {
            snprintf(vm->error_msg, sizeof(vm->error_msg),
                     "指令数超限 (%lld) — 可能死循环, ip=%u", max_insn, vm->ip);
            vm->has_error = 1; return -1;
        }
        uint8_t op = vm->code[vm->ip++];

        if (vm->verbose) {
            printf("; [%04u] %s", vm->ip - 1, sdvm_opname(op));
            if (op == OP_ICONST) printf(" %d", FETCH_I32());
            else if (op == OP_SCONST) {
                uint32_t si = FETCH_U32();
                printf(" #%u", si);
                if (si < vm->strpool_count) printf("=\"%s\"", vm->strpool[si]);
            }
            else if (op == OP_BCONST) printf(" %u", FETCH_U8());
            else if (op == OP_LOAD || op == OP_STORE) {
                uint8_t ri = FETCH_U8();
                printf(" r%u", ri);
                if (op == OP_LOAD && ri < LOCALS_MAX) {
                    printf(" =");
                    dump_value(&vm->locals[ri]);
                }
                if (op == OP_STORE) {
                    if (vm->sp >= 0) {
                        printf(" <-");
                        dump_value(&vm->stack[vm->sp]);
                    }
                }
            }
            else if (op == OP_JMP || op == OP_JIF) {
                int32_t off = FETCH_I32();
                printf(" %+d (->%u)", off, vm->ip + 4 + off);
            }
            else if (op == OP_BIF) printf(" bif:%d args:%d", vm->code[vm->ip], vm->code[vm->ip + 1]);
            else if (op == OP_CALL) {
                uint32_t fi = FETCH_U32();
                printf(" func:%u args:%u", fi, vm->code[vm->ip + 4]);
            } else if (op == OP_ANON) {
                uint32_t fi = FETCH_U32();
                printf(" func:%u", fi);
                vm->ip -= 4;
            } else if (op == OP_CALLR) {
                printf(" args:%u", FETCH_U8());
                vm->ip -= 1;
            }
            printf("\n");
            /* DEBUG: 在关键操作后打印状态 */
            if (op == OP_STORE || op == OP_ADD || op == OP_DUP || op == OP_LT || op == OP_JIF || op == OP_JMP || op == OP_CALL || op == OP_RET) {
                debug_state(vm, sdvm_opname(op));
            }
        }

        switch (op) {

        /* ── 栈操作 ────────────────────────── */
        case OP_NOP:
            break;

        case OP_ICONST: {
            int32_t val = FETCH_I32();
            ADVANCE(4);
            Value v = { .type = VAL_INT, .data.int_val = val };
            PUSH(v);
            break;
        }

        case OP_FCONST: {
            double val = FETCH_F64();
            ADVANCE(8);
            Value v = { .type = VAL_FLOAT, .data.float_val = val };
            PUSH(v);
            break;
        }

        case OP_SCONST: {
            uint32_t idx = FETCH_U32();
            ADVANCE(4);
            if (idx >= vm->strpool_count) {
                snprintf(vm->error_msg, sizeof(vm->error_msg),
                         "字符串索引越界: %u >= %u", idx, vm->strpool_count);
                vm->has_error = 1; return -1;
            }
            Value v = { .type = VAL_STRING, .data.str_val = vm->strpool[idx] };
            PUSH(v);
            break;
        }

        case OP_BCONST: {
            uint8_t val = FETCH_U8();
            ADVANCE(1);
            Value v = { .type = VAL_BOOL, .data.bool_val = val };
            PUSH(v);
            break;
        }

        case OP_NULL: {
            Value v = { .type = VAL_NULL };
            PUSH(v);
            break;
        }

        case OP_DUP: {
            if (vm->sp < 0) {
                snprintf(vm->error_msg, sizeof(vm->error_msg), "DUP: 栈空");
                vm->has_error = 1; return -1;
            }
            Value _dup_val = vm->stack[vm->sp];
            PUSH(_dup_val);
            break;
        }

        case OP_POP: {
            if (vm->sp < 0) {
                snprintf(vm->error_msg, sizeof(vm->error_msg), "POP: 栈空");
                vm->has_error = 1; return -1;
            }
            vm->sp--;
            break;
        }

        /* ── 局部变量 ────────────────────────── */
        case OP_LOAD: {
            uint8_t idx = FETCH_U8();
            ADVANCE(1);
            if (idx >= LOCALS_MAX) {
                snprintf(vm->error_msg, sizeof(vm->error_msg),
                         "局部变量索引越界: %u", idx);
                vm->has_error = 1; return -1;
            }
            PUSH(vm->locals[idx]);
            break;
        }

        case OP_STORE: {
            uint8_t idx = FETCH_U8();
            ADVANCE(1);
            if (idx >= LOCALS_MAX) {
                snprintf(vm->error_msg, sizeof(vm->error_msg),
                         "局部变量索引越界: %u", idx);
                vm->has_error = 1; return -1;
            }
            if (vm->sp < 0) {
                snprintf(vm->error_msg, sizeof(vm->error_msg), "STORE: 栈空");
                vm->has_error = 1; return -1;
            }
            vm->locals[idx] = vm->stack[vm->sp--];
            if ((int)idx + 1 > vm->local_count)
                vm->local_count = idx + 1;
            break;
        }

        /* ── 算术运算 ────────────────────────── */
        case OP_ADD: {
            Value b, a;
            POP(b); POP(a);
            if (a.type == VAL_INT && b.type == VAL_INT) {
                Value r = { .type = VAL_INT, .data.int_val = a.data.int_val + b.data.int_val };
                PUSH(r);
            } else {
                Value r = { .type = VAL_FLOAT,
                    .data.float_val = val_to_double(&a) + val_to_double(&b) };
                PUSH(r);
            }
            break;
        }

        case OP_SUB: {
            Value b, a;
            POP(b); POP(a);
            if (a.type == VAL_INT && b.type == VAL_INT) {
                Value r = { .type = VAL_INT, .data.int_val = a.data.int_val - b.data.int_val };
                PUSH(r);
            } else {
                Value r = { .type = VAL_FLOAT,
                    .data.float_val = val_to_double(&a) - val_to_double(&b) };
                PUSH(r);
            }
            break;
        }

        case OP_MUL: {
            Value b, a;
            POP(b); POP(a);
            if (a.type == VAL_INT && b.type == VAL_INT) {
                Value r = { .type = VAL_INT, .data.int_val = a.data.int_val * b.data.int_val };
                PUSH(r);
            } else {
                Value r = { .type = VAL_FLOAT,
                    .data.float_val = val_to_double(&a) * val_to_double(&b) };
                PUSH(r);
            }
            break;
        }

        case OP_DIV: {
            Value b, a;
            POP(b); POP(a);
            if (a.type == VAL_INT && b.type == VAL_INT) {
                if (b.data.int_val == 0) {
                    snprintf(vm->error_msg, sizeof(vm->error_msg), "除法: 除数为零");
                    vm->has_error = 1; return -1;
                }
                Value r = { .type = VAL_FLOAT,
                    .data.float_val = (double)a.data.int_val / (double)b.data.int_val };
                PUSH(r);
            } else {
                double bd = val_to_double(&b);
                if (bd == 0.0) {
                    snprintf(vm->error_msg, sizeof(vm->error_msg), "除法: 除数为零");
                    vm->has_error = 1; return -1;
                }
                Value r = { .type = VAL_FLOAT,
                    .data.float_val = val_to_double(&a) / bd };
                PUSH(r);
            }
            break;
        }

        case OP_MOD: {
            Value b, a;
            POP(b); POP(a);
            if (a.type != VAL_INT || b.type != VAL_INT) {
                snprintf(vm->error_msg, sizeof(vm->error_msg), "取模: 操作数必须为整数");
                vm->has_error = 1; return -1;
            }
            if (b.data.int_val == 0) {
                snprintf(vm->error_msg, sizeof(vm->error_msg), "取模: 除数为零");
                vm->has_error = 1; return -1;
            }
            Value r = { .type = VAL_INT, .data.int_val = a.data.int_val % b.data.int_val };
            PUSH(r);
            break;
        }

        case OP_NEG: {
            Value a;
            POP(a);
            if (a.type == VAL_INT) {
                Value r = { .type = VAL_INT, .data.int_val = -a.data.int_val };
                PUSH(r);
            } else {
                Value r = { .type = VAL_FLOAT, .data.float_val = -val_to_double(&a) };
                PUSH(r);
            }
            break;
        }

        /* ── 比较运算 ────────────────────────── */
        case OP_EQ: {
            Value b, a;
            POP(b); POP(a);
            int result = 0;
            if (a.type == VAL_INT && b.type == VAL_INT)
                result = a.data.int_val == b.data.int_val;
            else if (a.type == VAL_FLOAT || b.type == VAL_FLOAT)
                result = val_to_double(&a) == val_to_double(&b);
            else if (a.type == VAL_BOOL && b.type == VAL_BOOL)
                result = a.data.bool_val == b.data.bool_val;
            else if (a.type == VAL_STRING && b.type == VAL_STRING)
                result = a.data.str_val && b.data.str_val &&
                         strcmp(a.data.str_val, b.data.str_val) == 0;
            else if (a.type == VAL_NULL && b.type == VAL_NULL)
                result = 1;
            else
                result = 0;
            Value r = { .type = VAL_BOOL, .data.bool_val = result };
            PUSH(r);
            break;
        }

        case OP_NE: {
            Value b, a;
            POP(b); POP(a);
            int result = 1;
            if (a.type == VAL_INT && b.type == VAL_INT)
                result = a.data.int_val == b.data.int_val;
            else if (a.type == VAL_FLOAT || b.type == VAL_FLOAT)
                result = val_to_double(&a) == val_to_double(&b);
            else if (a.type == VAL_BOOL && b.type == VAL_BOOL)
                result = a.data.bool_val == b.data.bool_val;
            else if (a.type == VAL_STRING && b.type == VAL_STRING)
                result = a.data.str_val && b.data.str_val &&
                         strcmp(a.data.str_val, b.data.str_val) == 0;
            else if (a.type == VAL_NULL && b.type == VAL_NULL)
                result = 1;
            else
                result = 0;
            Value r = { .type = VAL_BOOL, .data.bool_val = !result };
            PUSH(r);
            break;
        }

        case OP_LT: {
            Value b, a;
            POP(b); POP(a);
            int result;
            if (a.type == VAL_INT && b.type == VAL_INT)
                result = a.data.int_val < b.data.int_val;
            else
                result = val_to_double(&a) < val_to_double(&b);
            Value r = { .type = VAL_BOOL, .data.bool_val = result };
            PUSH(r);
            break;
        }

        case OP_GT: {
            Value b, a;
            POP(b); POP(a);
            int result;
            if (a.type == VAL_INT && b.type == VAL_INT)
                result = a.data.int_val > b.data.int_val;
            else
                result = val_to_double(&a) > val_to_double(&b);
            Value r = { .type = VAL_BOOL, .data.bool_val = result };
            PUSH(r);
            break;
        }

        case OP_LE: {
            Value b, a;
            POP(b); POP(a);
            int result;
            if (a.type == VAL_INT && b.type == VAL_INT)
                result = a.data.int_val <= b.data.int_val;
            else
                result = val_to_double(&a) <= val_to_double(&b);
            Value r = { .type = VAL_BOOL, .data.bool_val = result };
            PUSH(r);
            break;
        }

        case OP_GE: {
            Value b, a;
            POP(b); POP(a);
            int result;
            if (a.type == VAL_INT && b.type == VAL_INT)
                result = a.data.int_val >= b.data.int_val;
            else
                result = val_to_double(&a) >= val_to_double(&b);
            Value r = { .type = VAL_BOOL, .data.bool_val = result };
            PUSH(r);
            break;
        }

        /* ── 逻辑运算 ────────────────────────── */
        case OP_NOT: {
            Value a;
            POP(a);
            int truthy = 0;
            switch (a.type) {
            case VAL_INT:    truthy = a.data.int_val != 0; break;
            case VAL_FLOAT:  truthy = a.data.float_val != 0.0; break;
            case VAL_BOOL:   truthy = a.data.bool_val; break;
            case VAL_STRING: truthy = a.data.str_val && a.data.str_val[0] != '\0'; break;
            case VAL_NULL:   truthy = 0; break;
            }
            Value r = { .type = VAL_BOOL, .data.bool_val = !truthy };
            PUSH(r);
            break;
        }

        /* ── 控制流 ──────────────────────────── */
        case OP_JMP: {
            int32_t offset = FETCH_I32();
            ADVANCE(4);
            vm->ip = (uint32_t)((int32_t)vm->ip + offset);
            break;
        }

        case OP_JIF: {
            int32_t offset = FETCH_I32();
            ADVANCE(4);
            Value cond;
            POP(cond);
            int truthy = 0;
            switch (cond.type) {
            case VAL_INT:    truthy = cond.data.int_val != 0; break;
            case VAL_FLOAT:  truthy = cond.data.float_val != 0.0; break;
            case VAL_BOOL:   truthy = cond.data.bool_val; break;
            case VAL_STRING: truthy = cond.data.str_val && cond.data.str_val[0] != '\0'; break;
            case VAL_NULL:   truthy = 0; break;
            }
            if (!truthy) {
                vm->ip = (uint32_t)((int32_t)vm->ip + offset);
            }
            break;
        }

        case OP_BIF: {
            uint8_t bif_idx = FETCH_U8();
            ADVANCE(1);
            uint8_t argc = FETCH_U8();
            ADVANCE(1);
            if (bif_idx >= BIF_COUNT) {
                snprintf(vm->error_msg, sizeof(vm->error_msg),
                         "未知 BIF 索引: %d", bif_idx);
                vm->has_error = 1; return -1;
            }
            if (dispatch_bif(vm, bif_idx, argc) != 0) return -1;
            break;
        }

        case OP_RET: {
            if (vm->call_sp >= 0) {
                /* 函数返回: 恢复调用者上下文 */
                CallFrame* cf = &vm->call_stack[vm->call_sp];

                /* 保存返回值 (如果有，否则为 null) */
                Value ret_val;
                if (vm->sp >= 0) {
                    ret_val = vm->stack[vm->sp];
                    vm->sp--;
                } else {
                    ret_val.type = VAL_NULL;
                }

                /* 恢复调用者的 locals */
                memcpy(vm->locals, cf->saved_locals, sizeof(Value) * LOCALS_MAX);
                vm->local_count = (int)cf->saved_local_count;

                /* 恢复 sp 和 ip */
                vm->sp = cf->return_sp;
                vm->ip = cf->return_ip;

                /* 把返回值压回调用者的栈 */
                vm->sp++;
                vm->stack[vm->sp] = ret_val;

                vm->call_sp--;
            } else {
                /* 顶层返回: 结束程序 */
                return 0;
            }
            break;
        }

        case OP_HALT:
            return 0;

        /* ── 函数调用 ────────────────────────── */
        case OP_CALL: {
            uint32_t func_idx = FETCH_U32();
            ADVANCE(4);
            uint8_t arg_count = FETCH_U8();
            ADVANCE(1);

            if (func_idx >= vm->func_count) {
                snprintf(vm->error_msg, sizeof(vm->error_msg),
                         "函数索引越界: %u >= %u", func_idx, vm->func_count);
                vm->has_error = 1; return -1;
            }

            if (vm->call_sp >= MAX_CALL_DEPTH - 1) {
                snprintf(vm->error_msg, sizeof(vm->error_msg), "调用栈溢出");
                vm->has_error = 1; return -1;
            }

            FuncEntry* fe = &vm->func_table[func_idx];

            /* 检查参数个数 */
            if (arg_count != fe->arg_count) {
                snprintf(vm->error_msg, sizeof(vm->error_msg),
                         "函数 func[%u] 参数个数不匹配: 期望 %u, 实际 %u",
                         func_idx, fe->arg_count, arg_count);
                vm->has_error = 1; return -1;
            }

            /* 保存调用者上下文 */
            vm->call_sp++;
            CallFrame* cf = &vm->call_stack[vm->call_sp];
            cf->return_ip = vm->ip;
            cf->return_sp = vm->sp - (int)arg_count;  /* 弹出参数前的位置 */
            memcpy(cf->saved_locals, vm->locals, sizeof(Value) * LOCALS_MAX);
            cf->saved_local_count = (uint32_t)vm->local_count;

            /* 从栈拷贝参数到函数局部变量 slot 0..arg_count-1 */
            int base = vm->sp - (int)arg_count + 1;
            for (uint32_t i = 0; i < arg_count; i++) {
                vm->locals[i] = vm->stack[base + i];
            }
            /* 清空未被参数填充的局部变量 */
            for (int i = (int)arg_count; i < (int)fe->local_count && i < LOCALS_MAX; i++) {
                vm->locals[i].type = VAL_NULL;
            }

            /* 弹出参数 */
            vm->sp -= (int)arg_count;
            vm->local_count = (int)fe->local_count;

            /* 跳转到函数代码 */
            vm->ip = fe->code_offset;
            break;
        }

        case OP_ANON: {
            uint32_t func_idx = FETCH_U32();
            ADVANCE(4);

            if (func_idx >= vm->func_count) {
                snprintf(vm->error_msg, sizeof(vm->error_msg),
                         "匿名函数索引越界: %u >= %u", func_idx, vm->func_count);
                vm->has_error = 1; return -1;
            }

            Value v = { .type = VAL_FUNC, .data.func_idx = func_idx };
            PUSH(v);
            break;
        }

        case OP_CALLR: {
            uint8_t arg_count = FETCH_U8();
            ADVANCE(1);

            /* 从栈顶弹出函数引用 */
            Value func_val;
            if (vm->sp < 0) {
                snprintf(vm->error_msg, sizeof(vm->error_msg), "CALLR: 栈空，找不到函数引用");
                vm->has_error = 1; return -1;
            }
            func_val = vm->stack[vm->sp - (int)arg_count];
            /* 注意: 函数引用在 args 之下 (callee 先被编译) */
            /*编译器编译: self.compile_expression(expr.callee) 然后 for arg in args: compile_expression(arg)*/
            /* 所以栈上: [function_ref] [arg0] [arg1] ... [argN] */
            /* 我们需要把 function_ref 单独拿出来 */
            /* 实际上 function_ref 应该在 arg0 下面一个位置 */
            /* 即 stack[sp - arg_count] 是 function_ref */
            if (func_val.type != VAL_FUNC) {
                snprintf(vm->error_msg, sizeof(vm->error_msg),
                         "CALLR: 栈顶元素不是函数引用 (type=%d)", func_val.type);
                vm->has_error = 1; return -1;
            }

            uint32_t func_idx = func_val.data.func_idx;

            if (func_idx >= vm->func_count) {
                snprintf(vm->error_msg, sizeof(vm->error_msg),
                         "CALLR: 函数索引越界: %u >= %u", func_idx, vm->func_count);
                vm->has_error = 1; return -1;
            }

            if (vm->call_sp >= MAX_CALL_DEPTH - 1) {
                snprintf(vm->error_msg, sizeof(vm->error_msg), "调用栈溢出");
                vm->has_error = 1; return -1;
            }

            FuncEntry* fe = &vm->func_table[func_idx];

            if ((uint32_t)arg_count != fe->arg_count) {
                snprintf(vm->error_msg, sizeof(vm->error_msg),
                         "CALLR: 参数个数不匹配: 期望 %u, 实际 %u",
                         fe->arg_count, arg_count);
                vm->has_error = 1; return -1;
            }

            /* 保存调用者上下文 */
            vm->call_sp++;
            CallFrame* cf = &vm->call_stack[vm->call_sp];
            cf->return_ip = vm->ip;
            /* 栈上有: [func_ref] [arg0] [arg1] ... [argN] */
            /* 需要移除 func_ref 和 args */
            cf->return_sp = vm->sp - (int)arg_count - 1;
            memcpy(cf->saved_locals, vm->locals, sizeof(Value) * LOCALS_MAX);
            cf->saved_local_count = (uint32_t)vm->local_count;

            /* 拷贝参数: func_ref 在 base-1, args 在 base..sp */
            int base = vm->sp - (int)arg_count + 1;  /* arg0 的位置 */
            for (uint32_t i = 0; i < arg_count; i++) {
                vm->locals[i] = vm->stack[base + i];
            }
            for (int i = (int)arg_count; i < (int)fe->local_count && i < LOCALS_MAX; i++) {
                vm->locals[i].type = VAL_NULL;
            }

            vm->sp -= ((int)arg_count + 1);  /* 移除 func_ref + args */
            vm->local_count = (int)fe->local_count;
            vm->ip = fe->code_offset;
            break;
        }

        /* ── I/O ─────────────────────────────── */
        case OP_PRINT: {
            if (vm->sp < 0) {
                snprintf(vm->error_msg, sizeof(vm->error_msg), "PRINT: 栈空");
                vm->has_error = 1; return -1;
            }
            sdvm_print_value(&vm->stack[vm->sp]);
            vm->sp--;
            break;
        }

        case OP_SCAN: {
            char scanbuf[4096];
            if (!fgets(scanbuf, sizeof(scanbuf), stdin)) {
                scanbuf[0] = '\0';
            } else {
                size_t len = strlen(scanbuf);
                while (len > 0 && (scanbuf[len-1] == '\n' || scanbuf[len-1] == '\r'))
                    scanbuf[--len] = '\0';
            }
            Value v = { .type = VAL_STRING, .data.str_val = sdvm_heap_strdup(vm, scanbuf) };
            PUSH(v);
            break;
        }

        default:
            snprintf(vm->error_msg, sizeof(vm->error_msg),
                     "未知操作码: 0x%02X @ %u", op, vm->ip - 1);
            vm->has_error = 1;
            return -1;
        }
    }

    return 0;
}

#undef PUSH
#undef POP
#undef PEEK
#undef FETCH_U32
#undef FETCH_I32
#undef FETCH_F64
#undef FETCH_U8
#undef ADVANCE

/* ═══════════════════════════════════════════════
   文件工具
   ═══════════════════════════════════════════════ */

uint8_t* sdvm_read_file(const char* path, size_t* out_size) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return NULL; }

    uint8_t* buf = (uint8_t*)malloc((size_t)sz);
    if (!buf) { fclose(f); return NULL; }

    size_t nread = fread(buf, 1, (size_t)sz, f);
    fclose(f);

    if ((long)nread != sz) { free(buf); return NULL; }
    *out_size = (size_t)sz;
    return buf;
}

/* ─── 动态字符串堆管理 ─────────────────────────── */
const char* sdvm_heap_strdup(SDVM* vm, const char* src) {
    if (!src) src = "";
    size_t len = strlen(src);
    char* copy = (char*)malloc(len + 1);
    if (!copy) return NULL;
    memcpy(copy, src, len + 1);

    if (vm->heap_str_count >= vm->heap_str_cap) {
        uint32_t new_cap = vm->heap_str_cap ? vm->heap_str_cap * 2 : 16;
        char** new_arr = (char**)realloc(vm->heap_strs, new_cap * sizeof(char*));
        if (!new_arr) { free(copy); return NULL; }
        vm->heap_strs = new_arr;
        vm->heap_str_cap = new_cap;
    }
    vm->heap_strs[vm->heap_str_count++] = copy;
    return copy;
}

void sdvm_free(SDVM* vm) {
    free(vm->code);
    vm->code = NULL;

    if (vm->strpool) {
        for (uint32_t i = 0; i < vm->strpool_count; i++) {
            free(vm->strpool[i]);
        }
        free(vm->strpool);
        vm->strpool = NULL;
    }

    if (vm->heap_strs) {
        for (uint32_t i = 0; i < vm->heap_str_count; i++) {
            free(vm->heap_strs[i]);
        }
        free(vm->heap_strs);
        vm->heap_strs = NULL;
        vm->heap_str_count = 0;
        vm->heap_str_cap = 0;
    }

    sdvm_init(vm);
}
