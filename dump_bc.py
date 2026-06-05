"""Bytecode dumper"""
import sys, struct
sys.path.insert(0, r'D:\stay\SDVM')
from compiler import compile_file

# Opcode definitions from compiler.py
OP_ICONST, OP_FCONST, OP_SCONST, OP_BCONST, OP_NULL = range(1, 6)
OP_LOAD, OP_STORE = 6, 7
OP_ADD, OP_SUB, OP_MUL, OP_DIV = 8, 9, 10, 11
OP_NEG, OP_NOT = 12, 13
OP_LT, OP_GT, OP_LE, OP_GE = 14, 15, 16, 17
OP_EQ, OP_NE = 18, 19
OP_JMP, OP_JIF = 20, 21
OP_CALL, OP_RET = 22, 23
OP_BIF, OP_POP = 24, 25
OP_DUP, OP_HALT = 26, 27
OP_NOP, OP_ANON, OP_CALLR = 28, 29, 30

OP_NAMES = {
    1: "ICONST", 2: "FCONST", 3: "SCONST", 4: "BCONST", 5: "NULL",
    6: "LOAD", 7: "STORE", 8: "ADD", 9: "SUB", 10: "MUL", 11: "DIV",
    12: "NEG", 13: "NOT", 14: "LT", 15: "GT", 16: "LE", 17: "GE",
    18: "EQ", 19: "NE", 20: "JMP", 21: "JIF", 22: "CALL", 23: "RET",
    24: "BIF", 25: "POP", 26: "DUP", 27: "HALT", 28: "NOP", 29: "ANON", 30: "CALLR"
}

OpcodeMeta = {
    OP_ICONST: ('i',), OP_FCONST: ('q',), OP_SCONST: ('u',), OP_BCONST: ('b',),
    OP_LOAD: ('b',), OP_STORE: ('b',), OP_JMP: ('i',), OP_JIF: ('i',),
    OP_CALL: ('u', 'b'), OP_BIF: ('b', 'b'), OP_ANON: ('u',), OP_CALLR: ('b',),
}

def dump_bytecode(bytes_data, offset=0):
    i = 0
    lines = []
    while i < len(bytes_data):
        op = bytes_data[i]
        name = OP_NAMES.get(op, f"UNK({op})")
        line = f"  [{offset+i:04d}] {name}"
        i += 1
        if op in OpcodeMeta:
            for m in OpcodeMeta[op]:
                if m == 'b':
                    val = bytes_data[i]; line += f" {val}"; i += 1
                elif m == 'i':
                    val = struct.unpack('<i', bytes_data[i:i+4])[0]; line += f" {val:+d}"; i += 4
                elif m == 'u':
                    val = struct.unpack('<I', bytes_data[i:i+4])[0]; line += f" #{val}"; i += 4
                elif m == 'q':
                    val = struct.unpack('<d', bytes_data[i:i+8])[0]; line += f" {val}"; i += 8
        lines.append(line)
    return lines

def main():
    import os
    os.chdir(r'D:\stay\SDVM')
    
    bytecode, func_table, strpool = compile_file('test_debug.star')
    
    lines = []
    lines.append("=== String Pool ===")
    for idx, s in enumerate(strpool):
        lines.append(f"  [{idx}] {repr(s)}")
    
    lines.append(f"\n=== Function Table (count={len(func_table)}) ===")
    for fi, fe in enumerate(func_table):
        lines.append(f"\n--- func[{fi}]: {fe.get('name', '?')} ---")
        lines.append(f"  code_offset={fe['code_offset']}, arg_count={fe['arg_count']}, local_count={fe['local_count']}")
    
    # Sort functions by code_offset
    sorted_funcs = sorted(func_table, key=lambda f: f['code_offset'])
    for idx_f in range(len(sorted_funcs)):
        fe = sorted_funcs[idx_f]
        code_start = fe['code_offset']
        if idx_f + 1 < len(sorted_funcs):
            code_end = sorted_funcs[idx_f + 1]['code_offset']
        else:
            code_end = len(bytecode)
        func_bytes = bytecode[code_start:code_end]
        fi = func_table.index(fe)
        lines.append(f"\n--- func[{fi}]: {fe.get('name', '?')} ---")
        lines.extend(dump_bytecode(func_bytes, code_start))
    
    out = '\n'.join(lines)
    with open(r'D:\stay\SDVM\bc_dump.txt', 'w', encoding='utf-8') as f:
        f.write(out)
    print("Done!")

if __name__ == '__main__':
    main()
