<#
.SYNOPSIS
    Downloads a set of notification icons (Microsoft Fluent Emoji, MIT licence) into toaster's images folder.

.DESCRIPTION
    Fetches 256x256 PNG icons from Microsoft's Fluent Emoji repository
    (https://github.com/microsoft/fluentui-emoji, MIT licence), pinned to one
    commit, and checks the sha256 of every file. Each icon is saved under a
    short name, so after embedding it is used as e.g. `toaster -i success`.

    The MIT licence text is saved next to the icons as
    fluentui-emoji-LICENSE.txt (not embedded: only images are).

    Files that are already present with the right sha256 are not downloaded
    again. Re-run CMake configure afterwards to embed the images into toaster.exe.

.PARAMETER Name
    Icon names to download, e.g. success,failure. Defaults to all of them.
    A single comma-separated string also works (handy from CMD).

.PARAMETER List
    List the icons instead of downloading. -Name filters by wildcard.

.PARAMETER Destination
    Images folder. Defaults to the msvc-x64 preset's C:\temp\toast_notification\images.

.PARAMETER Force
    Download every file again even when it is already present and valid.

.EXAMPLE
    .\scripts\Get-Images.ps1
    Downloads every icon into C:\temp\toast_notification\images.

.EXAMPLE
    .\scripts\Get-Images.ps1 -Name success,failure,warning -Destination D:\toaster\images

.EXAMPLE
    .\scripts\Get-Images.ps1 -List
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [string[]] $Name,

    [switch] $List,

    [string] $Destination = 'C:\temp\toast_notification\images',

    [switch] $Force
)

Set-StrictMode -Version 3.0
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue' # the progress bar makes Invoke-WebRequest very slow on Windows PowerShell 5.1

