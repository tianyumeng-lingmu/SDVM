import subprocess, os

exe = r'd:\stay\SDVM\SDVM.exe'
size = os.path.getsize(exe)
mtime = os.path.getmtime(exe)

import datetime
mtime_str = datetime.datetime.fromtimestamp(mtime).strftime('%Y-%m-%d %H:%M:%S')

# Check which BIFs are compiled in by parsing sdvm.h
with open(r'd:\stay\SDVM\sdvm.h', 'r') as f:
    h_content = f.read()
    # Find BIF_NET_START
    for line in h_content.split('\n'):
        if 'BIF_NET' in line or 'BIF_COUNT' in line:
            print(line)

with open(r'd:\stay\SDVM\sdvm_test_log.txt', 'a', encoding='utf-8') as f:
    f.write(f"\n=== SDVM.exe info ===\n")
    f.write(f"Size: {size} bytes\n")
    f.write(f"Modified: {mtime_str}\n")
    
    # Read the old binary to compare
    # Check what's inside sdvm.h BIF definitions
    with open(r'd:\stay\SDVM\sdvm.h', 'r') as hf:
        for line in hf:
            if 'BIF_' in line:
                f.write(f"  {line.strip()}\n")
