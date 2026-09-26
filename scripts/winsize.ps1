# Move / resize a standalone window (for screenshots and UI tests).
param([string] $App = "DY Nodal", [int] $X = 40, [int] $Y = 20, [int] $W = 1196, [int] $H = 1000)
Add-Type @"
using System; using System.Runtime.InteropServices;
public class WinSize {
  [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr h, int x, int y, int w, int hh, bool repaint);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
}
"@
$p = Get-Process | Where-Object { $_.ProcessName -like "$App*" } | Select-Object -First 1
if (-not $p) { throw "standalone not running" }
[WinSize]::ShowWindow($p.MainWindowHandle, 9) | Out-Null   # restore if minimised
Start-Sleep -Milliseconds 300
[WinSize]::MoveWindow($p.MainWindowHandle, $X, $Y, $W, $H, $true) | Out-Null
