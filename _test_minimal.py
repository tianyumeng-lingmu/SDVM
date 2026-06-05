import os, sys
f = open(r'd:\stay\SDVM\_test_output.txt', 'w')
f.write('Python working\n')
f.write(f'CWD: {os.getcwd()}\n')
f.write(f'Args: {sys.argv}\n')
f.close()
