import sys, os

# Write directly to a file on d:
out = open(r'd:\stay\SDVM\simple_test.txt', 'w', encoding='utf-8')
out.write('hello from python\n')
out.close()
