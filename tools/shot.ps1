# Screenshot a window WITHOUT stealing focus.
#
# Field guide §2 traps, both honoured here:
#   * PW_CLIENTONLY | PW_RENDERFULLCONTENT (= 3): client area only, so screenshot
#     coordinates == ClientToScreen coordinates. Without CLIENTONLY you are off by the
#     title bar (~31 px), which previously produced hours of "clicking doesn't work".
#   * PrintWindow is used instead of BitBlt-from-screen, so the window may be occluded.
#
# Usage:
#   powershell -File tools\shot.ps1 -ProcessName OpenSpore -Out analysis/shots/loader.png
param(
  [string]$ProcessName = "OpenSpore",
  [string]$WindowTitle = "",
  [string]$Out = "analysis/shots/shot.png",
  [int]$WaitMs = 3000,
  [switch]$Kill
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$src = @"
using System;
using System.Runtime.InteropServices;
public class ShotNative {
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
}
"@
if (-not ("ShotNative" -as [type])) { Add-Type -TypeDefinition $src | Out-Null }

Start-Sleep -Milliseconds $WaitMs

$procs = Get-Process -Name $ProcessName -ErrorAction SilentlyContinue
if (-not $procs) { throw "no process named $ProcessName" }

$hwnd = [IntPtr]::Zero
foreach ($p in $procs) {
  if ($p.MainWindowHandle -ne [IntPtr]::Zero -and [ShotNative]::IsWindowVisible($p.MainWindowHandle)) {
    $hwnd = $p.MainWindowHandle
    if ($WindowTitle -eq "" -or $p.MainWindowTitle -like "*$WindowTitle*") { break }
  }
}
if ($hwnd -eq [IntPtr]::Zero) { throw "no visible main window for $ProcessName" }

$rc = New-Object ShotNative+RECT
[void][ShotNative]::GetClientRect($hwnd, [ref]$rc)
$w = $rc.Right - $rc.Left
$h = $rc.Bottom - $rc.Top
if ($w -le 0 -or $h -le 0) { throw "client rect is empty ($w x $h)" }

$bmp = New-Object System.Drawing.Bitmap($w, $h, [System.Drawing.Imaging.PixelFormat]::Format32bppPArgb)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$hdc = $g.GetHdc()
# 3 = PW_CLIENTONLY (1) | PW_RENDERFULLCONTENT (2)
$ok = [ShotNative]::PrintWindow($hwnd, $hdc, 3)
$g.ReleaseHdc($hdc)
$g.Dispose()
if (-not $ok) { throw "PrintWindow failed" }

# Report what the pixels actually contain: a uniform-black capture is the classic sign of
# a lost/minimised/DRM-protected surface, and "nothing appeared" is not evidence (§4.3).
# Sample BEFORE Dispose - the first version of this script sampled after disposing the
# bitmap, which is a use-after-free on the GDI side.
$colors = @{}
$step_y = [Math]::Max(1, [int]($h / 40))
$step_x = [Math]::Max(1, [int]($w / 40))
for ($y = 0; $y -lt $h; $y += $step_y) {
  for ($x = 0; $x -lt $w; $x += $step_x) {
    $c = $bmp.GetPixel($x, $y)
    $key = "{0:X2}{1:X2}{2:X2}" -f $c.R, $c.G, $c.B
    $colors[$key] = 1 + $colors[$key]
  }
}
$top = $colors.GetEnumerator() | Sort-Object Value -Descending | Select-Object -First 5

$dir = Split-Path -Parent $Out
if ($dir -and -not (Test-Path $dir)) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
$full = if ([IO.Path]::IsPathRooted($Out)) { $Out } else { Join-Path (Get-Location) $Out }
$bmp.Save($full, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()

"saved   : $full"
"client  : ${w}x${h}"
"distinct: $($colors.Count) colours (sampled)"
foreach ($t in $top) { "  top colour #$($t.Key) x$($t.Value)" }

if ($Kill) {
  foreach ($p in $procs) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
  "killed  : $($procs.Count) process(es)"
}
