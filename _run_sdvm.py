import subprocess

# Test 1: Run SDVM without arguments (should show usage)
result = subprocess.run(
    [r'd:\stay\SDVM\SDVM.exe'],
    capture_output=True, text=True
)
with open(r'd:\stay\SDVM\sdvm_test_log.txt', 'w', encoding='utf-8') as f:
    f.write(f"=== Test 1: No args ===\n")
    f.write(f"STDOUT: [{result.stdout}]\n")
    f.write(f"STDERR: [{result.stderr}]\n")
    f.write(f"Exit code: {result.returncode}\n\n")

# Test 2: Check if .dance file exists and has content
import os
dance_path = r'd:\stay\SDVM\test_webstar.dance'
with open(r'd:\stay\SDVM\sdvm_test_log.txt', 'a', encoding='utf-8') as f:
    if os.path.exists(dance_path):
        size = os.path.getsize(dance_path)
        f.write(f"=== .dance file ===\n")
        f.write(f"Path: {dance_path}\n")
        f.write(f"Size: {size} bytes\n")
        with open(dance_path, 'rb') as df:
            header = df.read(16)
            f.write(f"Header hex: {header.hex()}\n")
    else:
        f.write(f"File not found: {dance_path}\n")

# Test 3: Try SDVM with verbose
result2 = subprocess.run(
    [r'd:\stay\SDVM\SDVM.exe', dance_path, '-v'],
    capture_output=True, text=True
)
with open(r'd:\stay\SDVM\sdvm_test_log.txt', 'a', encoding='utf-8') as f:
    f.write(f"\n=== Test 3: With .dance file ===\n")
    f.write(f"STDOUT length: {len(result2.stdout)}\n")
    f.write(f"STDERR length: {len(result2.stderr)}\n")
    if result2.stdout:
        f.write(f"STDOUT: [{result2.stdout[:500]}]\n")
    if result2.stderr:
        f.write(f"STDERR: [{result2.stderr[:500]}]\n")
    f.write(f"Exit code: {result2.returncode}\n")
