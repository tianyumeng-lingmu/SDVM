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
       StrCount(4) + Strings + CodeSize(4) + Code
"""

import struct
import sys
import os

# ─── 导入 star_dance 的前端 ──────────────────────
# 将 star_dance 目录加入 path（与 parser/lexer 自身导入一致，避免模块重复加载）
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
        ListLiteral, NewExpr, GetAttr, ThrowStmt, TryStmt
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

# 内置函数名 → BIF 索引映射
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
}


class CompileError(Exception):
    """编译错误"""
    pass


class BackpatchEntry:
    """回填记录"""
    def __init__(self, offset_pos: int, target_label: str):
        self.offset_pos = offset_pos   # 字节码中存放偏移量的位置
        self.target_label = target_label  # 目标标签


class Compiler:
    """.star → .dance 编译器"""

    def __init__(self):
        self.code = bytearray()          # 字节码缓冲区
        self.strpool = []                # 字符串常量池
        self.strpool_map = {}            # 字符串 → 索引
        self.locals = {}                 # 变量名 → slot 索引
        self.next_local = 0              # 下一个局部变量槽
        self.labels = {}                 # 标签名 → 字节码位置
        self.backpatches = []            # 待回填的列表
        self.loop_labels = []            # 循环标签栈（break/continue）
        self.errors = 0                  # 编译错误计数

    # ─── 字符串常量池 ────────────────────────────
    def add_string(self, s: str) -> int:
        """向常量池添加字符串，返回索引"""
        if s not in self.strpool_map:
            idx = len(self.strpool)
            self.strpool.append(s)
            self.strpool_map[s] = idx
            return idx
        return self.strpool_map[s]

    # ─── 局部变量 ───────────────────────────────
    def alloc_local(self, name: str) -> int:
        """分配局部变量槽，返回索引"""
        if name not in self.locals:
            self.locals[name] = self.next_local
            self.next_local += 1
        return self.locals[name]

    def get_local(self, name: str) -> int:
        """获取变量索引"""
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
        """定义标签指向当前位置"""
        self.labels[name] = self.get_pos()

    def add_backpatch(self, offset_pos: int, target_label: str):
        """记录一个需要回填的跳转偏移"""
        self.backpatches.append(BackpatchEntry(offset_pos, target_label))

    def resolve_backpatches(self):
        """解析所有回填"""
        for bp in self.backpatches:
            if bp.target_label not in self.labels:
                raise CompileError(f"标签 '{bp.target_label}' 未定义")
            target = self.labels[bp.target_label]
            offset = target - (bp.offset_pos + 4)  # 相对下一条指令的偏移
            # 写入偏移量
            self.code[bp.offset_pos:bp.offset_pos+4] = struct.pack('<i', offset)

    def _flatten_body(self, body):
        """展开语句体，支持 [Block(...)] 或 [Stmt, Stmt, ...]"""
        stmts = []
        for s in body:
            if isinstance(s, Block):
                stmts.extend(s.statements)
            else:
                stmts.append(s)
        return stmts

    def _compile_body(self, body):
        """编译语句列表（自动展开 Block）"""
        for s in self._flatten_body(body):
            self.compile_statement(s)

    # ─── 主编译入口 ─────────────────────────────
    def compile(self, ast: Program):
        """编译整个 Program"""
        self.errors = 0
        
        try:
            # 先编译 start 块（常量/初始值设定），让变量先分配
            if ast.start_block:
                self._compile_body(ast.start_block.statements)
            
            # 再编译 main 块中的语句
            if ast.main_block:
                self._compile_body(ast.main_block.statements)
            
            # 最后 halt
            self.emit(OP_HALT)
            
            # 解析回填
            self.resolve_backpatches()

        except CompileError as e:
            print(f"[编译错误] {e}")
            self.errors += 1

    def compile_statement(self, stmt):
        """编译单个语句"""
        if isinstance(stmt, VarDecl):
            self.compile_var_decl(stmt)
        elif isinstance(stmt, ConstDecl):
            self.compile_var_decl(stmt)  # ConstDecl 和 VarDecl 编译方式相同
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
            # 处理赋值表达式 i = i + 1 → 编译为 STORE
            if isinstance(stmt.expr, Assign):
                self.compile_assign(stmt.expr)
            else:
                self.compile_expression(stmt.expr)
                self.emit(OP_POP)  # 丢弃表达式结果
        elif isinstance(stmt, ReturnStmt):
            if stmt.value:
                self.compile_expression(stmt.value)
            self.emit(OP_RET)
        elif isinstance(stmt, (LifeDecl, ThingDecl)):
            # Life/Thing 声明在 v1 中暂不支持编译
            pass  # 直接跳过，不自持编译
        elif isinstance(stmt, ThrowStmt):
            raise CompileError("try/throw 暂不支持编译到 .dance")
        elif isinstance(stmt, TryStmt):
            raise CompileError("try/catch 暂不支持编译到 .dance")
        elif isinstance(stmt, CaseStmt):
            self.compile_case(stmt)
        else:
            raise CompileError(f"不支持的语句类型: {type(stmt).__name__}")

    def compile_var_decl(self, stmt: VarDecl):
        """变量声明: int a = expr; → compile(expr) + STORE"""
        slot = self.alloc_local(stmt.name)
        if stmt.initializer:
            self.compile_expression(stmt.initializer)
        else:
            # 无初始化值 → 默认值 null
            self.emit(OP_NULL)
        self.emit(OP_STORE)
        self.emit_u8(slot)

    def compile_assign(self, stmt: Assign):
        """赋值: target = expr;"""
        if isinstance(stmt.target, Identifier):
            slot = self.get_local(stmt.target.name)
            self.compile_expression(stmt.value)
            self.emit(OP_STORE)
            self.emit_u8(slot)
        else:
            raise CompileError(f"不支持的赋值目标: {type(stmt.target).__name__}")

    def compile_see(self, stmt: SeeStmt):
        """see(expr, ...)"""
        for arg in stmt.args:
            self.compile_expression(arg)
        self.emit(OP_BIF)
        self.emit_u8(BIF_SEE)
        self.emit_u8(len(stmt.args))

    def compile_if(self, stmt: IfStmt):
        """if(cond) { then } else { else }
        
        生成:
            compile(cond)
            JIF else_label
            compile(then)
            JMP end_label
        else_label:
            compile(else)
        end_label:
        """
        # 条件
        self.compile_expression(stmt.condition)
        
        else_label = f"_if_else_{self.get_pos()}"
        end_label = f"_if_end_{self.get_pos()}"
        
        # JIF 到 else 分支
        self.emit(OP_JIF)
        jif_pos = self.get_pos()
        self.emit_i32(0)  # placeholder
        self.add_backpatch(jif_pos, else_label)
        
        # then 分支
        self._compile_body(stmt.then_block)
        
        if stmt.else_block:
            # JMP 跳过 else
            self.emit(OP_JMP)
            jmp_pos = self.get_pos()
            self.emit_i32(0)  # placeholder
            self.add_backpatch(jmp_pos, end_label)
        
        self.define_label(else_label)
        
        if stmt.else_block:
            self._compile_body(stmt.else_block)
            self.define_label(end_label)

    def compile_while(self, stmt: WhileStmt):
        """while(cond) { body }
        
        生成:
        loop_start:
            compile(cond)
            JIF end_label
            compile(body)
            JMP loop_start
        end_label:
        """
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
        # 跳回 loop_start
        target = self.labels[loop_start]
        offset = target - (jmp_pos2 + 4)
        self.code[jmp_pos2:jmp_pos2+4] = struct.pack('<i', offset)
        
        self.define_label(end_label)
        self.loop_labels.pop()

    def compile_for(self, stmt: ForStmt):
        """for(init; cond; update) { body }"""
        loop_start = f"_for_start_{self.get_pos()}"
        check_label = f"_for_check_{self.get_pos()}"
        end_label = f"_for_end_{self.get_pos()}"
        
        if stmt.init:
            self.compile_statement(stmt.init)
        
        self.loop_labels.append((check_label, end_label))
        self.define_label(check_label)
        
        # 条件
        if stmt.condition:
            self.compile_expression(stmt.condition)
        else:
            # 无条件 → 永远 true
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
                # compile_assign 已经消费了栈值（STORE），不需要 POP
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

    def compile_foreach(self, stmt: ForeachStmt):
        """foreach var in iterable { body }
        
        简化实现: 限于 int/float 的可迭代对象
        实际 v1 用 for 循环代替
        """
        raise CompileError("foreach 暂不支持编译到 .dance，请使用 for 循环替代")

    def compile_break(self, stmt: BreakStmt):
        if not self.loop_labels:
            raise CompileError("break 只能在循环中使用")
        _, end_label = self.loop_labels[-1]
        self.emit(OP_JMP)
        jmp_pos = self.get_pos()
        self.emit_i32(0)
        self.add_backpatch(jmp_pos, end_label)

    def compile_continue(self, stmt: ContinueStmt):
        if not self.loop_labels:
            raise CompileError("continue 只能在循环中使用")
        loop_start, _ = self.loop_labels[-1]
        self.emit(OP_JMP)
        jmp_pos = self.get_pos()
        self.emit_i32(0)
        self.add_backpatch(jmp_pos, loop_start)

    def compile_cutdown(self, stmt: CutDownStmt):
        """cutdown → 跳转到最外层循环的 end_label"""
        if not self.loop_labels:
            raise CompileError("cutdown 只能在循环中使用")
        _, end_label = self.loop_labels[0]  # 最外层
        self.emit(OP_JMP)
        jmp_pos = self.get_pos()
        self.emit_i32(0)
        self.add_backpatch(jmp_pos, end_label)

    def compile_case(self, stmt: CaseStmt):
        """case(expr) { when val { body } else { body } }
        
        简化为链式 if-else:
            compile(expr) → temp %slot
            temp == when1 ? → body1
            temp == when2 ? → body2
            ...
            else_body
        """
        # 分配临时槽存放 expr
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
        
        # else 分支
        for s in stmt.else_body:
            self.compile_statement(s)
        
        self.define_label(end_label)
        self.next_local -= 1  # 释放临时槽

    # ─── 表达式编译 ─────────────────────────────
    def compile_expression(self, expr):
        """编译表达式，结果留在栈顶"""
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
        elif isinstance(expr, ListLiteral):
            raise CompileError("列表字面量暂不支持编译")
        elif isinstance(expr, NewExpr):
            raise CompileError("new 表达式暂不支持编译")
        elif isinstance(expr, GetAttr):
            raise CompileError("属性访问暂不支持编译")
        else:
            raise CompileError(f"不支持的表达式: {type(expr).__name__}")

    def compile_literal(self, expr: Literal):
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

    def compile_identifier(self, expr: Identifier):
        slot = self.get_local(expr.name)
        self.emit(OP_LOAD)
        self.emit_u8(slot)

    def compile_binary(self, expr: BinaryOp):
        """二元运算"""
        op = expr.op

        # 逻辑运算符 (惰性求值)
        if op == '&&' or op == '||':
            self.compile_logical(expr)
            return

        # 正常的二元运算
        self.compile_expression(expr.left)
        self.compile_expression(expr.right)

        op_map = {
            '+': OP_ADD, '-': OP_SUB, '*': OP_MUL, '/': OP_DIV,
            '%': OP_MOD,
            '==': OP_EQ, '!=': OP_NE, '<': OP_LT, '>': OP_GT,
            '<=': OP_LE, '>=': OP_GE,
            '===': OP_EQ,  # 严格等于 → 在 SDVM 中 same as == (值比较)
            '!>': OP_LE,   # 不大于 → <=
            '!<': OP_GE,   # 不小于 → >=
            # 位运算: 用 ICONST/FCONST + arithmetic 简化，
            # 实际的位运算在 v2 支持
            '<<': None, '>>': None, '>>>': None, '<<<': None,
            '&': None, '|': None,
        }

        if op in op_map:
            bytecode = op_map[op]
            if bytecode is not None:
                self.emit(bytecode)
            else:
                # 位运算: 转为 int 后用 Python 运算
                if op == '<<':
                    # 暂不支持，用加法模拟 (v1 简化)
                    raise CompileError(f"位运算 '{op}' 暂不支持编译到 .dance")
                elif op == '>>':
                    raise CompileError(f"位运算 '{op}' 暂不支持编译到 .dance")
                elif op == '>>>':
                    raise CompileError(f"位运算 '{op}' 暂不支持编译到 .dance")
                elif op == '<<<':
                    raise CompileError(f"位运算 '{op}' 暂不支持编译到 .dance")
                elif op == '&':
                    raise CompileError(f"位运算 '{op}' 暂不支持编译到 .dance")
                elif op == '|':
                    raise CompileError(f"位运算 '{op}' 暂不支持编译到 .dance")
        else:
            raise CompileError(f"不支持的运算符: {op}")

    def compile_logical(self, expr: BinaryOp):
        """惰性求值的逻辑运算
        
        a && b:
            compile(a)
            DUP
            JIF end        ; a is false → result is a (false)
            POP            ; a is true → discard
            compile(b)     ; result is b
        end:
        
        a || b:
            compile(a)
            DUP
            JIF use_b      ; a is false → need to check b
            JMP end        ; a is true → result is a (true), skip
        use_b:
            POP            ; drop false a
            compile(b)
        end:
        """
        end_label = f"_logical_end_{self.get_pos()}"
        
        self.compile_expression(expr.left)
        self.emit(OP_DUP)
        
        if expr.op == '&&':
            # a is false → end (result = false). a is true → POP + compile(b)
            self.emit(OP_JIF)
            jif_pos = self.get_pos()
            self.emit_i32(0)
            self.add_backpatch(jif_pos, end_label)
            self.emit(OP_POP)
            self.compile_expression(expr.right)
        elif expr.op == '||':
            # a is false → go to use_b. a is true → JMP to end
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

    def compile_unary(self, expr: UnaryOp):
        """一元运算"""
        self.compile_expression(expr.operand)
        
        if expr.op == '-':
            self.emit(OP_NEG)
        elif expr.op == '!':
            self.emit(OP_NOT)
        elif expr.op == '++':
            # i++ → LOAD, DUP, ICONST 1, ADD, STORE (结果用原值)
            # ++i → LOAD, ICONST 1, ADD, DUP, STORE (结果用新值)
            if isinstance(expr.operand, Identifier):
                slot = self.get_local(expr.operand.name)
                if expr.is_prefix:
                    # ++i: 先加载，加1，存回，结果留栈
                    self.emit(OP_LOAD)
                    self.emit_u8(slot)
                    self.emit(OP_ICONST)
                    self.emit_i32(1)
                    self.emit(OP_ADD)
                    self.emit(OP_DUP)
                    self.emit(OP_STORE)
                    self.emit_u8(slot)
                else:
                    # i++: 先加载（保留副本），加1，存回，原始值在栈
                    self.emit(OP_LOAD)
                    self.emit_u8(slot)
                    self.emit(OP_DUP)
                    self.emit(OP_ICONST)
                    self.emit_i32(1)
                    self.emit(OP_ADD)
                    self.emit(OP_STORE)
                    self.emit_u8(slot)
            else:
                raise CompileError("++/-- 只能用于变量")
        elif expr.op == '--':
            if isinstance(expr.operand, Identifier):
                slot = self.get_local(expr.operand.name)
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
            else:
                raise CompileError("++/-- 只能用于变量")
        else:
            raise CompileError(f"不支持的一元运算符: {expr.op}")

    def compile_call(self, expr: CallExpr):
        """函数调用
        
        内置函数: BIF 指令
        普通函数: 暂不支持 (v1 只支持内置函数)
        """
        if isinstance(expr.callee, Identifier):
            name = expr.callee.name
            
            if name in BIF_MAP:
                bif_idx = BIF_MAP[name]
                # 编译参数
                for arg in expr.args:
                    self.compile_expression(arg)
                self.emit(OP_BIF)
                self.emit_u8(bif_idx)
                self.emit_u8(len(expr.args))
            else:
                raise CompileError(f"函数 '{name}' 未定义 (v1 只支持内置函数)")
        else:
            raise CompileError("暂不支持方法调用编译")

    # ─── 输出 ───────────────────────────────────
    def write_dance(self, path: str):
        """写出 .dance 二进制文件"""
        # 检查是否有错误
        if self.errors > 0:
            print(f"编译失败: {self.errors} 个错误")
            return False
        
        with open(path, 'wb') as f:
            # Magic: "SDNC"
            f.write(b'SDNC')
            
            # Version: 1
            f.write(struct.pack('<I', 1))
            
            # StrCount
            f.write(struct.pack('<I', len(self.strpool)))
            
            # Strings
            for s in self.strpool:
                encoded = s.encode('utf-8')
                f.write(struct.pack('<I', len(encoded)))
                f.write(encoded)
            
            # CodeSize + Code
            f.write(struct.pack('<I', len(self.code)))
            f.write(bytes(self.code))
        
        print(f"编译成功: {path}")
        print(f"  字符串常量: {len(self.strpool)}")
        print(f"  字节码大小: {len(self.code)} bytes")
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
    
    # 读取源码
    with open(input_path, 'r', encoding='utf-8') as f:
        source = f.read()
    
    # 词法分析
    try:
        lexer = Lexer(source)
        tokens = lexer.tokenize()
    except Exception as e:
        print(f"[词法错误] {e}")
        return 1
    
    # 语法分析
    try:
        parser_obj = Parser(tokens)
        ast = parser_obj.parse()
    except Exception as e:
        print(f"[语法错误] {e}")
        return 1
    
    # 编译
    compiler = Compiler()
    compiler.compile(ast)
    
    if compiler.errors > 0:
        return 1
    
    # 输出路径
    output_path = args.output
    if not output_path:
        base = os.path.splitext(input_path)[0]
        output_path = base + '.dance'
    
    compiler.write_dance(output_path)
    return 0


if __name__ == '__main__':
    sys.exit(main())
