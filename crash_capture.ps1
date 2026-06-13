param([string]$GamePath,[string]$Label)
$root="C:\Users\charl\Documents\SuyuEclipse\build-qt-gui"; $qt="C:\Qt\5.15.2\msvc2019_64"
$env:PATH="$root\bin;$root\vcpkg_installed\x64-windows\debug\bin;$root\vcpkg_installed\x64-windows\bin;$qt\bin;$env:PATH"
$env:QT_PLUGIN_PATH="$qt\plugins"; $env:QT_QPA_PLATFORM_PLUGIN_PATH="$qt\plugins\platforms"
$err="C:\Users\charl\Documents\SuyuEclipse\artifacts\live\${Label}_stderr.txt"
$out="C:\Users\charl\Documents\SuyuEclipse\artifacts\live\${Label}_stdout.txt"
$p=Start-Process -FilePath "$root\bin\suyu.exe" -ArgumentList '-gamer' -PassThru -RedirectStandardError $err -RedirectStandardOutput $out
$deadline=(Get-Date).AddSeconds(40); $up=$false
while((Get-Date) -lt $deadline){ try{ $c=New-Object System.Net.Sockets.TcpClient;$c.Connect("127.0.0.1",9742);$c.Close();$up=$true;break }catch{ Start-Sleep -Milliseconds 500 } }
if(-not $up){ Write-Output "no MCP"; if(-not $p.HasExited){Stop-Process -Id $p.Id -Force} }
else {
  Start-Sleep -Milliseconds 600
  try{ $c=New-Object System.Net.Sockets.TcpClient;$c.ReceiveTimeout=8000;$c.SendTimeout=8000;$c.Connect("127.0.0.1",9742);$s=$c.GetStream();$w=New-Object System.IO.StreamWriter($s);$w.AutoFlush=$true;$r=New-Object System.IO.StreamReader($s);$o=@{jsonrpc="2.0";id=1;method="tools/call";params=@{name="launch_game_path";arguments=@{path=$GamePath}}};$w.WriteLine(($o|ConvertTo-Json -Compress -Depth 10));try{$resp=$r.ReadLine()}catch{$resp="(launch call closed: $($_.Exception.Message))"};$c.Close();Write-Output "LAUNCH RESP: $resp" }catch{ Write-Output "launch err: $($_.Exception.Message)" }
  Start-Sleep -Seconds 12
  Write-Output "HasExited after launch=$($p.HasExited)"
  if($p.HasExited){ Write-Output "ExitCode=$($p.ExitCode)" }
  if(-not $p.HasExited){ Stop-Process -Id $p.Id -Force }
}
Write-Output "=== STDERR ==="
if(Test-Path $err){ Get-Content $err -Tail 40 } else { Write-Output "(none)" }
Write-Output "=== LOG TAIL (boot) ==="
$log="C:\Users\charl\AppData\Roaming\suyu\log\suyu_log.txt"
if(Test-Path $log){ Get-Content $log -Tail 25 | Where-Object { $_ -notmatch 'settings.cpp' } }
