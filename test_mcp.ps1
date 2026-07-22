function Resolve-SuyuExecutable {
  $candidates = @(
    "C:\Users\charl\Documents\SuyuEclipse\build-qt-gui\bin\suyu.exe",
    "C:\Users\charl\Documents\SuyuEclipse\build\bin\suyu.exe",
    "C:\Users\charl\Documents\SuyuEclipse\build-ninja\bin\suyu.exe"
  )

  foreach ($candidate in $candidates) {
    if (Test-Path $candidate) {
      return $candidate
    }
  }

  throw "Could not locate a built suyu.exe in the known build directories."
}

function Import-VsDevEnvironment {
  $vsdev = "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\Common7\Tools\VsDevCmd.bat"
  if (-not (Test-Path $vsdev)) {
    return
  }

  $cmd = ('"{0}" -arch=x64 -host_arch=x64 && set' -f $vsdev)
  cmd /c $cmd | ForEach-Object {
    if ($_ -match '^(.*?)=(.*)$') {
      Set-Item -Path ("Env:" + $matches[1]) -Value $matches[2]
    }
  }
}

Import-VsDevEnvironment
$exe = Resolve-SuyuExecutable
$shots = @(
  "C:\Users\charl\Documents\SuyuEclipse\artifacts\mcp-programmer.png",
  "C:\Users\charl\Documents\SuyuEclipse\artifacts\mcp-hacker.png",
  "C:\Users\charl\Documents\SuyuEclipse\artifacts\mcp-export-dialog.png"
)
foreach($s in $shots){ if(Test-Path $s){ Remove-Item $s -Force } }

function Wait-ForMcpServer {
  param(
    [int]$Port = 9742,
    [int]$TimeoutSeconds = 45
  )

  $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
  while ((Get-Date) -lt $deadline) {
    try {
      $client = New-Object System.Net.Sockets.TcpClient
      $client.Connect("127.0.0.1", $Port)
      $client.Close()
      return $true
    } catch {
      Start-Sleep -Milliseconds 500
    }
  }

  return $false
}

$p = Start-Process -FilePath $exe -ArgumentList '-gamer' -PassThru
if (-not (Wait-ForMcpServer)) {
  if(-not $p.HasExited){ Stop-Process -Id $p.Id -Force }
  throw "Timed out waiting for the MCP server on 127.0.0.1:9742."
}

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
$results += Invoke-McpRequest -Method "tools/list" -Id 2
$results += Invoke-McpRequest -Method "tools/call" -Params @{ name="get_nintendo_account_state"; arguments=@{} } -Id 3
$results += Invoke-McpRequest -Method "tools/call" -Params @{ name="set_app_mode"; arguments=@{ mode="programmer" } } -Id 4
$results += Invoke-McpRequest -Method "tools/call" -Params @{ name="capture_ui_screenshot"; arguments=@{ target="main_window"; path=$shots[0] } } -Id 5
$results += Invoke-McpRequest -Method "tools/call" -Params @{ name="set_app_mode"; arguments=@{ mode="hacker" } } -Id 6
$results += Invoke-McpRequest -Method "tools/call" -Params @{ name="capture_ui_screenshot"; arguments=@{ target="main_window"; path=$shots[1] } } -Id 7
$results += Invoke-McpRequest -Method "tools/call" -Params @{ name="trigger_ui_action"; arguments=@{ action="export_game" } } -Id 8
Start-Sleep -Seconds 2
$results += Invoke-McpRequest -Method "tools/call" -Params @{ name="capture_ui_screenshot"; arguments=@{ target="active_modal"; path=$shots[2] } } -Id 9

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
