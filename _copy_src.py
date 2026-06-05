import shutil, os

src_c = r'd:\stay\SDVM\sdvm.c'
src_h = r'd:\stay\SDVM\sdvm.h'
dst_c = r'd:\stay\SDVM\tcc_src\sdvm.c'
dst_h = r'd:\stay\SDVM\tcc_src\sdvm.h'

shutil.copy2(src_c, dst_c)
shutil.copy2(src_h, dst_h)

print(f'Copied sdvm.c ({os.path.getsize(dst_c)} bytes)')
print(f'Copied sdvm.h ({os.path.getsize(dst_h)} bytes)')
