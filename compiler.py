#!/usr/bin/env python3
"""
╔═══════════════════════════════════════════════════╗
║  SDVM 编译器 — .star → .dance 字节码             ║
║  使用 star_dance 的词法/语法分析器，生成 SDVM      ║
║  可执行的 .dance 二进制文件                        ║
╚═══════════════════════════════════════════════════╝

用法:
    python compiler.py <输入.star> [-o <输出.dance>]

工作原理:
    1. 导入 star_dance 的 lexer + parser 解析 .star 文件
    2. 遍历 AST 为每个节点发射字节码指令
    3. 写出 .dance 二进制: Magic(4) + Version(4) +
       StrCount(4) + Strings + FuncTable + CodeSize(4) + Code
"""

import struct
import sys
import os

# ─── 导入 star_dance 的前端 ──────────────────────
_star_dance_path = os.path.join(os.path.dirname(__file__), '..', 'star_dance')
if os.path.isdir(_star_dance_path):
    sys.path.insert(0, _star_dance_path)
else:
    sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

try:
    from lexer import Lexer
    from parser import Parser
    from ast_nodes import (
        Program, StartBlock, MainBlock, Block,
        VarDecl, ConstDecl, Assign, SeeStmt, IfStmt, WhileStmt, ForStmt,
        ForeachStmt, CaseStmt, WhenClause,
        BreakStmt, ContinueStmt, CutDownStmt,
        BinaryOp, UnaryOp, Literal, Identifier,
        CallExpr, ExprStmt, LifeDecl, ThingDecl, ReturnStmt,
        ListLiteral, NewExpr, GetAttr, IndexExpr, ThrowStmt, TryStmt,
        AnonymouFunc, NamedArgument, UseStmt,
    )
    from tokens import Token
except ImportError as e:
    print(f"错误: 无法导入 star_dance 模块 — {e}")
    print("提示: compiler.py 需要在 star_dance 同级或上级目录运行")
    sys.exit(1)


# ═══════════════════════════════════════════════════
#  指令编码常量 (与 sdvm.h 保持一致)
# ═══════════════════════════════════════════════════

# 栈操作 0x00-0x0F
OP_NOP     = 0x00
OP_ICONST  = 0x01  # +4: int32 LE
OP_FCONST  = 0x02  # +8: double
OP_SCONST  = 0x03  # +4: strpool index
OP_BCONST  = 0x04  # +1: 0/1
OP_NULL    = 0x05
OP_DUP     = 0x06
OP_POP     = 0x07

# 局部变量 0x10-0x1F
OP_LOAD    = 0x10  # +1: local index
OP_STORE   = 0x11  # +1: local index

# 算术 0x20-0x2F
OP_ADD     = 0x20
OP_SUB     = 0x21
OP_MUL     = 0x22
OP_DIV     = 0x23
OP_MOD     = 0x24
OP_NEG     = 0x25

# 比较 0x30-0x37
OP_EQ      = 0x30
OP_NE      = 0x31
OP_LT      = 0x32
OP_GT      = 0x33
OP_LE      = 0x34
OP_GE      = 0x35

# 逻辑 0x38-0x3F
OP_NOT     = 0x38

# 控制流 0x40-0x4F
OP_JMP     = 0x40  # +4: signed offset
OP_JIF     = 0x41  # +4: jump if false
OP_BIF     = 0x42  # +1: bif_idx, +1: argc
OP_RET     = 0x43
OP_HALT    = 0x44

# 函数 0x50-0x5F
OP_CALL    = 0x50  # +4: func_idx, +1: arg_count (编译时已知的函数)
OP_ANON    = 0x51  # +4: func_idx (推送函数引用)
OP_CALLR   = 0x52  # +1: arg_count (从栈顶弹出函数引用并调用)

# 对象操作 0x60-0x6F
OP_NEWOBJ  = 0x60  # +1: num_pairs (从栈取 key-value 对创建对象)
OP_GETATTR = 0x61  # +4: strpool_idx (读取对象属性)
OP_SETATTR = 0x62  # +4: strpool_idx (设置对象属性)
OP_GETINDEX = 0x63 # - : 下标访问 expr[idx]
OP_SETINDEX = 0x64 # - : 下标赋值 expr[idx] = val

# I/O 0x70-0x7F
OP_PRINT   = 0x70
OP_SCAN    = 0x71

# 内置函数索引
BIF_SEE    = 0
BIF_INT    = 1
BIF_FLOAT  = 2
BIF_STR    = 3
BIF_BOOL   = 4
BIF_TYPE   = 5
BIF_ID     = 6
BIF_LEN    = 7
BIF_INSERT = 8

# 网络 BIF
BIF_NET_START   = 10  # net_start(port) → server_handle
BIF_NET_ACCEPT  = 11  # net_accept(handle) → client_handle
BIF_NET_READLINE = 12 # net_readline(handle) → string
BIF_NET_WRITE   = 13  # net_write(handle, string) → void
BIF_NET_CLOSE   = 14  # net_close(handle) → void

# JSON BIF
BIF_JSON_ENCODE = 16  # json_encode(val) → JSON 字符串
BIF_JSON_DECODE = 17  # json_decode(str) → 值

# 文件 BIF
BIF_FILE_READ   = 18  # file_read(path) → 字符串
BIF_FILE_WRITE  = 19  # file_write(path, content) → void
BIF_FILE_EXISTS = 20  # file_exists(path) → bool

