$ErrorActionPreference = 'Stop'

$exePath = 'C:\Users\charl\Documents\SuyuEclipse\build-ninja\bin\suyu.exe'
$gamePath = 'C:\Program Files (x86)\Steam\steamapps\common\Super Smash Bros. Ultimate\Super Smash Bros. Ultimate.xci'
$logPath = Join-Path $env:APPDATA 'suyu\log\suyu_log.txt'
$reportPath = 'C:\Users\charl\Documents\SuyuEclipse\artifacts\suyu_smash_monitor_report.json'

Get-Process suyu -ErrorAction SilentlyContinue | Stop-Process -Force
if (Test-Path $logPath) {
    Remove-Item $logPath -Force
}

$process = Start-Process -FilePath $exePath -ArgumentList ('-g "{0}"' -f $gamePath) -WorkingDirectory (Split-Path $exePath) -PassThru
$checkpoints = 10, 30, 60, 90
$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

$states = foreach ($seconds in $checkpoints) {
    $remainingMilliseconds = [Math]::Max(0, ($seconds * 1000) - [int]$stopwatch.ElapsedMilliseconds)
    [System.Threading.Tasks.Task]::Delay($remainingMilliseconds).Wait()

    $running = Get-Process -Id $process.Id -ErrorAction SilentlyContinue
    if ($null -ne $running) {
        $responding = $null
        try {
            $responding = $running.Responding
        } catch {
        }

        [pscustomobject]@{
            checkpoint_s = $seconds
            alive = $true
            pid = $running.Id
            responding = $responding
            threads = $running.Threads.Count
            ws_mb = [Math]::Round($running.WorkingSet64 / 1MB, 1)
            title = $running.MainWindowTitle
        }
    } else {
        $exitCode = $null
        try {
            $process.Refresh()
            if ($process.HasExited) {
                $exitCode = $process.ExitCode
            }
        } catch {
        }

        [pscustomobject]@{
            checkpoint_s = $seconds
            alive = $false
            pid = $process.Id
            exit_code = $exitCode
        }
    }
}

$lines = if (Test-Path $logPath) { Get-Content -Path $logPath } else { @() }
$getUnknownEventMatches = @()
$exactIApplicationFunctionsMatches = @()
$otherUnknownMatches = @()
$panicMatches = @()
$relevantIndexes = @()
$launchLines = @()

for ($index = 0; $index -lt $lines.Count; $index++) {
    $line = $lines[$index]
    $lineNumber = $index + 1

    if ($line -match 'Failed to open game file|Super Smash Bros\.|Booting game:|LoadROM:') {
        $launchLines += [pscustomobject]@{
            line = $lineNumber
            text = $line
        }
    }

    if ($line -match 'GetUnknownEvent') {
        $getUnknownEventMatches += [pscustomobject]@{
            line = $lineNumber
            text = $line
        }
        $relevantIndexes += $index
    }

    if ($line -like "*Unknown / unimplemented function '210(<unknown>)': port='IApplicationFunctions'*") {
        $exactIApplicationFunctionsMatches += [pscustomobject]@{
            line = $lineNumber
            text = $line
        }
        $relevantIndexes += $index
    } elseif ($line -match 'Unknown / unimplemented function') {
        $otherUnknownMatches += [pscustomobject]@{
            line = $lineNumber
            text = $line
        }
        $relevantIndexes += $index
    }

    if ($line -match 'Userspace PANIC!') {
        $panicMatches += [pscustomobject]@{
            line = $lineNumber
            text = $line
        }
        $relevantIndexes += $index
    }
}

$excerpt = @()
if ($relevantIndexes.Count -gt 0) {
    $lastRelevantIndex = ($relevantIndexes | Sort-Object -Unique)[-1]
    $startIndex = [Math]::Max(0, $lastRelevantIndex - 12)
    $endIndex = [Math]::Min($lines.Count - 1, $lastRelevantIndex + 12)

    for ($index = $startIndex; $index -le $endIndex; $index++) {
        $excerpt += [pscustomobject]@{
            line = $index + 1
            text = $lines[$index]
        }
    }
}

$finalRunning = Get-Process -Id $process.Id -ErrorAction SilentlyContinue
$finalAlive = $null -ne $finalRunning
$finalExitCode = $null
if (-not $finalAlive) {
    try {
        $process.Refresh()
        if ($process.HasExited) {
            $finalExitCode = $process.ExitCode
        }
    } catch {
    }
}

$result = [pscustomobject]@{
    exe = $exePath
    game = $gamePath
    log_path = $logPath
    log_exists = (Test-Path $logPath)
    launched_pid = $process.Id
    final_alive = $finalAlive
    final_exit_code = $finalExitCode
    checkpoints = $states
    counts = [pscustomobject]@{
        GetUnknownEvent = $getUnknownEventMatches.Count
        IApplicationFunctions210 = $exactIApplicationFunctionsMatches.Count
        OtherUnknownUnimplemented = $otherUnknownMatches.Count
        UserspacePanic = $panicMatches.Count
    }
    launch_lines = if ($launchLines.Count -gt 12) { $launchLines[-12..-1] } else { $launchLines }
    excerpt = if ($excerpt.Count -gt 25) { $excerpt[-25..-1] } else { $excerpt }
}

$result | ConvertTo-Json -Depth 6 | Tee-Object -FilePath $reportPath
