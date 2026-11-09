$wsh = New-Object -ComObject WScript.Shell
$desktop = [Environment]::GetFolderPath('Desktop')
$exePath = 'C:\Users\ab429\Desktop\nexus\build\windows-x64-release\bin\nexus.exe'
$icoPath = 'C:\Users\ab429\Desktop\nexus\build\windows-x64-release\bin\nexus.ico'

$sc = $wsh.CreateShortcut("$desktop\NEXUS.lnk")
$sc.TargetPath = $exePath
$sc.WorkingDirectory = 'C:\Users\ab429\Desktop\nexus\build\windows-x64-release\bin'
$sc.IconLocation = "$icoPath,0"
$sc.Description = 'NEXUS Browser'
$sc.Save()

$sc2 = $wsh.CreateShortcut("$desktop\NEXUS Browser.lnk")
$sc2.TargetPath = $exePath
$sc2.WorkingDirectory = 'C:\Users\ab429\Desktop\nexus\build\windows-x64-release\bin'
$sc2.IconLocation = "$icoPath,0"
$sc2.Description = 'NEXUS Browser'
$sc2.Save()

Write-Host "Updated NEXUS shortcuts on Desktop successfully."
