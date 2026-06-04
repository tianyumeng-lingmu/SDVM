# ⭐ SDVM — 星舞虚拟机

**Star Dance Virtual Machine** — 类 JVM 栈式虚拟机，执行 `.dance` 字节码。

SDVM 由两部分组成：

- **编译器**（Python）— 将 `.star` 源码编译为 `.dance` 字节码
- **运行时**（C 语言）— 加载并执行 `.dance` 字节码

---

## 📁 目录结构

```
SDVM/
├── python/              ← Python 编译器源码
│   └── compiler.py      .star → .dance 字节码编译器
│
├── src/                 ← C 语言运行时源码
│   ├── main.c           入口，文件加载 + 命令行参数
│   ├── sdvm.c           虚拟机核心（取指、栈操作、内置函数）
│   └── sdvm.h           指令定义、Value 类型、API
│
├── release/             ← 编译版可执行程序 + 运行脚本
│   ├── SDVM.exe         已编译的虚拟机
│   ├── sdvm.bat         批处理运行脚本
│   ├── sdvm.ps1         PowerShell 运行脚本
│   └── run_sdvm.bat     简易运行脚本（当前目录模式）
│
├── samples/             ← Star 语言示例程序
│   ├── sa.star          交互式计算器
│   ├── test_full.star   功能测试（循环、条件、变量）
│   └── ces.star         其它测试示例
│
├── SDVM.slnx            Visual Studio 解决方案
├── SDVM.vcxproj         MSBuild 项目文件
└── .gitignore           Git 忽略配置
```

---

## 🚀 快速开始

### 运行示例

```bash
# 方式一：使用批处理脚本（推荐）
sdvm samples\sa.star

# 方式二：直接运行
python python\compiler.py samples\sa.star -o samples\sa.dance
release\SDVM.exe samples\sa.dance
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
- `type`: INT / FLOAT / BOOL / STRING / NULL
- `data`: `int64` / `double` / `bool` / `char*`

### 指令集

| 指令 | 说明 |
|------|------|
| **栈操作** | |
| `ICONST` | 压入 int32 常量 |
| `FCONST` | 压入 double 常量 |
| `SCONST` | 压入字符串（常量池索引） |
| `BCONST` | 压入布尔值 |
| `NULL` | 压入 null |
| `DUP` / `POP` | 复制 / 弹出栈顶 |
| **局部变量** | |
| `LOAD` / `STORE` | 局部变量 ↔ 栈 |
| **算术** | `ADD` `SUB` `MUL` `DIV` `MOD` `NEG` |
| **比较** | `EQ` `NE` `LT` `GT` `LE` `GE` |
| **逻辑** | `NOT` |
| **控制流** | |
| `JMP` | 无条件跳转（相对偏移） |
| `JIF` | false 时跳转 |
| `BIF` | 调用内置函数 |
| `RET` / `HALT` | 返回 / 停止 |
| **I/O** | |
| `PRINT` | 打印栈顶值 |
| `SCAN` | 读取输入到栈 |

---

## 🛠️ 开发

### 从源码编译

依赖：Visual Studio 2026+（v143 / v145 工具集）

```bash
# 用 MSBuild 编译
msbuild SDVM.vcxproj /p:Configuration=Release /p:Platform=x64

# 或在 Visual Studio 中直接打开 SDVM.slnx 编译
```

### 编译器

```bash
# 编译 .star → .dance
python python\compiler.py samples\test_full.star -o samples\test_full.dance

# 调试模式执行
release\SDVM.exe samples\test_full.dance -v
```

### 测试

```bash
# 完整的测试
sdvm samples\test_full.star
```

---

## 📝 注意

- **Windows 编码**：控制台需要在 UTF-8 模式下运行（`chcp 65001`），运行时已自动设置
- **PowerShell 管道输入**：通过管道传值到 `sdvm.ps1` 可能因 BOM 问题异常，建议直接交互式使用
- **PATH**：将 `D:\stay\SDVM` 加入环境变量，即可随处使用 `sdvm` 命令