# 字符串 BIF
BIF_STR_AT       = 21  # str_at(s, idx) → str
BIF_STR_SUB      = 22  # str_sub(s, start, end) → str
BIF_STR_FIND     = 23  # str_find(s, pattern) → int
BIF_STR_CONTAINS = 24  # str_contains(s, pattern) → bool
BIF_STR_TRIM     = 25  # str_trim(s) → str
BIF_STR_UPPER    = 26  # str_upper(s) → str
BIF_STR_LOWER    = 27  # str_lower(s) → str
BIF_STR_SPLIT    = 28  # str_split(s, delimiter) → object
BIF_FFI_LOAD     = 29  # ffi_load(path) → int
BIF_FFI_FREE     = 30  # ffi_free(handle) → void
BIF_FFI_CALL     = 31  # ffi_call(handle, name, ret_type, ...) → value

BIF_MAP = {
    'see': BIF_SEE,
    'int': BIF_INT,
    'float': BIF_FLOAT,
    'str': BIF_STR,
    'bool': BIF_BOOL,
    'type': BIF_TYPE,
    'ID': BIF_ID,
    'len': BIF_LEN,
    'insert': BIF_INSERT,
    'net_start': BIF_NET_START,
    'net_accept': BIF_NET_ACCEPT,
    'net_readline': BIF_NET_READLINE,
    'net_write': BIF_NET_WRITE,
    'net_close': BIF_NET_CLOSE,
    'json_encode': BIF_JSON_ENCODE,
    'json_decode': BIF_JSON_DECODE,
    'file_read':   BIF_FILE_READ,
    'file_write':  BIF_FILE_WRITE,
    'file_exists': BIF_FILE_EXISTS,
    'str_at':      BIF_STR_AT,
    'str_sub':     BIF_STR_SUB,
    'str_find':    BIF_STR_FIND,
    'str_contains': BIF_STR_CONTAINS,
    'str_trim':    BIF_STR_TRIM,
    'str_upper':   BIF_STR_UPPER,
    'str_lower':   BIF_STR_LOWER,
    'str_split':   BIF_STR_SPLIT,
    'ffi_load':    BIF_FFI_LOAD,
    'ffi_free':    BIF_FFI_FREE,
    'ffi_call':    BIF_FFI_CALL,
}


class CompileError(Exception):
    """编译错误"""
    pass


class BackpatchEntry:
    """回填记录"""
    def __init__(self, offset_pos: int, target_label: str):
        self.offset_pos = offset_pos
        self.target_label = target_label


class FuncDef:
    """函数定义"""
    def __init__(self, name, param_names, body, is_anonymous=False):
        self.name = name           # str | None (匿名为 None)
        self.param_names = list(param_names)  # 参数名列表（按顺序）
        self.body = body           # list[ASTNode]
        self.is_anonymous = is_anonymous
        
        # 编译上下文
        self.code = bytearray()
        # 参数占据 slots 0..n-1
        self.locals = {p: i for i, p in enumerate(param_names)}
        self.next_local = len(param_names)
        self.labels = {}
        self.backpatches = []
        self.loop_labels = []
        self.code_offset = 0  # 最终布局时填入


