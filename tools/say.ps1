# Short spoken status for the person at this PC (Polish voice, Windows SAPI).
#   .\say.ps1 "Robię testy na Amidze"
param([Parameter(Mandatory=$true)][string]$Text)
Add-Type -AssemblyName System.Speech
$s = New-Object System.Speech.Synthesis.SpeechSynthesizer
try { $s.SelectVoice('Microsoft Paulina Desktop') } catch {}
$s.Rate = 0
$s.Speak($Text)
$s.Dispose()
