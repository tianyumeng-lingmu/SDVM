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

/* 兼容 TCC 没有 strtoll 的问题 */
#if defined(__TINYC__) && !defined(strtoll)
#define strtoll _strtoi64
#endif

/* 兼容 TCC 缺少 _strdup */
#if defined(__TINYC__)
static char* tcc_strdup(const char* s) {
    size_t len = strlen(s);
    char* d = (char*)malloc(len + 1);
    if (d) memcpy(d, s, len + 1);
    return d;
}
#define _strdup tcc_strdup
#endif

/* copy包的临时存储区 */
static Value s_copy_temp = { .type = VAL_NULL };

/* ─── Win32 API（用于 FFI）— 在本地 winsock2.h 之前引入 ── */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define FFI_HAS_WINDOWS_H  /* 告知本地 winsock2.h 跳过已定义的 OVERLAPPED */
#endif

/* ─── Winsock 网络支持 ────────────────────── */
#include <winsock2.h>
#include <ws2tcpip.h>
/* TCC uses ws2_32.def for linking instead of #pragma comment(lib) */

/* ─── Winsock 初始化 (单例) ───────────────── */
static int _winsock_inited = 0;

/* ─── FFI 动态库加载 ────────────────────── */
/* FFI 调用函数指针类型 (最多 8 个 int64 参数) */
typedef int64_t (*ffi_fn_0)();
typedef int64_t (*ffi_fn_1)(int64_t);
typedef int64_t (*ffi_fn_2)(int64_t, int64_t);
typedef int64_t (*ffi_fn_3)(int64_t, int64_t, int64_t);
typedef int64_t (*ffi_fn_4)(int64_t, int64_t, int64_t, int64_t);
typedef int64_t (*ffi_fn_5)(int64_t, int64_t, int64_t, int64_t, int64_t);
typedef int64_t (*ffi_fn_6)(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);
typedef int64_t (*ffi_fn_7)(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);
typedef int64_t (*ffi_fn_8)(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);

