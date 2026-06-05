"""Bytecode dump using correct opcode values from compiler.py"""
import sys, os, struct

# Set up paths like compiler.py does
script_dir = r'D:\stay\SDVM'
star_dance_path = os.path.join(script_dir, '..', 'star_dance')
if os.path.isdir(star_dance_path):
    sys.path.insert(0, star_dance_path)
else:
    sys.path.insert(0, os.path.join(script_dir, '..'))

os.chdir(script_dir)

log_path = r'D:\stay\SDVM\_dump_log.txt'
dump_path = r'D:\stay\SDVM\bc_dump.txt'

log = open(log_path, 'w', encoding='utf-8')

# Actual opcodes from compiler.py
OP_NOP    = 0x00
OP_ICONST = 0x01
OP_FCONST = 0x02
OP_SCONST = 0x03
OP_BCONST = 0x04
OP_NULL   = 0x05
OP_DUP    = 0x06
OP_POP    = 0x07

OP_LOAD   = 0x10
OP_STORE  = 0x11

OP_ADD    = 0x20
OP_SUB    = 0x21
OP_MUL    = 0x22
OP_DIV    = 0x23
OP_MOD    = 0x24
OP_NEG    = 0x25

OP_EQ     = 0x30
OP_NE     = 0x31
OP_LT     = 0x32
OP_GT     = 0x33
OP_LE     = 0x34
OP_GE     = 0x35

OP_NOT    = 0x38

OP_JMP    = 0x40
OP_JIF    = 0x41
OP_BIF    = 0x42
OP_RET    = 0x43
OP_HALT   = 0x44

OP_CALL   = 0x50
OP_ANON   = 0x51
OP_CALLR  = 0x52

OP_PRINT  = 0x70
OP_SCAN   = 0x71

# Opcode sizes (excluding opcode byte)
OP_SIZES = {
    OP_ICONST: 4, OP_FCONST: 8, OP_SCONST: 4, OP_BCONST: 1,
    OP_LOAD: 1, OP_STORE: 1,
    OP_JMP: 4, OP_JIF: 4,
    OP_BIF: 2,  # bif_idx(1) + argc(1)
    OP_CALL: 5,  # func_idx(4) + arg_count(1)
    OP_ANON: 4,  # func_idx(4)
    OP_CALLR: 1, # arg_count(1)
    OP_PRINT: 1, OP_SCAN: 1,
}

OP_NAMES = {
    0x00: 'NOP', 0x01: 'ICONST', 0x02: 'FCONST', 0x03: 'SCONST',
    0x04: 'BCONST', 0x05: 'NULL', 0x06: 'DUP', 0x07: 'POP',
    0x10: 'LOAD', 0x11: 'STORE',
    0x20: 'ADD', 0x21: 'SUB', 0x22: 'MUL', 0x23: 'DIV', 0x24: 'MOD',
    0x25: 'NEG',
    0x30: 'EQ', 0x31: 'NE', 0x32: 'LT', 0x33: 'GT', 0x34: 'LE', 0x35: 'GE',
    0x38: 'NOT',
    0x40: 'JMP', 0x41: 'JIF', 0x42: 'BIF', 0x43: 'RET', 0x44: 'HALT',
    0x50: 'CALL', 0x51: 'ANON', 0x52: 'CALLR',
    0x70: 'PRINT', 0x71: 'SCAN',
}

