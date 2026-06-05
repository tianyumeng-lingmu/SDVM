import subprocess, os

exe = r'd:\stay\SDVM\SDVM.exe'
tcc = r'd:\stay\SDVM\tcc\tcc\tcc.exe'

if os.path.exists(exe):
    os.remove(exe)

result = subprocess.run(
    [tcc, '-I', r'd:\stay\SDVM\tcc_src', 
     '-I', r'd:\stay\SDVM\tcc\tcc\include', 
     '-I', r'd:\stay\SDVM\tcc\tcc\include\winapi',
     '-o', exe,
     r'd:\stay\SDVM\tcc_src\main.c', 
     r'd:\stay\SDVM\tcc_src\sdvm.c'],
    capture_output=True, text=True, cwd=r'd:\stay\SDVM'
)

with open(r'd:\stay\SDVM\_compile_full.txt', 'w', encoding='utf-8') as f:
    f.write(f'STDOUT:\n{result.stdout}\n')
    f.write(f'STDERR:\n{result.stderr}\n')
    f.write(f'Return code: {result.returncode}\n')

print(f'Return code: {result.returncode}')
if result.stderr:
    print(f'STDERR:\n{result.stderr}')
else:
    print('STDERR empty')
if result.stdout:
    print(f'STDOUT:\n{result.stdout}')

if os.path.exists(exe):
    print(f'SDVM.exe created: {os.path.getsize(exe)} bytes')
else:
    print('SDVM.exe NOT created')
