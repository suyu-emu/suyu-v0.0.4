$root="C:\Users\charl\Documents\SuyuEclipse\build-qt-gui"; $qt="C:\Qt\5.15.2\msvc2019_64"
$env:PATH="$root\bin;$root\vcpkg_installed\x64-windows\debug\bin;$root\vcpkg_installed\x64-windows\bin;$qt\bin;$env:PATH"
$env:QT_PLUGIN_PATH="$qt\plugins"; $env:QT_QPA_PLATFORM_PLUGIN_PATH="$qt\plugins\platforms"
$p=Start-Process -FilePath "$root\bin\suyu.exe" -ArgumentList '-gamer' -PassThru
$deadline=(Get-Date).AddSeconds(45); $up=$false
while((Get-Date) -lt $deadline){ try{ $c=New-Object System.Net.Sockets.TcpClient;$c.Connect("127.0.0.1",9742);$c.Close();$up=$true;break }catch{ Start-Sleep -Milliseconds 500 } }
Write-Output "HasExited=$($p.HasExited) up=$up"
if($up){
  Start-Sleep -Milliseconds 600
  $c=New-Object System.Net.Sockets.TcpClient;$c.ReceiveTimeout=8000;$c.SendTimeout=8000;$c.Connect("127.0.0.1",9742)
  $s=$c.GetStream();$w=New-Object System.IO.StreamWriter($s);$w.AutoFlush=$true;$r=New-Object System.IO.StreamReader($s)
  $o=@{jsonrpc="2.0";id=1;method="tools/call";params=@{name="get_emulator_state";arguments=@{}}}
  $w.WriteLine(($o|ConvertTo-Json -Compress -Depth 10)); $resp=$r.ReadLine(); $c.Close()
  Write-Output $resp
}
if(-not $p.HasExited){ Stop-Process -Id $p.Id -Force }
