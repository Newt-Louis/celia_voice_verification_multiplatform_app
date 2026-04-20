param(
    [Parameter(Mandatory = $true)]
    [string]$PackageDir
)

$ErrorActionPreference = 'Stop'

$package = Get-Item -LiteralPath $PackageDir
$repoRoot = (Get-Item -LiteralPath (Join-Path $PSScriptRoot '..')).FullName
$distributeDir = Join-Path $package.FullName 'distribute'
$zipPath = Join-Path $distributeDir 'Voice Embedded Verification.zip'
$stubSource = Join-Path $repoRoot 'tools\windows-installer\installer_stub.cpp'
$buildDir = Join-Path $package.FullName '.installer-build'
$stubExe = Join-Path $buildDir 'installer_stub.exe'
$stubObj = Join-Path $buildDir 'installer_stub.obj'
$installerPath = Join-Path $distributeDir 'Voice Embedded Verification Setup.exe'

if (-not (Test-Path -LiteralPath $zipPath)) {
    throw "Missing portable zip: $zipPath"
}

if (Test-Path -LiteralPath $buildDir) {
    Remove-Item -LiteralPath $buildDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

& cl.exe /nologo /std:c++17 /EHsc /O2 /MT /D_UNICODE /DUNICODE $stubSource /Fo:$stubObj /Fe:$stubExe /link /SUBSYSTEM:WINDOWS shell32.lib user32.lib
if ($LASTEXITCODE -ne 0) {
    throw "cl.exe failed to build installer stub with exit code $LASTEXITCODE"
}

[byte[]]$zipBytes = [System.IO.File]::ReadAllBytes($zipPath)
[byte[]]$sizeBytes = [System.BitConverter]::GetBytes([UInt64]$zipBytes.LongLength)
[byte[]]$markerBytes = [System.Text.Encoding]::ASCII.GetBytes('CELIAZIP')

Copy-Item -LiteralPath $stubExe -Destination $installerPath -Force
$stream = [System.IO.File]::Open($installerPath, [System.IO.FileMode]::Append, [System.IO.FileAccess]::Write)
try {
    $stream.Write($zipBytes, 0, $zipBytes.Length)
    $stream.Write($sizeBytes, 0, $sizeBytes.Length)
    $stream.Write($markerBytes, 0, $markerBytes.Length)
} finally {
    $stream.Dispose()
}

Get-Item -LiteralPath $installerPath
