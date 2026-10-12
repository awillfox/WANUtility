# Builds the app, (re)registers the dev package only when the manifest changed, preserves
# LocalState (message DB + logs) across re-registration, and launches the app.
# Why: re-registering the same version is blocked (0x80073CFB), and Remove-AppxPackage deletes LocalState.
param([switch]$NoLaunch)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$msbuild = 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe'
$name = '29d4b340-cdbf-4ef2-8a8c-cb0bc4b163e0'

Get-Process WANUtility -ErrorAction SilentlyContinue | Stop-Process -Force
& $msbuild "$root\WANUtility.vcxproj" /p:Configuration=Debug /p:Platform=x64 /m /v:minimal /nologo
if ($LASTEXITCODE -ne 0) { throw "Build failed ($LASTEXITCODE)" }

$layout = "$root\x64\Debug\WANUtility"
$manifest = "$layout\AppxManifest.xml"
$stamp = "$layout\.registered-manifest-hash"
$hash = (Get-FileHash $manifest).Hash
$pkg = Get-AppxPackage $name
$upToDate = $pkg -and ($pkg.InstallLocation -eq $layout) -and (Test-Path $stamp) -and ((Get-Content $stamp) -eq $hash)

if (-not $upToDate) {
    $backup = Join-Path $env:TEMP 'WANUtility-LocalState-backup'
    if ($pkg) {
        $ls = "$env:LOCALAPPDATA\Packages\$($pkg.PackageFamilyName)\LocalState"
        if (Test-Path $ls) {
            Remove-Item $backup -Recurse -Force -ErrorAction SilentlyContinue
            Copy-Item $ls $backup -Recurse
            Write-Host "Backed up LocalState to $backup"
        }
        Remove-AppxPackage $pkg.PackageFullName
    }
    Add-AppxPackage -Register $manifest
    $pkg = Get-AppxPackage $name
    $ls = "$env:LOCALAPPDATA\Packages\$($pkg.PackageFamilyName)\LocalState"
    if (Test-Path $backup) {
        New-Item -ItemType Directory -Force $ls | Out-Null
        Copy-Item "$backup\*" $ls -Recurse -Force
        Write-Host "Restored LocalState"
    }
    $hash | Set-Content $stamp
    Write-Host "Registered package from $layout"
}

if (-not $NoLaunch) { Start-Process "shell:AppsFolder\$($pkg.PackageFamilyName)!App" }
Write-Host "LocalState: $env:LOCALAPPDATA\Packages\$($pkg.PackageFamilyName)\LocalState"
