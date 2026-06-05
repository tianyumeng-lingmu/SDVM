# ⭐ SDVM — 星舞虚拟机

**Star Dance Virtual Machine** — 类 JVM 栈式虚拟机，执行 `.dance` 字节码。

> 当前版本：v2.2 — 支持 FFI、包系统、namespace 调用、life 命途、对象/JSON、文件 I/O、字符串操作、匿名函数、位运算、foreach 遍历

SDVM 由三部分组成：
- **前端解析器**（Python）— `star_dance/` 词法/语法分析，生成 AST
- **编译器**（Python）— 将 AST 编译为 `.dance` 字节码
- **运行时**（C 语言）— 加载并执行 `.dance` 字节码

---

## 📁 目录结构

```
SDVM/
├── compiler.py          .star → .dance 字节码编译器（Python）
├── sdvm.c               虚拟机核心（C 语言运行时）
├── sdvm.h               指令定义、Value 类型、API
│
├── packages/            官方包
│   ├── system.star      Windows 系统 API 封装（kernel32.dll）
│   ├── rand.star        随机数生成包（msvcrt.dll）
│   ├── webstar.star     HTTP 服务器包
│   └── test_dep.star    包依赖测试
│
├── tcc_src/             使用 TCC（Tiny C Compiler）编译
│   ├── main.c           入口
│   ├── sdvm.c           虚拟机副本
│   ├── sdvm.h           头文件副本
│   ├── stdint.h         自定义 stdint（TCC 兼容，x64 修复）
│   ├── stddef.h         自定义 stddef（TCC 兼容）
│   ├── winsock2.h       自定义 Winsock 头（网络功能）
│   └── ws2tcpip.h       自定义 ws2tcpip 头
│
├── *.star               测试脚本
├── sdvm_new.exe         TCC 编译的虚拟机可执行文件
└── README.md
```

> 前端解析器位于同级目录 `star_dance/`（lexer.py / parser.py / ast_nodes.py / tokens.py）

---

## 🚀 快速开始

### 运行示例

```bash
# 编译 + 运行
python compiler.py samples\test_full.star -o tcc_src\test_full.dance
sdvm_new.exe tcc_src\test_full.dance

# 调试模式（显示指令执行过程）
sdvm_new.exe tcc_src\test_full.dance -v
```

### 从源码编译

依赖：TCC (Tiny C Compiler) 或 MSVC

```bash
# TCC
cd tcc_src
tcc sdvm.c main.c -I. -o sdvm_new.exe

# MSVC
cl sdvm.c main.c /Fe:sdvm_new.exe /I.
```

---

## 💻 StarDance 语言语法

### 程序入口

每个可执行文件必须有 `main` 命途，内含 `thing main()` 作为主程序入口：

```stardance
main {
    thing main() {
        see("Hello, World!\n");
    }
}
```

模块级 `thing`（函数）必须有 `return()` 语句，可放在 `main` 命途外部：

```stardance
thing double(x) {
    return x * 2;
}

main {
    thing main() {
        int r = double(21);
        see(r, "\n");
    }
}
```

### 变量声明

```
int a = 42;
float pi = 3.14;
str name = "hello";
bool flag = true;
```

不指定初始值则为 `null`：

```
int x;   // x = null
```

### 控制流

```
if (x > 0) {
    see("正数");
} else {
    see("非正数");
}

while (i < 10) {
    see(i);
    i++;
}

for (i = 0; i < 10; i++) {
    see(i);
}

foreach x in [10, 20, 30, 40, 50] {
    see(x);
}

foreach item in my_list {
    if (item > 0) {
        see(item);
    }
}

break;      // 跳出循环
continue;   // 继续下一次循环
cutdown;    // 跳出所有层循环
```

### 运算符

| 类型 | 运算符 |
|------|--------|
| 算术 | `+` `-` `*` `/` `%` `/^`（整除） |
| 移位 | `<<` `>>` `>>>` |
| 一元 | `-` `!` `++（前缀/后缀）` `--（前缀/后缀）` |
| 比较 | `==` `!=` `<` `>` `<=` `>=` |
| 逻辑 | `&&` `\|\|` |

```stardance
7 /^ 3          // → 2（整除）
1 << 5          // → 32（左移）
100 >> 3        // → 12（右移）
for (i = 0; i < 10; i++) { ... }  // 自增
```

> 💡 注意：`/` 返回浮点数（`7/3=2.33333`），`/^` 返回截断整数（`7/^3=2`）

### 内置函数

