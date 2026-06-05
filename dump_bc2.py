"""Simple bytecode dump - write to file to avoid quota issues"""
import sys, os, struct

# Set up paths like compiler.py does
script_dir = r'D:\stay\SDVM'
star_dance_path = os.path.join(script_dir, '..', 'star_dance')
if os.path.isdir(star_dance_path):
    sys.path.insert(0, star_dance_path)
else:
    sys.path.insert(0, os.path.join(script_dir, '..'))

os.chdir(script_dir)

# Write dump + log to D: (no quota issues)
log_path = r'D:\stay\SDVM\_dump_log.txt'
dump_path = r'D:\stay\SDVM\bc_dump.txt'

log = open(log_path, 'w', encoding='utf-8')
log.write("Starting...\n")

try:
    from lexer import Lexer
    from parser import Parser
    log.write("Parsing...\n")
    
    with open('test_debug.star', 'r', encoding='utf-8') as f:
        source = f.read()
    lexer = Lexer(source)
    tokens = lexer.tokenize()
    log.write(f"Tokenized: {len(tokens)} tokens\n")
    
    parser_obj = Parser(tokens)
    ast = parser_obj.parse()
    log.write("Parsed OK\n")
    
    # Import the compiler module properly
    import importlib.util
    spec = importlib.util.spec_from_file_location("sdvm_compiler", r'D:\stay\SDVM\compiler.py')
    sdvm_compiler = importlib.util.module_from_spec(spec)
    if os.path.isdir(star_dance_path):
        sys.path.insert(0, star_dance_path)
    spec.loader.exec_module(sdvm_compiler)
    
    Compiler = sdvm_compiler.Compiler
    compiler = Compiler()
    compiler.compile(ast)
    log.write(f"Compiled: code={len(compiler.code)}B, errors={compiler.errors}\n")
    log.write(f"strpool count: {len(compiler.strpool)}\n")
    log.write(f"func_defs count: {len(compiler.func_defs)}\n")
    
    # Build func table like write_dance does
    # func 0 = main code, func 1..N = user funcs
    total_code_size = len(compiler.code)
    offset = len(compiler.code)  # main code offset is 0, so user funcs start after main
    func_table_built = []
    
    # main (func 0)
    func_table_built.append({
        'name': None, 'arg_count': 0, 'local_count': compiler.next_local,
        'code_offset': 0, 'code_size': len(compiler.code)
    })
    
    for func in compiler.func_defs:
        func_table_built.append({
            'name': func.name,
            'arg_count': len(func.param_names),
            'local_count': func.next_local,
            'code_offset': offset,
            'code_size': len(func.code)
        })
        offset += len(func.code)
        total_code_size += len(func.code)
    
    lines = []
    lines.append(f"Total code: {total_code_size}B (main={func_table_built[0]['code_size']}B)")
    lines.append(f"Func table: {len(func_table_built)} functions")
    
    for fi, fe in enumerate(func_table_built):
        lines.append(f"  func[{fi}]: offset={fe['code_offset']}, size={fe['code_size']}, "
                     f"args={fe['arg_count']}, locals={fe['local_count']}, name={fe['name']}")
    
    # String pool
    lines.append(f"\nString pool ({len(compiler.strpool)}):")
    for i, s in enumerate(compiler.strpool):
        lines.append(f"  [{i}] {repr(s)}")
    
    # Assemble full bytecode: main code + all func codes
    full_code = bytes(compiler.code)
    for func in compiler.func_defs:
        full_code += bytes(func.code)
    
    # Dump each function's bytecode
    for fi, fe in enumerate(func_table_built):
        start = fe['code_offset']
        end = start + fe['code_size']
        code = full_code[start:end]
        lines.append(f"\n--- func[{fi}]: {fe['name'] or 'main'} (offset={start}, size={fe['code_size']}) ---")
        
        i = 0
        while i < len(code):
            op = code[i]
            op_names = {
                1:'ICONST',2:'FCONST',3:'SCONST',4:'BCONST',5:'NULL',
                6:'LOAD',7:'STORE',8:'ADD',9:'SUB',10:'MUL',11:'DIV',
                12:'NEG',13:'NOT',14:'LT',15:'GT',16:'LE',17:'GE',
                18:'EQ',19:'NE',20:'JMP',21:'JIF',22:'CALL',23:'RET',
                24:'BIF',25:'POP',26:'DUP',27:'HALT',28:'NOP',29:'ANON',30:'CALLR'
            }
            name = op_names.get(op, f'UNK{op}')
            addr = start + i
            i += 1
            line = f"  [{addr:04d}] {name}"
            if op == 1 and i + 4 <= len(code):  # ICONST
                val = struct.unpack('<i', code[i:i+4])[0]; line += f" {val}"; i += 4
            elif op == 2 and i + 8 <= len(code):  # FCONST
                val = struct.unpack('<d', code[i:i+8])[0]; line += f" {val}"; i += 8
            elif op == 3 and i + 4 <= len(code):  # SCONST
                val = struct.unpack('<I', code[i:i+4])[0]; line += f" #{val}"; i += 4
            elif op == 4 and i < len(code):  # BCONST
                line += f" {code[i]}"; i += 1
            elif op == 5:  # NULL
                pass
            elif op in (6, 7) and i < len(code):  # LOAD, STORE
                line += f" r{code[i]}"; i += 1
            elif op in (8,9,10,11,12,13,14,15,16,17,18,19,25,26,27,28):
                pass  # no operands
            elif op == 20 and i + 4 <= len(code):  # JMP
                val = struct.unpack('<i', code[i:i+4])[0]; line += f" {val:+d} (->{addr+4+val:04d})"; i += 4
            elif op == 21 and i + 4 <= len(code):  # JIF
                val = struct.unpack('<i', code[i:i+4])[0]; line += f" {val:+d} (->{addr+4+val:04d})"; i += 4
            elif op == 22 and i + 5 <= len(code):  # CALL
                fi2 = struct.unpack('<I', code[i:i+4])[0]; i += 4
                ac = code[i]; i += 1
                line += f" func[{fi2}] args={ac}"
            elif op == 23:  # RET
                pass
            elif op == 24 and i + 2 <= len(code):  # BIF
                bi = code[i]; i += 1
                ac = code[i]; i += 1
                line += f" bif:{bi} args={ac}"
            elif op == 29 and i + 4 <= len(code):  # ANON
                fi2 = struct.unpack('<I', code[i:i+4])[0]; line += f" func[{fi2}]"; i += 4
            elif op == 30 and i < len(code):  # CALLR
                line += f" args={code[i]}"; i += 1
            lines.append(line)
    
    out = '\n'.join(lines)
    with open(dump_path, 'w', encoding='utf-8') as f:
        f.write(out)
    log.write(f"Written {len(out)} chars to {dump_path}\n")
    
except Exception as e:
    import traceback
    log.write(f"ERROR: {e}\n")
    log.write(traceback.format_exc())

log.close()
