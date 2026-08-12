param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$solution = Join-Path $repositoryRoot "src\DrvInst\DrvInst.sln"
$installDirectory = Join-Path $repositoryRoot "src\DrvInst\Install\$Configuration\ARM64"
$destination = Join-Path $repositoryRoot "Libraries\arm64"
$requiredFiles = @(
    "DrvInstCA.dll",
    "DrvInstCA.exp",
    "DrvInstCA.lib",
    "DrvInstExt.dll",
    "DrvInstLib.wixlib",
    "VirtWinDrvExtension.wixobj"
)

$msbuild = Get-Command msbuild.exe -ErrorAction SilentlyContinue
if (-not $msbuild) {
    throw "MSBuild was not found. Run this from a Visual Studio Developer PowerShell prompt."
}

New-Item -ItemType Directory -Force -Path $installDirectory | Out-Null
& $msbuild.Source $solution /m /t:Build "/p:Configuration=$Configuration" "/p:Platform=ARM64"
if ($LASTEXITCODE -ne 0) {
    throw "The ARM64 custom-action solution build failed."
}

New-Item -ItemType Directory -Force -Path $destination | Out-Null
foreach ($file in $requiredFiles) {
    $source = Join-Path $installDirectory $file
    if (-not (Test-Path $source)) {
        throw "Expected build output was not found: $source"
    }
    Copy-Item -Force $source $destination
}

Write-Host "ARM64 WiX libraries copied to $destination"
