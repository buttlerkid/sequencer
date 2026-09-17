# Post mouse events to the DY Sequencer standalone window without touching the
# real cursor. Coordinates are in the pixel space of scripts/shot.ps1 output
# (window rect including title bar). Usage:
#   .\scripts\click.ps1 -X 1170 -Y 57                 # click
#   .\scripts\click.ps1 -X 300 -Y 500 -ToX 300 -ToY 380  # drag
#   .\scripts\click.ps1 -X 300 -Y 500 -Double
param([int] $X, [int] $Y, [int] $ToX = -1, [int] $ToY = -1, [switch] $Double)
Add-Type @"
using System; using System.Runtime.InteropServices;
public class Clk {
  [StructLayout(LayoutKind.Sequential)] public struct R { public int L,T,Rt,B; }
  [StructLayout(LayoutKind.Sequential)] public struct P { public int X,Y; }
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out R r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref P p);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool SendMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
}
"@
$p = Get-Process | Where-Object { $_.ProcessName -like "DY Sequencer*" } | Select-Object -First 1
if (-not $p) { throw "standalone not running" }
$h = $p.MainWindowHandle
$r = New-Object Clk+R; [Clk]::GetWindowRect($h, [ref]$r) | Out-Null
$c = New-Object Clk+P; $c.X = 0; $c.Y = 0; [Clk]::ClientToScreen($h, [ref]$c) | Out-Null
$offX = $c.X - $r.L; $offY = $c.Y - $r.T
function lp([int]$x, [int]$y) { [IntPtr](($y -shl 16) -bor ($x -band 0xFFFF)) }
$cx = $X - $offX; $cy = $Y - $offY
$WM_MOUSEMOVE = 0x200; $WM_LBUTTONDOWN = 0x201; $WM_LBUTTONUP = 0x202; $WM_LBUTTONDBLCLK = 0x203; $MK_LBUTTON = 1
[Clk]::SendMessage($h, $WM_MOUSEMOVE, [IntPtr]0, (lp $cx $cy)) | Out-Null
Start-Sleep -Milliseconds 40
[Clk]::SendMessage($h, $WM_LBUTTONDOWN, [IntPtr]$MK_LBUTTON, (lp $cx $cy)) | Out-Null
Start-Sleep -Milliseconds 60
if ($ToX -ge 0) {
    $tx = $ToX - $offX; $ty = $ToY - $offY
    $steps = 12
    for ($i = 1; $i -le $steps; $i++) {
        $mx = [int]($cx + ($tx - $cx) * $i / $steps); $my = [int]($cy + ($ty - $cy) * $i / $steps)
        [Clk]::SendMessage($h, $WM_MOUSEMOVE, [IntPtr]$MK_LBUTTON, (lp $mx $my)) | Out-Null
        Start-Sleep -Milliseconds 15
    }
    $cx = $tx; $cy = $ty
}
[Clk]::SendMessage($h, $WM_LBUTTONUP, [IntPtr]0, (lp $cx $cy)) | Out-Null
if ($Double) {
    Start-Sleep -Milliseconds 60
    [Clk]::SendMessage($h, $WM_LBUTTONDBLCLK, [IntPtr]$MK_LBUTTON, (lp $cx $cy)) | Out-Null
    Start-Sleep -Milliseconds 40
    [Clk]::SendMessage($h, $WM_LBUTTONUP, [IntPtr]0, (lp $cx $cy)) | Out-Null
}
"clicked client ($cx,$cy) offset ($offX,$offY)"
