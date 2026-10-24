# Copy-and-adapt provenance: Anamorph scripts/test-windows-installer.ps1 at
# e76a4a66e6ed50f3ade53be9717c2c002865e3c4 (first-party, ADR-0009).
# This product uses Inno's generic uninstaller icon, not Anamorph artwork.
[CmdletBinding()]
param(
    [switch]$SelfTest,
    [string]$Installer,
    [string]$StagingDir,
    [string]$Report = 'build/windows-installer-behavior.json'
)
$ErrorActionPreference = 'Stop'
$AppId = '{19AAA915-4527-4BA2-B6A7-06068E527C54}_is1'

function Registered-UninstallerPath([string]$Command, [string]$AppDirectory) {
    if ($Command -match '^"([^"\r\n]+\.exe)"(?:\s.*)?$') { $path = $Matches[1] }
    elseif ($Command -match '^([^"\r\n]+?\.exe)(?:\s.*)?$') { $path = $Matches[1] }
    else { throw 'unexpected uninstall command syntax' }
    if ([IO.Path]::GetDirectoryName($path) -ne $AppDirectory -or [IO.Path]::GetFileName($path) -notmatch '^unins[0-9]+\.exe$') {
        throw 'uninstaller outside isolated app directory or unexpected filename'
    }
    return $path
}

function Assert-Snapshot($Snapshot, [bool]$AppExpected, [bool]$VstExpected) {
    if (-not $Snapshot.Registered) { throw 'missing uninstall registration' }
    if (-not $Snapshot.UninstallerExists) { throw 'missing registered uninstaller' }
    if (-not $Snapshot.IconExists) { throw 'DisplayIcon resolves to a missing file' }
    if (-not $Snapshot.IconMatchesUninstaller) { throw 'DisplayIcon must reference the unconditional uninstaller' }
    if ($Snapshot.ExtractedIcons -lt 1) { throw 'DisplayIcon has no extractable native icon resource' }
    if ($Snapshot.AppExists -ne $AppExpected -or $Snapshot.AppShortcut -ne $AppExpected) { throw 'unexpected Standalone payload or shortcut' }
    if ($Snapshot.VstExists -ne $VstExpected) { throw 'unexpected VST3 payload' }
    if (-not $Snapshot.UninstallShortcut) { throw 'missing or incorrect uninstall shortcut' }
    if (-not $Snapshot.PayloadMatches) { throw 'installed payload differs from staged product' }
}

function Test-Liveness {
    $app = Join-Path ([IO.Path]::GetTempPath()) 'Anabasis parser fixture'
    $exe = Join-Path $app 'unins000.exe'
    foreach ($command in @($exe, ('"' + $exe + '"'), ('"' + $exe + '" /SILENT'))) {
        if ((Registered-UninstallerPath $command $app) -ne $exe) { throw 'valid uninstaller rejected' }
    }
    foreach ($command in @('malformed', (Join-Path $app 'other.exe'), (Join-Path ([IO.Path]::GetTempPath()) 'unins000.exe'))) {
        $bad = $false
        try { Registered-UninstallerPath $command $app | Out-Null } catch { $bad = $true }
        if (-not $bad) { throw 'unsafe uninstaller path accepted' }
    }
    $count = 0
    foreach ($components in @(@('vst3'), @('standalone'), @('vst3', 'standalone'))) {
        $a = $components -contains 'standalone'; $v = $components -contains 'vst3'
        $good = @{ Registered=$true; UninstallerExists=$true; IconExists=$true; IconMatchesUninstaller=$true;
                   ExtractedIcons=1; AppExists=$a; AppShortcut=$a; VstExists=$v; UninstallShortcut=$true; PayloadMatches=$true }
        Assert-Snapshot $good $a $v
        foreach ($field in @('Registered', 'UninstallerExists', 'IconExists', 'IconMatchesUninstaller', 'ExtractedIcons',
                             'AppExists', 'AppShortcut', 'VstExists', 'UninstallShortcut', 'PayloadMatches')) {
            $mutant = $good.Clone()
            if ($field -eq 'ExtractedIcons') { $mutant[$field] = 0 } else { $mutant[$field] = -not $mutant[$field] }
            $bad = $false
            try { Assert-Snapshot $mutant $a $v } catch { $bad = $true }
            if (-not $bad) { throw "snapshot gate accepted seeded failure: $field" }
            $count++
        }
    }
    Write-Host "Installer self-test: 3 valid component configurations; $count seeded failures rejected"
}

