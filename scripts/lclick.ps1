# Like click.ps1 but takes LOGICAL editor coordinates (1200x740 design space),
# so it works at any window scale. Also: -Park moves the window top-most (without
# activating it) to the given screen position/width, -Unpark restores normal z-order.
param([int] $X = -1, [int] $Y = -1, [int] $ToX = -1, [int] $ToY = -1, [switch] $Double,
      [switch] $Park, [int] $ParkX = 2560, [int] $ParkY = 0, [int] $ParkW = 900, [switch] $Unpark)
Add-Type @"
using System; using System.Runtime.InteropServices;
public class LC {
  [StructLayout(LayoutKind.Sequential)] public struct R { public int L,T,Rt,B; }
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out R r);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out R r);
  [DllImport("user32.dll")] public static extern bool SendMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
}
"@
[LC]::SetProcessDPIAware() | Out-Null
$p = Get-Process | Where-Object { $_.ProcessName -like "DY Sequencer*" } | Select-Object -First 1
if (-not $p) { throw "standalone not running" }
$h = $p.MainWindowHandle
$SWP_NOACTIVATE = 0x10; $SWP_NOSIZE = 1; $SWP_NOMOVE = 2
if ($Park) {
    $wr = New-Object LC+R; [LC]::GetWindowRect($h, [ref]$wr) | Out-Null
    $cr = New-Object LC+R; [LC]::GetClientRect($h, [ref]$cr) | Out-Null
    $titleH = ($wr.B - $wr.T) - 740.0 * ($cr.Rt / 1200.0)
    $s = ($ParkW - 2) / 1200.0
    $newH = [int](740 * $s + $titleH + 2)
    [LC]::SetWindowPos($h, [IntPtr](-1), $ParkX, $ParkY, $ParkW, $newH, $SWP_NOACTIVATE) | Out-Null
    Start-Sleep -Milliseconds 400
    "parked at $ParkX,$ParkY ${ParkW}x$newH (scale $([math]::Round($s,3)))"
}
if ($Unpark) {
    [LC]::SetWindowPos($h, [IntPtr](-2), 0, 0, 0, 0, $SWP_NOACTIVATE -bor $SWP_NOSIZE -bor $SWP_NOMOVE) | Out-Null
    "unparked"
}
if ($X -lt 0) { return }

$cr = New-Object LC+R; [LC]::GetClientRect($h, [ref]$cr) | Out-Null
$s = $cr.Rt / 1200.0
$titleH = $cr.B - 740.0 * $s
function map([int]$lx, [int]$ly) { return @([int]($lx * $s), [int]($titleH + $ly * $s)) }
function lp([int]$x, [int]$y) { [IntPtr](($y -shl 16) -bor ($x -band 0xFFFF)) }
$c = map $X $Y; $cx = $c[0]; $cy = $c[1]
$WM_MOUSEMOVE = 0x200; $WM_LBUTTONDOWN = 0x201; $WM_LBUTTONUP = 0x202; $WM_LBUTTONDBLCLK = 0x203; $MK = 1
[LC]::SendMessage($h, $WM_MOUSEMOVE, [IntPtr]0, (lp $cx $cy)) | Out-Null; Start-Sleep -Milliseconds 40
[LC]::SendMessage($h, $WM_LBUTTONDOWN, [IntPtr]$MK, (lp $cx $cy)) | Out-Null; Start-Sleep -Milliseconds 60
if ($ToX -ge 0) {
    $t = map $ToX $ToY; $tx = $t[0]; $ty = $t[1]
    for ($i = 1; $i -le 12; $i++) {
        $mx = [int]($cx + ($tx - $cx) * $i / 12); $my = [int]($cy + ($ty - $cy) * $i / 12)
        [LC]::SendMessage($h, $WM_MOUSEMOVE, [IntPtr]$MK, (lp $mx $my)) | Out-Null; Start-Sleep -Milliseconds 15
    }
    $cx = $tx; $cy = $ty
}
[LC]::SendMessage($h, $WM_LBUTTONUP, [IntPtr]0, (lp $cx $cy)) | Out-Null
if ($Double) {
    Start-Sleep -Milliseconds 60
    [LC]::SendMessage($h, $WM_LBUTTONDBLCLK, [IntPtr]$MK, (lp $cx $cy)) | Out-Null; Start-Sleep -Milliseconds 40
    [LC]::SendMessage($h, $WM_LBUTTONUP, [IntPtr]0, (lp $cx $cy)) | Out-Null
}
"clicked logical ($X,$Y) -> client ($cx,$cy) scale $([math]::Round($s,3))"
