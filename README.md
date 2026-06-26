# SDVM — 星舞虚拟机

**Star Dance Virtual Machine** — 类 JVM 栈式虚拟机，执行 `.dance` 字节码。

> 当前版本：v2.4 — 支持 FFI、包系统、namespace 调用、life 命途（含继承和抽象类）、对象/JSON、文件 I/O、字符串操作、匿名函数、位运算、foreach 遍历、生成器（pause）

SDVM 由三部分组成：
- **前端解析器**（Python）— `star_dance/` 词法/语法分析，生成 AST
- **编译器**（Python）— 将 AST 编译为 `.dance` 字节码
- **运行时**（C 语言）— 加载并执行 `.dance` 字节码

---

## 目录结构

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

## 快速开始

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

## StarDance 语言语法

### 程序入口

每个可执行文件必须有 `main` 命途，内含 `thing main()` 作为主程序入口：

```stardance
main {
    thing main() {
        see("Hello, World!\n");
    }
}
```

模块级 `thing`（函数）必须有 `return()` 或 `pause` 语句，可放在 `main` 命途外部：

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

```stardance
int a = 42;
float pi = 3.14;
str name = "hello";
bool flag = true;
```

不指定初始值则为 `null`：

```stardance
int x;   // x = null
```

### 控制流

```stardance
if (x > 0) {
    see("正数");
} else {
    see("非正数");
}

while (i < 10) {
    see(i);
    i = i + 1;
}

for (i = 0; i < 10; i = i + 1) {
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

### 生成器（Generator）

使用 `pause` 关键字创建生成器函数。调用生成器函数返回一个生成器对象，通过 `next()` 逐次获取值：

```stardance
thing myrange(from_val, to_val) {
    var i = from_val;
    while (i < to_val) {
        pause i;
        i = i + 1;
    }
    return(null);
}

main {
    thing main() {
        var gen = myrange(3, 8);
        var v = next(gen);   // 3
        see(v);
        v = next(gen);       // 4
        see(v);
        v = next(gen);       // 5
        see(v);
        v = next(gen);       // null（已耗尽）
        see(v);
    }
}
```

规则：
- 包含 `pause` 语句的函数自动成为生成器函数
- 调用生成器函数时**不执行函数体**，而是返回一个 `VAL_GENERATOR` 对象
- `next(gen)` 首次启动生成器，之后每次恢复至上一次 `pause` 的位置继续执行
- 生成器耗尽（函数执行到 `return` 或末尾）后，`next()` 返回 `null`
- 所有模块级 `thing` 函数必须有 `return` 或 `pause`（二选一）

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
for (i = 0; i < 10; i = i + 1) { ... }  // 自增
```

> 注意：`/` 返回浮点数（`7/3=2.33333`），`/^` 返回截断整数（`7/^3=2`）

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
| **生成器** | |
| `next(gen)` | 获取生成器的下一个值，耗尽返回 `null` |
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

## 包系统（Package）

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

## Life 命途（类）

StarDance 是面向对象语言，使用 `life` 定义类。在 `main` 命途所在文件中，`life` 必须嵌套在 `main{}` 内部。

### 基本用法

```stardance
life Animal {
    thing INIT(name) {          // 构造器（大写魔法方法）
        this.name = name;
    }
    thing speak() {
        see(this.name, " speaks\n");
    }
}

main {
    thing main() {
        object a = new Animal("Tom");
        a.speak();              // Tom speaks
    }
}
```

### 继承

使用 `join`（或 `extends`）关键字继承父类：

```stardance
life Dog join Animal {
    thing speak() {
        see(this.name, " barks\n");
    }
}

main {
    thing main() {
        object d = new Dog("Buddy");
        d.speak();              // Buddy barks
    }
}
```

### 抽象命途

当命途中的所有 `thing` 方法体均为 `pass` 时，该命途视为**抽象命途**。抽象命途不能被实例化（`new` 会报错误 18）。继承抽象命途的子类必须实现所有抽象方法：

```stardance
life Animal {
    thing speak() { pass; }     // 抽象方法
    thing walk()  { pass; }     // 抽象方法
}

life Dog join Animal {
    thing speak() {
        see("bark\n");           // 实现 speak
    }
    thing walk() { pass; }      // 仍可为 pass，继续留给子类
}

life Puppy join Dog {
    thing walk() {
        see("walk\n");           // 实现 walk
    }
}

main {
    thing main() {
        object p = new Puppy("Buddy");  // OK：所有抽象方法已实现
        p.speak();              // bark
        p.walk();               // walk
    }
}
```

规则：
- 抽象命途不需要 `abstract` 关键字，全 `pass` 方法体自动判定
- `new` 抽象命途直接报错：错误 18
- 子类未实现全部抽象方法也报错：错误 18
- 抽象命途不能 `join` 抽象命途（编译期报错）
- 非抽象命途可实例化，未实现且无继承的方法调用时返回 `null`