Test-Liveness
if ($SelfTest) { return }
if ($env:GITHUB_ACTIONS -ne 'true' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted' -or $env:RUNNER_OS -ne 'Windows') {
    throw 'native tests require a disposable GitHub-hosted Windows VM'
}
if (-not $Installer -or -not $StagingDir) { throw '-Installer and -StagingDir required' }
$Installer = (Resolve-Path -LiteralPath $Installer).Path
$StagingDir = (Resolve-Path -LiteralPath $StagingDir).Path
$repo = Split-Path -Parent $PSScriptRoot
$reg = "HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\$AppId"
$reg32 = "HKLM:\Software\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\$AppId"
$vstParent = Join-Path ${env:CommonProgramW6432} 'VST3'
$vst = Join-Path $vstParent 'Anabasis.vst3'
$group = Join-Path ([Environment]::GetFolderPath('CommonPrograms')) 'Anabasis'
foreach ($path in @($reg, $reg32, $vst, $group, (Join-Path $env:ProgramW6432 'Anabasis'))) {
    if (Test-Path -LiteralPath $path) { throw "pre-existing installation: $path" }
}
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class InstallerIcon {
    [DllImport("shell32.dll", CharSet=CharSet.Unicode)]
    public static extern uint ExtractIconEx(string path, int index, IntPtr[] large, IntPtr[] small, uint count);
    [DllImport("user32.dll")] public static extern bool DestroyIcon(IntPtr icon);
}
'@
function Extracted-IconCount([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return 0 }
    $large = [IntPtr[]]::new(1); $small = [IntPtr[]]::new(1)
    $count = [InstallerIcon]::ExtractIconEx($Path, 0, $large, $small, 1)
    foreach ($handle in @($large[0], $small[0])) { if ($handle -ne [IntPtr]::Zero) { [InstallerIcon]::DestroyIcon($handle) | Out-Null } }
    if ($count -eq [uint32]::MaxValue) { return 0 }
    return $count
}
function Invoke-Checked([string]$Exe, [string[]]$Arguments) {
    $p = Start-Process -FilePath $Exe -ArgumentList $Arguments -Wait -PassThru
    if ($p.ExitCode -ne 0) { throw "process failed: $Exe ($($p.ExitCode))" }
}
function Shortcut-Target([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $null }
    return (New-Object -ComObject WScript.Shell).CreateShortcut($Path).TargetPath
}
function Equal-File([string]$A, [string]$B) {
    return (Test-Path -LiteralPath $A -PathType Leaf) -and (Test-Path -LiteralPath $B -PathType Leaf) -and
           ((Get-FileHash -LiteralPath $A).Hash -eq (Get-FileHash -LiteralPath $B).Hash)
}
$run = [Guid]::NewGuid().ToString('N')
$root = Join-Path $env:RUNNER_TEMP "Anabasis-installer-$run"
New-Item -ItemType Directory -Path $root | Out-Null
$other = Join-Path $vstParent "Sibling-$run.vst3"
New-Item -ItemType Directory -Path $other -Force | Out-Null
$otherFile = Join-Path $other 'unrelated.keep'
Set-Content -LiteralPath $otherFile -Value 'sibling product'
$presets = Join-Path $env:APPDATA 'RollyTech\Anabasis\Presets'
$preset = Join-Path $presets "installer-test-$run.anabasis"
New-Item -ItemType Directory -Path $presets -Force | Out-Null
Set-Content -LiteralPath $preset -Value 'user preset sentinel'
$settings = Join-Path (Split-Path -Parent $presets) "settings-test-$run.keep"
Set-Content -LiteralPath $settings -Value 'user settings sentinel'
$results = [Collections.Generic.List[object]]::new()
$failure = $null; $snapshot = $null; $log = $null
function Read-Snapshot([string]$App) {
    $entry = Get-ItemProperty -LiteralPath $reg
    $uninstaller = Registered-UninstallerPath $entry.UninstallString $App
    $icon = ($entry.DisplayIcon -replace ',0$', '').Trim('"')
    $exe = Join-Path $App 'Anabasis.exe'
    $vstModule = Join-Path $vst 'Contents\x86_64-win\Anabasis.vst3'
    $appExists = Test-Path -LiteralPath $exe -PathType Leaf
    $vstExists = Test-Path -LiteralPath $vstModule -PathType Leaf
    return @{
        Registered=(Test-Path -LiteralPath $reg); UninstallerExists=(Test-Path -LiteralPath $uninstaller -PathType Leaf);
        IconExists=(Test-Path -LiteralPath $icon -PathType Leaf); IconMatchesUninstaller=($icon -eq $uninstaller);
        ExtractedIcons=(Extracted-IconCount $icon); DisplayIcon=$entry.DisplayIcon; Uninstaller=$uninstaller;
        AppExists=$appExists; AppShortcut=((Shortcut-Target (Join-Path $group 'Anabasis.lnk')) -eq $exe);
        VstExists=$vstExists; UninstallShortcut=((Shortcut-Target (Join-Path $group 'Uninstall Anabasis.lnk')) -eq $uninstaller);
        PayloadMatches=(((-not $appExists) -or (Equal-File $exe (Join-Path $StagingDir 'Anabasis.exe'))) -and
                        ((-not $vstExists) -or (Equal-File $vstModule (Join-Path $StagingDir 'Anabasis.vst3\Contents\x86_64-win\Anabasis.vst3'))))
    }
}
function Uninstall-Test([string]$App, [string]$Label) {
    $uninstaller = Registered-UninstallerPath (Get-ItemProperty -LiteralPath $reg).UninstallString $App
    $unmanaged = Join-Path $App 'unmanaged.keep'
    Set-Content -LiteralPath $unmanaged -Value 'unrelated app-directory file'
    Invoke-Checked $uninstaller @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',"/LOG=`"$(Join-Path $root "$Label-uninstall.log")`"")
    $deadline = [DateTime]::UtcNow.AddSeconds(30)
    while ((Test-Path -LiteralPath $uninstaller) -and [DateTime]::UtcNow -lt $deadline) { Start-Sleep -Milliseconds 100 }
    foreach ($path in @($reg,$reg32,$uninstaller,$vst,$group,(Join-Path $App 'Anabasis.exe'))) {
        if (Test-Path -LiteralPath $path) { throw "owned registration/payload remains: $path" }
    }
    foreach ($path in @($unmanaged,$otherFile,$preset,$settings)) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "unrelated file deleted: $path" }
    }
    $remaining = @(Get-ChildItem -LiteralPath $App -Recurse -File -Force)
    if ($remaining.Count -ne 1 -or $remaining[0].FullName -ne $unmanaged) { throw 'unexpected residual owned files' }
    $results.Add(@{phase='uninstall';label=$Label;unmanaged_and_presets_preserved=$true;registration_removed=$true})
    Remove-Item -LiteralPath $unmanaged
    Remove-Item -LiteralPath $App
}
try {
    # Actual ISCC fault seed: change ONLY the icon directive back to the exact
    # old target. The same staged product and real registry checker are used.
    $text = Get-Content -LiteralPath (Join-Path $repo 'packaging/windows/Anabasis.iss') -Raw
    if ($text -notmatch '(?m)^UninstallDisplayIcon=\{uninstallexe\}\r?$') { throw 'unconditional icon directive missing' }
    $baseline = Join-Path $root 'Anabasis-before.iss'
    $text.Replace('UninstallDisplayIcon={uninstallexe}', 'UninstallDisplayIcon={app}\Anabasis.exe') | Set-Content -LiteralPath $baseline -Encoding utf8
    $versionMatch = Select-String -Path (Join-Path $repo 'CMakeLists.txt') -Pattern '^project\(Anabasis VERSION ([0-9]+\.[0-9]+\.[0-9]+) '
    $version = $versionMatch.Matches[0].Groups[1].Value
    $iscc = Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'
    & $iscc "/DAppVersion=$version" "/DStagingDir=$StagingDir" "/O$root" '/Fbefore' $baseline
    if ($LASTEXITCODE -ne 0) { throw 'baseline ISCC compile failed' }
    $app = Join-Path $root 'before'
    Invoke-Checked (Join-Path $root 'before.exe') @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/SP-','/TYPE=custom','/COMPONENTS=vst3',"/DIR=`"$app`"")
    $snapshot = Read-Snapshot $app
    if ($snapshot.IconExists -or $snapshot.AppExists -or -not $snapshot.UninstallerExists) { throw 'old VST3-only defect was not reproduced' }
    $rejected = $false
    try { Assert-Snapshot $snapshot $false $true } catch {
        if ($_.Exception.Message -ne 'DisplayIcon resolves to a missing file') { throw }
        $rejected = $true
    }
    if (-not $rejected) { throw 'native gate accepted old defect' }
    $results.Add(@{phase='before-fix-reproduction';snapshot=$snapshot;rejected_for_missing_icon=$true})
    Uninstall-Test $app 'before'
    foreach ($initial in @(@('vst3'), @('standalone'), @('vst3','standalone'))) {
        $label = $initial -join '-'; $app = Join-Path $root $label
        $hasApp=$false; $hasVst=$false
        # Inno preserves previously installed components when a later selection
        # excludes them; test the real union, without changing that contract.
        $selections = @($initial, $initial, @('vst3','standalone'), @('vst3'), @('standalone'), $initial)
        $lap = 0
        foreach ($selection in $selections) {
            $lap++; $hasApp = $hasApp -or ($selection -contains 'standalone'); $hasVst = $hasVst -or ($selection -contains 'vst3')
            $log = Join-Path $root "$label-install-$lap.log"
            Invoke-Checked $Installer @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/SP-','/TYPE=custom',"/COMPONENTS=`"$($selection -join ',')`"", "/DIR=`"$app`"", "/LOG=`"$log`"")
            $snapshot = Read-Snapshot $app
            Assert-Snapshot $snapshot $hasApp $hasVst
            $results.Add(@{phase='install-reinstall-component-change';initial=$initial;selection=$selection;lap=$lap;snapshot=$snapshot})
            if ($lap -eq 1) {
                # A real registry mutation must fail for its own missing target,
                # then restore the exact registered value before the next lap.
                $oldIcon = (Get-ItemProperty -LiteralPath $reg).DisplayIcon
                try {
                    Set-ItemProperty -LiteralPath $reg -Name DisplayIcon -Value (Join-Path $app 'missing.exe')
                    $bad = Read-Snapshot $app; $rejected=$false
                    try { Assert-Snapshot $bad $hasApp $hasVst } catch {
                        if ($_.Exception.Message -ne 'DisplayIcon resolves to a missing file') { throw }
                        $rejected=$true
                    }
                    if (-not $rejected) { throw 'real missing-icon mutation accepted' }
                    $results.Add(@{phase='registry-missing-icon-mutant';rejected=$true;initial=$initial})
                } finally { Set-ItemProperty -LiteralPath $reg -Name DisplayIcon -Value $oldIcon }
            }
        }
        Uninstall-Test $app $label
    }
} catch {
    $failure = $_.Exception.Message
    if ($log -and (Test-Path -LiteralPath $log)) { Get-Content -LiteralPath $log -Tail 80 | Write-Host }
    throw
} finally {
    $reportPath = [IO.Path]::GetFullPath($Report)
    New-Item -ItemType Directory -Path (Split-Path -Parent $reportPath) -Force | Out-Null
    @{AppId=$AppId;installer=$Installer;tests=@($results.ToArray());failure=$failure;last_snapshot=$snapshot;isolated_logs=$root} |
        ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $reportPath -Encoding utf8
    # Remove exact test sentinels only. Inno and the disposable VM own cleanup of
    # installed payloads; there is no global plugin/receipt/preset cleanup.
    foreach ($file in @($otherFile,$preset,$settings)) { if (Test-Path -LiteralPath $file -PathType Leaf) { Remove-Item -LiteralPath $file } }
    if (@(Get-ChildItem -LiteralPath $other -Force).Count -eq 0) { Remove-Item -LiteralPath $other }
}
Write-Host 'Native Windows installer component/reinstall/uninstall matrix passed'
