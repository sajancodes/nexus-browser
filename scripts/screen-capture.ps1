param([int]$PidHint=0)
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System; using System.Runtime.InteropServices;
public static class WCap {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L; public int T; public int R; public int B; }
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
}
'@
if ($PidHint -eq 0) {
  $p = Get-Process nexus | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
} else {
  $p = Get-Process -Id $PidHint
}
$h = $p.MainWindowHandle
[WCap]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 700
$r = New-Object WCap+RECT
[WCap]::GetWindowRect($h, [ref]$r) | Out-Null
$w = $r.R - $r.L
$ht = $r.B - $r.T
Write-Output "Rect=$($r.L),$($r.T) Size=$w x $ht"
$bmp = New-Object System.Drawing.Bitmap($w, $ht)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($r.L, $r.T, 0, 0, (New-Object System.Drawing.Size($w, $ht)))
$out = 'C:\Users\ab429\AppData\Local\Temp\opencode\nexus-screen.png'
$bmp.Save($out)
$g.Dispose(); $bmp.Dispose()
$img = [System.Drawing.Bitmap]::FromFile($out)
foreach ($pt in @(@(100,100),@(640,400),@(640,50),@(10,40),@(100,120))) {
  $c = $img.GetPixel($pt[0], $pt[1])
  Write-Output ("{0},{1} = #{2:X2}{3:X2}{4:X2}" -f $pt[0], $pt[1], $c.R, $c.G, $c.B)
}
$img.Dispose()
Write-Output "Saved=$out"