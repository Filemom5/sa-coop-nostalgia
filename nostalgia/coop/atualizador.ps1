# SA-COOP Nostalgia - atualizador.
# Confere a ultima versao publicada em github.com/Filemom5/sa-coop-nostalgia, baixa os arquivos novos para a pasta do
# GTA (guardando os originais como .original na primeira vez) e abre o jogo. E rodado pelo "Jogar SA-COOP.bat" que fica
# na pasta do GTA. Se a pasta do jogo pedir administrador (Program Files), so a instalacao roda como administrador.
# O Open Limit Adjuster (com o .ini ajustado para 4 jogadores) so e instalado se nao houver nenhum limit adjuster na pasta
# do jogo nem em scripts\, e isso e conferido toda vez, nao so quando sai versao nova.
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
    'III.VC.SA.LimitAdjuster.asi'   = (Join-Path $Jogo 'III.VC.SA.LimitAdjuster.asi')
    'III.VC.SA.LimitAdjuster.ini'   = (Join-Path $Jogo 'III.VC.SA.LimitAdjuster.ini')
    'OpenLimitAdjuster-LICENSE.txt' = (Join-Path (Join-Path $Coop 'licenses') 'OpenLimitAdjuster.txt')
}
$Limite = @('III.VC.SA.LimitAdjuster.asi', 'III.VC.SA.LimitAdjuster.ini', 'OpenLimitAdjuster-LICENSE.txt')
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

function Versao-Instalada {
    if (Test-Path $ArquivoVersao) { return (Get-Content -Raw $ArquivoVersao).Trim() }
    return ''
}

# um limit adjuster qualquer (o Open ou o do fastman92) ja instalado, na pasta do jogo ou em scripts\
function Tem-LimitAdjuster {
    foreach ($pasta in @($Jogo, (Join-Path $Jogo 'scripts'))) {
        if ((Test-Path $pasta) -and (Get-ChildItem -Path $pasta -Filter '*.asi' -File -ErrorAction SilentlyContinue | Where-Object { $_.Name -like '*limitadjuster*' })) {
            return $true
        }
    }
    return $false
}

function Falta-LimitAdjuster($release) {
    return (-not (Tem-LimitAdjuster)) -and (@($release.assets | ForEach-Object { $_.name }) -contains $Limite[0])
}

function Instalar($release) {
    $versaoNova = $release.tag_name -ne (Versao-Instalada)
    $limiteFalta = Falta-LimitAdjuster $release
    # o .asi do Limit Adjuster por ultimo: com ele no lugar, o .ini ja esta junto (e conta como instalado)
    foreach ($arquivo in @($release.assets | Sort-Object { $_.name -eq $Limite[0] })) {
        if (-not $Destinos.Contains($arquivo.name)) { continue }
        if ($Limite -contains $arquivo.name) {
            if (-not $limiteFalta) { continue }
        } elseif (-not $versaoNova) {
            continue
        }
        $destino = $Destinos[$arquivo.name]
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $destino) | Out-Null
        $temporario = $destino + '.baixando'
        Write-Host ('  baixando ' + $arquivo.name + ' ...')
        Invoke-WebRequest -Uri $arquivo.browser_download_url -OutFile $temporario -UseBasicParsing -Headers $Cabecalho
        if ($arquivo.name -ne 'atualizador.ps1' -and (Test-Path $destino) -and -not (Test-Path ($destino + '.original'))) {
            Copy-Item -Force $destino ($destino + '.original')
        }
        Move-Item -Force $temporario $destino
    }
    if ($versaoNova) { Set-Content -Path $ArquivoVersao -Value $release.tag_name -Encoding ASCII }
}

Write-Host 'SA-COOP Nostalgia - procurando atualizacao...'
try {
    $release = Invoke-RestMethod -Uri ('https://api.github.com/repos/' + $Repo + '/releases/latest') -Headers $Cabecalho -TimeoutSec 20
    if ($SoInstalar) {
        Instalar $release
        exit 0
    }
    $instalada = Versao-Instalada
    $versaoNova = $release.tag_name -ne $instalada
    $limiteFalta = Falta-LimitAdjuster $release
    if (-not $versaoNova -and -not $limiteFalta) {
        Write-Host ('Tudo em dia (versao ' + $instalada + ').')
    } else {
        if ($versaoNova) { Write-Host ('Nova versao: ' + $release.tag_name + '. Instalando...') }
        if ($limiteFalta) { Write-Host 'Instalando o Open Limit Adjuster (limites para 4 jogadores)...' }
        if ((Pode-Escrever $Jogo) -and (Pode-Escrever $Coop)) {
            Instalar $release
        } else {
            Write-Host 'A pasta do jogo pede permissao de administrador: confirme na janela do Windows.'
            $argumentos = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', ('"' + $PSCommandPath + '"'), '-SoInstalar')
            $processo = Start-Process -FilePath 'powershell.exe' -Verb RunAs -Wait -PassThru -ArgumentList $argumentos
            if ($processo.ExitCode -ne 0) { throw 'a instalacao como administrador nao terminou' }
        }
        Write-Host ('Pronto: versao ' + $release.tag_name + '.')
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
