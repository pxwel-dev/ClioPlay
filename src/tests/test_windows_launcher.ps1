$ErrorActionPreference = 'Stop'
$launcher = Join-Path $PSScriptRoot '..\tools\install_windows.ps1'
$tokens = $null
$parseErrors = $null
[void][System.Management.Automation.Language.Parser]::ParseFile(
    (Resolve-Path $launcher), [ref]$tokens, [ref]$parseErrors)
if ($parseErrors.Count) { throw ($parseErrors | Out-String) }
. $launcher

$script:Calls = @()
$script:Failure = ''
$script:InstallerExit = 0
function wsl.exe {
    $script:Calls += ,@($args)
    if ($args -contains 'wslpath') {
        if ($script:Failure -eq 'path') { $global:LASTEXITCODE=1; return }
        $global:LASTEXITCODE=0
        '/mnt/c/ClioPlay project with spaces'
    }
    elseif ($args -contains '/bin/true') {
        $global:LASTEXITCODE = [int]($script:Failure -eq 'distro')
    }
    elseif ($args -contains '/bin/sh') {
        $global:LASTEXITCODE = [int]($script:Failure -eq 'tools')
    }
    else { $global:LASTEXITCODE = $script:InstallerExit }
}

function Start-Process {
    param($FilePath, $ArgumentList, [switch]$NoNewWindow, [switch]$Wait, [switch]$PassThru)
    if ($FilePath -ne 'wsl.exe' -or -not $NoNewWindow -or -not $Wait -or -not $PassThru) {
        throw 'Interactive child must inherit the console and preserve its exit code'
    }
    $script:Calls += ,@('interactive', $ArgumentList)
    [pscustomobject]@{ExitCode=$script:InstallerExit}
}

foreach ($failure in @('distro','path','tools')) {
    $script:Failure=$failure; $script:Calls=@()
    $result=Invoke-ClioPlayWindowsInstall
    if ($result -ne 1) { throw "Prerequisite failure was accepted: $failure" }
    foreach ($call in $script:Calls) {
        if ($call[0] -eq 'interactive') { throw 'Installer reached after prerequisite failure' }
    }
}
$script:Failure=''; $script:Calls=@(); $script:InstallerExit=23
$result=Invoke-ClioPlayWindowsInstall -Distro 'Custom Ubuntu'
if ($result -ne 23) { throw 'Installer failure exit code was not preserved' }
$last=$script:Calls[-1]
if ($last[0] -ne 'interactive' -or $last[1] -ne '"--distribution" "Custom Ubuntu" "--cd" "/mnt/c/ClioPlay project with spaces" "--exec" "python3" "./install.py"') {
    throw 'Distribution, path with spaces or entry point was not forwarded intact'
}
$script:InstallerExit=0
if ((Invoke-ClioPlayWindowsInstall) -ne 0) { throw 'Successful installer did not return zero' }
Write-Host 'Windows launcher syntax and mocked WSL boundary checks passed; no card installation simulated.'
