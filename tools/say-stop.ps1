# Stop a running say-repeat.ps1 (the person has answered).
New-Item -ItemType File -Force (Join-Path $env:TEMP 'amixcom-say.stop') | Out-Null
