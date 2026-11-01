# SA-COOP Nostalgia - atualizador.
# Confere a ultima versao publicada em github.com/Filemom5/sa-coop-nostalgia, baixa os arquivos novos para a pasta do
# GTA (guardando os originais como .original na primeira vez) e abre o jogo. E rodado pelo "Jogar SA-COOP.bat" que fica
# na pasta do GTA. Se a pasta do jogo pedir administrador (Program Files), so a instalacao roda como administrador.
param([switch]$SoInstalar)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'  # a barra de progresso do PowerShell 5 deixa o download muito lento

$Repo = 'Filemom5/sa-coop-nostalgia'
$Coop = $PSScriptRoot
$Jogo = Split-Path -Parent $Coop
$ArquivoVersao = Join-Path $Coop 'versao-nostalgia.txt'
$Destinos = [ordered]@{
    'coop.asi'        = (Join-Path $Jogo 'coop.asi')
    'coop.pdb'        = (Join-Path $Coop 'coop.pdb')
    'coop-server.exe' = (Join-Path $Coop 'coop-server.exe')
    'atualizador.ps1' = (Join-Path $Coop 'atualizador.ps1')
}
$Cabecalho = @{ 'User-Agent' = 'sa-coop-nostalgia-atualizador' }
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

function Pode-Escrever([string]$pasta) {
    try {
        $teste = Join-Path $pasta ('.teste-' + [guid]::NewGuid().ToString())
        [IO.File]::WriteAllText($teste, 'x')
        Remove-Item -Force $teste
        return $true
    } catch {
        return $false
    }
}

function Instalar($release) {
    foreach ($arquivo in $release.assets) {
        if (-not $Destinos.Contains($arquivo.name)) { continue }
        $destino = $Destinos[$arquivo.name]
        $temporario = $destino + '.baixando'
        Write-Host ('  baixando ' + $arquivo.name + ' ...')
        Invoke-WebRequest -Uri $arquivo.browser_download_url -OutFile $temporario -UseBasicParsing -Headers $Cabecalho
        if ($arquivo.name -ne 'atualizador.ps1' -and (Test-Path $destino) -and -not (Test-Path ($destino + '.original'))) {
            Copy-Item -Force $destino ($destino + '.original')
        }
        Move-Item -Force $temporario $destino
    }
    Set-Content -Path $ArquivoVersao -Value $release.tag_name -Encoding ASCII
}

Write-Host 'SA-COOP Nostalgia - procurando atualizacao...'
try {
    $release = Invoke-RestMethod -Uri ('https://api.github.com/repos/' + $Repo + '/releases/latest') -Headers $Cabecalho -TimeoutSec 20
    if ($SoInstalar) {
        Instalar $release
        exit 0
    }
    $instalada = ''
    if (Test-Path $ArquivoVersao) { $instalada = (Get-Content -Raw $ArquivoVersao).Trim() }
    if ($release.tag_name -eq $instalada) {
        Write-Host ('Tudo em dia (versao ' + $instalada + ').')
    } else {
        Write-Host ('Nova versao: ' + $release.tag_name + '. Instalando...')
        if ((Pode-Escrever $Jogo) -and (Pode-Escrever $Coop)) {
            Instalar $release
        } else {
            Write-Host 'A pasta do jogo pede permissao de administrador: confirme na janela do Windows.'
            $argumentos = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', ('"' + $PSCommandPath + '"'), '-SoInstalar')
            $processo = Start-Process -FilePath 'powershell.exe' -Verb RunAs -Wait -PassThru -ArgumentList $argumentos
            if ($processo.ExitCode -ne 0) { throw 'a instalacao como administrador nao terminou' }
        }
        Write-Host ('Atualizado para a versao ' + $release.tag_name + '.')
    }
} catch {
    Write-Host ('Nao deu para atualizar agora (' + $_.Exception.Message + ').')
    Write-Host 'Se o jogo estiver aberto, feche e tente de novo. O jogo abre com a versao que ja esta instalada.'
    if ($SoInstalar) {
        Start-Sleep -Seconds 5
        exit 1
    }
    Start-Sleep -Seconds 4
}

$executavel = Join-Path $Jogo 'gta_sa.exe'
if (-not (Test-Path $executavel)) { $executavel = Join-Path $Jogo 'gta-sa.exe' }
Write-Host 'Abrindo o GTA San Andreas...'
Start-Process -FilePath $executavel -WorkingDirectory $Jogo
