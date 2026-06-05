import sys, os

# Redirect output to files
sys.stdout = open(r'd:\stay\SDVM\_compile_out.txt', 'w', encoding='utf-8')
sys.stderr = open(r'd:\stay\SDVM\_compile_err.txt', 'w', encoding='utf-8')

# Add compiler's directory to path
sys.path.insert(0, r'd:\stay\SDVM')
sys.path.insert(0, r'd:\stay\star_dance')

import compiler

# Simulate command line
sys.argv = ['compiler.py', r'd:\stay\SDVM\test_webstar.star', '-o', r'd:\stay\SDVM\test_webstar.dance']
try:
    ret = compiler.main()
    print(f'Exit code: {ret}')
except Exception as e:
    import traceback
    print(f'Fatal: {e}')
    traceback.print_exc()
