@echo off
cd /d d:\stay\SDVM

echo === Starting compilation === > d:\stay\SDVM\build_log.txt 2>&1

echo Running compiler... >> d:\stay\SDVM\build_log.txt 2>&1
python -u compiler.py test_webstar.star -o test_webstar.dance >> d:\stay\SDVM\build_log.txt 2>&1

set EXIT_CODE=%ERRORLEVEL%
echo Exit code: %EXIT_CODE% >> d:\stay\SDVM\build_log.txt 2>&1
echo === Done === >> d:\stay\SDVM\build_log.txt 2>&1

exit /b %EXIT_CODE%
