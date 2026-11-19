# MX Dirt Mod uninstaller. Moves the mod's files out of MX Bikes\plugins into
# Documents\PiBoSo\MX Bikes\mx_dirt_mod_backups (nothing is deleted), including the soft-layer
# marks (plugins\mx_dirt_soft). Your recorded laps (plugins\mx_dirt_laps) and the game's own
# saved track wear stay where they are.
param([string]$GameDir='',[switch]$Quiet)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Windows.Forms
function Say($m){ if($Quiet){ Write-Host $m } else { [Windows.Forms.MessageBox]::Show($m,'MX Dirt Mod') | Out-Null } }
try{
    if(-not $GameDir){
        $d=$PSScriptRoot; for($i=0;$i -lt 4 -and $d;$i++){ if(Test-Path "$d\mxbikes.exe"){$GameDir=$d;break}; $d=Split-Path $d }
        # every Steam library (games installed on another drive are listed in libraryfolders.vdf)
        if(-not $GameDir){ $roots=@()
            try{ $s=(Get-ItemProperty 'HKCU:\Software\Valve\Steam' -ErrorAction Stop).SteamPath; if($s){ $s=$s -replace '/','\'; $roots+=$s
                $vdf="$s\steamapps\libraryfolders.vdf"; if(Test-Path $vdf){ $roots+=(Select-String -Path $vdf -Pattern '"path"\s+"([^"]+)"' | ForEach-Object { $_.Matches[0].Groups[1].Value -replace '\\\\','\' }) } } }catch{}
            $roots+="${env:ProgramFiles(x86)}\Steam"
            foreach($r in $roots){ if($r -and (Test-Path "$r\steamapps\common\MX Bikes\mxbikes.exe")){$GameDir="$r\steamapps\common\MX Bikes";break} } }
        # still not found: ask (Steam: right-click MX Bikes > Manage > Browse local files shows the folder)
        if(-not $GameDir -and -not $Quiet){ $dlg=New-Object Windows.Forms.FolderBrowserDialog
            $dlg.Description='Select your MX Bikes folder (the one with mxbikes.exe). In Steam: right-click MX Bikes > Manage > Browse local files.'
            if($dlg.ShowDialog() -eq 'OK' -and (Test-Path "$($dlg.SelectedPath)\mxbikes.exe")){$GameDir=$dlg.SelectedPath} } }
    if(-not $GameDir){ throw 'MX Bikes folder not found. Run: Uninstall.ps1 -GameDir "<your MX Bikes folder>"' }
    if(Get-Process -Name mxbikes -ErrorAction SilentlyContinue){ throw 'Close MX Bikes first.' }
    $P="$GameDir\plugins"; $B="$([Environment]::GetFolderPath('MyDocuments'))\PiBoSo\MX Bikes\mx_dirt_mod_backups\uninstall-$(Get-Date -Format 'yyyyMMdd-HHmmss')"
    New-Item -ItemType Directory -Force $B | Out-Null
    foreach($n in 'mx_dirt.dlo','mx_dirt_render.dlo','mx_dirt.ini','mx_dirt_render.ini','mx_dirt.jsonl','mx_dirt_render.log','mx_dirt_launcher','mx_dirt_soft','mx_dirt_terrain','mx_dirt_data','mx_dirt_presets','mx_dirt_tracks'){ if(Test-Path "$P\$n"){ Move-Item "$P\$n" $B -Force } }
    $lnk="$([Environment]::GetFolderPath('Desktop'))\MX Dirt Mod.lnk"; if(Test-Path $lnk){ Move-Item $lnk $B -Force }
    Say "MX Dirt Mod removed. The game is back to normal.`n`nRemoved files were moved to:`n$B"
}catch{ Say "Uninstall failed:`n$($_.Exception.Message)"; exit 1 }
