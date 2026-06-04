# ⭐ SDVM — 星舞虚拟机

**Star Dance Virtual Machine** — 类 JVM 栈式虚拟机，执行 `.dance` 字节码。

> 当前版本：v2.0 — 支持对象、JSON、文件 I/O、字符串索引与字符串函数

SDVM 由两部分组成：

- **编译器**（Python）— 将 `.star` 源码编译为 `.dance` 字节码
- **运行时**（C 语言）— 加载并执行 `.dance` 字节码

---

## 📁 目录结构

```
SDVM/
├── compiler.py          .star → .dance 字节码编译器（Python）
├── sdvm.c               虚拟机核心（C 语言运行时）
├── sdvm.h               指令定义、Value 类型、API
│
├── tcc_src/             使用 TCC（Tiny C Compiler）编译
│   ├── main.c           入口
│   ├── sdvm.c           虚拟机副本
│   ├── sdvm.h           头文件副本
│   ├── stdint.h         自定义 stdint（TCC 兼容）
│   ├── stddef.h         自定义 stddef（TCC 兼容）
│   ├── winsock2.h       自定义 Winsock 头（网络功能）
│   └── ws2tcpip.h       自定义 ws2tcpip 头
│
├── *.star               测试脚本
│   ├── test_string.star 字符串索引 + 字符串 BIF 测试
│   ├── test_object.star 对象/JSON/文件 I/O 测试
│   ├── test_full.star   综合功能测试
│   └── ...
│
├── sdvm_new.exe         TCC 编译的虚拟机可执行文件
└── triangle.star        蓝桥杯等腰三角形判定解法
```

---

## 🚀 快速开始

### 运行示例

```bash
# 方式一：直接用编译器 + VM
python compiler.py samples\test_full.star -o samples\test_full.dance
sdvm_new.exe samples\test_full.dance

# 方式二：调试模式
python compiler.py samples\test_full.star
sdvm_new.exe samples\test_full.dance -v
```

### 从源码编译（TCC）

依赖：TCC (Tiny C Compiler)

```bash
cd tcc_src
tcc sdvm.c main.c -I. -o sdvm_new.exe
```

### 从源码编译（MSVC）

依赖：Visual Studio 工具集

```bash
cl sdvm.c main.c /Fe:sdvm_new.exe /I.
```

### 运行计算器

```
> sdvm samples\sa.star
╔══════════════════════════╗
║    ✦ 星舞计算器 ✦       ║
╚══════════════════════════╝

支持的运算:
  +  加法          -  减法
  *  乘法          /  除法
  %  取模          ^  幂运算

输入 .exit 退出计算器
───────────────────────────

输入第一个数: 12
输入运算符 (+, -, *, /, %, ^): +
输入第二个数: 34

────────────────────
  12 + 34 = 46
────────────────────

继续计算？(y/n): n
```

---

## 💻 Star 语言语法

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

### 控制流

```
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

break;      // 跳出循环
continue;   // 继续下一次循环
cutdown;    // 跳出所有层循环
```

### 运算符

| 类型 | 运算符 |
|------|--------|
| 算术 | `+` `-` `*` `/` `%` |
| 一元 | `-` `!` `++` `--` |
| 比较 | `==` `!=` `<` `>` `<=` `>=` |
| 逻辑 | `&&` `\|\|` |

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

**OBJECT 类型**：`['key1': val1, 'key2': val2]` 字典风格列表，支持下标访问。
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

### 运行测试

```bash
# 字符串测试
python compiler.py test_string.star -o test_string.dance
sdvm_new.exe test_string.dance

# 对象/JSON/文件测试
python compiler.py test_object.star -o test_object.dance
sdvm_new.exe test_object.dance
```

---

## 📝 注意

- **Windows 编码**：控制台需要在 UTF-8 模式下运行（`chcp 65001`），运行时已自动设置
- **PowerShell 管道输入**：通过管道传值到 `sdvm.ps1` 可能因 BOM 问题异常，建议直接交互式使用
- **PATH**：将 `D:\stay\SDVM` 加入环境变量，即可随处使用 `sdvm` 命令
