# Copies the sign's files out of the game (gta3.img and data\maps\LA) into tools\sign\orig, for build.py and recipe.py.
# Only reads the game. Use: powershell -ExecutionPolicy Bypass -File extrair-placa.ps1 -Jogo "C:\...\GTA San Andreas"
param([Parameter(Mandatory = $true)][string]$Jogo)
$ErrorActionPreference = 'Stop'
$out = Join-Path $PSScriptRoot 'orig'
New-Item -ItemType Directory -Force (Join-Path $out 'LA') | Out-Null
Copy-Item (Join-Path $Jogo 'data\maps\LA\*') (Join-Path $out 'LA')
$fs = [IO.File]::OpenRead((Join-Path $Jogo 'models\gta3.img'))
$br = New-Object IO.BinaryReader($fs)
if ([Text.Encoding]::ASCII.GetString($br.ReadBytes(4)) -ne 'VER2') { throw 'gta3.img: unexpected format' }
$n = $br.ReadUInt32()
$entries = for ($i = 0; $i -lt $n; $i++) {
    $off = $br.ReadUInt32(); $sz = $br.ReadUInt16(); $arc = $br.ReadUInt16()
    $name = [Text.Encoding]::ASCII.GetString($br.ReadBytes(24)).Split([char]0)[0]
    [pscustomobject]@{ Name = $name; Off = [int64]$off * 2048; Size = [int64]([Math]::Max($sz, $arc)) * 2048 }
}
foreach ($e in $entries) {
    if ($e.Name -match '^(vinesign|lodvinesign|mulhouslahills|lahillsa_lod|lahills_\d\.col|lahills_stream)') {
        $fs.Position = $e.Off
        [IO.File]::WriteAllBytes((Join-Path $out $e.Name), $br.ReadBytes([int]$e.Size))
        Write-Host "  $($e.Name)"
    }
}
$fs.Close()
