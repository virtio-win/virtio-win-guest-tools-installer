param(
    [Parameter(Mandatory=$true)][string]$Full64,
    [Parameter(Mandatory=$true)][string]$Full86,
    [Parameter(Mandatory=$true)][string]$NoVss64,
    [Parameter(Mandatory=$true)][string]$NoVss86,
    [Parameter(Mandatory=$true)][string]$OutputPath
)

$ErrorActionPreference = 'Stop'
$installer = New-Object -ComObject WindowsInstaller.Installer
$inputs = [ordered]@{
    QgaFull64ProductCode = $Full64
    QgaFull86ProductCode = $Full86
    QgaNoVss64ProductCode = $NoVss64
    QgaNoVss86ProductCode = $NoVss86
}
$codes = [ordered]@{}
$upgradeCodes = @()
foreach ($entry in $inputs.GetEnumerator()) {
    if (-not (Test-Path -LiteralPath $entry.Value -PathType Leaf)) {
        throw "QGA MSI input is missing: $($entry.Value)"
    }
    # Read the supplied media MSI only; never open an installed/cached package for writing.
    $database = $installer.OpenDatabase([IO.Path]::GetFullPath($entry.Value), 0)
    $view = $database.OpenView('SELECT `Value` FROM `Property` WHERE `Property`=''ProductCode''')
    $view.Execute()
    $record = $view.Fetch()
    if (-not $record) { throw "QGA MSI has no ProductCode: $($entry.Value)" }
    $codes[$entry.Key] = ([guid]$record.StringData(1)).ToString('B').ToUpperInvariant()
    $view.Close()
    $view = $database.OpenView('SELECT `Value` FROM `Property` WHERE `Property`=''UpgradeCode''')
    $view.Execute()
    $record = $view.Fetch()
    if (-not $record) { throw "QGA MSI has no UpgradeCode: $($entry.Value)" }
    $upgradeCodes += ([guid]$record.StringData(1)).ToString('B').ToUpperInvariant()
    $view.Close()
    if ($entry.Key -like 'QgaNoVss*') {
        $view = $database.OpenView('SELECT `FileName` FROM `File`')
        $view.Execute()
        while ($record = $view.Fetch()) {
            $name = ([string]$record.StringData(1)).Split('|')[-1]
            if ($name -in 'qga-vss.dll','qga-vss.tlb') { throw "No-VSS input contains $name`: $($entry.Value)" }
        }
        $view.Close()
    }
}
if (@($codes.Values | Select-Object -Unique).Count -ne 4) {
    throw 'Full and no-VSS QGA inputs must have distinct ProductCodes. Do not relabel a full MSI as no-VSS.'
}

$families = @($upgradeCodes | Select-Object -Unique)
if ($families.Count -ne 1) {
    throw 'This build requires one shared QGA UpgradeCode family for its related-registration guard.'
}
$codes['QgaUpgradeCode'] = $families[0]

$lines = @('<?xml version="1.0" encoding="utf-8"?>', '<Include xmlns="http://wixtoolset.org/schemas/v4/wxs">')
foreach ($entry in $codes.GetEnumerator()) {
    $lines += '  <?define ' + $entry.Key + ' = "' + $entry.Value + '" ?>'
}
$lines += '</Include>'
$OutputPath = [IO.Path]::GetFullPath($OutputPath)
[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($OutputPath)) | Out-Null
Set-Content -LiteralPath $OutputPath -Value $lines -Encoding UTF8