/* ─── FFI 函数指针缓存 ────────────────────── */
static int ffi_cache_find(SDVM* vm, int64_t handle, const char* name) {
    for (int i = 0; i < FFI_CACHE_SIZE; i++) {
        if (vm->ffi_cache[i].handle == handle &&
            vm->ffi_cache[i].func_name &&
            strcmp(vm->ffi_cache[i].func_name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int ffi_cache_store(SDVM* vm, int64_t handle, const char* name, void* ptr) {
    /* 先找空槽 */
    for (int i = 0; i < FFI_CACHE_SIZE; i++) {
        if (vm->ffi_cache[i].handle == 0 && !vm->ffi_cache[i].func_name) {
            vm->ffi_cache[i].handle = handle;
            vm->ffi_cache[i].func_name = (char*)malloc(strlen(name) + 1);
            if (vm->ffi_cache[i].func_name) strcpy(vm->ffi_cache[i].func_name, name);
            vm->ffi_cache[i].func_ptr = ptr;
            return i;
        }
    }
    /* 满→覆盖最后一槽 (LRU 简化) */
    free(vm->ffi_cache[FFI_CACHE_SIZE - 1].func_name);
    vm->ffi_cache[FFI_CACHE_SIZE - 1].handle = handle;
    vm->ffi_cache[FFI_CACHE_SIZE - 1].func_name = (char*)malloc(strlen(name) + 1);
    if (vm->ffi_cache[FFI_CACHE_SIZE - 1].func_name) strcpy(vm->ffi_cache[FFI_CACHE_SIZE - 1].func_name, name);
    vm->ffi_cache[FFI_CACHE_SIZE - 1].func_ptr = ptr;
    return FFI_CACHE_SIZE - 1;
}

static void ffi_cache_clear_handle(SDVM* vm, int64_t handle) {
    for (int i = 0; i < FFI_CACHE_SIZE; i++) {
        if (vm->ffi_cache[i].handle == handle) {
            free(vm->ffi_cache[i].func_name);
            vm->ffi_cache[i].handle = 0;
            vm->ffi_cache[i].func_name = NULL;
            vm->ffi_cache[i].func_ptr = NULL;
        }
    }
}

static int ensure_winsock(void) {
    if (_winsock_inited) return 0;
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return -1;
    _winsock_inited = 1;
    return 0;
}

/* 分配新的套接字句柄 ID */
static int alloc_socket_id(SDVM* vm, uintptr_t sock) {
    for (int i = 0; i < 64; i++) {
        if (vm->sockets[i] == 0) {
            vm->sockets[i] = sock;
            vm->socket_count++;
            return i;
        }
    }
    return -1; /* 已满 */
}

/* 释放套接字句柄 ID */
static void free_socket_id(SDVM* vm, int id) {
    if (id >= 0 && id < 64 && vm->sockets[id] != 0) {
        closesocket((SOCKET)vm->sockets[id]);
        vm->sockets[id] = 0;
        vm->socket_count--;
    }
}

/* ─── Object 管理 ────────────────────────────── */
static Object* obj_create(SDVM* vm) {
    Object* obj = (Object*)malloc(sizeof(Object));
    if (!obj) return NULL;
    obj->entries = NULL;
    obj->count = 0;
    obj->capacity = 0;
    if (vm->object_count < 256)
        vm->objects[vm->object_count++] = obj;
    return obj;
}

static void obj_free(Object* obj) {
    if (!obj) return;
    for (int i = 0; i < obj->count; i++)
        free(obj->entries[i].key);
    free(obj->entries);
    free(obj);
}

static int obj_find(const Object* obj, const char* key) {
    for (int i = 0; i < obj->count; i++)
        if (strcmp(obj->entries[i].key, key) == 0) return i;
    return -1;
}

static void obj_set(Object* obj, const char* key, Value val) {
    int idx = obj_find(obj, key);
    if (idx >= 0) {
        obj->entries[idx].value = val;
        return;
    }
    if (obj->count >= obj->capacity) {
        int new_cap = obj->capacity ? obj->capacity * 2 : 4;
        ObjectEntry* new_entries = (ObjectEntry*)realloc(obj->entries, new_cap * sizeof(ObjectEntry));
        if (!new_entries) return;
        obj->entries = new_entries;
        obj->capacity = new_cap;
    }
    obj->entries[obj->count].key = _strdup(key);
    obj->entries[obj->count].value = val;
    obj->count++;
}

static Value obj_get(const Object* obj, const char* key) {
    int idx = obj_find(obj, key);
    if (idx >= 0) return obj->entries[idx].value;
    Value null_val;
    null_val.type = VAL_NULL;
    return null_val;
}

static void obj_remove(Object* obj, const char* key) {
    int idx = obj_find(obj, key);
    if (idx < 0) return;
    free(obj->entries[idx].key);
    for (int i = idx; i < obj->count - 1; i++)
        obj->entries[i] = obj->entries[i + 1];
    obj->count--;
}

static Object* obj_copy(SDVM* vm, const Object* src) {
    Object* obj = obj_create(vm);
    if (!obj) return NULL;
    for (int i = 0; i < src->count; i++)
        obj_set(obj, src->entries[i].key, src->entries[i].value);
    return obj;
}

static Object* obj_deep_copy(SDVM* vm, const Object* src) {
    Object* obj = obj_create(vm);
    if (!obj) return NULL;
    for (int i = 0; i < src->count; i++) {
        Value val = src->entries[i].value;
        if (val.type == VAL_OBJECT) {
            Object* sub = obj_deep_copy(vm, (Object*)val.data.ptr_val);
            val.data.ptr_val = sub;
        }
        obj_set(obj, src->entries[i].key, val);
    }
    return obj;
}

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
    case VAL_OBJECT: {
        Object* o = (Object*)v->data.ptr_val;
        printf("{");
        for (int i = 0; i < (o ? o->count : 0); i++) {
            if (i > 0) printf(", ");
            printf("\"%s\": ", o->entries[i].key);
            sdvm_print_value(&o->entries[i].value);
        }
        printf("}");
        break;
    }
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
    case VAL_OBJECT: return "object";
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
    case OP_IDIV:   return "IDIV";
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
    case OP_NEWOBJ:  return "NEWOBJ";
    case OP_GETATTR: return "GETATTR";
    case OP_SETATTR: return "SETATTR";
    case OP_GETINDEX: return "GETINDEX";
    case OP_SETINDEX: return "SETINDEX";
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

/* ─── JSON 编码辅助 ──────────────────────────── */
static void json_encode_puts(char** buf, size_t* cap, size_t* pos, const char* s, size_t len) {
    if (*pos + len >= *cap) {
        *cap = (*cap ? *cap * 2 : 256);
        if (*pos + len >= *cap) *cap = *pos + len + 256;
        char* nb = (char*)realloc(*buf, *cap);
        if (!nb) return;
        *buf = nb;
    }
    memcpy(*buf + *pos, s, len);
    *pos += len;
}

static void json_encode_value(const Value* v, char** buf, size_t* cap, size_t* pos) {
    char tmp[128];
    switch (v->type) {
    case VAL_NULL:
        json_encode_puts(buf, cap, pos, "null", 4);
        break;
    case VAL_INT:
        snprintf(tmp, sizeof(tmp), "%lld", (long long)v->data.int_val);
        json_encode_puts(buf, cap, pos, tmp, strlen(tmp));
        break;
    case VAL_FLOAT: {
        double d = v->data.float_val;
        if (d == (double)(int64_t)d)
            snprintf(tmp, sizeof(tmp), "%.1f", d);
        else
            snprintf(tmp, sizeof(tmp), "%g", d);
        json_encode_puts(buf, cap, pos, tmp, strlen(tmp));
        break;
    }
    case VAL_BOOL:
        json_encode_puts(buf, cap, pos, v->data.bool_val ? "true" : "false",
                         v->data.bool_val ? 4 : 5);
        break;
    case VAL_STRING: {
        const char* s = v->data.str_val ? v->data.str_val : "";
        json_encode_puts(buf, cap, pos, "\"", 1);
        for (; *s; s++) {
            if (*s == '"' || *s == '\\') {
                char esc[2] = { '\\', *s };
                json_encode_puts(buf, cap, pos, esc, 2);
            } else if (*s == '\n') {
                json_encode_puts(buf, cap, pos, "\\n", 2);
            } else if (*s == '\r') {
                json_encode_puts(buf, cap, pos, "\\r", 2);
            } else if (*s == '\t') {
                json_encode_puts(buf, cap, pos, "\\t", 2);
            } else if ((unsigned char)*s < 32) {
                snprintf(tmp, sizeof(tmp), "\\u%04x", (unsigned char)*s);
                json_encode_puts(buf, cap, pos, tmp, 6);
            } else {
                json_encode_puts(buf, cap, pos, s, 1);
            }
        }
        json_encode_puts(buf, cap, pos, "\"", 1);
        break;
    }
    case VAL_OBJECT: {
        Object* o = (Object*)v->data.ptr_val;
        json_encode_puts(buf, cap, pos, "{", 1);
        for (int i = 0; i < (o ? o->count : 0); i++) {
            if (i > 0) json_encode_puts(buf, cap, pos, ",", 1);
            /* key (always string in JSON) */
            json_encode_puts(buf, cap, pos, "\"", 1);
            const char* k = o->entries[i].key ? o->entries[i].key : "";
            for (; *k; k++) {
                if (*k == '"' || *k == '\\') {
                    char esc[2] = { '\\', *k };
                    json_encode_puts(buf, cap, pos, esc, 2);
                } else {
                    json_encode_puts(buf, cap, pos, k, 1);
                }
            }
            json_encode_puts(buf, cap, pos, "\":", 2);
            json_encode_value(&o->entries[i].value, buf, cap, pos);
        }
        json_encode_puts(buf, cap, pos, "}", 1);
        break;
    }
    case VAL_FUNC:
        json_encode_puts(buf, cap, pos, "null", 4);
        break;
    }
}

/* ─── JSON 解码辅助（递归） ──────────────────── */
static const char* json_skip_ws(const char* p) {
    while (*p && (unsigned char)*p <= 32) p++;
    return p;
}

static Value json_decode_value(SDVM* vm, const char** pp) {
    Value result; result.type = VAL_NULL;
    const char* p = json_skip_ws(*pp);
    if (!*p) { *pp = p; return result; }

    if (*p == '{') {
        /* 解析对象 */
        p++; /* skip { */
        Object* obj = obj_create(vm);
        p = json_skip_ws(p);
        if (*p != '}') {
            while (1) {
                p = json_skip_ws(p);
                /* 解析字符串 key */
                if (*p != '"') break;
                p++; /* skip opening " */
                /* 改用动态构建 key */
                char key_buf[4096];
                int ki = 0;
                while (*p && *p != '"' && ki < (int)sizeof(key_buf)-1) {
                    if (*p == '\\') { p++; if (*p) key_buf[ki++] = *p++; }
                    else key_buf[ki++] = *p++;
                }
                key_buf[ki] = '\0';
                if (*p == '"') p++; /* skip closing " */
                p = json_skip_ws(p);
                if (*p != ':') break;
                p++; /* skip : */
                Value val = json_decode_value(vm, &p);
                obj_set(obj, key_buf, val);
                p = json_skip_ws(p);
                if (*p == ',') p++;
                else break;
            }
        }
        if (*p == '}') p++;
        result.type = VAL_OBJECT;
        result.data.ptr_val = obj;
        *pp = p;
        return result;
    }

    if (*p == '"') {
        p++;
        char str_buf[4096];
        int si = 0;
        while (*p && *p != '"' && si < (int)sizeof(str_buf)-1) {
            if (*p == '\\') {
                p++;
                switch (*p) {
                    case '"': str_buf[si++] = '"'; p++; break;
                    case '\\': str_buf[si++] = '\\'; p++; break;
                    case '/': str_buf[si++] = '/'; p++; break;
                    case 'b': str_buf[si++] = '\b'; p++; break;
                    case 'f': str_buf[si++] = '\f'; p++; break;
                    case 'n': str_buf[si++] = '\n'; p++; break;
                    case 'r': str_buf[si++] = '\r'; p++; break;
                    case 't': str_buf[si++] = '\t'; p++; break;
                    case 'u': {
                        /* 简单 unicode 转义 */
                        if (*(p+1) && *(p+2) && *(p+3) && *(p+4)) {
                            unsigned int code;
                            if (sscanf(p+1, "%4x", &code) == 1) {
                                if (code < 128) str_buf[si++] = (char)code;
                                else str_buf[si++] = '?';
                                p += 5;
                            } else { p++; str_buf[si++] = '?'; }
                        } else { p++; }
                        break;
                    }
                    default: str_buf[si++] = *p; p++; break;
                }
            } else {
                str_buf[si++] = *p++;
            }
        }
        str_buf[si] = '\0';
        if (*p == '"') p++;
        result.type = VAL_STRING;
        result.data.str_val = sdvm_heap_strdup(vm, str_buf);
        *pp = p;
        return result;
    }

    if (*p == 't' && strncmp(p, "true", 4) == 0) {
        result.type = VAL_BOOL; result.data.bool_val = 1;
        *pp = p + 4; return result;
    }
    if (*p == 'f' && strncmp(p, "false", 5) == 0) {
        result.type = VAL_BOOL; result.data.bool_val = 0;
        *pp = p + 5; return result;
    }
    if (*p == 'n' && strncmp(p, "null", 4) == 0) {
        *pp = p + 4; return result;
    }

    /* 尝试解析数字 */
    char* end = NULL;
    long long inum = strtoll(p, &end, 10);
    if (end != p) {
        /* 检查是否有小数点或科学计数法 */
        const char* check = end;
        if (*check == '.' || *check == 'e' || *check == 'E') {
            double fnum = strtod(p, &end);
            if (end != p) {
                result.type = VAL_FLOAT; result.data.float_val = fnum;
                *pp = end; return result;
            }
        }
        result.type = VAL_INT; result.data.int_val = inum;
        *pp = end; return result;
    }

    *pp = p;
    return result;
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
            r.data.int_val = strtoll(s, &end, 10);
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
        case VAL_OBJECT: r.data.int_val = (int64_t)((Object*)v.data.ptr_val)->count; break;
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

    /* ─── BIF_NET_START(port) → handle ───── */
    case BIF_NET_START: {
        if (argc != 1 || vm->sp < 0) {
            snprintf(vm->error_msg, sizeof(vm->error_msg),
                     "net_start 需要 1 个参数 (port)");
            vm->has_error = 1; return -1;
        }
        Value port_v = vm->stack[vm->sp--];
        if (port_v.type != VAL_INT) {
            snprintf(vm->error_msg, sizeof(vm->error_msg),
                     "net_start: port 参数必须为整数");
            vm->has_error = 1; return -1;
        }
        if (ensure_winsock() != 0) {
            snprintf(vm->error_msg, sizeof(vm->error_msg),
                     "net_start: Winsock 初始化失败");
            vm->has_error = 1; return -1;
        }

        int port = (int)port_v.data.int_val;
        SOCKET server = socket(AF_INET, SOCK_STREAM, 0);
        if (server == INVALID_SOCKET) {
            snprintf(vm->error_msg, sizeof(vm->error_msg),
                     "net_start: 创建 socket 失败 (error=%d)", WSAGetLastError());
            vm->has_error = 1; return -1;
        }

        int opt = 1;
        setsockopt(server, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons((unsigned short)port);
        addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(server, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            snprintf(vm->error_msg, sizeof(vm->error_msg),
                     "net_start: bind 失败 (port=%d, error=%d)", port, WSAGetLastError());
            closesocket(server);
            vm->has_error = 1; return -1;
        }
        if (listen(server, SOMAXCONN) == SOCKET_ERROR) {
            snprintf(vm->error_msg, sizeof(vm->error_msg),
                     "net_start: listen 失败 (error=%d)", WSAGetLastError());
            closesocket(server);
            vm->has_error = 1; return -1;
        }

        int h = alloc_socket_id(vm, (uintptr_t)server);
        if (h < 0) {
            closesocket(server);
            snprintf(vm->error_msg, sizeof(vm->error_msg),
                     "net_start: 套接字表已满");
            vm->has_error = 1; return -1;
        }

        Value r;
        r.type = VAL_INT;
        r.data.int_val = h;
        PUSH(r);
        break;
    }

    /* ─── BIF_NET_ACCEPT(handle) → client_handle ───── */
    case BIF_NET_ACCEPT: {
        if (argc != 1 || vm->sp < 0) {
            snprintf(vm->error_msg, sizeof(vm->error_msg),
                     "net_accept 需要 1 个参数 (handle)");
            vm->has_error = 1; return -1;
        }
        Value h_v = vm->stack[vm->sp--];
        if (h_v.type != VAL_INT) {
            snprintf(vm->error_msg, sizeof(vm->error_msg),
                     "net_accept: handle 参数必须为整数");
            vm->has_error = 1; return -1;
        }
        int h = (int)h_v.data.int_val;
        if (h < 0 || h >= 64 || vm->sockets[h] == 0) {
            snprintf(vm->error_msg, sizeof(vm->error_msg),
                     "net_accept: 无效的 handle %d", h);
            vm->has_error = 1; return -1;
        }

        SOCKET server = (SOCKET)vm->sockets[h];
        struct sockaddr_in client_addr;
        int addrlen = sizeof(client_addr);
        SOCKET client = accept(server, (struct sockaddr*)&client_addr, &addrlen);
        if (client == INVALID_SOCKET) {
            snprintf(vm->error_msg, sizeof(vm->error_msg),
                     "net_accept: accept 失败 (error=%d)", WSAGetLastError());
            vm->has_error = 1; return -1;
        }

        int ch = alloc_socket_id(vm, (uintptr_t)client);
        if (ch < 0) {
            closesocket(client);
            snprintf(vm->error_msg, sizeof(vm->error_msg),
                     "net_accept: 套接字表已满");
            vm->has_error = 1; return -1;
        }

        Value r;
        r.type = VAL_INT;
        r.data.int_val = ch;
        PUSH(r);
        break;
    }

    /* ─── BIF_NET_READLINE(handle) → string ───── */
    case BIF_NET_READLINE: {
        if (argc != 1 || vm->sp < 0) {
            snprintf(vm->error_msg, sizeof(vm->error_msg),
                     "net_readline 需要 1 个参数 (handle)");
            vm->has_error = 1; return -1;
        }
        Value h_v = vm->stack[vm->sp--];
        if (h_v.type != VAL_INT) {
            snprintf(vm->error_msg, sizeof(vm->error_msg),
                     "net_readline: handle 参数必须为整数");
            vm->has_error = 1; return -1;
        }
        int h = (int)h_v.data.int_val;
        if (h < 0 || h >= 64 || vm->sockets[h] == 0) {
            snprintf(vm->error_msg, sizeof(vm->error_msg),
                     "net_readline: 无效的 handle %d", h);
            vm->has_error = 1; return -1;
        }

        SOCKET s = (SOCKET)vm->sockets[h];
        char buf[4096];
        size_t pos = 0;
        while (pos < sizeof(buf) - 1) {
            char c;
            int n = recv(s, &c, 1, 0);
            if (n <= 0) break;
            if (c == '\n') break;
            if (c != '\r') buf[pos++] = c;
        }
        buf[pos] = '\0';

        Value r;
        r.type = VAL_STRING;
        r.data.str_val = sdvm_heap_strdup(vm, buf);
        PUSH(r);
        break;
    }

    /* ─── BIF_NET_WRITE(handle, str) → void ───── */
    case BIF_NET_WRITE: {
        if (argc != 2 || vm->sp < 1) {
            snprintf(vm->error_msg, sizeof(vm->error_msg),
                     "net_write 需要 2 个参数 (handle, str)");
            vm->has_error = 1; return -1;
        }
        Value str_v = vm->stack[vm->sp--];
        Value h_v = vm->stack[vm->sp--];
        if (h_v.type != VAL_INT) {
            snprintf(vm->error_msg, sizeof(vm->error_msg),
                     "net_write: handle 参数必须为整数");
            vm->has_error = 1; return -1;
        }
        int h = (int)h_v.data.int_val;
        if (h < 0 || h >= 64 || vm->sockets[h] == 0) {
            snprintf(vm->error_msg, sizeof(vm->error_msg),
                     "net_write: 无效的 handle %d", h);
            vm->has_error = 1; return -1;
        }

        const char* data = (str_v.type == VAL_STRING && str_v.data.str_val)
                           ? str_v.data.str_val : "";
        SOCKET s = (SOCKET)vm->sockets[h];
        int len = (int)strlen(data);
        send(s, data, len, 0);
        /* void BIF: push null 保持栈平衡 (编译器会 emit POP) */
        { Value _null; _null.type = VAL_NULL; PUSH(_null); }
        break;
    }

    /* ─── BIF_NET_CLOSE(handle) → void ───── */
    case BIF_NET_CLOSE: {
        if (argc != 1 || vm->sp < 0) {
            snprintf(vm->error_msg, sizeof(vm->error_msg),
                     "net_close 需要 1 个参数 (handle)");
            vm->has_error = 1; return -1;
        }
        Value h_v = vm->stack[vm->sp--];
        if (h_v.type != VAL_INT) {
            snprintf(vm->error_msg, sizeof(vm->error_msg),
                     "net_close: handle 参数必须为整数");
            vm->has_error = 1; return -1;
        }
        int h = (int)h_v.data.int_val;
        free_socket_id(vm, h);
        /* void BIF: push null 保持栈平衡 (编译器会 emit POP) */
        { Value _null; _null.type = VAL_NULL; PUSH(_null); }
        break;
    }

    /* ─── JSON / 文件 BIF ────────────────── */
    case BIF_JSON_ENCODE: {
        if (argc != 1 || vm->sp < 0) {
            snprintf(vm->error_msg, sizeof(vm->error_msg), "json_encode 需要 1 个参数");
            vm->has_error = 1; return -1;
        }
        Value v = vm->stack[vm->sp--];
        char* buf = NULL;
        size_t cap = 0, pos = 0;
        json_encode_value(&v, &buf, &cap, &pos);
        Value r;
        r.type = VAL_STRING;
        r.data.str_val = buf ? sdvm_heap_strdup(vm, buf) : sdvm_heap_strdup(vm, "null");
        free(buf);
        PUSH(r);
        break;
    }

    case BIF_JSON_DECODE: {
        if (argc != 1 || vm->sp < 0) {
            snprintf(vm->error_msg, sizeof(vm->error_msg), "json_decode 需要 1 个参数(字符串)");
            vm->has_error = 1; return -1;
        }
        Value v = vm->stack[vm->sp--];
        const char* str = (v.type == VAL_STRING && v.data.str_val) ? v.data.str_val : "";
        const char* p = str;
        Value r = json_decode_value(vm, &p);
        PUSH(r);
        break;
    }

    case BIF_FILE_READ: {
        if (argc != 1 || vm->sp < 0) {
            snprintf(vm->error_msg, sizeof(vm->error_msg), "file_read 需要 1 个参数(路径)");
            vm->has_error = 1; return -1;
        }
        Value v = vm->stack[vm->sp--];
        const char* path = (v.type == VAL_STRING && v.data.str_val) ? v.data.str_val : "";
        FILE* f = fopen(path, "rb");
        if (!f) {
            Value r; r.type = VAL_NULL; PUSH(r); break;
        }
        fseek(f, 0, SEEK_END);
        long fsize = ftell(f);
        fseek(f, 0, SEEK_SET);
        char* content = (char*)malloc(fsize + 1);
        if (!content) { fclose(f); Value r; r.type = VAL_NULL; PUSH(r); break; }
        size_t rd = fread(content, 1, fsize, f);
        fclose(f);
        content[rd] = '\0';
        Value r;
        r.type = VAL_STRING;
        r.data.str_val = sdvm_heap_strdup(vm, content);
        free(content);
        PUSH(r);
        break;
    }

    case BIF_FILE_WRITE: {
        if (argc != 2 || vm->sp < 1) {
            snprintf(vm->error_msg, sizeof(vm->error_msg), "file_write 需要 2 个参数(路径, 内容)");
            vm->has_error = 1; return -1;
        }
        Value content_v = vm->stack[vm->sp--];
        Value path_v = vm->stack[vm->sp--];
        const char* path = (path_v.type == VAL_STRING && path_v.data.str_val) ? path_v.data.str_val : "";
        const char* content = (content_v.type == VAL_STRING && content_v.data.str_val) ? content_v.data.str_val : "";
        FILE* f = fopen(path, "wb");
        if (!f) {
            Value r; r.type = VAL_BOOL; r.data.bool_val = 0; PUSH(r); break;
        }
        fwrite(content, 1, strlen(content), f);
        fclose(f);
        Value r; r.type = VAL_BOOL; r.data.bool_val = 1; PUSH(r);
        break;
    }

    case BIF_FILE_EXISTS: {
        if (argc != 1 || vm->sp < 0) {
            snprintf(vm->error_msg, sizeof(vm->error_msg), "file_exists 需要 1 个参数(路径)");
            vm->has_error = 1; return -1;
        }
        Value v = vm->stack[vm->sp--];
        const char* path = (v.type == VAL_STRING && v.data.str_val) ? v.data.str_val : "";
        FILE* f = fopen(path, "rb");
        int exists = (f != NULL);
        if (f) fclose(f);
        Value r; r.type = VAL_BOOL; r.data.bool_val = exists;
        PUSH(r);
        break;
    }

    /* ─── 字符串函数 ──────────────────────── */
    case BIF_STR_AT: {
        if (argc != 2 || vm->sp < 1) {
            snprintf(vm->error_msg, sizeof(vm->error_msg), "str_at 需要 2 个参数 (str, idx)");
            vm->has_error = 1; return -1;
        }
        Value idx_v = vm->stack[vm->sp--];
        Value str_v = vm->stack[vm->sp--];
        if (str_v.type != VAL_STRING || idx_v.type != VAL_INT) {
            snprintf(vm->error_msg, sizeof(vm->error_msg), "str_at: 参数类型错误，需要 (str, int)");
            vm->has_error = 1; return -1;
        }
        const char* s = str_v.data.str_val ? str_v.data.str_val : "";
        int64_t idx = idx_v.data.int_val;
        size_t len = strlen(s);
        if (idx < 0 || (size_t)idx >= len) {
            snprintf(vm->error_msg, sizeof(vm->error_msg), "str_at: 索引越界 (%lld, 长度 %zu)", (long long)idx, len);
            vm->has_error = 1; return -1;
        }
        char buf[2] = { s[idx], '\0' };
        Value r; r.type = VAL_STRING; r.data.str_val = sdvm_heap_strdup(vm, buf);
        PUSH(r);
        break;
    }
    case BIF_STR_SUB: {
        if (argc < 2 || argc > 3 || vm->sp < 1) {
            snprintf(vm->error_msg, sizeof(vm->error_msg), "str_sub 需要 2~3 个参数 (str, start, [end])");
            vm->has_error = 1; return -1;
        }
        int64_t end = -1;
        if (argc == 3) {
            Value end_v = vm->stack[vm->sp--];
            if (end_v.type != VAL_INT) {
                snprintf(vm->error_msg, sizeof(vm->error_msg), "str_sub: end 必须是整数");
                vm->has_error = 1; return -1;
            }
            end = end_v.data.int_val;
        }
        Value start_v = vm->stack[vm->sp--];
        Value str_v = vm->stack[vm->sp--];
        if (str_v.type != VAL_STRING || start_v.type != VAL_INT) {
            snprintf(vm->error_msg, sizeof(vm->error_msg), "str_sub: 参数类型错误，需要 (str, int, [int])");
            vm->has_error = 1; return -1;
        }
        const char* s = str_v.data.str_val ? str_v.data.str_val : "";
        size_t len = strlen(s);
        int64_t start = start_v.data.int_val;
        if (start < 0) start = 0;
        if ((size_t)start > len) start = (int64_t)len;
        if (end < 0 || (size_t)end > len) end = (int64_t)len;
        if (end < start) end = start;
        size_t sub_len = (size_t)(end - start);
        char* buf = (char*)malloc(sub_len + 1);
        if (!buf) { vm->has_error = 1; return -1; }
        memcpy(buf, s + start, sub_len);
        buf[sub_len] = '\0';
        Value r; r.type = VAL_STRING; r.data.str_val = sdvm_heap_strdup(vm, buf);
        free(buf);
        PUSH(r);
        break;
    }
    case BIF_STR_FIND: {
        if (argc != 2 || vm->sp < 1) {
            snprintf(vm->error_msg, sizeof(vm->error_msg), "str_find 需要 2 个参数 (str, pattern)");
            vm->has_error = 1; return -1;
        }
        Value pat_v = vm->stack[vm->sp--];
        Value str_v = vm->stack[vm->sp--];
        if (str_v.type != VAL_STRING || pat_v.type != VAL_STRING) {
            snprintf(vm->error_msg, sizeof(vm->error_msg), "str_find: 参数必须是字符串");
            vm->has_error = 1; return -1;
        }
        const char* haystack = str_v.data.str_val ? str_v.data.str_val : "";
        const char* needle = pat_v.data.str_val ? pat_v.data.str_val : "";
        const char* found = strstr(haystack, needle);
        Value r; r.type = VAL_INT;
        r.data.int_val = found ? (int64_t)(found - haystack) : -1;
        PUSH(r);
        break;
    }
    case BIF_STR_CONTAINS: {
        if (argc != 2 || vm->sp < 1) {
            snprintf(vm->error_msg, sizeof(vm->error_msg), "str_contains 需要 2 个参数 (str, pattern)");
            vm->has_error = 1; return -1;
        }
        Value pat_v = vm->stack[vm->sp--];
        Value str_v = vm->stack[vm->sp--];
        if (str_v.type != VAL_STRING || pat_v.type != VAL_STRING) {
            snprintf(vm->error_msg, sizeof(vm->error_msg), "str_contains: 参数必须是字符串");
            vm->has_error = 1; return -1;
        }
        const char* haystack = str_v.data.str_val ? str_v.data.str_val : "";
        const char* needle = pat_v.data.str_val ? pat_v.data.str_val : "";
        Value r; r.type = VAL_BOOL; r.data.bool_val = (strstr(haystack, needle) != NULL) ? 1 : 0;
        PUSH(r);
        break;
    }
    case BIF_STR_TRIM: {
        if (argc != 1 || vm->sp < 0) {
            snprintf(vm->error_msg, sizeof(vm->error_msg), "str_trim 需要 1 个参数 (str)");
            vm->has_error = 1; return -1;
        }
        Value v = vm->stack[vm->sp--];
        if (v.type != VAL_STRING) {
            snprintf(vm->error_msg, sizeof(vm->error_msg), "str_trim: 参数必须是字符串");
            vm->has_error = 1; return -1;
        }
        const char* s = v.data.str_val ? v.data.str_val : "";
        while (*s && (unsigned char)*s <= ' ') s++;
        if (*s == '\0') {
            Value r; r.type = VAL_STRING; r.data.str_val = sdvm_heap_strdup(vm, "");
            PUSH(r); break;
        }
        size_t len = strlen(s);
        while (len > 0 && (unsigned char)s[len-1] <= ' ') len--;
        char* buf = (char*)malloc(len + 1);
        if (!buf) { vm->has_error = 1; return -1; }
        memcpy(buf, s, len);
        buf[len] = '\0';
        Value r; r.type = VAL_STRING; r.data.str_val = sdvm_heap_strdup(vm, buf);
        free(buf);
        PUSH(r);
        break;
    }
    case BIF_STR_UPPER:
    case BIF_STR_LOWER: {
        if (argc != 1 || vm->sp < 0) {
            snprintf(vm->error_msg, sizeof(vm->error_msg), "str_upper/lower 需要 1 个参数 (str)");
            vm->has_error = 1; return -1;
        }
        Value v = vm->stack[vm->sp--];
        if (v.type != VAL_STRING) {
            snprintf(vm->error_msg, sizeof(vm->error_msg), "str_upper/lower: 参数必须是字符串");
            vm->has_error = 1; return -1;
        }
        const char* s = v.data.str_val ? v.data.str_val : "";
        size_t len = strlen(s);
        char* buf = (char*)malloc(len + 1);
        if (!buf) { vm->has_error = 1; return -1; }
        for (size_t i = 0; i < len; i++) {
            if (bif_idx == BIF_STR_UPPER)
                buf[i] = (char)toupper((unsigned char)s[i]);
            else
                buf[i] = (char)tolower((unsigned char)s[i]);
        }
        buf[len] = '\0';
        Value r; r.type = VAL_STRING; r.data.str_val = sdvm_heap_strdup(vm, buf);
        free(buf);
        PUSH(r);
        break;
    }

    case BIF_STR_SPLIT: {
        /* str_split(s, delimiter): 按分隔符分割字符串，返回带数字键的对象 */
        if (argc != 2 || vm->sp < 1) {
            snprintf(vm->error_msg, sizeof(vm->error_msg), "str_split 需要 2 个参数 (str, delimiter)");
            vm->has_error = 1; return -1;
        }
        Value delim_v = vm->stack[vm->sp--];
        Value str_v = vm->stack[vm->sp--];
        if (str_v.type != VAL_STRING || delim_v.type != VAL_STRING) {
            snprintf(vm->error_msg, sizeof(vm->error_msg), "str_split: 参数必须是字符串");
            vm->has_error = 1; return -1;
        }
        const char* s = str_v.data.str_val ? str_v.data.str_val : "";
        const char* delim = delim_v.data.str_val ? delim_v.data.str_val : "";
        size_t delim_len = strlen(delim);

        Object* obj = obj_create(vm);
        if (!obj) { vm->has_error = 1; return -1; }

        if (delim_len == 0) {
            /* 空分隔符：拆成单个字符 */
            size_t len = strlen(s);
            for (size_t i = 0; i < len; i++) {
                char buf[2] = { s[i], '\0' };
                char key[16];
                snprintf(key, sizeof(key), "%zu", i);
                Value val; val.type = VAL_STRING; val.data.str_val = sdvm_heap_strdup(vm, buf);
                obj_set(obj, key, val);
            }
        } else {
            /* 按分隔符分割 */
            int part_idx = 0;
            const char* start = s;
            while (1) {
                const char* found = strstr(start, delim);
                size_t part_len;
                if (found) {
                    part_len = (size_t)(found - start);
                } else {
                    part_len = strlen(start);
                }
                char* part = (char*)malloc(part_len + 1);
                if (!part) { obj_free(obj); vm->has_error = 1; return -1; }
                memcpy(part, start, part_len);
                part[part_len] = '\0';
                char key[16];
                snprintf(key, sizeof(key), "%d", part_idx);
                Value val; val.type = VAL_STRING; val.data.str_val = sdvm_heap_strdup(vm, part);
                obj_set(obj, key, val);
                free(part);

                if (!found) break;
                part_idx++;
                start = found + delim_len;
            }
        }
        Value r; r.type = VAL_OBJECT; r.data.ptr_val = obj;
        PUSH(r);
        break;
    }

    case BIF_FFI_LOAD: {
        /* ffi_load(path): 加载 DLL，返回句柄 */
        if (argc != 1) {
            snprintf(vm->error_msg, sizeof(vm->error_msg), "ffi_load 需要 1 个参数 (path)");
            vm->has_error = 1; return -1;
        }
        Value path_v = vm->stack[vm->sp--];
        if (path_v.type != VAL_STRING) {
            snprintf(vm->error_msg, sizeof(vm->error_msg), "ffi_load: 参数必须是字符串路径");
            vm->has_error = 1; return -1;
        }
        const char* path = path_v.data.str_val ? path_v.data.str_val : "";
        FFI_HANDLE hMod = (FFI_HANDLE)FFI_LOAD(path);
        if (!hMod) {
            snprintf(vm->error_msg, sizeof(vm->error_msg), "ffi_load: 无法加载库 '%s'", path);
            vm->has_error = 1; return -1;
        }
        Value r; r.type = VAL_INT; r.data.int_val = (int64_t)(intptr_t)hMod;
        PUSH(r);
        break;
    }

    case BIF_FFI_FREE: {
        /* ffi_free(handle): 释放 DLL */
        if (argc != 1) {
            snprintf(vm->error_msg, sizeof(vm->error_msg), "ffi_free 需要 1 个参数 (handle)");
            vm->has_error = 1; return -1;
        }
        Value h_v = vm->stack[vm->sp--];
        int64_t hMod_val = h_v.data.int_val;
        if (hMod_val) {
            FFI_FREE((FFI_HANDLE)(intptr_t)hMod_val);
            ffi_cache_clear_handle(vm, hMod_val);
        }
        Value r; r.type = VAL_NULL; PUSH(r);
        break;
    }

    case BIF_FFI_CALL: {
        /* ffi_call(handle, name, ret_type, arg1, arg2, ...)
           调用 C 函数，支持缓存加速 */
        if (argc < 3) {
            snprintf(vm->error_msg, sizeof(vm->error_msg), "ffi_call 至少需要 3 个参数 (handle, name, ret_type)");
            vm->has_error = 1; return -1;
        }
        int nargs = argc - 3;  /* 函数实际参数个数 */

        /* 从栈弹出函数参数 (逆序弹出，支持 int/float/string 自动转换) */
        int64_t args[16];
        if (nargs > 16) {
            vm->has_error = 1; return -1;
        }
        for (int i = nargs - 1; i >= 0; i--) {
            Value v = vm->stack[vm->sp--];
            if (v.type == VAL_STRING) {
                /* 自动传 char* 指针 */
                args[i] = (int64_t)(intptr_t)(v.data.str_val ? v.data.str_val : "");
            } else if (v.type == VAL_FLOAT) {
                int64_t tmp; memcpy(&tmp, &v.data.float_val, sizeof(tmp));
                args[i] = tmp;
            } else {
                args[i] = val_to_int64(&v);
            }
        }

        /* 弹出固定参数: ret_type, name, handle */
        Value ret_type_v = vm->stack[vm->sp--];
        Value name_v = vm->stack[vm->sp--];
        Value handle_v = vm->stack[vm->sp--];

        const char* ret_type = ret_type_v.type == VAL_STRING && ret_type_v.data.str_val
                               ? ret_type_v.data.str_val : "i";
        const char* func_name = name_v.type == VAL_STRING && name_v.data.str_val
                                ? name_v.data.str_val : "";
        int64_t handle_val = handle_v.type == VAL_INT ? handle_v.data.int_val : 0;
        void* func_ptr = NULL;

        /* 查缓存 */
        if (handle_val) {
            int cache_idx = ffi_cache_find(vm, handle_val, func_name);
            if (cache_idx >= 0) {
                func_ptr = vm->ffi_cache[cache_idx].func_ptr;
            } else {
                /* 缓存未命中 → GetProcAddress + 存入缓存 */
                func_ptr = FFI_GET_PROC((FFI_HANDLE)(intptr_t)handle_val, func_name);
                if (func_ptr) {
                    ffi_cache_store(vm, handle_val, func_name, func_ptr);
                }
            }
        }
        if (!func_ptr) {
            snprintf(vm->error_msg, sizeof(vm->error_msg),
                     "ffi_call: 找不到函数 '%s'", func_name);
            vm->has_error = 1; return -1;
        }

        /* 根据参数个数调用 */
        int64_t result = 0;
        switch (nargs) {
            case 0:  result = ((ffi_fn_0)func_ptr)(); break;
            case 1:  result = ((ffi_fn_1)func_ptr)(args[0]); break;
            case 2:  result = ((ffi_fn_2)func_ptr)(args[0], args[1]); break;
            case 3:  result = ((ffi_fn_3)func_ptr)(args[0], args[1], args[2]); break;
            case 4:  result = ((ffi_fn_4)func_ptr)(args[0], args[1], args[2], args[3]); break;
            case 5:  result = ((ffi_fn_5)func_ptr)(args[0], args[1], args[2], args[3], args[4]); break;
            case 6:  result = ((ffi_fn_6)func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5]); break;
            case 7:  result = ((ffi_fn_7)func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5], args[6]); break;
            case 8:  result = ((ffi_fn_8)func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7]); break;
            default:
                snprintf(vm->error_msg, sizeof(vm->error_msg),
                         "ffi_call: 不支持的参数个数 %d (最多 8 个)", nargs);
                vm->has_error = 1; return -1;
        }

        /* 根据返回类型处理:
           "v" = void, "f" = double, "i" = int64, "s" = string, "p" = 指针 */
        if (ret_type[0] == 'v') {
            Value r; r.type = VAL_NULL; PUSH(r);
        } else if (ret_type[0] == 'f') {
            double d;
            memcpy(&d, &result, sizeof(d));
            Value r; r.type = VAL_FLOAT; r.data.float_val = d; PUSH(r);
        } else if (ret_type[0] == 's') {
            /* char* 返回 → 拷贝到 VM 堆，返回 VAL_STRING */
            const char* cstr = (const char*)(intptr_t)result;
            const char* heap_str = sdvm_heap_strdup(vm, cstr ? cstr : "");
            Value r; r.type = VAL_STRING; r.data.str_val = heap_str; PUSH(r);
        } else if (ret_type[0] == 'p') {
            /* 指针返回 → 包装为 int64 (opaque handle) */
            Value r; r.type = VAL_INT; r.data.int_val = result; PUSH(r);
        } else {
            /* 默认返回 int64 */
            Value r; r.type = VAL_INT; r.data.int_val = result; PUSH(r);
        }
        break;
    }

    /* ─── 列表 / 范围 BIF ──────────────────────────── */
    case BIF_RANGE: {
        int64_t start = 0, end = 0;
        if (argc == 1) {
            if (vm->sp < 0) { snprintf(vm->error_msg, sizeof(vm->error_msg), "BIF_RANGE: 栈空"); vm->has_error = 1; return -1; }
            Value end_v = vm->stack[vm->sp--];
            end = end_v.data.int_val;
        } else {
            if (vm->sp < 1) { snprintf(vm->error_msg, sizeof(vm->error_msg), "BIF_RANGE: 栈空"); vm->has_error = 1; return -1; }
            Value end_v = vm->stack[vm->sp--];
            Value start_v = vm->stack[vm->sp--];
            start = start_v.data.int_val;
            end = end_v.data.int_val;
        }
        Object* obj = obj_create(vm);
        if (!obj) { vm->has_error = 1; return -1; }
        for (int64_t i = start; i < end; i++) {
            char key[32];
            snprintf(key, sizeof(key), "%lld", (long long)(i - start));
            Value val; val.type = VAL_INT; val.data.int_val = i;
            obj_set(obj, key, val);
        }
        Value r; r.type = VAL_OBJECT; r.data.ptr_val = obj;
        PUSH(r);
        break;
    }

    case BIF_LIST_ADD: {
        if (vm->sp < 0) { snprintf(vm->error_msg, sizeof(vm->error_msg), "BIF_LIST_ADD: 栈空"); vm->has_error = 1; return -1; }
        Value val = vm->stack[vm->sp--];
        int64_t insert_idx = -1;
        if (argc == 3) {
            if (vm->sp < 0) { snprintf(vm->error_msg, sizeof(vm->error_msg), "BIF_LIST_ADD: 栈空"); vm->has_error = 1; return -1; }
            Value idx_v = vm->stack[vm->sp--];
            insert_idx = idx_v.data.int_val;
        }
        if (vm->sp < 0) { snprintf(vm->error_msg, sizeof(vm->error_msg), "BIF_LIST_ADD: 栈空"); vm->has_error = 1; return -1; }
        Value list_v = vm->stack[vm->sp--];
        Object* obj = (Object*)list_v.data.ptr_val;

        if (insert_idx >= 0) {
            /* Insert at index: shift keys >= insert_idx up by 1 */
            int64_t max_key = -1;
            for (int i = 0; i < obj->count; i++) {
                int64_t k = strtol(obj->entries[i].key, NULL, 10);
                if (k > max_key) max_key = k;
            }
            for (int64_t k = max_key; k >= insert_idx; k--) {
                char old_key[32], new_key[32];
                snprintf(old_key, sizeof(old_key), "%lld", (long long)k);
                snprintf(new_key, sizeof(new_key), "%lld", (long long)(k + 1));
                int idx = obj_find(obj, old_key);
                if (idx >= 0) {
                    Value ov = obj->entries[idx].value;
                    obj_set(obj, new_key, ov);
                    obj_remove(obj, old_key);
                }
            }
            char key[32];
            snprintf(key, sizeof(key), "%lld", (long long)insert_idx);
            obj_set(obj, key, val);
        } else {
            /* Push: find max key + 1 */
            int64_t max_key = -1;
            for (int i = 0; i < obj->count; i++) {
                int64_t k = strtol(obj->entries[i].key, NULL, 10);
                if (k > max_key) max_key = k;
            }
            char key[32];
            snprintf(key, sizeof(key), "%lld", (long long)(max_key + 1));
            obj_set(obj, key, val);
        }
        Value r; r.type = VAL_NULL;
        PUSH(r);
        break;
    }

    case BIF_LIST_POP: {
        if (vm->sp < 0) { snprintf(vm->error_msg, sizeof(vm->error_msg), "BIF_LIST_POP: 栈空"); vm->has_error = 1; return -1; }
        int64_t pop_idx = -1;
        if (argc == 2) {
            Value idx_v = vm->stack[vm->sp--];
            pop_idx = idx_v.data.int_val;
        }
        Value list_v = vm->stack[vm->sp--];
        Object* obj = (Object*)list_v.data.ptr_val;
        Value result;
        result.type = VAL_NULL;

        if (pop_idx >= 0) {
            char key[32];
            snprintf(key, sizeof(key), "%lld", (long long)pop_idx);
            int idx = obj_find(obj, key);
            if (idx >= 0) {
                result = obj->entries[idx].value;
                obj_remove(obj, key);
                /* 重新编号 */
                char old_key[32], new_key[32];
                for (int64_t k = pop_idx + 1; ; k++) {
                    snprintf(old_key, sizeof(old_key), "%lld", (long long)k);
                    int fi = obj_find(obj, old_key);
                    if (fi < 0) break;
                    Value v = obj->entries[fi].value;
                    snprintf(new_key, sizeof(new_key), "%lld", (long long)(k - 1));
                    obj_set(obj, new_key, v);
                    obj_remove(obj, old_key);
                }
            }
        } else {
            /* Find max key */
            int64_t max_key = -1;
            for (int i = 0; i < obj->count; i++) {
                int64_t k = strtol(obj->entries[i].key, NULL, 10);
                if (k > max_key) max_key = k;
            }
            if (max_key >= 0) {
                char key[32];
                snprintf(key, sizeof(key), "%lld", (long long)max_key);
                int idx = obj_find(obj, key);
                if (idx >= 0) {
                    result = obj->entries[idx].value;
                    obj_remove(obj, key);
                }
            }
        }
        PUSH(result);
        break;
    }

    case BIF_LIST_REMOVE: {
        if (vm->sp < 0) { snprintf(vm->error_msg, sizeof(vm->error_msg), "BIF_LIST_REMOVE: 栈空"); vm->has_error = 1; return -1; }
        Value idx_v = vm->stack[vm->sp--];
        Value list_v = vm->stack[vm->sp--];
        Object* obj = (Object*)list_v.data.ptr_val;
        int64_t rem_idx = idx_v.data.int_val;
        char key[32];
        snprintf(key, sizeof(key), "%lld", (long long)rem_idx);
        obj_remove(obj, key);
        /* 重新编号：移除后 key > rem_idx 的条目 key 减 1 */
        char old_key[32], new_key[32];
        for (int64_t k = rem_idx + 1; ; k++) {
            snprintf(old_key, sizeof(old_key), "%lld", (long long)k);
            int idx = obj_find(obj, old_key);
            if (idx < 0) break;
            Value v = obj->entries[idx].value;
            snprintf(new_key, sizeof(new_key), "%lld", (long long)(k - 1));
            obj_set(obj, new_key, v);
            obj_remove(obj, old_key);
        }
        Value r; r.type = VAL_NULL;
        PUSH(r);
        break;
    }

    case BIF_LIST_SORT: {
        if (vm->sp < 0) { snprintf(vm->error_msg, sizeof(vm->error_msg), "BIF_LIST_SORT: 栈空"); vm->has_error = 1; return -1; }
        Value list_v = vm->stack[vm->sp--];
        Object* obj = (Object*)list_v.data.ptr_val;
        /* Bubble sort by value (int) */
        for (int i = 0; i < obj->count - 1; i++) {
            for (int j = 0; j < obj->count - i - 1; j++) {
                int64_t a = obj->entries[j].value.data.int_val;
                int64_t b = obj->entries[j + 1].value.data.int_val;
                if (a > b) {
                    ObjectEntry tmp = obj->entries[j];
                    obj->entries[j] = obj->entries[j + 1];
                    obj->entries[j + 1] = tmp;
                }
            }
        }
        /* 重编号 keys 为 0,1,2,... */
        for (int i = 0; i < obj->count; i++) {
            free(obj->entries[i].key);
            char key[32];
            snprintf(key, sizeof(key), "%d", i);
            obj->entries[i].key = strdup(key);
        }
        Value r; r.type = VAL_NULL;
        PUSH(r);
        break;
    }

    case BIF_LIST_REVERSE: {
        if (vm->sp < 0) { snprintf(vm->error_msg, sizeof(vm->error_msg), "BIF_LIST_REVERSE: 栈空"); vm->has_error = 1; return -1; }
        Value list_v = vm->stack[vm->sp--];
        Object* obj = (Object*)list_v.data.ptr_val;
        for (int i = 0, j = obj->count - 1; i < j; i++, j--) {
            ObjectEntry tmp = obj->entries[i];
            obj->entries[i] = obj->entries[j];
            obj->entries[j] = tmp;
        }
        /* 重编号 keys 为 0,1,2,... */
        for (int i = 0; i < obj->count; i++) {
            free(obj->entries[i].key);
            char key[32];
            snprintf(key, sizeof(key), "%d", i);
            obj->entries[i].key = strdup(key);
        }
        Value r; r.type = VAL_NULL;
        PUSH(r);
        break;
    }

    case BIF_LIST_CLEAR: {
        if (vm->sp < 0) { snprintf(vm->error_msg, sizeof(vm->error_msg), "BIF_LIST_CLEAR: 栈空"); vm->has_error = 1; return -1; }
        Value list_v = vm->stack[vm->sp--];
        Object* obj = (Object*)list_v.data.ptr_val;
        for (int i = 0; i < obj->count; i++)
            free(obj->entries[i].key);
        obj->count = 0;
        Value r; r.type = VAL_NULL;
        PUSH(r);
        break;
    }

    case BIF_LIST_COPY: {
        if (vm->sp < 0) { snprintf(vm->error_msg, sizeof(vm->error_msg), "BIF_LIST_COPY: 栈空"); vm->has_error = 1; return -1; }
        Value list_v = vm->stack[vm->sp--];
        Object* src = (Object*)list_v.data.ptr_val;
        Object* copy = obj_copy(vm, src);
        if (!copy) { vm->has_error = 1; return -1; }
        Value r; r.type = VAL_OBJECT; r.data.ptr_val = copy;
        PUSH(r);
        break;
    }

    case BIF_COPY_COPY: {
        /* copy.copy(a): 把对象 a 存入临时区，不返回值 */
        if (vm->sp < 0) { snprintf(vm->error_msg, sizeof(vm->error_msg), "BIF_COPY_COPY: 栈空"); vm->has_error = 1; return -1; }
        s_copy_temp = vm->stack[vm->sp--];
        Value cr; cr.type = VAL_NULL;
        PUSH(cr);
        break;
    }

    case BIF_COPY_PASTE: {
        /* copy.paste(): 粘贴返回临时区对象 */
        PUSH(s_copy_temp);
        break;
    }

    case BIF_COPY_CLEAN: {
        /* copy.clean(): 清除临时区 */
        s_copy_temp.type = VAL_NULL;
        s_copy_temp.data.int_val = 0;
        Value cr; cr.type = VAL_NULL;
        PUSH(cr);
        break;
    }

    case BIF_CLONE: {
        /* copy.clone(a) / a.clone(): 存入临时区并粘贴返回 */
        if (vm->sp < 0) { snprintf(vm->error_msg, sizeof(vm->error_msg), "BIF_CLONE: 栈空"); vm->has_error = 1; return -1; }
        Value cv = vm->stack[vm->sp--];
        s_copy_temp = cv;
        PUSH(cv);
        break;
    }

    case BIF_DEEPCLONE: {
        /* copy.deepclone(a) / a.deepclone(): 深拷贝并粘贴返回 */
        if (vm->sp < 0) { snprintf(vm->error_msg, sizeof(vm->error_msg), "BIF_DEEPCLONE: 栈空"); vm->has_error = 1; return -1; }
        Value src = vm->stack[vm->sp--];
        Value dst;
        if (src.type == VAL_OBJECT) {
            Object* obj = obj_deep_copy(vm, (Object*)src.data.ptr_val);
            if (!obj) { vm->has_error = 1; return -1; }
            dst.type = VAL_OBJECT;
            dst.data.ptr_val = obj;
        } else {
            dst = src;
        }
        s_copy_temp = dst;
        PUSH(dst);
        break;
    }

    case BIF_COPY_DEEPCOPY: {
        /* copy.deepcopy(a): 深拷贝存入临时区 */
        if (vm->sp < 0) { snprintf(vm->error_msg, sizeof(vm->error_msg), "BIF_COPY_DEEPCOPY: 栈空"); vm->has_error = 1; return -1; }
        Value src = vm->stack[vm->sp--];
        Value dst;
        if (src.type == VAL_OBJECT) {
            Object* obj = obj_deep_copy(vm, (Object*)src.data.ptr_val);
            if (!obj) { vm->has_error = 1; return -1; }
            dst.type = VAL_OBJECT;
            dst.data.ptr_val = obj;
        } else {
            dst = src;
        }
        s_copy_temp = dst;
        Value cr; cr.type = VAL_NULL;
        PUSH(cr);
        break;
    }

    default:
        snprintf(vm->error_msg, sizeof(vm->error_msg),
                 "未知的内置函数索引: %d", bif_idx);
        vm->has_error = 1;
        return -1;
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
    vm->socket_count = 0;
    memset(vm->sockets, 0, sizeof(vm->sockets));
    vm->object_count = 0;
    memset(vm->objects, 0, sizeof(vm->objects));

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
            else if (op == OP_SCONST) printf(" #%u", FETCH_U32());
            else if (op == OP_BCONST) printf(" %u", FETCH_U8());
            else if (op == OP_LOAD || op == OP_STORE) printf(" r%u", FETCH_U8());
            else if (op == OP_JMP || op == OP_JIF) printf(" %+d", FETCH_I32());
            else if (op == OP_NEWOBJ) printf(" %u", vm->code[vm->ip]);
            else if (op == OP_GETATTR || op == OP_SETATTR) {
                uint32_t _idx = (uint32_t)vm->code[vm->ip] | ((uint32_t)vm->code[vm->ip+1] << 8) |
                                ((uint32_t)vm->code[vm->ip+2] << 16) | ((uint32_t)vm->code[vm->ip+3] << 24);
                printf(" #%u", _idx);
            } else if (op == OP_BIF) printf(" bif:%d args:%d", vm->code[vm->ip], vm->code[vm->ip + 1]);
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

        case OP_IDIV: {
            Value b, a;
            POP(b); POP(a);
            if (a.type != VAL_INT || b.type != VAL_INT) {
                snprintf(vm->error_msg, sizeof(vm->error_msg), "整除: 操作数必须为整数");
                vm->has_error = 1; return -1;
            }
            if (b.data.int_val == 0) {
                snprintf(vm->error_msg, sizeof(vm->error_msg), "整除: 除数为零");
                vm->has_error = 1; return -1;
            }
            Value r = { .type = VAL_INT, .data.int_val = a.data.int_val / b.data.int_val };
            PUSH(r);
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

        /* ── 对象操作 ──────────────────────────── */
        case OP_NEWOBJ: {
            uint8_t n = FETCH_U8();
            ADVANCE(1);
            Object* obj = obj_create(vm);
            if (!obj) {
                snprintf(vm->error_msg, sizeof(vm->error_msg), "NEWOBJ: 内存不足");
                vm->has_error = 1; return -1;
            }
            /* 编译器 push 顺序: value 先, key 后 */
            /* 栈(底→顶): [val0, key0, val1, key1, ...] */
            /* 所以从栈顶先弹出 key_n-1, 再弹出 val_n-1 */
            for (int i = 0; i < n; i++) {
                if (vm->sp < 0) {
                    snprintf(vm->error_msg, sizeof(vm->error_msg), "NEWOBJ: 栈下溢");
                    vm->has_error = 1; return -1;
                }
                Value key_v = vm->stack[vm->sp--];
                if (vm->sp < 0) {
                    snprintf(vm->error_msg, sizeof(vm->error_msg), "NEWOBJ: 栈下溢(value)");
                    vm->has_error = 1; return -1;
                }
                Value val = vm->stack[vm->sp--];
                const char* key_str = (key_v.type == VAL_STRING && key_v.data.str_val)
                                     ? key_v.data.str_val : "";
                obj_set(obj, key_str, val);
            }
            Value r; r.type = VAL_OBJECT; r.data.ptr_val = obj;
            PUSH(r);
            break;
        }

        case OP_GETATTR: {
            uint32_t str_idx = FETCH_U32();
            ADVANCE(4);
            if (vm->sp < 0) {
                snprintf(vm->error_msg, sizeof(vm->error_msg), "GETATTR: 栈空");
                vm->has_error = 1; return -1;
            }
            Value obj_v = vm->stack[vm->sp--];
            if (obj_v.type != VAL_OBJECT) {
                snprintf(vm->error_msg, sizeof(vm->error_msg), "GETATTR: 不是对象");
                vm->has_error = 1; return -1;
            }
            const char* key = (str_idx < vm->strpool_count) ? vm->strpool[str_idx] : "";
            Value result = obj_get((Object*)obj_v.data.ptr_val, key);
            PUSH(result);
            break;
        }

        case OP_SETATTR: {
            uint32_t str_idx = FETCH_U32();
            ADVANCE(4);
            if (vm->sp < 1) {
                snprintf(vm->error_msg, sizeof(vm->error_msg), "SETATTR: 栈元素不足");
                vm->has_error = 1; return -1;
            }
            Value val = vm->stack[vm->sp--];
            Value obj_v = vm->stack[vm->sp--];
            if (obj_v.type != VAL_OBJECT) {
                snprintf(vm->error_msg, sizeof(vm->error_msg), "SETATTR: 不是对象");
                vm->has_error = 1; return -1;
            }
            const char* key = (str_idx < vm->strpool_count) ? vm->strpool[str_idx] : "";
            obj_set((Object*)obj_v.data.ptr_val, key, val);
            PUSH(val);  /* 像赋值表达式一样返回值 */
            break;
        }

        case OP_GETINDEX: {
            if (vm->sp < 1) {
                snprintf(vm->error_msg, sizeof(vm->error_msg), "GETINDEX: 栈元素不足");
                vm->has_error = 1; return -1;
            }
            Value idx = vm->stack[vm->sp--];
            Value obj = vm->stack[vm->sp--];

            if (obj.type == VAL_STRING && idx.type == VAL_INT) {
                /* 字符串下标: s[idx] → 返回单个字符 */
                const char* s = obj.data.str_val ? obj.data.str_val : "";
                size_t len = strlen(s);
                int64_t i = idx.data.int_val;
                if (i < 0 || (size_t)i >= len) {
                    snprintf(vm->error_msg, sizeof(vm->error_msg), "GETINDEX: 字符串索引越界 (%lld, 长度 %zu)", (long long)i, len);
                    vm->has_error = 1; return -1;
                }
                char buf[2] = { s[i], '\0' };
                Value r; r.type = VAL_STRING; r.data.str_val = sdvm_heap_strdup(vm, buf);
                PUSH(r);
            } else if (obj.type == VAL_OBJECT) {
                const char* key = "";
                char key_buf[32];
                if (idx.type == VAL_STRING) {
                    key = idx.data.str_val ? idx.data.str_val : "";
                } else if (idx.type == VAL_INT) {
                    snprintf(key_buf, sizeof(key_buf), "%lld", (long long)idx.data.int_val);
                    key = key_buf;
                } else {
                    snprintf(vm->error_msg, sizeof(vm->error_msg), "GETINDEX: 对象索引类型不支持 (idx=%d)", idx.type);
                    vm->has_error = 1; return -1;
                }
                Value r = obj_get((Object*)obj.data.ptr_val, key);
                if (r.type == VAL_NULL) {
                    snprintf(vm->error_msg, sizeof(vm->error_msg), "GETINDEX: 键 '%s' 不存在", key);
                    vm->has_error = 1; return -1;
                }
                PUSH(r);
            } else {
                snprintf(vm->error_msg, sizeof(vm->error_msg), "GETINDEX: 不支持的索引类型 (obj=%d, idx=%d)", obj.type, idx.type);
                vm->has_error = 1; return -1;
            }
            break;
        }

        case OP_SETINDEX: {
            if (vm->sp < 2) {
                snprintf(vm->error_msg, sizeof(vm->error_msg), "SETINDEX: 栈元素不足");
                vm->has_error = 1; return -1;
            }
            Value val = vm->stack[vm->sp--];
            Value idx = vm->stack[vm->sp--];
            Value obj = vm->stack[vm->sp--];

            if (obj.type == VAL_STRING) {
                snprintf(vm->error_msg, sizeof(vm->error_msg), "SETINDEX: 字符串不可变，不能通过下标修改");
                vm->has_error = 1; return -1;
            } else if (obj.type == VAL_OBJECT) {
                const char* key = "";
                char key_buf[32];
                if (idx.type == VAL_STRING) {
                    key = idx.data.str_val ? idx.data.str_val : "";
                } else if (idx.type == VAL_INT) {
                    snprintf(key_buf, sizeof(key_buf), "%lld", (long long)idx.data.int_val);
                    key = key_buf;
                } else {
                    snprintf(vm->error_msg, sizeof(vm->error_msg), "SETINDEX: 对象索引类型不支持 (idx=%d)", idx.type);
                    vm->has_error = 1; return -1;
                }
                obj_set((Object*)obj.data.ptr_val, key, val);
                PUSH(val);
            } else {
                snprintf(vm->error_msg, sizeof(vm->error_msg), "SETINDEX: 不支持的索引类型");
                vm->has_error = 1; return -1;
            }
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
    if (!f) {
        return NULL;
    }
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

    /* 释放 FFI 缓存 */
    for (int i = 0; i < FFI_CACHE_SIZE; i++) {
        free(vm->ffi_cache[i].func_name);
        vm->ffi_cache[i].func_name = NULL;
    }

    /* 释放对象 */
    for (int i = 0; i < vm->object_count; i++)
        obj_free(vm->objects[i]);
    vm->object_count = 0;

    sdvm_init(vm);
}
