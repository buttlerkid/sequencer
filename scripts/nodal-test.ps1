# Launch the DY Nodal standalone for UI checks without touching the mouse:
#   -Preset n   loads factory preset n when the window opens
#   -Strike s   taps the plate every s seconds
# then moves the window to a known place/size so shot.ps1 / lclick.ps1 line up.
param([int] $Preset = -1, [double] $Strike = 0, [int] $X = 40, [int] $Y = 20, [int] $W = 1196, [int] $H = 1000)
$root = Split-Path -Parent $PSScriptRoot
Stop-Process -Name "DY Nodal" -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 300
[Environment]::SetEnvironmentVariable("DY_NODAL_TEST_PRESET", $(if ($Preset -ge 0) { "$Preset" } else { $null }), "Process")
[Environment]::SetEnvironmentVariable("DY_NODAL_TEST_STRIKE", $(if ($Strike -gt 0) { "$Strike" } else { $null }), "Process")
Start-Process (Join-Path $root "build\Nodal\DYNodal_artefacts\Release\Standalone\DY Nodal.exe")
[Environment]::SetEnvironmentVariable("DY_NODAL_TEST_PRESET", $null, "Process")
[Environment]::SetEnvironmentVariable("DY_NODAL_TEST_STRIKE", $null, "Process")
Start-Sleep -Seconds 3
& (Join-Path $PSScriptRoot "winsize.ps1") -App "DY Nodal" -X $X -Y $Y -W $W -H $H
