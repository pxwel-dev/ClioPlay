param([string]$Distribution = 'Ubuntu-24.04')

$script:ClioPlayInstallRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))

function ConvertTo-ClioPlayProcessArgument {
    param([string]$Value)
    # Start-Process joins ArgumentList into a Windows command line. Quote every
    # argument and escape embedded quotes/trailing slashes using argv rules.
    return '"' + (($Value -replace '(\\*)"', '$1$1\"') -replace '(\\+)$', '$1$1') + '"'
}

function Invoke-ClioPlayWindowsInstall {
    param([string]$Distro = 'Ubuntu-24.04')
    $ErrorActionPreference = 'Stop'
    try {
        if (-not (Get-Command wsl.exe -ErrorAction SilentlyContinue)) {
            throw 'WSL is not installed. Follow WINDOWS.md, then run Install.cmd again.'
        }
        & wsl.exe --distribution $Distro --exec /bin/true | Out-Host
        if ($LASTEXITCODE -ne 0) {
            throw "Cannot start $Distro. Complete the one-time setup in WINDOWS.md."
        }
        $linuxRoot = & wsl.exe --distribution $Distro --exec wslpath -a -u $script:ClioPlayInstallRoot
        if ($LASTEXITCODE -ne 0 -or -not $linuxRoot) {
            throw 'Cannot translate the project folder into a WSL path. Extract it to a local Windows drive.'
        }
        $linuxRoot = ($linuxRoot | Select-Object -Last 1).Trim()
        $check = 'command -v python3 >/dev/null && command -v osmium >/dev/null && command -v c++ >/dev/null && command -v cabextract >/dev/null && python3 -m venv --help >/dev/null'
        & wsl.exe --distribution $Distro --cd $linuxRoot --exec /bin/sh -c $check | Out-Host
        if ($LASTEXITCODE -ne 0) {
            throw 'Required Ubuntu tools are missing. Run the apt command in WINDOWS.md.'
        }
        Write-Host 'ClioPlay installer - Windows through WSL'
        Write-Host 'Use Linux paths in the prompts, such as /mnt/c/Users/you/Downloads/map.osm.pbf.'
        Write-Host 'For SD-card drive letters and mounting, follow WINDOWS.md.'
        # Keep the real Windows console handles: piping this interactive child
        # through PowerShell would risk making Python's stdin non-interactive.
        $arguments = @('--distribution', $Distro, '--cd', $linuxRoot,
                       '--exec', 'python3', './install.py')
        $line = ($arguments | ForEach-Object { ConvertTo-ClioPlayProcessArgument $_ }) -join ' '
        $process = Start-Process -FilePath 'wsl.exe' -ArgumentList $line -NoNewWindow -Wait -PassThru
        return $process.ExitCode
    }
    catch {
        Write-Host $_.Exception.Message -ForegroundColor Red
        return 1
    }
}

if ($MyInvocation.InvocationName -ne '.') {
    exit (Invoke-ClioPlayWindowsInstall -Distro $Distribution)
}
