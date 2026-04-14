<#
.SYNOPSIS
    Register, unregister, start, or stop mergerfs via the WinFSP Launcher.

.DESCRIPTION
    Uses the WinFSP Launcher service (WinFsp.Launcher) to manage mergerfs
    instances. The launcher handles process lifecycle, auto-restart on crash,
    and integrates with the Windows Service Control Manager.

    On 'register', mergerfs.exe is copied to a local directory
    (C:\Program Files\MergerFS) so it is accessible to service accounts
    that may not have network drives mapped.

.PARAMETER Action
    One of: register, unregister, start, stop, info, list

.PARAMETER Branches
    MSYS-style branch spec for 'start' (e.g. "/c/data1:/d/data2").

.PARAMETER MountPoint
    MSYS-style mount point for 'start' (e.g. "/m" for M:\).

.PARAMETER Options
    Additional mergerfs options (e.g. "category.create=mfs,minfreespace=10G").

.PARAMETER InstanceName
    Instance name for start/stop/info.  Default: "default".

.PARAMETER ClassName
    WinFSP Launcher class name.  Default: "mergerfs".

.PARAMETER ExePath
    Path to mergerfs.exe.  Auto-detected if not specified.

.PARAMETER InstallDir
    Local directory to copy mergerfs.exe into.
    Default: "C:\Program Files\MergerFS".

.EXAMPLE
    # Register mergerfs with the WinFSP Launcher (one-time)
    .\mergerfs-launcher.ps1 register

.EXAMPLE
    # Start an instance
    .\mergerfs-launcher.ps1 start -Branches "/c/media:/d/media" -MountPoint "/m"

.EXAMPLE
    # Start with custom options and instance name
    .\mergerfs-launcher.ps1 start -InstanceName "media" `
        -Branches "/c/media:/d/media" -MountPoint "/m" `
        -Options "category.create=mfs,volname=MediaPool"

.EXAMPLE
    # Check status / stop / list
    .\mergerfs-launcher.ps1 info -InstanceName "media"
    .\mergerfs-launcher.ps1 list
    .\mergerfs-launcher.ps1 stop -InstanceName "media"

.EXAMPLE
    # Unregister (remove from WinFSP Launcher)
    .\mergerfs-launcher.ps1 unregister
#>

param(
    [Parameter(Mandatory, Position=0)]
    [ValidateSet("register","unregister","start","stop","info","list")]
    [string]$Action,

    [string]$Branches,
    [string]$MountPoint,
    [string]$Options,
    [string]$InstanceName = "default",
    [string]$ClassName = "mergerfs",
    [string]$ExePath,
    [string]$InstallDir = "C:\Program Files\MergerFS"
)

# --- Find launchctl ---
$winfspRegPath = "HKLM:\SOFTWARE\WOW6432Node\WinFsp"
$winfspDir = $null
if (Test-Path $winfspRegPath) {
    $winfspDir = (Get-ItemProperty $winfspRegPath -ErrorAction SilentlyContinue).InstallDir
}
if (-not $winfspDir) {
    $winfspRegPath = "HKLM:\SOFTWARE\WinFsp"
    if (Test-Path $winfspRegPath) {
        $winfspDir = (Get-ItemProperty $winfspRegPath -ErrorAction SilentlyContinue).InstallDir
    }
}

$launchctl = $null
if ($winfspDir) {
    $candidates = @(
        (Join-Path $winfspDir "bin\launchctl-x64.exe"),
        (Join-Path $winfspDir "bin\launchctl.exe")
    )
    $sxsDir = (Get-ItemProperty $winfspRegPath -ErrorAction SilentlyContinue).SxsDir
    if ($sxsDir) {
        $candidates = @(
            (Join-Path $sxsDir "bin\launchctl-x64.exe"),
            (Join-Path $sxsDir "bin\launchctl.exe")
        ) + $candidates
    }
    foreach ($c in $candidates) {
        if (Test-Path $c) { $launchctl = $c; break }
    }
}

if (-not $launchctl) {
    Write-Error "Cannot find WinFSP launchctl. Is WinFSP installed?"
    exit 1
}

# --- Find mergerfs.exe ---
if (-not $ExePath) {
    $scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
    $candidates = @(
        (Join-Path $scriptDir "mergerfs.exe"),
        (Join-Path $scriptDir "..\build\mergerfs.exe"),
        (Join-Path $scriptDir "..\mergerfs.exe")
    )
    foreach ($c in $candidates) {
        if (Test-Path $c) { $ExePath = (Resolve-Path $c).Path; break }
    }
}

# --- Check WinFSP Launcher service ---
function Assert-LauncherRunning {
    $svc = Get-Service -Name "WinFsp.Launcher" -ErrorAction SilentlyContinue
    if (-not $svc -or $svc.Status -ne "Running") {
        Write-Error "WinFSP Launcher service is not running. Start it with: sc start WinFsp.Launcher"
        exit 1
    }
}

