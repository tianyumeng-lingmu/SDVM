import struct, sys

# Write everything to a file
log = open(r'd:\stay\SDVM\_dump_output.txt', 'w', encoding='utf-8')

with open(r'd:\stay\SDVM\test_webstar.dance', 'rb') as f:
    data = f.read()

log.write(f"File size: {len(data)} bytes\n")
log.write(f"Magic: {data[0:6]}\n")

spc = struct.unpack_from('<I', data, 6)[0]
log.write(f"Strpool count: {spc}\n")

offset = 10
for i in range(spc):
    slen = struct.unpack_from('<I', data, offset)[0]
    offset += 4
    s = data[offset:offset+slen].decode('utf-8', errors='replace')
    log.write(f"  str[{i}] ({slen}): '{s}'\n")
    offset += slen + 1

log.write(f"After strpool, offset: {offset}\n")

fc = struct.unpack_from('<I', data, offset)[0]
offset += 4
log.write(f"Func count: {fc}\n")

for i in range(fc):
    name_off = struct.unpack_from('<I', data, offset)[0]
    body_off = struct.unpack_from('<I', data, offset + 4)[0]
    body_sz = struct.unpack_from('<I', data, offset + 8)[0]
    local_c = struct.unpack_from('<I', data, offset + 12)[0]
    offset += 16
    name_bytes = data[name_off:]
    name_end = name_bytes.find(b'\x00')
    name = name_bytes[:name_end].decode('utf-8', errors='replace')
    log.write(f"  func[{i}]: '{name}' body_off={body_off} body_sz={body_sz} locals={local_c}\n")

code_start = offset
log.write(f"\nCode starts at: {code_start}\n")

i = code_start
bif_count = 0
while i < len(data):
    op = data[i]
    if op == 0x0B:
        bif_idx = data[i + 1]
        argc = data[i + 2]
        log.write(f"  BIF at offset {i}: bif_idx={bif_idx}, args={argc}\n")
        bif_count += 1
        i += 3
    elif op == 0x0A:
        func_idx = data[i + 1]
        argc = data[i + 2]
        log.write(f"  CALL at offset {i}: func_idx={func_idx}, args={argc}\n")
        i += 3
    else:
        i += 1

log.write(f"\nTotal BIF instructions: {bif_count}\n")
log.write(f"Total code bytes: {len(data) - code_start}\n")
log.close()
