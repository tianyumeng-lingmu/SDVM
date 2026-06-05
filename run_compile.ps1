$log = "d:\stay\SDVM\build_log.txt"
"=== Starting compilation ===" | Out-File $log -Encoding UTF8
cd d:\stay\SDVM
$p = Start-Process -FilePath python -ArgumentList @("-u", "compiler.py", "test_webstar.star", "-o", "test_webstar.dance") -NoNewWindow -RedirectStandardOutput "$log.tmp" -RedirectStandardError "$log.err" -Wait
$exitCode = $p.ExitCode
"Exit code: " + $exitCode | Out-File $log -Encoding UTF8 -Append
"=== Done ===" | Out-File $log -Encoding UTF8 -Append