# --- Registry key (reg.exe format) ---
$regKeyBase = "HKLM\SOFTWARE\WOW6432Node\WinFsp\Services"
$testOut = reg.exe query $regKeyBase 2>&1
if ($LASTEXITCODE -ne 0) {
    $regKeyBase = "HKLM\SOFTWARE\WinFsp\Services"
}

# --- Actions ---
switch ($Action) {

    "register" {
        if (-not $ExePath -or -not (Test-Path $ExePath)) {
            Write-Error "Cannot find mergerfs.exe. Use -ExePath to specify."
            exit 1
        }

        # Copy to local install directory so service accounts can access it.
        # First stage to local temp (no elevation, works with network drives),
        # then use gsudo to place in the install dir (may need elevation).
        $localExe = Join-Path $InstallDir "mergerfs.exe"
        Write-Host "Copying mergerfs.exe to $InstallDir ..."
        $tmpCopy = Join-Path $env:TEMP "mergerfs_stage.exe"
        Copy-Item -Force -Path $ExePath -Destination $tmpCopy
        gsudo --chdir C:\ {
            param($staged, $dstDir)
            New-Item -ItemType Directory -Force -Path $dstDir | Out-Null
            Copy-Item -Force -Path $staged -Destination (Join-Path $dstDir "mergerfs.exe")
        } -args $tmpCopy, $InstallDir
        Remove-Item -Force -Path $tmpCopy -ErrorAction SilentlyContinue
        if (-not (Test-Path $localExe)) {
            Write-Error "Failed to copy mergerfs.exe to $localExe"
            exit 1
        }

        $regKey = "$regKeyBase\$ClassName"

        # CommandLine template:
        #   %1 = branches (e.g. /c/data1:/d/data2)
        #   %2 = mount point (e.g. /m)
        #   %3 = extra options (optional)
        # -f keeps mergerfs in foreground for the launcher to manage.
        $cmdLine = "-f -o allow_other,%3 %1 %2"

        Write-Host "Registering '$ClassName' with WinFSP Launcher..."

        gsudo --chdir C:\ {
            param($key, $exe, $cmd, $sec)
            reg.exe add $key /v Executable   /t REG_SZ    /d $exe /f
            reg.exe add $key /v CommandLine   /t REG_SZ    /d $cmd /f
            reg.exe add $key /v Security      /t REG_SZ    /d $sec /f
            reg.exe add $key /v JobControl    /t REG_DWORD /d 1    /f
        } -args $regKey, $localExe, $cmdLine, "D:P(A;;RPWPLC;;;WD)"

        if ($LASTEXITCODE -eq 0) {
            Write-Host ""
            Write-Host "Registration complete."
            Write-Host "  Class      : $ClassName"
            Write-Host "  Executable : $localExe"
            Write-Host "  CommandLine: $cmdLine"
            Write-Host ""
            Write-Host "To start an instance:"
            Write-Host "  .\mergerfs-launcher.ps1 start -Branches '/c/data1:/d/data2' -MountPoint '/m'"
            Write-Host ""
            Write-Host "Or via launchctl directly:"
            Write-Host "  & '$launchctl' start mergerfs default /c/data1:/d/data2 /m category.create=mfs"
        } else {
            Write-Error "Failed to write registry entries."
        }
    }

    "unregister" {
        $regKey = "$regKeyBase\$ClassName"

        $testOut = reg.exe query "$regKey" 2>&1
        if ($LASTEXITCODE -ne 0) {
            Write-Host "'$ClassName' is not registered."
            exit 0
        }

        gsudo --chdir C:\ reg.exe delete "$regKey" /f
        if ($LASTEXITCODE -eq 0) {
            Write-Host "Unregistered '$ClassName' from WinFSP Launcher."
        } else {
            Write-Error "Failed to remove registry entry."
        }

        # Optionally remove the installed exe
        $localExe = Join-Path $InstallDir "mergerfs.exe"
        if (Test-Path $localExe) {
            Write-Host "Removing $localExe ..."
            gsudo --chdir C:\ { param($f) Remove-Item -Force -Path $f } -args $localExe
        }
    }

    "start" {
        Assert-LauncherRunning

        if (-not $Branches) {
            Write-Error "-Branches is required for start (e.g. '/c/data1:/d/data2')"
            exit 1
        }
        if (-not $MountPoint) {
            Write-Error "-MountPoint is required for start (e.g. '/m')"
            exit 1
        }

        $optStr = if ($Options) { $Options } else { "category.create=ff" }

        Write-Host "Starting mergerfs instance '$InstanceName'..."
        Write-Host "  Branches  : $Branches"
        Write-Host "  MountPoint: $MountPoint"
        Write-Host "  Options   : $optStr"

        & $launchctl start $ClassName $InstanceName $Branches $MountPoint $optStr
    }

    "stop" {
        Assert-LauncherRunning

        Write-Host "Stopping mergerfs instance '$InstanceName'..."
        & $launchctl stop $ClassName $InstanceName
    }

    "info" {
        Assert-LauncherRunning

        & $launchctl info $ClassName $InstanceName
    }

    "list" {
        Assert-LauncherRunning

        & $launchctl list
    }
}
