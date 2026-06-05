import os

# Write stdint.h with pure ASCII encoding (no BOM, no multibyte chars)
content = """/* Minimal stdint.h for TCC */
#ifndef _STDINT_H
#define _STDINT_H

typedef signed char        int8_t;
typedef unsigned char      uint8_t;
typedef short              int16_t;
typedef unsigned short     uint16_t;
typedef int                int32_t;
typedef unsigned int       uint32_t;
typedef long long          int64_t;
typedef unsigned long long uint64_t;

typedef long              intptr_t;
typedef unsigned long     uintptr_t;

#endif
"""

with open(r'd:\stay\SDVM\tcc_src\stdint.h', 'w', encoding='ascii') as f:
    f.write(content)

# Verify
with open(r'd:\stay\SDVM\tcc_src\stdint.h', 'rb') as f:
    raw = f.read(30)
print(f'First 30 bytes: {raw}')
print(f'Bytes hex: {raw[:10].hex()}')
