# Launch a DY Nodal standalone for UI checks without touching the mouse:
#   -Preset n      loads factory preset n when the window opens
#   -Strike s      taps the plate every s seconds (or plays -Notes, held for 1 s)
#   -Notes "60,64" the notes to play on each -Strike tick
#   -Tab 1         opens the Play / Instrument tab of the bottom panel
#   -Instrument    runs "DY Nodal Instrument" instead of the effect
# then moves the window to a known place/size so shot.ps1 / lclick.ps1 line up.
param([int] $Preset = -1, [double] $Strike = 0, [string] $Notes = "", [int] $Tab = -1, [switch] $Instrument,
      [int] $X = 40, [int] $Y = 20, [int] $W = 1196, [int] $H = 1000)
$root = Split-Path -Parent $PSScriptRoot
Stop-Process -Name "DY Nodal*" -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 300
$exe = if ($Instrument) { "build\Nodal\DYNodalInstrument_artefacts\Release\Standalone\DY Nodal Instrument.exe" }
       else { "build\Nodal\DYNodal_artefacts\Release\Standalone\DY Nodal.exe" }
$vars = @{
    DY_NODAL_TEST_PRESET = $(if ($Preset -ge 0) { "$Preset" } else { $null })
    DY_NODAL_TEST_STRIKE = $(if ($Strike -gt 0) { "$Strike" } else { $null })
    DY_NODAL_TEST_NOTES  = $(if ($Notes) { $Notes } else { $null })
    DY_NODAL_TEST_TAB    = $(if ($Tab -ge 0) { "$Tab" } else { $null })
}
foreach ($k in $vars.Keys) { [Environment]::SetEnvironmentVariable($k, $vars[$k], "Process") }
Start-Process (Join-Path $root $exe)
foreach ($k in $vars.Keys) { [Environment]::SetEnvironmentVariable($k, $null, "Process") }
Start-Sleep -Seconds 3
$app = if ($Instrument) { "DY Nodal Instrument" } else { "DY Nodal" }
& (Join-Path $PSScriptRoot "winsize.ps1") -App $app -X $X -Y $Y -W $W -H $H