def disasm_op(code, addr):
    """Disassemble one instruction at addr, return (line_text, next_addr)"""
    if addr >= len(code):
        return "(end)", addr
    op = code[addr]
    name = OP_NAMES.get(op, f'UNK({op:#04x})')
    line = f"  [{addr:04d}] {name}"
    i = addr + 1
    
    if op == OP_ICONST and i + 4 <= len(code):
        val = struct.unpack('<i', code[i:i+4])[0]
        line += f" {val}"
        i += 4
    elif op == OP_FCONST and i + 8 <= len(code):
        val = struct.unpack('<d', code[i:i+8])[0]
        line += f" {val:g}"
        i += 8
    elif op == OP_SCONST and i + 4 <= len(code):
        val = struct.unpack('<I', code[i:i+4])[0]
        line += f" #{val}"
        i += 4
    elif op == OP_BCONST and i < len(code):
        line += f" {code[i]}"
        i += 1
    elif op == OP_LOAD and i < len(code):
        line += f" r{code[i]}"
        i += 1
    elif op == OP_STORE and i < len(code):
        line += f" r{code[i]}"
        i += 1
    elif op == OP_JMP and i + 4 <= len(code):
        offset = struct.unpack('<i', code[i:i+4])[0]
        target = addr + 5 + offset
        line += f" {offset:+d} (->{target:04d})"
        i += 4
    elif op == OP_JIF and i + 4 <= len(code):
        offset = struct.unpack('<i', code[i:i+4])[0]
        target = addr + 5 + offset
        line += f" {offset:+d} (->{target:04d})"
        i += 4
    elif op == OP_BIF and i + 2 <= len(code):
        bif_idx = code[i]
        argc = code[i+1]
        bif_names = {0:'see',1:'int',2:'float',3:'str',4:'bool',5:'type'}
        bname = bif_names.get(bif_idx, f'bif{bif_idx}')
        line += f" {bname}({argc})"
        i += 2
    elif op == OP_CALL and i + 5 <= len(code):
        fi = struct.unpack('<I', code[i:i+4])[0]
        ac = code[i+4]
        line += f" func[{fi}] args={ac}"
        i += 5
    elif op == OP_ANON and i + 4 <= len(code):
        fi = struct.unpack('<I', code[i:i+4])[0]
        line += f" func[{fi}]"
        i += 4
    elif op == OP_CALLR and i < len(code):
        line += f" args={code[i]}"
        i += 1
    elif op == OP_PRINT and i < len(code):
        line += f" {code[i]}"
        i += 1
    elif op == OP_SCAN and i < len(code):
        line += f" {code[i]}"
        i += 1
    # else: no operands (NOP, NULL, DUP, POP, ADD, SUB, etc.)
    
    return line, i

try:
    from lexer import Lexer
    from parser import Parser
    log.write("Parsing test_debug.star...\n")
    
    with open('test_debug.star', 'r', encoding='utf-8') as f:
        source = f.read()
    lexer = Lexer(source)
    tokens = lexer.tokenize()
    
    parser_obj = Parser(tokens)
    ast = parser_obj.parse()
    log.write("Parsed OK\n")
    
    import importlib.util
    spec = importlib.util.spec_from_file_location("sdvm_compiler", r'D:\stay\SDVM\compiler.py')
    sdvm_compiler = importlib.util.module_from_spec(spec)
    if os.path.isdir(star_dance_path):
        sys.path.insert(0, star_dance_path)
    spec.loader.exec_module(sdvm_compiler)
    Compiler = sdvm_compiler.Compiler
    
    compiler = Compiler()
    compiler.compile(ast)
    log.write(f"Compiled: main_code={len(compiler.code)}B, errors={compiler.errors}\n")
    log.write(f"func_defs={len(compiler.func_defs)}\n")
    
    lines = []
    lines.append(f"=== test_debug.star ===")
    lines.append(f"String pool ({len(compiler.strpool)}):")
    for i, s in enumerate(compiler.strpool):
        lines.append(f"  [{i}] {repr(s)}")
    
    # Build func table - main first
    funcs = []
    funcs.append({'name': None, 'code': bytes(compiler.code),
                  'arg_count': 0, 'local_count': compiler.next_local,
                  'code_offset': 0})
    offset = len(compiler.code)
    for func in compiler.func_defs:
        funcs.append({'name': func.name, 'code': bytes(func.code),
                      'arg_count': len(func.param_names),
                      'local_count': func.next_local,
                      'code_offset': offset})
        offset += len(func.code)
    
    lines.append(f"\nFunctions ({len(funcs)}):")
    for fi, fe in enumerate(funcs):
        lines.append(f"  func[{fi}]: offset={fe['code_offset']}, "
                     f"size={len(fe['code'])}, args={fe['arg_count']}, "
                     f"locals={fe['local_count']}, name={fe['name']}")
    
    for fi, fe in enumerate(funcs):
        code = fe['code']
        lines.append(f"\n--- func[{fi}]: {fe['name'] or '(main)'} ---")
        addr = 0
        while addr < len(code):
            line, next_addr = disasm_op(code, addr)
            lines.append(line)
            addr = next_addr
    
    out = '\n'.join(lines)
    with open(dump_path, 'w', encoding='utf-8') as f:
        f.write(out)
    log.write(f"Written to {dump_path}\n")
    
except Exception as e:
    import traceback
    log.write(f"ERROR: {e}\n")
    log.write(traceback.format_exc())

log.close()
