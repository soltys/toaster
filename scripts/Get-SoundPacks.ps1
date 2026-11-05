<#
.SYNOPSIS
    Downloads CESP sound packs (peon-ping / OpenPeon packs) into toaster's sounds folder.

.DESCRIPTION
    Looks the packs up in the OpenPeon registry (index.json), downloads each
    pack's openpeon.json manifest, every sound it lists and its icon, and checks
    the sha256 of the manifest and of every sound.

    Layout written to -Destination, which is what cmake/SoundPacks.cmake reads:

        <Destination>\peon.json            (the pack's openpeon.json)
        <Destination>\peon\sounds\PeonReady1.wav
        <Destination>\glados.json
        <Destination>\glados\GLaDOS_P2.png
        <Destination>\glados\sounds\Hello.mp3

    Files that are already present with the right sha256 are not downloaded
    again, so re-running the script only fetches what is missing or changed.
    Re-run CMake configure afterwards to embed the packs into toaster.exe.

.PARAMETER Name
    Pack names from the registry, e.g. peon, glados. Defaults to peon and glados.
    A single comma-separated string also works (handy from CMD).

.PARAMETER All
    Download every pack in the registry.

.PARAMETER List
    List the packs in the registry instead of downloading. -Name filters by
    wildcard (e.g. -List -Name *warcraft*, matched against name and display name).

.PARAMETER Destination
    Sounds folder. Defaults to the msvc-x64 preset's C:\temp\toast_notification\sounds.

.PARAMETER Registry
    URL of the registry index.json.

.PARAMETER Ref
    Git branch or tag to download every selected pack from, instead of the
    default ref.

    Default: packs from PeonPing/og-packs (peon, glados, ...) come from its
    main branch, because the registry pin lags behind (peon is pinned to v1.0.0
    there, while main has 1.1.0 with PeonWorkComplete.wav). Packs from other
    repositories use the registry's pinned source_ref.

    When the ref differs from the registry's pin, the registry's manifest hash
    is not checked, but every sound is still checked against the sha256 in the
    downloaded manifest.

.PARAMETER Pinned
    Use the registry's pinned source_ref for every pack, including og-packs.

.PARAMETER Force
    Download every file again even when it is already present and valid.

.EXAMPLE
    .\scripts\Get-SoundPacks.ps1
    Downloads peon and glados into C:\temp\toast_notification\sounds.

.EXAMPLE
    .\scripts\Get-SoundPacks.ps1 -Name peon,aoe2 -Destination D:\toaster\sounds

.EXAMPLE
    .\scripts\Get-SoundPacks.ps1 -Name peon -Pinned
    Downloads the version of peon that the registry pins (v1.0.0) instead of og-packs main.

.EXAMPLE
    .\scripts\Get-SoundPacks.ps1 -List -Name *portal*
#>
[CmdletBinding(DefaultParameterSetName = 'Named')]
param(
    [Parameter(Position = 0, ParameterSetName = 'Named')]
    [Parameter(Position = 0, ParameterSetName = 'List')]
    [string[]] $Name,

    [Parameter(Mandatory, ParameterSetName = 'All')]
    [switch] $All,

    [Parameter(Mandatory, ParameterSetName = 'List')]
    [switch] $List,

    [string] $Destination = 'C:\temp\toast_notification\sounds',

    [string] $Registry = 'https://raw.githubusercontent.com/PeonPing/registry/main/index.json',

    [string] $Ref,

    [switch] $Pinned,

    [switch] $Force
)

Set-StrictMode -Version 3.0
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue' # the progress bar makes Invoke-WebRequest very slow on Windows PowerShell 5.1

# Windows PowerShell 5.1 may default to TLS 1.0, which GitHub refuses.
if ($PSVersionTable.PSEdition -ne 'Core') {
    [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
}

# Accept "peon,glados" as one argument too (powershell -File passes it as a single string).
if ($Name) {
    $Name = @($Name | ForEach-Object { $_ -split ',' } | ForEach-Object { $_.Trim() } | Where-Object { $_ })
}

function Get-Sha256([string] $Path) {
    (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Read-JsonFile([string] $Path) {
    [IO.File]::ReadAllText($Path, [Text.Encoding]::UTF8) | ConvertFrom-Json
}

function Get-Property($Object, [string] $Property) {
    if ($null -ne $Object -and $Object.PSObject.Properties[$Property]) { $Object.$Property } else { $null }
}

# Same rule as cmake/SoundPacks.cmake (_toaster_sanitize), so <pack>\ matches what CMake looks for.
function ConvertTo-PackFolder([string] $PackName) {
    $PackName.ToLowerInvariant() -replace '[^a-z0-9_]', '_'
}

# Pack paths come from the internet: only allow plain relative paths that stay inside the pack folder.
function Test-SafeRelativePath([string] $RelativePath) {
    if ([string]::IsNullOrWhiteSpace($RelativePath)) { return $false }
    if ($RelativePath -match '^[\\/]' -or $RelativePath -match ':') { return $false }
    foreach ($segment in $RelativePath -split '[\\/]') {
        if ($segment -eq '' -or $segment -eq '.' -or $segment -eq '..') { return $false }
    }
    return $true
}

function Join-Url([string] $Base, [string] $RelativePath) {
    $escaped = ($RelativePath -split '[\\/]' | ForEach-Object { [Uri]::EscapeDataString($_) }) -join '/'
    "$Base/$escaped"
}

# Downloads $Url to $Path through a temporary file; checks $Sha256 when given.
function Save-Url([string] $Url, [string] $Path, [string] $Sha256) {
    $dir = Split-Path -Parent $Path
    if (-not (Test-Path -LiteralPath $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
    $tmp = "$Path.download"
    try {
        Invoke-WebRequest -Uri $Url -OutFile $tmp -UseBasicParsing
        if ($Sha256) {
            $actual = Get-Sha256 $tmp
            if ($actual -ne $Sha256.ToLowerInvariant()) {
                throw "sha256 mismatch for $Url`n  expected $($Sha256.ToLowerInvariant())`n  actual   $actual"
            }
        }
        Move-Item -LiteralPath $tmp -Destination $Path -Force
    }
    finally {
        if (Test-Path -LiteralPath $tmp) { Remove-Item -LiteralPath $tmp -Force }
    }
}

# CMake embeds every *.json in the sounds folder and stops on two manifests with
# the same pack name. Renames other manifests of this pack (e.g. a hand-copied
# jp2.json next to jan_pawel_2.json) to *.json.old so they are ignored.
function Remove-DuplicateManifest([string] $FolderName) {
    $keep = "$FolderName.json"
    foreach ($file in Get-ChildItem -LiteralPath $Destination -Filter '*.json' -File) {
        if ($file.Name -ieq $keep) { continue }
        try { $other = Get-Property (Read-JsonFile $file.FullName) 'name' } catch { continue }
        if ($other -and (ConvertTo-PackFolder $other) -eq $FolderName) {
            $old = "$($file.FullName).old"
            Move-Item -LiteralPath $file.FullName -Destination $old -Force
            Write-Warning "$($file.Name) is another copy of pack '$other'; renamed it to $(Split-Path -Leaf $old) so CMake ignores it"
        }
    }
}

function Install-Pack($Entry) {
    $repo = Get-Property $Entry 'source_repo'
    $ref = Get-Property $Entry 'source_ref'
    $manifestSha = Get-Property $Entry 'manifest_sha256'
    $wanted = if ($Ref) { $Ref } elseif (-not $Pinned -and $repo -eq 'PeonPing/og-packs') { 'main' } else { $null }
    if ($wanted -and $wanted -ne $ref) {
        $ref = $wanted
        $manifestSha = $null # the registry's hash is for its pinned ref only
    }
    $path = Get-Property $Entry 'source_path'
    if (-not $repo -or -not $ref) { throw "registry entry has no source_repo/source_ref" }

    $base = "https://raw.githubusercontent.com/$repo/$([Uri]::EscapeDataString($ref))"
    if ($path -and $path -ne '.') {
        if (-not (Test-SafeRelativePath $path)) { throw "unsafe source_path '$path'" }
        $base = Join-Url $base $path
    }

    # Manifest first: it lists the files and their hashes.
    $manifestTmp = Join-Path $Destination ".$($Entry.name).openpeon.json.download"
    Save-Url (Join-Url $base 'openpeon.json') $manifestTmp $manifestSha
    try {
        $manifest = Read-JsonFile $manifestTmp
        $packName = Get-Property $manifest 'name'
        if (-not $packName) { throw "openpeon.json has no name" }
        $folderName = ConvertTo-PackFolder $packName
        $packDir = Join-Path $Destination $folderName

        # Collect sound files once; the same file may be listed in several categories.
        $files = [ordered]@{}
        $categories = Get-Property $manifest 'categories'
        if ($categories) {
            foreach ($category in $categories.PSObject.Properties) {
                foreach ($sound in @(Get-Property $category.Value 'sounds')) {
                    $file = Get-Property $sound 'file'
                    if (-not $file) { continue }
                    if (-not (Test-SafeRelativePath $file)) { throw "unsafe file path '$file'" }
                    if (-not $files.Contains($file)) { $files[$file] = Get-Property $sound 'sha256' }
                }
            }
        }

        $downloaded = 0
        $kept = 0
        foreach ($file in $files.Keys) {
            $sha = $files[$file]
            $target = Join-Path $packDir ($file -replace '/', '\')
            if (-not $Force -and (Test-Path -LiteralPath $target) -and
                (-not $sha -or (Get-Sha256 $target) -eq $sha.ToLowerInvariant())) {
                $kept++
                continue
            }
            Save-Url (Join-Url $base $file) $target $sha
            $downloaded++
        }

        # The icon is optional: a missing one is only a warning.
        $icon = Get-Property $manifest 'icon'
        if ($icon) {
            $iconTarget = Join-Path $packDir ($icon -replace '/', '\')
            if (-not (Test-SafeRelativePath $icon)) {
                Write-Warning "Pack '$packName': unsafe icon path '$icon'; skipped"
            }
            elseif ($Force -or -not (Test-Path -LiteralPath $iconTarget)) {
                try { Save-Url (Join-Url $base $icon) $iconTarget $null; $downloaded++ }
                catch { Write-Warning "Pack '$packName': icon '$icon' could not be downloaded: $($_.Exception.Message)" }
            }
        }

        # Written last, so a pack only shows up for CMake once all its files are in place.
        Remove-DuplicateManifest $folderName
        Move-Item -LiteralPath $manifestTmp -Destination (Join-Path $Destination "$folderName.json") -Force
        $display = Get-Property $manifest 'display_name'
        $version = Get-Property $manifest 'version'
        Write-Host ("{0} ({1}) {2} from {3}: {4} downloaded, {5} already up to date" -f $packName, $display, $version, $ref, $downloaded, $kept)
    }
    finally {
        if (Test-Path -LiteralPath $manifestTmp) { Remove-Item -LiteralPath $manifestTmp -Force }
    }
}

# --- main ---------------------------------------------------------------------

Write-Verbose "Registry: $Registry"
$index = (Invoke-WebRequest -Uri $Registry -UseBasicParsing).Content | ConvertFrom-Json
$packs = @($index.packs)

if ($List) {
    $shown = $packs
    if ($Name) {
        $shown = $packs | Where-Object {
            $p = $_
            $Name | Where-Object { $p.name -like $_ -or (Get-Property $p 'display_name') -like $_ }
        }
    }
    $shown | Sort-Object name | Format-Table -AutoSize @(
        @{ Label = 'Name'; Expression = { $_.name } },
        @{ Label = 'Display name'; Expression = { Get-Property $_ 'display_name' } },
        @{ Label = 'Sounds'; Expression = { Get-Property $_ 'sound_count' } },
        @{ Label = 'Size (KB)'; Expression = { $size = Get-Property $_ 'total_size_bytes'; if ($size) { [math]::Round($size / 1KB) } } },
        @{ Label = 'Language'; Expression = { Get-Property $_ 'language' } }
    )
    return
}

$failed = $false
if ($All) {
    $selected = $packs
}
else {
    if (-not $Name) { $Name = @('peon', 'glados', 'jan_pawel_2') }
    $selected = foreach ($n in $Name) {
        $entry = $packs | Where-Object { $_.name -eq $n } | Select-Object -First 1
        if (-not $entry) { Write-Error "Pack '$n' is not in the registry. Use -List to see the available packs." -ErrorAction Continue; $failed = $true; continue }
        $entry
    }
}

if (-not (Test-Path -LiteralPath $Destination)) { New-Item -ItemType Directory -Path $Destination -Force | Out-Null }
$Destination = (Resolve-Path -LiteralPath $Destination).Path
Write-Host "Destination: $Destination"

foreach ($entry in @($selected)) {
    try {
        Install-Pack $entry
    }
    catch {
        Write-Error "Pack '$($entry.name)': $($_.Exception.Message)" -ErrorAction Continue
        $failed = $true
    }
}

if ($failed) { exit 1 }
Write-Host 'Done. Re-run CMake configure (cmake --preset msvc-x64) to embed the packs.'
