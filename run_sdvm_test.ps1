& "d:\stay\SDVM\SDVM.exe" "d:\stay\SDVM\test_webstar.dance" "-v" *> "d:\stay\SDVM\sdvm_test_log.txt"
$exitCode = $LASTEXITCODE
"Exit code: $exitCode" | Out-File -FilePath "d:\stay\SDVM\sdvm_test_log.txt" -Encoding UTF8 -Append
