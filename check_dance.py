"""Check the .dance file structure"""
import struct

with open(r'D:\stay\SDVM\test_debug.dance', 'rb') as f:
    data = f.read()

lines = []
lines.append(f"File size: {len(data)} bytes")
lines.append(f"Magic: {data[0:4]}")
print(f"Version: {struct.unpack('<I', data[4:8])[0]}")

# String pool
off = 8
str_count = struct.unpack('<I', data[off:off+4])[0]
off += 4
print(f"\nString pool count: {str_count}")
for i in range(str_count):
    slen = struct.unpack('<I', data[off:off+4])[0]
    off += 4
    s = data[off:off+slen].decode('utf-8')
    off += slen
    print(f"  [{i}] len={slen} '{s}'")

# Func table
func_count = struct.unpack('<I', data[off:off+4])[0]
off += 4
print(f"\nFunc table count: {func_count}")
for i in range(func_count):
    name_idx = struct.unpack('<I', data[off:off+4])[0]
    arg_count = struct.unpack('<I', data[off+4:off+8])[0]
    local_count = struct.unpack('<I', data[off+8:off+12])[0]
    code_offset = struct.unpack('<I', data[off+12:off+16])[0]
    off += 16
    print(f"  func[{i}]: name_idx={name_idx}, args={arg_count}, locals={local_count}, code_offset={code_offset}")

# Code size
code_size = struct.unpack('<I', data[off:off+4])[0]
off += 4
print(f"\nCode size: {code_size} bytes (remaining: {len(data) - off})")

# Disassemble code
code = data[off:off+code_size]
print(f"\nDisassembly:")
addr = 0
while addr < len(code):
    op = code[addr]
    op_names = {
        0x00:'NOP', 0x01:'ICONST', 0x02:'FCONST', 0x03:'SCONST',
        0x04:'BCONST', 0x05:'NULL', 0x06:'DUP', 0x07:'POP',
        0x10:'LOAD', 0x11:'STORE', 0x20:'ADD', 0x21:'SUB',
        0x22:'MUL', 0x23:'DIV', 0x24:'MOD', 0x25:'NEG',
        0x30:'EQ', 0x31:'NE', 0x32:'LT', 0x33:'GT',
        0x34:'LE', 0x35:'GE', 0x38:'NOT',
        0x40:'JMP', 0x41:'JIF', 0x42:'BIF', 0x43:'RET', 0x44:'HALT',
        0x50:'CALL', 0x51:'ANON', 0x52:'CALLR',
        0x70:'PRINT', 0x71:'SCAN'
    }
    name = op_names.get(op, f'UNK({op:#04x})')
    line = f"  [{addr:04d}] {name}"
    addr += 1
    
    if op == 0x01:  # ICONST
        v = struct.unpack('<i', code[addr:addr+4])[0]; line += f" {v}"; addr += 4
    elif op == 0x02:  # FCONST
        v = struct.unpack('<d', code[addr:addr+8])[0]; line += f" {v}"; addr += 8
    elif op == 0x03:  # SCONST
        v = struct.unpack('<I', code[addr:addr+4])[0]; line += f" #{v}"; addr += 4
    elif op == 0x04:  # BCONST
        line += f" {code[addr]}"; addr += 1
    elif op in (0x10, 0x11):  # LOAD, STORE
        line += f" r{code[addr]}"; addr += 1
    elif op == 0x40:  # JMP
        v = struct.unpack('<i', code[addr:addr+4])[0]; line += f" {v:+d}"; addr += 4
    elif op == 0x41:  # JIF
        v = struct.unpack('<i', code[addr:addr+4])[0]; line += f" {v:+d}"; addr += 4
    elif op == 0x42:  # BIF
        bif = code[addr]; ac = code[addr+1]
        bif_names = {0:'see',1:'int',2:'float',3:'str',4:'bool',5:'type'}
        bname = bif_names.get(bif, f'bif{bif}')
        line += f" {bname}({ac})"; addr += 2
    elif op == 0x50:  # CALL
        fi = struct.unpack('<I', code[addr:addr+4])[0]; ac = code[addr+4]
        line += f" func[{fi}] args={ac}"; addr += 5
    elif op == 0x51:  # ANON
        fi = struct.unpack('<I', code[addr:addr+4])[0]; line += f" func[{fi}]"; addr += 4
    elif op == 0x52:  # CALLR
        line += f" args={code[addr]}"; addr += 1
    elif op in (0x70, 0x71):  # PRINT, SCAN
        line += f" {code[addr]}"; addr += 1
    
    print(line)

print(f"\nTotal disassembled: {addr} / {len(code)} bytes")
if addr < len(code):
    print(f"WARNING: {len(code) - addr} bytes not disassembled!")
