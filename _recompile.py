import subprocess, os

exe = r'd:\stay\SDVM\SDVM.exe'
log_path = r'd:\stay\SDVM\_recompile_log.txt'

with open(log_path, 'w', encoding='utf-8') as log:
    # Check current SDVM.exe
    if os.path.exists(exe):
        size = os.path.getsize(exe)
        mtime = os.path.getmtime(exe)
        import datetime
        log.write(f"Current SDVM.exe: size={size}, modified={datetime.datetime.fromtimestamp(mtime)}\n")
        log.write(f"Removing...\n")
        os.remove(exe)
        log.write(f"Removed: {not os.path.exists(exe)}\n")
    else:
        log.write(f"SDVM.exe not found\n")

    log.write(f"\nRecompiling...\n")
    
    tcc = r'd:\stay\SDVM\tcc\tcc\tcc.exe'
    result = subprocess.run(
        [tcc, '-I', r'd:\stay\SDVM\tcc_src', '-I', r'd:\stay\SDVM\tcc\tcc\include', 
         '-I', r'd:\stay\SDVM\tcc\tcc\include\winapi', '-o', exe,
         r'd:\stay\SDVM\tcc_src\main.c', r'd:\stay\SDVM\tcc_src\sdvm.c', '-lws2_32'],
        capture_output=True, text=True, cwd=r'd:\stay\SDVM'
    )
    
    log.write(f"TCC exit code: {result.returncode}\n")
    if result.stdout:
        log.write(f"STDOUT: {result.stdout[:500]}\n")
    if result.stderr:
        log.write(f"STDERR: {result.stderr[:500]}\n")
    
    if os.path.exists(exe):
        size = os.path.getsize(exe)
        log.write(f"\nNew SDVM.exe: size={size}\n")
    else:
        log.write(f"\nFAILED: SDVM.exe not created\n")
    
    # Now test
    log.write(f"\nTesting...\n")
    test_result = subprocess.run(
        [exe, r'd:\stay\SDVM\test_webstar.dance', '-v'],
        capture_output=True, text=True, cwd=r'd:\stay\SDVM'
    )
    log.write(f"Test exit code: {test_result.returncode}\n")
    log.write(f"STDOUT:\n{test_result.stdout[:1000]}\n")
    log.write(f"STDERR:\n{test_result.stderr[:1000]}\n")
