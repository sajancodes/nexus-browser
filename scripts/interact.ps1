param([int]$PidHint=0)
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
$td = 'using System; using System.Runtime.InteropServices; public static class W19 { [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L; public int T; public int R; public int B; } [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h); [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r); }'
Add-Type -TypeDefinition $td
if ($PidHint -eq 0) { $p = Get-Process nexus | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1 } else { $p = Get-Process -Id $PidHint }
$h = $p.MainWindowHandle
[W19]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 700
function Title { (Get-Process -Id $p.Id).MainWindowTitle }
function ClickAt([int]$sx,[int]$sy) {
  [System.Windows.Forms.Cursor]::Position = New-Object System.Drawing.Point($sx,$sy)
  Start-Sleep -Milliseconds 150
  $td2 = 'using System; using System.Runtime.InteropServices; public static class ME { [DllImport("user32.dll")] public static extern void mouse_event(uint dwFlags, int dx, int dy, uint cButtons, UIntPtr dwExtraInfo); }'
  Add-Type -TypeDefinition $td2
  [ME]::mouse_event(0x0002,0,0,0,[UIntPtr]::Zero)
  Start-Sleep -Milliseconds 50
  [ME]::mouse_event(0x0004,0,0,0,[UIntPtr]::Zero)
  Start-Sleep -Milliseconds 200
}
Write-Output "Initial title: $(Title)"
ClickAt 768 101
Start-Sleep -Milliseconds 300
[System.Windows.Forms.SendKeys]::SendWait('^l')
Start-Sleep -Seconds 2
[System.Windows.Forms.SendKeys]::SendWait('example.com')
Start-Sleep -Milliseconds 500
[System.Windows.Forms.SendKeys]::SendWait('{ENTER}')
Start-Sleep -Seconds 6
Write-Output "After navigate: $(Title)"
[System.Windows.Forms.SendKeys]::SendWait('^t')
Start-Sleep -Milliseconds 800
Write-Output "After new tab: $(Title)"
[System.Windows.Forms.SendKeys]::SendWait('^+p')
Start-Sleep -Milliseconds 800
Write-Output "After dashboard toggle: $(Title)"
[System.Windows.Forms.SendKeys]::SendWait('^+p')
Start-Sleep -Milliseconds 500
Write-Output "After dashboard close: $(Title)"