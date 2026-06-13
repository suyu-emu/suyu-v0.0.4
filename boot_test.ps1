param([string]$GamePath, [string]$Label, [int]$WatchSeconds=50)
$root="C:\Users\charl\Documents\SuyuEclipse\build-qt-gui"; $qt="C:\Qt\5.15.2\msvc2019_64"
$art="C:\Users\charl\Documents\SuyuEclipse\artifacts\live"
$env:PATH="$root\bin;$root\vcpkg_installed\x64-windows\debug\bin;$root\vcpkg_installed\x64-windows\bin;$qt\bin;$env:PATH"
$env:QT_PLUGIN_PATH="$qt\plugins"; $env:QT_QPA_PLATFORM_PLUGIN_PATH="$qt\plugins\platforms"
function Mcp($m,$pp,$id){ try{ $c=New-Object System.Net.Sockets.TcpClient;$c.ReceiveTimeout=20000;$c.SendTimeout=20000;$c.Connect("127.0.0.1",9742);$s=$c.GetStream();$w=New-Object System.IO.StreamWriter($s);$w.AutoFlush=$true;$r=New-Object System.IO.StreamReader($s);$o=@{jsonrpc="2.0";id=$id;method=$m};if($pp){$o.params=$pp};$w.WriteLine(($o|ConvertTo-Json -Compress -Depth 10));$resp=$r.ReadLine();$c.Close();return $resp}catch{return "ERR $($_.Exception.Message)"} }
function Field($json,$name){ if($json -match ('"'+$name+'\\":\s*(true|false|[0-9.]+)')){return $matches[1]}; if($json -match ('"'+$name+'\\":\s*\\"([^\\]*)\\"')){return $matches[1]}; return "?" }

$p=Start-Process -FilePath "$root\bin\suyu.exe" -ArgumentList '-gamer' -PassThru
$deadline=(Get-Date).AddSeconds(45); $up=$false
while((Get-Date) -lt $deadline){ try{ $c=New-Object System.Net.Sockets.TcpClient;$c.Connect("127.0.0.1",9742);$c.Close();$up=$true;break }catch{ Start-Sleep -Milliseconds 500 } }
if(-not $up){ Write-Output "FATAL: no MCP"; if(-not $p.HasExited){Stop-Process -Id $p.Id -Force}; exit 1 }
Start-Sleep -Milliseconds 800

Write-Output "=== LAUNCH $Label ==="
Write-Output (Mcp "tools/call" @{name="launch_game_path";arguments=@{path=$GamePath}} 1)
$startTs = Get-Date
for($i=0; $i -lt $WatchSeconds; $i+=3){
  Start-Sleep -Seconds 3
  $st = Mcp "tools/call" @{name="get_emulator_state";arguments=@{}} 2
  $t = [int]((Get-Date)-$startTs).TotalSeconds
  Write-Output ("[{0,3}s] running={1} thread={2} first_frame={3} fps={4}" -f $t,(Field $st 'game_running'),(Field $st 'emulation_thread_running'),(Field $st 'first_frame_displayed'),(Field $st 'fps'))
}
Write-Output "=== THREAD DIAGNOSTICS ==="
Write-Output (Mcp "tools/call" @{name="get_thread_diagnostics";arguments=@{include_backtrace=$true;max_backtrace=6}} 3)
Mcp "tools/call" @{name="capture_ui_screenshot";arguments=@{target="main_window";path="$art\boot_$Label.png"}} 4 | Out-Null
Write-Output "=== LOG TAIL ==="
Write-Output (Mcp "tools/call" @{name="get_log_tail";arguments=@{lines=30}} 5)
Mcp "tools/call" @{name="stop_emulation";arguments=@{}} 6 | Out-Null
Start-Sleep -Seconds 2
if(-not $p.HasExited){ Stop-Process -Id $p.Id -Force }
