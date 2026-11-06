param(
    [Parameter(Mandatory=$true)][string]$Full64,
    [Parameter(Mandatory=$true)][string]$Full86,
    [Parameter(Mandatory=$true)][string]$NoVss64,
    [Parameter(Mandatory=$true)][string]$NoVss86,
    [Parameter(Mandatory=$true)][string]$OutputPath
)

$ErrorActionPreference = 'Stop'
$installer = New-Object -ComObject WindowsInstaller.Installer
$inputs = [ordered]@{ Full64=$Full64; Full86=$Full86; NoVss64=$NoVss64; NoVss86=$NoVss86 }
$packages = [ordered]@{}
foreach ($entry in $inputs.GetEnumerator()) {
    if (-not (Test-Path -LiteralPath $entry.Value -PathType Leaf)) {
        throw "QGA MSI input is missing: $($entry.Value)"
    }
    # Read the supplied media MSI only; never open an installed/cached package for writing.
    $database = $installer.OpenDatabase([IO.Path]::GetFullPath($entry.Value), 0)
    $properties = @{}
    foreach ($name in 'ProductCode','UpgradeCode','ProductVersion') {
        $view = $database.OpenView("SELECT ``Value`` FROM ``Property`` WHERE ``Property``='$name'")
        $view.Execute()
        $record = $view.Fetch()
        if (-not $record) { throw "QGA MSI has no $name`: $($entry.Value)" }
        $value = [string]$record.StringData(1)
        $properties[$name] = if ($name -eq 'ProductVersion') {
            # MSI compares the first three ProductVersion fields.
            ([version]$value).ToString(3)
        } else {
            ([guid]$value).ToString('B').ToUpperInvariant()
        }
        $view.Close()
    }
    $packages[$entry.Key] = $properties
    if ($entry.Key -like 'NoVss*') {
        $view = $database.OpenView('SELECT `FileName` FROM `File`')
        $view.Execute()
        while ($record = $view.Fetch()) {
            $name = ([string]$record.StringData(1)).Split('|')[-1]
            if ($name -in 'qga-vss.dll','qga-vss.tlb') { throw "No-VSS input contains $name`: $($entry.Value)" }
        }
        $view.Close()
    }
}
if (@($packages.Values | ForEach-Object { $_.ProductCode } | Select-Object -Unique).Count -ne 4) {
    throw 'Full and no-VSS QGA inputs must have distinct ProductCodes. Do not relabel a full MSI as no-VSS.'
}
if ($packages.Full64.UpgradeCode -ne $packages.Full86.UpgradeCode -or
    $packages.NoVss64.UpgradeCode -ne $packages.NoVss86.UpgradeCode) {
    throw 'Each QGA variant must use one stable UpgradeCode family for both architectures.'
}
if ($packages.Full64.UpgradeCode -eq $packages.NoVss64.UpgradeCode) {
    throw 'Full and no-VSS QGA must have distinct stable UpgradeCodes so upgrades retain the installed variant.'
}
foreach ($arch in '64','86') {
    if ($packages["Full$arch"].ProductVersion -ne $packages["NoVss$arch"].ProductVersion) {
        throw "Full and no-VSS QGA payload versions must match for architecture $arch."
    }
}
$metadata = [ordered]@{
    QgaFullUpgradeCode = $packages.Full64.UpgradeCode
    QgaNoVssUpgradeCode = $packages.NoVss64.UpgradeCode
    Qga64Version = $packages.Full64.ProductVersion
    Qga86Version = $packages.Full86.ProductVersion
}
$lines = @('<?xml version="1.0" encoding="utf-8"?>', '<Include xmlns="http://wixtoolset.org/schemas/v4/wxs">')
foreach ($entry in $metadata.GetEnumerator()) {
    $lines += '  <?define ' + $entry.Key + ' = "' + $entry.Value + '" ?>'
}
$lines += '</Include>'
$OutputPath = [IO.Path]::GetFullPath($OutputPath)
[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($OutputPath)) | Out-Null
Set-Content -LiteralPath $OutputPath -Value $lines -Encoding UTF8
