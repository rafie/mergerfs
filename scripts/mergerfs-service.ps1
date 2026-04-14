<#
.SYNOPSIS
    Install, uninstall, start, or stop mergerfs as a Windows Service.

.DESCRIPTION
    Registers mergerfs.exe as a Windows Service using 'sc create'.
    The service runs mergerfs in the foreground (-f flag) so the SCM
    can manage its lifecycle.  Uses gsudo for elevation when needed.

    On 'install', mergerfs.exe is copied to a local directory
    (C:\Program Files\MergerFS) so it is accessible to service accounts
    that may not have network drives mapped.

.PARAMETER Action
    One of: install, uninstall, start, stop, status, restart

.PARAMETER Branches
    MSYS-style branch spec (e.g. "/c/data1:/d/data2:/e/data3").
    Required for 'install'.

.PARAMETER MountPoint
    MSYS-style mount point (e.g. "/m" for M:\).
    Required for 'install'.

.PARAMETER Options
    Additional mergerfs options (e.g. "category.create=mfs,minfreespace=10G").
    Optional.

.PARAMETER ExePath
    Path to mergerfs.exe.  Defaults to mergerfs.exe next to this script,
    or in the build directory.

.PARAMETER ServiceName
    Windows service name.  Default: "mergerfs".

.PARAMETER InstallDir
    Local directory to copy mergerfs.exe into.
    Default: "C:\Program Files\MergerFS".

.EXAMPLE
    # Install with two branches mounted on M:
    .\mergerfs-service.ps1 install -Branches "/c/media:/d/media" -MountPoint "/m"

.EXAMPLE
    # Install with options
    .\mergerfs-service.ps1 install -Branches "/c/a:/d/b" -MountPoint "/m" `
        -Options "category.create=mfs,minfreespace=10G,volname=MediaPool"

.EXAMPLE
    # Start / stop / status
    .\mergerfs-service.ps1 start
    .\mergerfs-service.ps1 status
    .\mergerfs-service.ps1 stop

.EXAMPLE
    # Uninstall
    .\mergerfs-service.ps1 uninstall
#>

param(
    [Parameter(Mandatory, Position=0)]
    [ValidateSet("install","uninstall","start","stop","status","restart")]
    [string]$Action,

    [string]$Branches,
    [string]$MountPoint,
    [string]$Options,
    [string]$ExePath,
    [string]$ServiceName = "mergerfs",
    [string]$InstallDir = "C:\Program Files\MergerFS"
)

# --- Resolve mergerfs.exe path ---
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
    if (-not $ExePath) {
        Write-Error "Cannot find mergerfs.exe. Use -ExePath to specify."
        exit 1
    }
}

if (-not (Test-Path $ExePath)) {
    Write-Error "mergerfs.exe not found at: $ExePath"
    exit 1
}

# --- Actions ---
switch ($Action) {

    "install" {
        if (-not $Branches) {
            Write-Error "-Branches is required for install (e.g. '/c/data1:/d/data2')"
            exit 1
        }
        if (-not $MountPoint) {
            Write-Error "-MountPoint is required for install (e.g. '/m')"
            exit 1
        }

        # Check if service already exists
        $svc = Get-Service -Name $ServiceName -ErrorAction SilentlyContinue
        if ($svc) {
            Write-Error "Service '$ServiceName' already exists. Run 'uninstall' first."
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

        # Build the command line.
        # -f = foreground (required for SCM management)
        $optStr = "allow_other"
        if ($Options) { $optStr += ",$Options" }

        $binPath = "`"$localExe`" -f -o $optStr $Branches $MountPoint"

        Write-Host "Installing service '$ServiceName'..."
        Write-Host "  Executable : $localExe"
        Write-Host "  Branches   : $Branches"
        Write-Host "  MountPoint : $MountPoint"
        Write-Host "  Options    : $optStr"
        Write-Host "  binPath    : $binPath"
        Write-Host ""

        gsudo --chdir C:\ sc.exe create $ServiceName binPath= $binPath start= demand DisplayName= "MergerFS File System" type= own

        if ($LASTEXITCODE -eq 0) {
            # Set MSYS2_ARG_CONV_EXCL in the service environment so paths aren't mangled
            $regPath = "HKLM\SYSTEM\CurrentControlSet\Services\$ServiceName"
            gsudo --chdir C:\ reg.exe add "$regPath" /v Environment /t REG_MULTI_SZ /d "MSYS2_ARG_CONV_EXCL=*" /f

            Write-Host ""
            Write-Host "Service installed successfully."
            Write-Host "  To start:              .\mergerfs-service.ps1 start"
            Write-Host "  To auto-start on boot: gsudo sc.exe config $ServiceName start= auto"
        } else {
            Write-Error "Failed to create service (exit code $LASTEXITCODE)"
        }
    }

    "uninstall" {
        $svc = Get-Service -Name $ServiceName -ErrorAction SilentlyContinue
        if (-not $svc) {
            Write-Host "Service '$ServiceName' does not exist."
            exit 0
        }

        if ($svc.Status -eq "Running") {
            Write-Host "Stopping service..."
            gsudo --chdir C:\ sc.exe stop $ServiceName | Out-Null
            Start-Sleep -Seconds 3
        }

        Write-Host "Removing service '$ServiceName'..."
        gsudo --chdir C:\ sc.exe delete $ServiceName

        if ($LASTEXITCODE -eq 0) {
            Write-Host "Service removed."
        } else {
            Write-Error "Failed to remove service (exit code $LASTEXITCODE)"
        }

        # Optionally remove the installed exe
        $localExe = Join-Path $InstallDir "mergerfs.exe"
        if (Test-Path $localExe) {
            Write-Host "Removing $localExe ..."
            gsudo --chdir C:\ { param($f) Remove-Item -Force -Path $f } -args $localExe
        }
    }

    "start" {
        Write-Host "Starting service '$ServiceName'..."
        gsudo --chdir C:\ sc.exe start $ServiceName
    }

    "stop" {
        Write-Host "Stopping service '$ServiceName'..."
        gsudo sc.exe stop $ServiceName
    }

    "status" {
        sc.exe query $ServiceName
    }

    "restart" {
        Write-Host "Restarting service '$ServiceName'..."
        gsudo sc.exe stop $ServiceName | Out-Null
        Start-Sleep -Seconds 3
        gsudo --chdir C:\ sc.exe start $ServiceName
    }
}