class Compiler:
    """.star → .dance 编译器"""

    def __init__(self):
        self.code = bytearray()          # 主代码（main）字节码
        self.strpool = []                # 字符串常量池
        self.strpool_map = {}            # 字符串 → 索引
        self.locals = {}                 # main 的变量名 → slot
        self.next_local = 0              # main 的下一个变量槽
        self.labels = {}                 # main 的标签 → 位置
        self.backpatches = []            # main 的待回填列表
        self.loop_labels = []            # main 的循环标签栈
        self.errors = 0                  # 编译错误计数
        
        # 函数表
        self.func_defs = []              # list[FuncDef] (index 0 = main)
        self.func_map = {}               # name → index in func_defs
        self._saved_ctx = None           # 函数上下文切换时使用

    # ─── 上下文切换 ────────────────────────────
    def _push_func_context(self, func):
        """切换到函数的编译上下文"""
        self._saved_ctx = (
            self.code, self.locals, self.next_local,
            self.labels, self.backpatches, self.loop_labels
        )
        self.code = func.code
        self.locals = func.locals
        self.next_local = func.next_local
        self.labels = func.labels
        self.backpatches = func.backpatches
        self.loop_labels = func.loop_labels

    def _pop_func_context(self, func):
        """恢复主函数的编译上下文，保存函数状态"""
        # 保存函数的最终状态
        func.code = self.code
        func.locals = self.locals
        func.next_local = self.next_local
        func.labels = self.labels
        func.backpatches = func.backpatches
        func.loop_labels = func.loop_labels
        
        # 恢复主上下文
        (self.code, self.locals, self.next_local,
         self.labels, self.backpatches, self.loop_labels) = self._saved_ctx
        self._saved_ctx = None

    # ─── 字符串常量池 ────────────────────────────
    def add_string(self, s: str) -> int:
        if s not in self.strpool_map:
            idx = len(self.strpool)
            self.strpool.append(s)
            self.strpool_map[s] = idx
            return idx
        return self.strpool_map[s]

    # ─── 局部变量 ───────────────────────────────
    def alloc_local(self, name: str) -> int:
        if name not in self.locals:
            self.locals[name] = self.next_local
            self.next_local += 1
        return self.locals[name]

    def get_local(self, name: str) -> int:
        if name not in self.locals:
            raise CompileError(f"变量 '{name}' 未声明")
        return self.locals[name]

    # ─── 字节码发射 ─────────────────────────────
    def emit(self, byte: int):
        self.code.append(byte)

    def emit_u32(self, val: int):
        self.code.extend(struct.pack('<I', val))

    def emit_i32(self, val: int):
        self.code.extend(struct.pack('<i', val))

    def emit_f64(self, val: float):
        self.code.extend(struct.pack('<d', val))

    def emit_u8(self, val: int):
        self.code.append(val & 0xFF)

    def get_pos(self) -> int:
        return len(self.code)

    # ─── 标签 / 回填 ────────────────────────────
    def define_label(self, name: str):
        self.labels[name] = self.get_pos()

    def add_backpatch(self, offset_pos: int, target_label: str):
        self.backpatches.append(BackpatchEntry(offset_pos, target_label))

    def resolve_backpatches(self):
        for bp in self.backpatches:
            if bp.target_label not in self.labels:
                raise CompileError(f"标签 '{bp.target_label}' 未定义")
            target = self.labels[bp.target_label]
            offset = target - (bp.offset_pos + 4)
            self.code[bp.offset_pos:bp.offset_pos+4] = struct.pack('<i', offset)

    def _has_return_in_body(self, body) -> bool:
        """递归检查语句列表中是否包含 return 语句"""
        for stmt in body:
            if isinstance(stmt, ReturnStmt):
                return True
            if isinstance(stmt, Block):
                if self._has_return_in_body(stmt.statements):
                    return True
            if isinstance(stmt, IfStmt):
                if self._has_return_in_body(stmt.then_block):
                    return True
                if stmt.else_block and self._has_return_in_body(stmt.else_block):
                    return True
            if isinstance(stmt, WhileStmt):
                if self._has_return_in_body(stmt.body):
                    return True
            if isinstance(stmt, ForStmt):
                if self._has_return_in_body(stmt.body):
                    return True
            if isinstance(stmt, ForeachStmt):
                if self._has_return_in_body(stmt.body):
                    return True
            if isinstance(stmt, CaseStmt):
                for wc in stmt.when_clauses:
                    if self._has_return_in_body(wc.body):
                        return True
                if self._has_return_in_body(stmt.else_body):
                    return True
        return False

    def _flatten_body(self, body):
        stmts = []
        for s in body:
            if isinstance(s, Block):
                stmts.extend(s.statements)
            else:
                stmts.append(s)
        return stmts

    def _compile_body(self, body):
        for s in self._flatten_body(body):
            self.compile_statement(s)

    def _get_packages_dir(self) -> str:
        """返回包目录路径 (compiler.py 同级的 packages/)"""
        return os.path.join(os.path.dirname(os.path.abspath(__file__)), 'packages')

    def _import_package(self, package_name: str, visited: set = None):
        """导入包：找到并解析 packages/{name}.star，将其 thing 声明加入函数表

        Args:
            package_name: 包名 (如 'system')
            visited: 已导入的包集合（用于递归时检测循环依赖）
        """
        if visited is None:
            visited = set()
        if package_name in visited:
            raise CompileError(f"循环包依赖检测: '{package_name}' 已被导入")
        visited.add(package_name)

        pkg_dir = self._get_packages_dir()
        pkg_path = os.path.join(pkg_dir, f'{package_name}.star')
        if not os.path.exists(pkg_path):
            raise CompileError(f"找不到包 '{package_name}' (搜索路径: {pkg_path})")

        # 解析包文件
        from lexer import Lexer
        from parser import Parser
        with open(pkg_path, 'r', encoding='utf-8') as f:
            source = f.read()
        lexer = Lexer(source)
        tokens = lexer.tokenize()
        parser = Parser(tokens)
        pkg_ast = parser.parse()

        # 包文件不允许有 main{} 命途块
        if pkg_ast.main_block and len(pkg_ast.main_block.statements) > 0:
            raise CompileError(
                f"包 '{package_name}' 不能包含 main 命途块")

        # 处理包内的 start{} 块：递归处理 use 语句（包依赖）
        if pkg_ast.start_block:
            for stmt in pkg_ast.start_block.statements:
                if isinstance(stmt, UseStmt):
                    self._import_package(stmt.package_name, visited)
                # 非 use 语句（如初始化代码）暂时不处理
                # 后续可扩展为编译注入到主程序的 start 块中

        # 将包内的 thing 声明加入当前编译的函数表
        for decl in pkg_ast.func_decls:
            if decl.name in self.func_map:
                raise CompileError(
                    f"导入包 '{package_name}' 时发生命名冲突: 函数 '{decl.name}' 已存在")
            if not self._has_return_in_body(decl.body):
                raise CompileError(
                    f"包 '{package_name}' 中的函数 '{decl.name}' 缺少 return() 语句")
            func_index = len(self.func_defs) + 1  # +1 for main(func 0)
            func = FuncDef(decl.name, decl.params, decl.body)
            self.func_defs.append(func)
            self.func_map[decl.name] = func_index

    # ─── 主编译入口 ─────────────────────────────
    def compile(self, ast: Program):
        """编译整个 Program"""
        self.errors = 0
        
        try:
            # ★ 文件级约束检查：一个文件不能同时有 lifecycle 块和函数声明
            has_lifecycle = ast.start_block is not None or ast.main_block is not None
            has_funcs = len(ast.func_decls) > 0
            if has_lifecycle and has_funcs:
                raise CompileError(
                    "文件不能同时包含 start/main 命途块和 thing 函数声明。"
                    "请将函数分离到独立的包文件中。")
            if ast.main_block and ast.start_block is None:
                raise CompileError(
                    "main 命途必须与 start 命途同时存在")

            # 0. 如果只有 lifecycle（没有函数），确保 func_defs 包含 main
            if not has_funcs:
                pass  # func 0 就是 main，不需要额外处理

            # 收集模块级函数声明并验证
            func_index = 1  # index 0 是 main
            for decl in ast.func_decls:
                # 验证函数必须包含 return() 语句
                if not self._has_return_in_body(decl.body):
                    raise CompileError(
                        f"函数 '{decl.name}' 缺少 return() 语句 — "
                        f"所有模块级 thing（函数）必须有 return() 语句，即使只是 return(null);"
                    )
                func = FuncDef(decl.name, decl.params, decl.body)
                self.func_defs.append(func)
                self.func_map[decl.name] = func_index
                func_index += 1
            
            # 1. 先编译 start 块
            if ast.start_block:
                self._compile_body(ast.start_block.statements)
            
            # 2. 再编译 main 块
            if ast.main_block:
                self._compile_body(ast.main_block.statements)
            
            # 3. HALT 结束主代码
            self.emit(OP_HALT)
            
            # 4. 解析主代码回填
            self.resolve_backpatches()
            
            # 5. 编译各个函数
            for func in self.func_defs:
                self._push_func_context(func)
                self._compile_body(func.body)
                # 函数末尾自动添加 return(null) 确保函数有返回值
                self.emit(OP_NULL)
                self.emit(OP_RET)
                self.resolve_backpatches()
                self._pop_func_context(func)

        except CompileError as e:
            print(f"[编译错误] {e}")
            self.errors += 1

    def compile_statement(self, stmt):
        """编译单个语句"""
        if isinstance(stmt, VarDecl):
            self.compile_var_decl(stmt)
        elif isinstance(stmt, ConstDecl):
            self.compile_var_decl(stmt)
        elif isinstance(stmt, Assign):
            self.compile_assign(stmt)
        elif isinstance(stmt, SeeStmt):
            self.compile_see(stmt)
        elif isinstance(stmt, IfStmt):
            self.compile_if(stmt)
        elif isinstance(stmt, WhileStmt):
            self.compile_while(stmt)
        elif isinstance(stmt, ForStmt):
            self.compile_for(stmt)
        elif isinstance(stmt, ForeachStmt):
            self.compile_foreach(stmt)
        elif isinstance(stmt, BreakStmt):
            self.compile_break(stmt)
        elif isinstance(stmt, ContinueStmt):
            self.compile_continue(stmt)
        elif isinstance(stmt, CutDownStmt):
            self.compile_cutdown(stmt)
        elif isinstance(stmt, ExprStmt):
            if isinstance(stmt.expr, Assign):
                self.compile_assign(stmt.expr)
            else:
                self.compile_expression(stmt.expr)
                self.emit(OP_POP)
        elif isinstance(stmt, ReturnStmt):
            if stmt.value:
                self.compile_expression(stmt.value)
            self.emit(OP_RET)
        elif isinstance(stmt, (LifeDecl, ThingDecl)):
            pass  # 方法声明暂不支持
        elif isinstance(stmt, ThrowStmt):
            raise CompileError("try/throw 暂不支持编译到 .dance")
        elif isinstance(stmt, TryStmt):
            raise CompileError("try/catch 暂不支持编译到 .dance")
        elif isinstance(stmt, CaseStmt):
            self.compile_case(stmt)
        elif isinstance(stmt, UseStmt):
            self.compile_use(stmt)
        else:
            raise CompileError(f"不支持的语句类型: {type(stmt).__name__}")

    def compile_var_decl(self, stmt):
        slot = self.alloc_local(stmt.name)
        if stmt.initializer:
            self.compile_expression(stmt.initializer)
        else:
            self.emit(OP_NULL)
        self.emit(OP_STORE)
        self.emit_u8(slot)

    def compile_assign(self, stmt):
        if isinstance(stmt.target, Identifier):
            # 隐式声明：如果变量不存在则自动分配槽位
            slot = self.alloc_local(stmt.target.name)
            self.compile_expression(stmt.value)
            self.emit(OP_STORE)
            self.emit_u8(slot)
        elif isinstance(stmt.target, GetAttr):
            # obj.attr = value
            self.compile_expression(stmt.target.obj)  # 编译对象
            self.compile_expression(stmt.value)        # 编译值
            attr_name = stmt.target.attr
            str_idx = self.add_string(attr_name)
            self.emit(OP_SETATTR)
            self.emit_u32(str_idx)
        elif isinstance(stmt.target, IndexExpr):
            # expr[idx] = value
            self.compile_expression(stmt.target.obj)   # 对象
            self.compile_expression(stmt.target.index)  # 索引
            self.compile_expression(stmt.value)         # 值
            self.emit(OP_SETINDEX)
        else:
            raise CompileError(f"不支持的赋值目标: {type(stmt.target).__name__}")

    def compile_see(self, stmt):
        for arg in stmt.args:
            self.compile_expression(arg)
        self.emit(OP_BIF)
        self.emit_u8(BIF_SEE)
        self.emit_u8(len(stmt.args))

    def compile_use(self, stmt: UseStmt):
        """use 语句：导入包（编译时），不生成任何字节码"""
        self._import_package(stmt.package_name)

    def compile_if(self, stmt):
        self.compile_expression(stmt.condition)
        else_label = f"_if_else_{self.get_pos()}"
        end_label = f"_if_end_{self.get_pos()}"
        self.emit(OP_JIF)
        jif_pos = self.get_pos()
        self.emit_i32(0)
        self.add_backpatch(jif_pos, else_label)
        self._compile_body(stmt.then_block)
        if stmt.else_block:
            self.emit(OP_JMP)
            jmp_pos = self.get_pos()
            self.emit_i32(0)
            self.add_backpatch(jmp_pos, end_label)
        self.define_label(else_label)
        if stmt.else_block:
            self._compile_body(stmt.else_block)
            self.define_label(end_label)

    def compile_while(self, stmt):
        loop_start = f"_while_start_{self.get_pos()}"
        end_label = f"_while_end_{self.get_pos()}"
        self.loop_labels.append((loop_start, end_label))
        self.define_label(loop_start)
        self.compile_expression(stmt.condition)
        self.emit(OP_JIF)
        jif_pos = self.get_pos()
        self.emit_i32(0)
        self.add_backpatch(jif_pos, end_label)
        self._compile_body(stmt.body)
        self.emit(OP_JMP)
        jmp_pos2 = self.get_pos()
        self.emit_i32(0)
        target = self.labels[loop_start]
        offset = target - (jmp_pos2 + 4)
        self.code[jmp_pos2:jmp_pos2+4] = struct.pack('<i', offset)
        self.define_label(end_label)
        self.loop_labels.pop()

    def compile_for(self, stmt):
        loop_start = f"_for_start_{self.get_pos()}"
        check_label = f"_for_check_{self.get_pos()}"
        end_label = f"_for_end_{self.get_pos()}"
        if stmt.init:
            self.compile_statement(stmt.init)
        self.loop_labels.append((check_label, end_label))
        self.define_label(check_label)
        if stmt.condition:
            self.compile_expression(stmt.condition)
        else:
            self.emit(OP_BCONST)
            self.emit_u8(1)
        self.emit(OP_JIF)
        jif_pos = self.get_pos()
        self.emit_i32(0)
        self.add_backpatch(jif_pos, end_label)
        self._compile_body(stmt.body)
        self.define_label(loop_start)
        if stmt.update:
            if isinstance(stmt.update, Assign):
                self.compile_assign(stmt.update)
            elif isinstance(stmt.update, ExprStmt):
                self.compile_expression(stmt.update.expr)
                self.emit(OP_POP)
            else:
                self.compile_expression(stmt.update)
                self.emit(OP_POP)
        self.emit(OP_JMP)
        jmp_pos = self.get_pos()
        self.emit_i32(0)
        target = self.labels[check_label]
        offset = target - (jmp_pos + 4)
        self.code[jmp_pos:jmp_pos+4] = struct.pack('<i', offset)
        self.define_label(end_label)
        self.loop_labels.pop()

    def compile_foreach(self, stmt):
        raise CompileError(
            "foreach 暂不支持编译到 .dance — "
            "需要 SDVM 添加迭代器 BIF 支持后才能实现，"
            "请使用 for(init; cond; update) 循环替代")

    def compile_break(self, stmt):
        if not self.loop_labels:
            raise CompileError("break 只能在循环中使用")
        _, end_label = self.loop_labels[-1]
        self.emit(OP_JMP)
        jmp_pos = self.get_pos()
        self.emit_i32(0)
        self.add_backpatch(jmp_pos, end_label)

    def compile_continue(self, stmt):
        if not self.loop_labels:
            raise CompileError("continue 只能在循环中使用")
        loop_start, _ = self.loop_labels[-1]
        self.emit(OP_JMP)
        jmp_pos = self.get_pos()
        self.emit_i32(0)
        self.add_backpatch(jmp_pos, loop_start)

    def compile_cutdown(self, stmt):
        if not self.loop_labels:
            raise CompileError("cutdown 只能在循环中使用")
        _, end_label = self.loop_labels[0]
        self.emit(OP_JMP)
        jmp_pos = self.get_pos()
        self.emit_i32(0)
        self.add_backpatch(jmp_pos, end_label)

    def compile_case(self, stmt):
        case_slot = self.next_local
        self.next_local += 1
        self.compile_expression(stmt.expr)
        self.emit(OP_STORE)
        self.emit_u8(case_slot)
        next_labels = []
        end_label = f"_case_end_{self.get_pos()}"
        for i, wc in enumerate(stmt.when_clauses):
            next_label = f"_case_next_{self.get_pos()}_{i}"
            next_labels.append(next_label)
            self.emit(OP_LOAD)
            self.emit_u8(case_slot)
            self.compile_expression(wc.value)
            self.emit(OP_EQ)
            self.emit(OP_JIF)
            jif_pos = self.get_pos()
            self.emit_i32(0)
            self.add_backpatch(jif_pos, next_label)
            for s in wc.body:
                self.compile_statement(s)
            self.emit(OP_JMP)
            jmp_pos = self.get_pos()
            self.emit_i32(0)
            self.add_backpatch(jmp_pos, end_label)
            self.define_label(next_label)
        for s in stmt.else_body:
            self.compile_statement(s)
        self.define_label(end_label)
        self.next_local -= 1

    # ─── 表达式编译 ─────────────────────────────
    def compile_expression(self, expr):
        if isinstance(expr, Literal):
            self.compile_literal(expr)
        elif isinstance(expr, Identifier):
            self.compile_identifier(expr)
        elif isinstance(expr, BinaryOp):
            self.compile_binary(expr)
        elif isinstance(expr, UnaryOp):
            self.compile_unary(expr)
        elif isinstance(expr, CallExpr):
            self.compile_call(expr)
        elif isinstance(expr, AnonymouFunc):
            self.compile_anonymou(expr)
        elif isinstance(expr, ListLiteral):
            self.compile_list_literal(expr)
        elif isinstance(expr, NewExpr):
            raise CompileError("new 表达式暂不支持编译")
        elif isinstance(expr, GetAttr):
            self.compile_getattr(expr)
        elif isinstance(expr, IndexExpr):
            self.compile_index_expr(expr)
        else:
            raise CompileError(f"不支持的表达式: {type(expr).__name__}")

    def compile_literal(self, expr):
        val = expr.value
        if val is True:
            self.emit(OP_BCONST)
            self.emit_u8(1)
        elif val is False:
            self.emit(OP_BCONST)
            self.emit_u8(0)
        elif isinstance(val, int):
            self.emit(OP_ICONST)
            self.emit_i32(val)
        elif isinstance(val, float):
            self.emit(OP_FCONST)
            self.emit_f64(val)
        elif isinstance(val, str):
            idx = self.add_string(val)
            self.emit(OP_SCONST)
            self.emit_u32(idx)
        elif val is None:
            self.emit(OP_NULL)
        else:
            raise CompileError(f"不支持的字面量: {val!r}")

    def compile_identifier(self, expr):
        slot = self.get_local(expr.name)
        self.emit(OP_LOAD)
        self.emit_u8(slot)

    def compile_binary(self, expr):
        op = expr.op
        if op == '&&' or op == '||':
            self.compile_logical(expr)
            return
        self.compile_expression(expr.left)
        self.compile_expression(expr.right)
        op_map = {
            '+': OP_ADD, '-': OP_SUB, '*': OP_MUL, '/': OP_DIV,
            '%': OP_MOD,
            '==': OP_EQ, '!=': OP_NE, '<': OP_LT, '>': OP_GT,
            '<=': OP_LE, '>=': OP_GE,
            '===': OP_EQ,
            '!>': OP_LE, '!<': OP_GE,
            '<<': None, '>>': None, '>>>': None, '<<<': None,
            '&': None, '|': None,
        }
        if op in op_map:
            bytecode = op_map[op]
            if bytecode is not None:
                self.emit(bytecode)
            else:
                raise CompileError(f"位运算 '{op}' 暂不支持编译到 .dance")
        else:
            raise CompileError(f"不支持的运算符: {op}")

    def compile_logical(self, expr):
        end_label = f"_logical_end_{self.get_pos()}"
        self.compile_expression(expr.left)
        self.emit(OP_DUP)
        if expr.op == '&&':
            self.emit(OP_JIF)
            jif_pos = self.get_pos()
            self.emit_i32(0)
            self.add_backpatch(jif_pos, end_label)
            self.emit(OP_POP)
            self.compile_expression(expr.right)
        elif expr.op == '||':
            use_b_label = f"_logical_use_b_{self.get_pos()}"
            self.emit(OP_JIF)
            jif_pos = self.get_pos()
            self.emit_i32(0)
            self.add_backpatch(jif_pos, use_b_label)
            self.emit(OP_JMP)
            jmp_pos = self.get_pos()
            self.emit_i32(0)
            self.add_backpatch(jmp_pos, end_label)
            self.define_label(use_b_label)
            self.emit(OP_POP)
            self.compile_expression(expr.right)
        self.define_label(end_label)

    def compile_unary(self, expr):
        # ++/-- 需要先处理，避免 compile_expression(operand) 额外 emit OP_LOAD
        if expr.op in ('++', '--'):
            if not isinstance(expr.operand, Identifier):
                raise CompileError("++/-- 只能用于变量")
            slot = self.get_local(expr.operand.name)
            if expr.op == '++':
                if expr.is_prefix:
                    # prefix ++i: LOAD i, +1, DUP, STORE i  → 栈顶 = i+1 (新值)
                    self.emit(OP_LOAD)
                    self.emit_u8(slot)
                    self.emit(OP_ICONST)
                    self.emit_i32(1)
                    self.emit(OP_ADD)
                    self.emit(OP_DUP)
                    self.emit(OP_STORE)
                    self.emit_u8(slot)
                else:
                    # postfix i++: LOAD i, DUP, +1, STORE i  → 栈顶 = i (原值)
                    self.emit(OP_LOAD)
                    self.emit_u8(slot)
                    self.emit(OP_DUP)
                    self.emit(OP_ICONST)
                    self.emit_i32(1)
                    self.emit(OP_ADD)
                    self.emit(OP_STORE)
                    self.emit_u8(slot)
            else:  # '--'
                if expr.is_prefix:
                    self.emit(OP_LOAD)
                    self.emit_u8(slot)
                    self.emit(OP_ICONST)
                    self.emit_i32(1)
                    self.emit(OP_SUB)
                    self.emit(OP_DUP)
                    self.emit(OP_STORE)
                    self.emit_u8(slot)
                else:
                    self.emit(OP_LOAD)
                    self.emit_u8(slot)
                    self.emit(OP_DUP)
                    self.emit(OP_ICONST)
                    self.emit_i32(1)
                    self.emit(OP_SUB)
                    self.emit(OP_STORE)
                    self.emit_u8(slot)
            return

        self.compile_expression(expr.operand)
        if expr.op == '-':
            self.emit(OP_NEG)
        elif expr.op == '!':
            self.emit(OP_NOT)
        else:
            raise CompileError(f"不支持的一元运算符: {expr.op}")

    def _resolve_call_args(self, param_names, args):
        """将调用参数映射到函数参数槽位，返回按槽位顺序排列的表达式列表"""
        named_args = {}
        positional_args = []
        for arg in args:
            if isinstance(arg, NamedArgument):
                if arg.name in named_args:
                    raise CompileError(f"重复的命名参数 '{arg.name}'")
                named_args[arg.name] = arg.value
            else:
                positional_args.append(arg)
        
        if len(positional_args) > len(param_names):
            raise CompileError(f"参数过多: 期望 {len(param_names)} 个，实际 {len(positional_args)} 个位置参数")
        
        ordered = [None] * len(param_names)
        used = set()
        
        # 位置参数按顺序
        for i, expr in enumerate(positional_args):
            ordered[i] = expr
            used.add(i)
            # 同时检查是否也有同名的命名参数
            if i < len(param_names) and param_names[i] in named_args:
                raise CompileError(f"参数 '{param_names[i]}' 同时被位置参数和命名参数指定")
        
        # 命名参数按名映射
        for name, expr in named_args.items():
            if name not in param_names:
                raise CompileError(f"命名参数 '{name}' 不是函数参数 (可选: {param_names})")
            slot = param_names.index(name)
            if slot in used:
                raise CompileError(f"参数 '{name}' (slot {slot}) 已被赋值")
            ordered[slot] = expr
            used.add(slot)
        
        # 检查是否所有参数都填满
        for i, expr in enumerate(ordered):
            if expr is None:
                raise CompileError(f"缺少参数 '{param_names[i]}'")
        
        return ordered

    def compile_call(self, expr: CallExpr):
        if isinstance(expr.callee, Identifier):
            name = expr.callee.name
            
            if name in BIF_MAP:
                bif_idx = BIF_MAP[name]
                for arg in expr.args:
                    self.compile_expression(arg)
                self.emit(OP_BIF)
                self.emit_u8(bif_idx)
                self.emit_u8(len(expr.args))
            elif name in self.func_map:
                func_idx = self.func_map[name]
                func = self.func_defs[func_idx - 1]
                ordered_exprs = self._resolve_call_args(func.param_names, expr.args)
                for arg_expr in ordered_exprs:
                    self.compile_expression(arg_expr)
                self.emit(OP_CALL)
                self.emit_u32(func_idx)  # 函数索引
                self.emit_u8(len(func.param_names))  # 参数个数
            else:
                # 可能是一个变量（匿名函数引用），使用 OP_CALLR
                self.compile_expression(expr.callee)  # 编译变量名 → 加载函数引用
                for arg in expr.args:
                    self.compile_expression(arg)
                self.emit(OP_CALLR)
                self.emit_u8(len(expr.args))
        elif isinstance(expr.callee, AnonymouFunc):
            # 直接调用匿名函数：anonymou(x,y){...}(1,2)
            # 需要先处理匿名函数（添加到 func_defs），然后 OP_CALL
            anon_func = expr.callee
            anon_idx = self._add_anonymous_func(anon_func)
            ordered_exprs = self._resolve_call_args(anon_func.params, expr.args)
            for arg_expr in ordered_exprs:
                self.compile_expression(arg_expr)
            self.emit(OP_CALL)
            self.emit_u32(anon_idx)
            self.emit_u8(len(anon_func.params))
        else:
            # 复杂 callee 表达式 → 编译 callee 产生函数引用，然后 OP_CALLR
            self.compile_expression(expr.callee)
            for arg in expr.args:
                self.compile_expression(arg)
            self.emit(OP_CALLR)
            self.emit_u8(len(expr.args))

    def _add_anonymous_func(self, anon_expr):
        """将匿名函数添加到函数表，返回函数索引"""
        func = FuncDef(None, anon_expr.params, anon_expr.body, is_anonymous=True)
        self.func_defs.append(func)
        return len(self.func_defs)  # 注意：func 0 = main, 所以 index = len(func_defs)

    def compile_anonymou(self, expr: AnonymouFunc):
        """编译匿名函数表达式 (推送函数引用到栈)"""
        func_idx = self._add_anonymous_func(expr)
        self.emit(OP_ANON)
        self.emit_u32(func_idx)

    def compile_list_literal(self, expr):
        """编译特殊列表（字典/对象）['a':1, 'b':[1,2,3]]"""
        # entries 是 (key_or_None, value_node) 列表
        # 只编译有键的条目（字典/对象模式）
        pairs = [(k, v) for k, v in expr.entries if k is not None]
        for key_str, val_ast in pairs:
            self.compile_expression(val_ast)   # 值先入栈
            # key 已经是字符串（解析器预处理好的），直接放入常量池
            idx = self.add_string(key_str)
            self.emit(OP_SCONST)
            self.emit_u32(idx)

        # 发射 OP_NEWOBJ n
        n = len(pairs)
        self.emit(OP_NEWOBJ)
        self.emit(n)

    def compile_getattr(self, expr):
        """编译属性访问 obj.attr"""
        # 编译对象表达式
        self.compile_expression(expr.obj)
        # attr 是属性名（字符串），存入字符串池
        attr_name = expr.attr
        str_idx = self.add_string(attr_name)
        self.emit(OP_GETATTR)
        self.emit_u32(str_idx)

    def compile_index_expr(self, expr):
        """编译下标访问 expr[idx]"""
        self.compile_expression(expr.obj)
        self.compile_expression(expr.index)
        self.emit(OP_GETINDEX)

    # ─── 输出 ───────────────────────────────────
    def write_dance(self, path: str):
        if self.errors > 0:
            print(f"编译失败: {self.errors} 个错误")
            return False
        
        # 先把所有函数名加入字符串池，确保它们被写入文件
        for func in self.func_defs:
            if func.name is not None:
                self.add_string(func.name)
        
        with open(path, 'wb') as f:
            # Magic: "SDNC"
            f.write(b'SDNC')
            
            # Version: 2 (支持函数表)
            f.write(struct.pack('<I', 2))
            
            # StrCount + Strings
            f.write(struct.pack('<I', len(self.strpool)))
            for s in self.strpool:
                encoded = s.encode('utf-8')
                f.write(struct.pack('<I', len(encoded)))
                f.write(encoded)
            
            # FuncCount (函数数量，含 main)
            # func_defs 包含所有用户定义函数
            # main 代码作为 func 0
            total_funcs = 1 + len(self.func_defs)  # main + user funcs
            
            # 计算代码偏移：先收集所有代码块
            code_blocks = []
            code_blocks.append(bytes(self.code))  # block 0 = main code
            
            # 编译各函数代码（可能已在上一步编译完成）
            func_code_list = []
            for func in self.func_defs:
                func_code_list.append(bytes(func.code))
            
            # 计算每个函数的 code_offset（相对于 CodeData 起始位置）
            current_offset = 0
            main_code_size = len(self.code)
            
            # main 的偏移量永远是 0
            self.main_code_offset = 0
            
            current_offset += main_code_size
            for i, func in enumerate(self.func_defs):
                func.code_offset = current_offset
                current_offset += len(func.code)
            
            total_code_size = current_offset
            
            # 写出 FuncCount
            f.write(struct.pack('<I', total_funcs))
            
            # 写出 FuncTable
            # func 0: main
            f.write(struct.pack('<I', 0xFFFFFFFF))  # name_idx = -1 (main 不用字符串索引)
            f.write(struct.pack('<I', 0))            # arg_count = 0
            f.write(struct.pack('<I', self.next_local))  # local_count
            f.write(struct.pack('<I', 0))            # code_offset = 0
            
            # user funcs
            for func in self.func_defs:
                if func.name is not None:
                    name_idx = self.add_string(func.name)
                else:
                    name_idx = 0xFFFFFFFF  # 匿名
                f.write(struct.pack('<I', name_idx))
                f.write(struct.pack('<I', len(func.param_names)))  # arg_count
                f.write(struct.pack('<I', func.next_local))  # local_count
                f.write(struct.pack('<I', func.code_offset))

            # CodeSize
            f.write(struct.pack('<I', total_code_size))
            
            # CodeData: main code + all func codes
            f.write(bytes(self.code))
            for func in self.func_defs:
                f.write(bytes(func.code))
        
        total_funcs_output = 1 + len(self.func_defs)
        print(f"编译成功: {path}")
        print(f"  字符串常量: {len(self.strpool)}")
        print(f"  函数: {total_funcs_output} (main + {len(self.func_defs)} user)")
        print(f"  字节码大小: {total_code_size} bytes (主代码: {len(self.code)})")
        print(f"  局部变量: {self.next_local}")
        return True


