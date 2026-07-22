$ErrorActionPreference = 'Continue'
$root = "C:\Users\charl\Documents\SuyuEclipse\build-qt-gui"
$qt   = "C:\Qt\5.15.2\msvc2019_64"
$env:PATH = "$root\bin;$root\vcpkg_installed\x64-windows\debug\bin;$root\vcpkg_installed\x64-windows\bin;$qt\bin;$env:PATH"
$env:QT_PLUGIN_PATH = "$qt\plugins"
$env:QT_QPA_PLATFORM_PLUGIN_PATH = "$qt\plugins\platforms"
$exe = "$root\bin\suyu.exe"
$art = "C:\Users\charl\Documents\SuyuEclipse\artifacts\live"
New-Item -ItemType Directory -Force -Path $art | Out-Null

function Wait-ForMcp { param([int]$Port=9742,[int]$TimeoutSeconds=60)
  $deadline=(Get-Date).AddSeconds($TimeoutSeconds)
  while((Get-Date) -lt $deadline){ try{ $c=New-Object System.Net.Sockets.TcpClient; $c.Connect("127.0.0.1",$Port); $c.Close(); return $true }catch{ Start-Sleep -Milliseconds 500 } }
  return $false
}
function Mcp { param([string]$Method,[hashtable]$Params=@{},[int]$Id=1)
  try{
    $c=New-Object System.Net.Sockets.TcpClient; $c.ReceiveTimeout=15000; $c.SendTimeout=15000
    $c.Connect("127.0.0.1",9742); $s=$c.GetStream()
    $w=New-Object System.IO.StreamWriter($s); $w.AutoFlush=$true
    $r=New-Object System.IO.StreamReader($s)
    $o=@{jsonrpc="2.0";id=$Id;method=$Method}; if($Params.Count -gt 0){$o.params=$Params}
    $w.WriteLine(($o|ConvertTo-Json -Compress -Depth 12)); $resp=$r.ReadLine(); $c.Close(); return $resp
  }catch{ return "ERROR: $($_.Exception.Message)" }
}
function Call { param([string]$Name,[hashtable]$A=@{},[int]$Id=1)
  Mcp -Method "tools/call" -Params @{ name=$Name; arguments=$A } -Id $Id
}

$p = Start-Process -FilePath $exe -ArgumentList '-gamer' -PassThru
if(-not (Wait-ForMcp)){ if(-not $p.HasExited){Stop-Process -Id $p.Id -Force}; Write-Output "FATAL: MCP never came up"; exit 1 }
Start-Sleep -Seconds 2

Write-Output "### initialize";        Mcp -Method "initialize" -Params @{protocolVersion="2024-11-05";capabilities=@{};clientInfo=@{name="live";version="1"}} -Id 1
Write-Output "### tools/list";         Mcp -Method "tools/list" -Id 2
Write-Output "### get_system_info";     Call -Name "get_system_info" -Id 3
Write-Output "### get_keys_status";     Call -Name "get_keys_status" -Id 4
Write-Output "### get_emulator_state";  Call -Name "get_emulator_state" -Id 5
Write-Output "### list_game_directories"; Call -Name "list_game_directories" -Id 6
Write-Output "### list_installed_titles"; Call -Name "list_installed_titles" -Id 7
Write-Output "### get_nintendo_account_state"; Call -Name "get_nintendo_account_state" -Id 8
Write-Output "### shot gamer";          Call -Name "capture_ui_screenshot" -A @{target="main_window";path="$art\01_gamer.png"} -Id 9
Write-Output "### mode programmer";     Call -Name "set_app_mode" -A @{mode="programmer"} -Id 10
Start-Sleep -Milliseconds 800
Write-Output "### shot programmer";     Call -Name "capture_ui_screenshot" -A @{target="main_window";path="$art\02_programmer.png"} -Id 11
Write-Output "### mode hacker";         Call -Name "set_app_mode" -A @{mode="hacker"} -Id 12
Start-Sleep -Milliseconds 800
Write-Output "### shot hacker";         Call -Name "capture_ui_screenshot" -A @{target="main_window";path="$art\03_hacker.png"} -Id 13
Write-Output "### export dialog";       Call -Name "trigger_ui_action" -A @{action="export_game"} -Id 14
Start-Sleep -Seconds 1
Write-Output "### shot export modal";   Call -Name "capture_ui_screenshot" -A @{target="active_modal";path="$art\04_export.png"} -Id 15
Write-Output "### get_log_tail";        Call -Name "get_log_tail" -A @{lines=40} -Id 16

if(-not $p.HasExited){ Stop-Process -Id $p.Id -Force }
Write-Output "### SHOTS"
Get-ChildItem $art -Filter *.png | ForEach-Object { "  $($_.Name) $($_.Length) bytes" }