# Windows PowerShell 5.1 may default to TLS 1.0, which GitHub refuses.
if ($PSVersionTable.PSEdition -ne 'Core') {
    [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
}

# Pinned commit of microsoft/fluentui-emoji, so the sha256 values below stay valid.
$Commit = '1ffb34c752ecf5d402f04cfb4b392c77f57c54bc'
$BaseUrl = "https://raw.githubusercontent.com/microsoft/fluentui-emoji/$Commit"

# Name, use, path in the repo's assets folder, sha256.
$Icons = @(
    @('success',     'Task succeeded',              'Check mark button/3D/check_mark_button_3d.png',   '256e3dd950cf1705ab72511ab242472b4bd7db519aa402a6182c1aaf8c58def9'),
    @('failure',     'Task failed',                 'Cross mark/3D/cross_mark_3d.png',                 '2753b89dcd790c7f25fa3c5fd5641821860257364163abd87a2f57bb268466b9'),
    @('warning',     'Warning',                     'Warning/3D/warning_3d.png',                       '318f534c5d93100443f6e48defc47d76140473d797087af5821587f0feb8003c'),
    @('info',        'Information',                 'Information/3D/information_3d.png',               'b3924702264b5959989cc1a8ab011c23748b682af85c566132e3d6b120c9b2b0'),
    @('question',    'Input needed',                'Red question mark/3D/red_question_mark_3d.png',   'fb2af4458d9bf975a4a3a841e8368c9d7315dc881a3080b3fcbcf522934d4cca'),
    @('build',       'Build',                       'Hammer and wrench/3D/hammer_and_wrench_3d.png',   '573a9abccd004b5ef5fbf513f720cc48e835c52e01563d2064b9480bb0e6ee90'),
    @('deploy',      'Deploy / release',            'Rocket/3D/rocket_3d.png',                         '4636925a1dfc76a45dc83c08b983699d39143a0978f794a4e2dfb6cbffdcd7ab'),
    @('bell',        'Generic notification',        'Bell/3D/bell_3d.png',                             'e1c9efed838326cb746462b824456ef382742237017871f4d0fe54c407a9d5bf'),
    @('done',        'Finished (hourglass)',        'Hourglass done/3D/hourglass_done_3d.png',         '3b5e903ed916a692cdb7db131d43f6fc6040ea21303809fd1bfc148cf4af531a'),
    @('waiting',     'Still running',               'Hourglass not done/3D/hourglass_not_done_3d.png', 'ab4183328a0ec7a0440a6fb48113fdb9145f51235e960c7f17097ebb9f13af97'),
    @('timer',       'Timer / duration',            'Stopwatch/3D/stopwatch_3d.png',                   '8dc8f855072855723fde9ef8d6ce5b25db19102222b4a838bceea1e442bd78cf'),
    @('alarm',       'Reminder',                    'Alarm clock/3D/alarm_clock_3d.png',               '1aa6929e1baa440db535fd321868dc76ddd888bf3e4f4333d37800ff2ec41c74'),
    @('celebrate',   'Big success',                 'Party popper/3D/party_popper_3d.png',             '70a8e2f9daf1646be9e773751b7f3b6b281a2415cf29c3c3aeebd9c923cdc3e0'),
    @('trophy',      'Achievement',                 'Trophy/3D/trophy_3d.png',                         '7c4a9805f354eb2aec35063a95a4043306efc726324c0d128883f24497857bbc'),
    @('thumbs_up',   'Approved',                    'Thumbs up/Default/3D/thumbs_up_3d_default.png',   'a48dbe89a1a16969593db76f5bec45e9e537e0f090e7609caa8d9a3ff598d1d2'),
    @('thumbs_down', 'Rejected',                    'Thumbs down/Default/3D/thumbs_down_3d_default.png', '12020dc8a616138d0ab3daa94008b97d0ed35dbdb1f0d80d5f17657e8aee492f'),
    @('stop',        'Stopped / blocked',           'Stop sign/3D/stop_sign_3d.png',                   '0f4ee16d06621100accaf48b082137fffb481cb5ba435178cfe200fdc6f11f79'),
    @('fire',        'Incident / hot',              'Fire/3D/fire_3d.png',                             'd56355132d73df1741edcf37cb4e21ad54f1c46eeb76fa17224607abf4128c3f'),
    @('crash',       'Crash',                       'Collision/3D/collision_3d.png',                   '124ac1fc19f11512b0370197e6ddbd7b035f193df52c75e569f8e9059b7ccc13'),
    @('bug',         'Bug / test failure',          'Bug/3D/bug_3d.png',                               'f8fca56bd7f689e758c7de388c343acecf1c40cc09e666e6fcc99c2e38df0031'),
    @('package',     'Package / install',           'Package/3D/package_3d.png',                       '9c7c74bb551e7f0a3a088791ac715c4ece8c45b2d1910915bc34159f148dee0e'),
    @('save',        'Saved / backup',              'Floppy disk/3D/floppy_disk_3d.png',               '6c0bf4706265b25d742ce2159e68e191036760765c2f7361dc1898d19836e8ab'),
    @('locked',      'Security / permissions',      'Locked/3D/locked_3d.png',                         '8c8e9209b8420ac7caf8c94fde6586044d9b91e85728aaaf4b98c2482761ea16'),
    @('settings',    'Configuration',               'Gear/3D/gear_3d.png',                             '415296d4fadabc2e0f6593143ea632948fef20a1cd73fbbc3dbda571f0421988'),
    @('computer',    'Machine / system',            'Laptop/3D/laptop_3d.png',                         '3b8a267f6c3d2721af925782bdbc11c4e1bc9ae806c5c875557b100393665aab'),
    @('idea',        'Tip / suggestion',            'Light bulb/3D/light_bulb_3d.png',                 'df7b09aa18301507e6fb8c279df66d9afa465b6fed3e63d2efa84dacebdbbf79'),
    @('sparkles',    'New / something nice',        'Sparkles/3D/sparkles_3d.png',                     'e4490c9ff74a3fd73a8f99c760d65b6ed51b8a3e4ad28da137fec050b77e1058'),
    @('robot',       'AI agent / automation',       'Robot/3D/robot_3d.png',                           '3f9b5a00b53c8bddda36b6a3f68d00718d9d7d8c04f986a3248e55ae93a7961b'),
    @('mail',        'Message',                     'Envelope/3D/envelope_3d.png',                     '9671859aa64b65b6977b7ab6adc2e64220e75edf531a8e67c4b76e01ace04230'),
    @('coffee',      'Break / long wait',           'Hot beverage/3D/hot_beverage_3d.png',             'edeaa3fca5eace5f2546efcd3b3e42ef1fd389a0c51d374dbf7863731db8f314')
) | ForEach-Object { [pscustomobject]@{ Name = $_[0]; Use = $_[1]; Path = $_[2]; Sha256 = $_[3] } }

# Accept "success,failure" as one argument too (powershell -File passes it as a single string).
if ($Name) {
    $Name = @($Name | ForEach-Object { $_ -split ',' } | ForEach-Object { $_.Trim().ToLowerInvariant() } | Where-Object { $_ })
}

function Get-Sha256([string] $Path) {
    (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Join-Url([string] $Base, [string] $RelativePath) {
    $escaped = ($RelativePath -split '/' | ForEach-Object { [Uri]::EscapeDataString($_) }) -join '/'
    "$Base/$escaped"
}

# Downloads $Url to $Path through a temporary file; checks $Sha256 when given.
function Save-Url([string] $Url, [string] $Path, [string] $Sha256) {
    $tmp = "$Path.download"
    try {
        Invoke-WebRequest -Uri $Url -OutFile $tmp -UseBasicParsing
        if ($Sha256) {
            $actual = Get-Sha256 $tmp
            if ($actual -ne $Sha256) {
                throw "sha256 mismatch for $Url`n  expected $Sha256`n  actual   $actual"
            }
        }
        Move-Item -LiteralPath $tmp -Destination $Path -Force
    }
    finally {
        if (Test-Path -LiteralPath $tmp) { Remove-Item -LiteralPath $tmp -Force }
    }
}

# --- main ---------------------------------------------------------------------

if ($List) {
    $shown = $Icons
    if ($Name) { $shown = $Icons | Where-Object { $i = $_; $Name | Where-Object { $i.Name -like $_ } } }
    $shown | Format-Table -AutoSize Name, Use, @{ Label = 'Fluent Emoji'; Expression = { ($_.Path -split '/')[0] } }
    return
}

$failed = $false
if ($Name) {
    $selected = foreach ($n in $Name) {
        $icon = $Icons | Where-Object { $_.Name -eq $n } | Select-Object -First 1
        if (-not $icon) { Write-Error "Unknown icon '$n'. Use -List to see the available icons." -ErrorAction Continue; $failed = $true; continue }
        $icon
    }
}
else {
    $selected = $Icons
}

if (-not (Test-Path -LiteralPath $Destination)) { New-Item -ItemType Directory -Path $Destination -Force | Out-Null }
$Destination = (Resolve-Path -LiteralPath $Destination).Path
Write-Host "Destination: $Destination"

$downloaded = 0
$kept = 0
foreach ($icon in @($selected)) {
    $target = Join-Path $Destination "$($icon.Name).png"
    if (-not $Force -and (Test-Path -LiteralPath $target) -and (Get-Sha256 $target) -eq $icon.Sha256) {
        $kept++
        continue
    }
    try {
        Save-Url (Join-Url "$BaseUrl/assets" $icon.Path) $target $icon.Sha256
        $downloaded++
    }
    catch {
        Write-Error "Icon '$($icon.Name)': $($_.Exception.Message)" -ErrorAction Continue
        $failed = $true
    }
}

# Keep the licence next to the icons (MIT requires the notice to travel with copies).
$license = Join-Path $Destination 'fluentui-emoji-LICENSE.txt'
if ($Force -or -not (Test-Path -LiteralPath $license)) {
    try { Save-Url "$BaseUrl/LICENSE" $license $null }
    catch { Write-Warning "Licence file could not be downloaded: $($_.Exception.Message)" }
}

Write-Host ("{0} downloaded, {1} already up to date" -f $downloaded, $kept)
if ($failed) { exit 1 }
Write-Host 'Done. Re-run CMake configure (cmake --preset msvc-x64) to embed the images.'