| 函数 | 说明 |
|------|------|
| `see(...)` | 打印一个或多个值到控制台 |
| `insert(prompt)` | 从标准输入读取一行字符串 |
| `int(x)` | 转换为整数 |
| `float(x)` | 转换为浮点数 |
| `str(x)` | 转换为字符串 |
| `bool(x)` | 转换为布尔值 |
| `type(x)` | 返回值的类型名 |
| `len(x)` | 返回长度 |
| **JSON / 文件 I/O** | |
| `json_encode(obj)` | 将对象编码为 JSON 字符串 |
| `json_decode(json_str)` | 将 JSON 字符串解码为对象 |
| `file_read(path)` | 读取文件内容为字符串 |
| `file_write(path, content)` | 写入字符串到文件 |
| `file_exists(path)` | 检查文件是否存在 |
| **字符串函数** | |
| `str_at(s, idx)` | 取字符串第 idx 个字符 |
| `str_sub(s, start, end)` | 取子串 [start, end)，end=-1 到末尾 |
| `str_find(s, pattern)` | 查找子串位置，返回索引或 -1 |
| `str_contains(s, pattern)` | 检查字符串是否包含子串 |
| `str_trim(s)` | 去除字符串两端空白 |
| `str_upper(s)` | 转大写 |
| `str_lower(s)` | 转小写 |
| `str_split(s, delimiter)` | 按分隔符分割字符串，返回带数字键的对象 |
| **FFI（外部函数接口）** | |
| `ffi_load(dll_path)` | 加载 DLL，返回句柄 |
| `ffi_free(handle)` | 释放 DLL |
| `ffi_call(handle, func_name, ret_type, ...)` | 调用 DLL 函数（ret_type: "i"=int, "f"=float, "v"=void） |

---

## 📦 包系统（Package）

`use` 关键字导入包，函数调用须使用 `包名.函数名()` 命名空间语法：

```stardance
start {
    use system;
    use rand;
}

main {
    thing main() {
        int pid = system.GetCurrentProcessId();  // 命名空间限定
        int n = rand.range(1, 6);                // 掷骰子
    }
}
```

### system 包（Windows 系统 API）

基于 FFI 调用 kernel32.dll / msvcrt.dll：

| 函数 | 说明 | 底层 |
|------|------|------|
| **进程/系统信息** | | |
| `GetTickCount64()` | 系统运行时间（毫秒） | kernel32!GetTickCount64 |
| `GetCurrentProcessId()` | 当前进程 ID | kernel32!GetCurrentProcessId |
| `GetCurrentThreadId()` | 当前线程 ID | kernel32!GetCurrentThreadId |
| `GetLastError()` | 最后错误码 | kernel32!GetLastError |
| `IsDebuggerPresent()` | 调试器检测 | kernel32!IsDebuggerPresent |
| `GetProcessVersion(pid)` | 进程版本 | kernel32!GetProcessVersion |
| **控制台** | | |
| `SetConsoleTitle(title)` | 设置控制台标题 | kernel32!SetConsoleTitleA |
| `GetStdHandle(dev)` | 获取标准句柄 | kernel32!GetStdHandle |
| **时间/定时** | | |
| `Sleep(ms)` | 休眠 | kernel32!Sleep |
| `uptime()` | 系统运行时间（秒） | GetTickCount64 / 1000 |
| **声音** | | |
| `Beep(freq, ms)` | 蜂鸣 | kernel32!Beep |
| **进程控制** | | |
| `exit(code)` | 退出进程 | kernel32!ExitProcess |
| **随机数** | | |
| `srandom(seed)` | 设置种子 | msvcrt!srand |
| `random()` | 随机整数 0..32767 | msvcrt!rand |
| `random_range(min, max)` | 范围随机 | rand + 取模 |

### rand 包（随机数专用）

```stardance
start { use rand; }

main {
    thing main() {
        rand.seed();           // 时间自动种子
        rand.seed_with(42);    // 自定义种子（可重复）
        int n = rand.next();   // 0..32767
        int d = rand.range(1, 6);  // 掷骰子
        float u = rand.unit(); // 0.0..1.0
    }
}
```

### webstar 包（HTTP 服务器）

```stardance
start { use webstar; }

main {
    thing main() {
        int srv = webstar.web_start(8080);
        int conn = webstar.web_accept(srv);
        str line = webstar.web_read_line(conn);
        str resp = webstar.web_response(200, "text/html", "<h1>OK</h1>");
        webstar.web_send(conn, resp);
        webstar.web_close(conn);
        webstar.web_close(srv);
    }
}
```

---

## 🧬 Life 命途（类）

StarDance 是面向对象语言，使用 `life` 定义类：

```stardance
life Animal {
    thing INIT(name) {          // 构造器（大写魔法方法）
        this.name = name;
    }
    thing speak() {
        see(this.name, " speaks\n");
    }
    thing STR() {               // 字符串转换
        return "Animal: " + this.name;
    }
    static thing create(n) {    // 静态方法
        return new Animal(n);
    }
    fix life Animal() {}        // 固定构造
    finish life Animal() {}     // 终结器
}
```

> ⚠️ life 命途目前仅完成解析器支持，编译器暂未实现运行时（在 `main` 命途所在文件中，`life` 必须嵌套在 `main{}` 内部）。

---

## 🏗️ 架构

### 编译流程

```
  .star 源码                .dance 字节码
  ┌──────────┐     ┌──────────────┐     ┌──────────┐
  │ star_    │────→│ compiler.py  │────→│ SDVM.exe │
  │ dance    │词法 │ 语法 → 字节码 │加载 │ 栈式解释 │
  │ 解析器   │语法 │              │执行 │ 执行引擎 │
  └──────────┘     └──────────────┘     └──────────┘
                      ↑
                  packages/*.star
                  （use 导入时递归编译）
```

