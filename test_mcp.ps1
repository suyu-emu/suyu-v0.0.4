$exe = "C:\Users\charl\Documents\SuyuEclipse\build-ninja\bin\suyu.exe"
$shots = @(
  "C:\Users\charl\Documents\SuyuEclipse\artifacts\mcp-programmer.png",
  "C:\Users\charl\Documents\SuyuEclipse\artifacts\mcp-hacker.png",
  "C:\Users\charl\Documents\SuyuEclipse\artifacts\mcp-export-dialog.png"
)
foreach($s in $shots){ if(Test-Path $s){ Remove-Item $s -Force } }

$p = Start-Process -FilePath $exe -PassThru
Start-Sleep -Seconds 5

function Invoke-McpRequest {
  param([string]$Method, [hashtable]$Params = @{}, [int]$Id = 1)
  try {
    $client = New-Object System.Net.Sockets.TcpClient
    $client.ReceiveTimeout = 6000
    $client.SendTimeout = 6000
    $client.Connect("127.0.0.1", 9742)
    $stream = $client.GetStream()
    $writer = New-Object System.IO.StreamWriter($stream)
    $writer.AutoFlush = $true
    $reader = New-Object System.IO.StreamReader($stream)
    $obj = @{ jsonrpc = "2.0"; id = $Id; method = $Method }
    if($Params.Count -gt 0){ $obj.params = $Params }
    $json = ($obj | ConvertTo-Json -Compress -Depth 10)
    $writer.WriteLine($json)
    $resp = $reader.ReadLine()
    $client.Close()
    return $resp
  } catch {
    return "ERROR: $($_.Exception.Message)"
  }
}

$results = @()
$results += Invoke-McpRequest -Method "initialize" -Params @{ protocolVersion="2024-11-05"; capabilities=@{}; clientInfo=@{name="manual-test";version="1.0"} } -Id 1
$results += Invoke-McpRequest -Method "tools/call" -Params @{ name="set_app_mode"; arguments=@{ mode="programmer" } } -Id 2
$results += Invoke-McpRequest -Method "tools/call" -Params @{ name="capture_ui_screenshot"; arguments=@{ target="main_window"; path=$shots[0] } } -Id 3
$results += Invoke-McpRequest -Method "tools/call" -Params @{ name="set_app_mode"; arguments=@{ mode="hacker" } } -Id 4
$results += Invoke-McpRequest -Method "tools/call" -Params @{ name="capture_ui_screenshot"; arguments=@{ target="main_window"; path=$shots[1] } } -Id 5
$results += Invoke-McpRequest -Method "tools/call" -Params @{ name="trigger_ui_action"; arguments=@{ action="export_game" } } -Id 6
Start-Sleep -Seconds 2
$results += Invoke-McpRequest -Method "tools/call" -Params @{ name="capture_ui_screenshot"; arguments=@{ target="active_modal"; path=$shots[2] } } -Id 7

if(-not $p.HasExited){ Stop-Process -Id $p.Id -Force }

$results | ForEach-Object { Write-Output $_ }
foreach($s in $shots){
  if(Test-Path $s){
    $i = Get-Item $s
    Write-Output "SHOT $($i.FullName) SIZE=$($i.Length)"
  } else {
    Write-Output "SHOT MISSING $s"
  }
}