# ═══════════════════════════════════════════════════
#  CLI 入口
# ═══════════════════════════════════════════════════

def main():
    import argparse
    
    parser = argparse.ArgumentParser(description='SDVM 编译器 — .star → .dance')
    parser.add_argument('input', help='输入 .star 文件')
    parser.add_argument('-o', '--output', help='输出 .dance 文件 (默认: 输入文件名.dance)')
    args = parser.parse_args()
    
    input_path = args.input
    
    if not os.path.exists(input_path):
        print(f"错误: 文件 '{input_path}' 不存在")
        return 1
    
    with open(input_path, 'r', encoding='utf-8') as f:
        source = f.read()
    
    try:
        lexer = Lexer(source)
        tokens = lexer.tokenize()
    except Exception as e:
        print(f"[词法错误] {e}")
        return 1
    
    try:
        parser_obj = Parser(tokens)
        ast = parser_obj.parse()
    except Exception as e:
        print(f"[语法错误] {e}")
        return 1
    
    compiler = Compiler()
    compiler.compile(ast)
    
    if compiler.errors > 0:
        return 1
    
    output_path = args.output
    if not output_path:
        base = os.path.splitext(input_path)[0]
        output_path = base + '.dance'
    
    compiler.write_dance(output_path)
    return 0


if __name__ == '__main__':
    sys.exit(main())
