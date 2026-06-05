import sys

# Read first 20 bytes of stdint.h
with open(r'd:\stay\SDVM\tcc_src\stdint.h', 'rb') as f:
    data = f.read(20)

with open(r'd:\stay\SDVM\_bom_check.txt', 'w') as f:
    f.write(f'First 20 bytes hex: {data.hex()}\n')
    f.write(f'Elements: {[hex(b) for b in data]}\n')
