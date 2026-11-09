param(
  [int]$ProcessId,
  [string]$OutputPath = "$env:TEMP\nexus-window.png"
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

public static class NativeWindowCapture {
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);

    [DllImport("user32.dll")]
    public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdcBlt, uint flags);

    [DllImport("user32.dll")]
    public static extern bool SetWindowPos(IntPtr hWnd, IntPtr insertAfter,
        int x, int y, int cx, int cy, uint flags);

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    public const uint SWP_NOSIZE = 0x0001;
    public const uint SWP_NOMOVE = 0x0002;
    public const uint SWP_SHOWWINDOW = 0x0040;
    public static readonly IntPtr HWND_TOPMOST = new IntPtr(-1);
    public static readonly IntPtr HWND_NOTOPMOST = new IntPtr(-2);
}
'@

$process = Get-Process -Id $ProcessId
$hwnd = $process.MainWindowHandle
if ($hwnd -eq [IntPtr]::Zero) {
  throw "Process $ProcessId has no main window."
}

[NativeWindowCapture]::SetWindowPos($hwnd, [NativeWindowCapture]::HWND_TOPMOST,
  0, 0, 0, 0, [NativeWindowCapture]::SWP_NOMOVE -bor [NativeWindowCapture]::SWP_NOSIZE -bor [NativeWindowCapture]::SWP_SHOWWINDOW) | Out-Null
[NativeWindowCapture]::SetForegroundWindow($hwnd) | Out-Null
Start-Sleep -Milliseconds 700

$rect = New-Object NativeWindowCapture+RECT
[NativeWindowCapture]::GetWindowRect($hwnd, [ref]$rect) | Out-Null
$width = $rect.Right - $rect.Left
$height = $rect.Bottom - $rect.Top
if ($width -le 0 -or $height -le 0) {
  throw "Invalid window rectangle."
}

$bitmap = New-Object System.Drawing.Bitmap($width, $height)
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$hdc = $graphics.GetHdc()
# PW_RENDERFULLCONTENT = 2; supported on Windows 8.1+.
$captured = [NativeWindowCapture]::PrintWindow($hwnd, $hdc, 2)
$graphics.ReleaseHdc($hdc)
$graphics.Dispose()

$bitmap.Save($OutputPath, [System.Drawing.Imaging.ImageFormat]::Png)
$bitmap.Dispose()

[NativeWindowCapture]::SetWindowPos($hwnd, [NativeWindowCapture]::HWND_NOTOPMOST,
  0, 0, 0, 0, [NativeWindowCapture]::SWP_NOMOVE -bor [NativeWindowCapture]::SWP_NOSIZE) | Out-Null

Write-Output "Captured=$captured Path=$OutputPath Size=${width}x${height}"
