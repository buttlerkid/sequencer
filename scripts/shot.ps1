# Capture the DY Sequencer standalone window to a PNG (z-order independent).
param([string] $Out = "build\shots\standalone.png")
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public class Shot {
  [StructLayout(LayoutKind.Sequential)] public struct R { public int L,T,Rt,B; }
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out R r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
}
"@
$p = Get-Process | Where-Object { $_.ProcessName -like "DY Sequencer*" } | Select-Object -First 1
if (-not $p) { throw "standalone not running" }
$h = $p.MainWindowHandle
$r = New-Object Shot+R; [Shot]::GetWindowRect($h, [ref]$r) | Out-Null
$w = $r.Rt - $r.L; $hh = $r.B - $r.T
$bmp = New-Object System.Drawing.Bitmap $w, $hh
$g = [System.Drawing.Graphics]::FromImage($bmp)
$hdc = $g.GetHdc()
[Shot]::PrintWindow($h, $hdc, 2) | Out-Null   # PW_RENDERFULLCONTENT
$g.ReleaseHdc($hdc)
New-Item -ItemType Directory -Force (Split-Path $Out) | Out-Null
$bmp.Save((Resolve-Path (Split-Path $Out)).Path + "\" + (Split-Path -Leaf $Out))
"saved $Out ($w x $hh)"