### .dance 二进制格式

```
┌────────┬────────┬──────────┬─────────┬────────┬──────┐
│ Magic  │Version │StrCount  │ Strings │CodeSize│ Code │
│ "SDNC" │  u32   │   u32    │ (变长)  │  u32   │(变长)│
│  4字节  │  4字节  │  4字节    │         │  4字节  │      │
└────────┴────────┴──────────┴─────────┴────────┴──────┘
```

- **Magic**: `SDNC` (4 字节)
- **Version**: 1 (4 字节小端序)
- **StrCount**: 字符串常量数量
- **Strings**: 每个字符串 = length(u32) + UTF-8 编码内容
- **CodeSize**: 字节码大小
- **Code**: 指令序列

### 运行时栈

SDVM 使用基于值类型的栈式架构：

```
              ┌──────────────┐
    sp →      │   Value      │  ← 类型 + union 数据
              ├──────────────┤
              │   ...        │
              ├──────────────┤
              │   locals[]   │  ← 256 个局部变量槽
              └──────────────┘
```

每个 `Value` 包含：
- `type`: INT / FLOAT / BOOL / STRING / NULL / OBJECT / FUNC
- `data`: `int64` / `double` / `bool` / `char*` / `void*` (Object指针)

**OBJECT 类型**：`['key1': val1, 'key2': val2]` 字典风格，支持下标访问。
**FUNC 类型**：匿名函数引用，通过 `anonymou` 关键字创建。

### 指令集

| 指令 | Opcode | 说明 |
|------|--------|------|
| **栈操作** | | |
| `ICONST` | 0x00 | 压入 int32 常量 |
| `FCONST` | 0x01 | 压入 double 常量 |
| `SCONST` | 0x02 | 压入字符串（常量池索引） |
| `BCONST` | 0x03 | 压入布尔值 |
| `NULL` | 0x04 | 压入 null |
| `DUP` | 0x05 | 复制栈顶 |
| `POP` | 0x06 | 弹出栈顶 |
| **局部变量** | | |
| `LOAD` | 0x10 | 局部变量 → 栈 |
| `STORE` | 0x11 | 栈 → 局部变量 |
| **算术** | | |
| `ADD` | 0x20 | + |
| `SUB` | 0x21 | - |
| `MUL` | 0x22 | * |
| `DIV` | 0x23 | / |
| `MOD` | 0x24 | % |
| `NEG` | 0x25 | 一元负号 |
| `IDIV` | 0x26 | `/^` 整除（整数除法截断） |
| `SHL` | 0x27 | `<<` 左移 |
| `SHR` | 0x28 | `>>` 右移（算术右移） |
| `USHR` | 0x29 | `>>>` 无符号右移 |
| **比较** | | |
| `EQ` | 0x30 | == |
| `NE` | 0x31 | != |
| `LT` | 0x32 | < |
| `GT` | 0x33 | > |
| `LE` | 0x34 | <= |
| `GE` | 0x35 | >= |
| **逻辑** | | |
| `NOT` | 0x40 | ! |
| **控制流** | | |
| `JMP` | 0x50 | 无条件跳转（相对偏移） |
| `JIF` | 0x51 | false 时跳转 |
| `BIF` | 0x52 | 调用内置函数 |
| `RET` | 0x53 | 返回 |
| `HALT` | 0x54 | 停止 |
| `CALL` | 0x55 | 调用函数（函数式） |
| `CALLR` | 0x56 | 调用函数（过程式） |
| `ANON` | 0x57 | 创建匿名函数闭包 |
| **I/O** | | |
| `PRINT` | 0x60 | 打印栈顶值 |
| `SCAN` | 0x61 | 读取输入到栈 |
| **对象操作** | | |
| `NEWOBJ` | 0x62 | 创建对象（压栈 key→value→NEWOBJ） |
| `GETINDEX` | 0x63 | 下标访问 `obj[idx]` |
| `SETINDEX` | 0x64 | 下标赋值 `obj[idx]=val` |
| `GETATTR` | 0x65 | 属性访问 `obj.attr` → value |
| `SETATTR` | 0x66 | 属性赋值 `obj.attr = val` |

---

## 🛠️ 开发

### 运行全部测试

```bash
python ..\batch_test2.py
```

### 单独编译运行

```bash
# 编译
python compiler.py test_xxx.star -o tcc_src\test_xxx.dance
# 运行
sdvm_new.exe tcc_src\test_xxx.dance
```

---

## 📝 注意

- **Windows 编码**：控制台需要在 UTF-8 模式下运行（`chcp 65001`），运行时已自动设置
- **函数必须有 return()**：所有模块级 `thing` 函数必须以 `return()` 结尾（可返回 null）
- **包命名空间**：包函数必须使用 `包名.函数名()` 方式调用，不可直接暴露到全局