### 其他修饰符

```stardance
static thing create(n) {    // 静态方法
    return new Animal(n);
}
fix life Animal() {}        // 固定构造（不可修改）
finish life Animal() {}     // 终结器（禁止继承）
```

---

## 架构

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

### .dance 二进制格式（v2）

```
┌────────┬────────┬──────────┬─────────┬──────────┬────────┬──────┐
│ Magic  │Version │StrCount  │ Strings │FuncCount │ Funcs  │Code  │
│ "SDNC" │  u32   │   u32    │ (变长)  │   u32    │(变长)  │Size  │
│  4字节  │  4字节  │  4字节    │         │  4字节    │        │u32   │
├────────┴────────┴──────────┴─────────┴──────────┴────────┴──────┤
│                          Code Data (变长)                        │
└──────────────────────────────────────────────────────────────────┘
```

- **Magic**: `SDNC` (4 字节)
- **Version**: 2 (4 字节小端序) — v2 支持函数表
- **StrCount**: 字符串常量数量
- **Strings**: 每个字符串 = length(u32) + UTF-8 编码内容，以 `0xFFFFFFFF` 结束
- **FuncCount**: 函数数量（含 main）
- **Funcs**: 每个函数条目 = name_idx(i32) + arg_count(u32) + local_count(u32) + code_offset(u32)，共 16 字节
- **CodeSize**: 字节码总大小
- **Code**: 主代码 + 所有函数代码连续存放

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
- `type`: INT / FLOAT / BOOL / STRING / NULL / FUNC / OBJECT / GENERATOR
- `data`: `int64` / `double` / `bool` / `char*` / `void*` (Object/Generator 指针)

**OBJECT 类型**：`['key1': val1, 'key2': val2]` 字典风格，支持下标访问。
**FUNC 类型**：匿名函数引用，通过 `anonymou` 关键字创建。
**GENERATOR 类型**：生成器对象，通过调用含 `pause` 的函数创建。

### 指令集

| 指令 | Opcode | 说明 |
|------|--------|------|
| **栈操作** | | |
| `ICONST` | 0x01 | 压入 int32 常量（+4 字节） |
| `FCONST` | 0x02 | 压入 double 常量（+8 字节） |
| `SCONST` | 0x03 | 压入字符串（常量池索引，+4 字节） |
| `BCONST` | 0x04 | 压入布尔值（+1 字节） |
| `NULL` | 0x05 | 压入 null |
| `DUP` | 0x06 | 复制栈顶 |
| `POP` | 0x07 | 弹出栈顶 |
| `SWAP` | 0x08 | 交换栈顶两个元素 |
| **局部变量** | | |
| `LOAD` | 0x10 | 局部变量 → 栈（+1 字节索引） |
| `STORE` | 0x11 | 栈 → 局部变量（+1 字节索引） |
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
| `NOT` | 0x38 | ! |
| **控制流** | | |
| `JMP` | 0x40 | 无条件跳转（相对偏移，+4 字节） |
| `JIF` | 0x41 | false 时跳转（+4 字节） |
| `BIF` | 0x42 | 调用内置函数（+1 索引 +1 参数数） |
| `RET` | 0x43 | 返回 |
| `HALT` | 0x44 | 停止 |
| `PAUSE` | 0x45 | 暂停生成器，弹出值返回给 `next()` 调用者 |
| **函数调用** | | |
| `CALL` | 0x50 | 调用已命名函数（+4 func_idx +1 arg_count） |
| `ANON` | 0x51 | 推送匿名函数引用（+4 func_idx） |
| `CALLR` | 0x52 | 动态调用（栈顶为函数引用，+1 arg_count） |
| **I/O** | | |
| `PRINT` | 0x70 | 打印栈顶值 |
| `SCAN` | 0x71 | 读取输入到栈 |
| **对象操作** | | |
| `NEWOBJ` | 0x60 | 创建对象（+1 num_pairs，从栈取 key-value） |
| `GETATTR` | 0x61 | 属性访问（+4 strpool_idx） |
| `SETATTR` | 0x62 | 属性赋值（+4 strpool_idx） |
| `GETINDEX` | 0x63 | 下标访问 `obj[idx]` |
| `SETINDEX` | 0x64 | 下标赋值 `obj[idx]=val` |

---

## 开发

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

## 注意

- **Windows 编码**：控制台需要在 UTF-8 模式下运行（`chcp 65001`），运行时已自动设置
- **函数必须有 return() 或 pause**：所有模块级 `thing` 函数必须以 `return()` 或 `pause` 结尾（生成器用 pause，普通函数用 return）
- **包命名空间**：包函数必须使用 `包名.函数名()` 方式调用，不可直接暴露到全局
