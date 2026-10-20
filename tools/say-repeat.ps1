# Say a waiting-for-you message, then again every 30 s until it is stopped
# (tools\say-stop.ps1, run when the person answers) - at most an hour.
#   .\say-repeat.ps1 "Mam wyniki, sprawdź"
param([Parameter(Mandatory=$true)][string]$Text, [int]$EverySec = 30, [int]$MaxMin = 60)
$stop = Join-Path $env:TEMP 'amixcom-say.stop'
Remove-Item $stop -ErrorAction SilentlyContinue
Add-Type -AssemblyName System.Speech
$s = New-Object System.Speech.Synthesis.SpeechSynthesizer
try { $s.SelectVoice('Microsoft Paulina Desktop') } catch {}
$end = (Get-Date).AddMinutes($MaxMin)
while (-not (Test-Path $stop) -and (Get-Date) -lt $end) {
    $s.Speak($Text)
    for ($i = 0; $i -lt $EverySec -and -not (Test-Path $stop); $i++) { Start-Sleep 1 }
}
$s.Dispose()
Remove-Item $stop -ErrorAction SilentlyContinue
