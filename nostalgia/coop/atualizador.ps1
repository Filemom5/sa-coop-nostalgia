# SA-COOP Nostalgia - atualizador. (Salvo em UTF-8 com BOM: o PowerShell 5 só lê os acentos assim.)
# Confere a última versão publicada em github.com/Filemom5/sa-coop-nostalgia e baixa os arquivos novos para a pasta do
# GTA, guardando os originais como .original na primeira vez. É rodado pelo launcher (coop\launcher.ps1, com -SemAbrir)
# ou pelo "Jogar SA-COOP.bat", e aí abre o jogo no fim. Se a pasta do jogo pedir administrador (Program Files), só a
# instalação roda como administrador (-SoInstalar).
# Toda vez, não só quando sai versão nova, ele também:
#  - baixa os arquivos que estiverem faltando (quem atualizou com um atualizador antigo, que não conhecia esses arquivos);
#  - instala o Open Limit Adjuster com o .ini ajustado se não houver nenhum limit adjuster na pasta do jogo nem em scripts\;
#  - sobe no .ini do Open Limit Adjuster os limites que estiverem abaixo do necessário para 4 jogadores;
#  - cria o atalho do launcher na área de trabalho (uma vez só), com o ícone do próprio gta_sa.exe;
#  - instala ou atualiza os mods extras da lista coop\mods-extras.txt (baixados dos links de lá, não do nosso GitHub).
# Fora da pasta do GTA (sem o gta_sa.exe ao lado da pasta coop) ele não instala nada: só avisa.
param([switch]$SoInstalar, [switch]$SemAbrir)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'  # a barra de progresso do PowerShell 5 deixa o download muito lento

$Repo = 'Filemom5/sa-coop-nostalgia'
$Coop = $PSScriptRoot
$Jogo = Split-Path -Parent $Coop
$PastaLauncher = Join-Path $Coop 'launcher'
$ArquivoVersao = Join-Path $Coop 'versao-nostalgia.txt'
$Destinos = [ordered]@{
    'coop.asi'                      = (Join-Path $Jogo 'coop.asi')
    'coop.pdb'                      = (Join-Path $Coop 'coop.pdb')
    'coop-server.exe'               = (Join-Path $Coop 'coop-server.exe')
    'atualizador.ps1'               = (Join-Path $Coop 'atualizador.ps1')
    'launcher.ps1'                  = (Join-Path $Coop 'launcher.ps1')
    'logo.png'                      = (Join-Path $PastaLauncher 'logo.png')
    'mods-extras.txt'               = (Join-Path $Coop 'mods-extras.txt')
    'III.VC.SA.LimitAdjuster.asi'   = (Join-Path $Jogo 'III.VC.SA.LimitAdjuster.asi')
    'III.VC.SA.LimitAdjuster.ini'   = (Join-Path $Jogo 'III.VC.SA.LimitAdjuster.ini')
    'OpenLimitAdjuster-LICENSE.txt' = (Join-Path (Join-Path $Coop 'licenses') 'OpenLimitAdjuster.txt')
}
$Limite = @('III.VC.SA.LimitAdjuster.asi', 'III.VC.SA.LimitAdjuster.ini', 'OpenLimitAdjuster-LICENSE.txt')
$SemCopia = @('atualizador.ps1', 'launcher.ps1', 'logo.png', 'mods-extras.txt')  # não guardam .original
# o mínimo de cada limite no [SALIMITS] do Open Limit Adjuster para 4 jogadores (o jogo tem 140 pedestres e 110 carros)
$Minimos = [ordered]@{ 'Peds' = 500; 'PedIntelligence' = 500; 'Vehicles' = 500; 'EntryInfoNode' = 4000 }
$Cabecalho = @{ 'User-Agent' = 'sa-coop-nostalgia-atualizador' }
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

function Pode-Escrever([string]$pasta) {
    if (-not (Test-Path $pasta)) { return $true }
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

# os arquivos de limit adjuster (*.asi ou *.ini) na pasta do jogo e em scripts\
function Arquivos-LimitAdjuster([string]$extensao) {
    foreach ($pasta in @($Jogo, (Join-Path $Jogo 'scripts'))) {
        if (Test-Path $pasta) {
            Get-ChildItem -Path $pasta -Filter ('*.' + $extensao) -File -ErrorAction SilentlyContinue |
                Where-Object { $_.Name -like '*limitadjuster*' }
        }
    }
}

# um limit adjuster qualquer (o Open ou o do fastman92) já instalado
function Tem-LimitAdjuster { return @(Arquivos-LimitAdjuster 'asi').Count -gt 0 }

function Falta-LimitAdjuster($release) {
    if (-not $release) { return $false }
    return (-not (Tem-LimitAdjuster)) -and (@($release.assets | ForEach-Object { $_.name }) -contains $Limite[0])
}

# os arquivos da versão (fora o Limit Adjuster) que não estão no lugar
function Arquivos-Faltando($release) {
    if (-not $release) { return @() }
    return @($release.assets | Where-Object {
        $Destinos.Contains($_.name) -and ($Limite -notcontains $_.name) -and -not (Test-Path $Destinos[$_.name])
    })
}

# Confere (e, com $gravar, corrige) o [SALIMITS] de um .ini do Open Limit Adjuster: um limite com número abaixo do mínimo
# sobe para o mínimo, um que falta é acrescentado; "unlimited" ou um número maior fica como está.
# Devolve $true se o arquivo precisava de ajuste.
function Ajustar-Ini([string]$caminho, [bool]$gravar) {
    $linhas = New-Object System.Collections.Generic.List[string]
    $linhas.AddRange([IO.File]::ReadAllLines($caminho))
    $secao = ''
    $cabecalho = -1
    $vistos = @{}
    $mudou = $false
    for ($i = 0; $i -lt $linhas.Count; $i++) {
        $texto = $linhas[$i].Trim()
        if ($texto -match '^\[([^\]]+)\]') {
            $secao = $Matches[1].Trim().ToUpper()
            if ($secao -eq 'SALIMITS' -and $cabecalho -lt 0) { $cabecalho = $i }
            continue
        }
        if ($secao -ne 'SALIMITS' -or $texto -notmatch '^([A-Za-z]+)\s*=\s*([^;\s]*)') { continue }
        $chave = $Matches[1]
        $valor = $Matches[2]
        $nome = @($Minimos.Keys | Where-Object { $_ -eq $chave })
        if ($nome.Count -eq 0) { continue }
        $nome = $nome[0]
        $vistos[$nome] = $true
        if ($valor -match '^\d+$' -and [long]$valor -lt $Minimos[$nome]) {
            $linhas[$i] = $nome + ' = ' + $Minimos[$nome]
            $mudou = $true
        }
    }
    $novas = @($Minimos.Keys | Where-Object { -not $vistos[$_] } | ForEach-Object { $_ + ' = ' + $Minimos[$_] })
    if ($novas.Count -gt 0) {
        $mudou = $true
        if ($cabecalho -ge 0) {
            $linhas.InsertRange($cabecalho + 1, [string[]]$novas)
        } else {
            $linhas.Add('')
            $linhas.Add('[SALIMITS]')
            $linhas.AddRange([string[]]$novas)
        }
    }
    if ($mudou -and $gravar) {
        if (-not (Test-Path ($caminho + '.original'))) { Copy-Item -Force $caminho ($caminho + '.original') }
        [IO.File]::WriteAllLines($caminho, $linhas.ToArray(), (New-Object System.Text.UTF8Encoding $false))
    }
    return $mudou
}

# os .ini do Open Limit Adjuster (o do fastman92 tem outro formato e fica de fora) que precisam de ajuste
function Inis-Para-Ajustar {
    return @(Arquivos-LimitAdjuster 'ini' | Where-Object { $_.Name -notlike '*fastman92*' -and (Ajustar-Ini $_.FullName $false) })
}

function Instalar($release) {
    $versaoNova = $release.tag_name -ne (Versao-Instalada)
    $limiteFalta = Falta-LimitAdjuster $release
    # o .asi do Limit Adjuster por último: com ele no lugar, o .ini já está junto (e conta como instalado)
    foreach ($arquivo in @($release.assets | Sort-Object { $_.name -eq $Limite[0] })) {
        if (-not $Destinos.Contains($arquivo.name)) { continue }
        $destino = $Destinos[$arquivo.name]
        if ($Limite -contains $arquivo.name) {
            if (-not $limiteFalta) { continue }
        } elseif (-not $versaoNova -and (Test-Path $destino)) {
            continue
        }
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $destino) | Out-Null
        $temporario = $destino + '.baixando'
        Write-Host ('Baixando ' + $arquivo.name + '...')
        Invoke-WebRequest -Uri $arquivo.browser_download_url -OutFile $temporario -UseBasicParsing -Headers $Cabecalho
        if ($SemCopia -notcontains $arquivo.name -and (Test-Path $destino) -and -not (Test-Path ($destino + '.original'))) {
            Copy-Item -Force $destino ($destino + '.original')
        }
        Move-Item -Force $temporario $destino
    }
    if ($versaoNova) { Set-Content -Path $ArquivoVersao -Value $release.tag_name -Encoding ASCII }
}

# Os mods extras de coop\mods-extras.txt, uma linha cada: nome | destino | link do .zip | versão. Destino "modloader": o
# conteúdo do .zip vai para modloader\<nome>; "jogo": para a pasta do jogo (mods de CLEO que não rodam pelo modloader).
# Instala o que falta e o que mudou de versão (a versão instalada fica em coop\mods\<nome>.versao); sem link, pula.
# Um mod que a pessoa desligou no menu do jogo continua desligado: isso fica no modloader.ini, que não é mexido aqui.
function Mods-Extras {
    $lista = Join-Path $Coop 'mods-extras.txt'
    if (-not (Test-Path $lista)) { return @() }
    $mods = @()
    foreach ($linha in (Get-Content -Encoding UTF8 $lista)) {
        $l = $linha.Trim()
        if (-not $l -or $l.StartsWith('#')) { continue }
        $c = @($l.Split('|') | ForEach-Object { $_.Trim() })
        if ($c.Count -lt 4 -or -not $c[2]) { continue }
        $mods += [pscustomobject]@{ Nome = $c[0]; Destino = $c[1]; Link = $c[2]; Versao = $c[3] }
    }
    return $mods
}

function Mods-Para-Instalar {
    return @(Mods-Extras | Where-Object {
        $marca = Join-Path (Join-Path $Coop 'mods') ($_.Nome + '.versao')
        -not (Test-Path $marca) -or (Get-Content -Raw $marca).Trim() -ne $_.Versao
    })
}

function Instalar-Mods {
    foreach ($mod in (Mods-Para-Instalar)) {
        try {
            Write-Host ('Instalando o mod ' + $mod.Nome + ' (' + $mod.Versao + ')...')
            $temp = Join-Path $env:TEMP ('nostalgia-mod-' + [guid]::NewGuid().ToString())
            New-Item -ItemType Directory -Force -Path $temp | Out-Null
            $zip = Join-Path $temp 'mod.zip'
            Invoke-WebRequest -Uri $mod.Link -OutFile $zip -UseBasicParsing -Headers $Cabecalho
            $aberto = Join-Path $temp 'aberto'
            Expand-Archive -Path $zip -DestinationPath $aberto -Force
            # um .zip com uma pasta só dentro: o conteúdo dela
            $raiz = $aberto
            $itens = @(Get-ChildItem -Force $aberto)
            if ($itens.Count -eq 1 -and $itens[0].PSIsContainer) { $raiz = $itens[0].FullName }
            if ($mod.Destino -eq 'jogo') { $alvo = $Jogo } else { $alvo = Join-Path (Join-Path $Jogo 'modloader') $mod.Nome }
            New-Item -ItemType Directory -Force -Path $alvo | Out-Null
            Copy-Item -Path (Join-Path $raiz '*') -Destination $alvo -Recurse -Force
            $marcas = Join-Path $Coop 'mods'
            New-Item -ItemType Directory -Force -Path $marcas | Out-Null
            Set-Content -Path (Join-Path $marcas ($mod.Nome + '.versao')) -Value $mod.Versao -Encoding ASCII
            Remove-Item -Recurse -Force $temp
        } catch {
            Write-Host ('Não deu para instalar o mod ' + $mod.Nome + ' (' + $_.Exception.Message + '). Tento de novo na próxima vez.')
        }
    }
}

function Fazer-Tudo($release) {
    if ($release) { Instalar $release }
    Instalar-Mods
    foreach ($ini in (Inis-Para-Ajustar)) {
        Write-Host ('Ajustando os limites em ' + $ini.Name + '...')
        $null = Ajustar-Ini $ini.FullName $true
    }
}

function Buscar-Versao {
    return Invoke-RestMethod -Uri ('https://api.github.com/repos/' + $Repo + '/releases/latest') -Headers $Cabecalho -TimeoutSec 20
}

function Executavel-Do-Jogo {
    $executavel = Join-Path $Jogo 'gta_sa.exe'
    if (-not (Test-Path $executavel)) { $executavel = Join-Path $Jogo 'gta-sa.exe' }
    return $executavel
}

# O atalho "SA-COOP Nostalgia" na área de trabalho abre o launcher. É criado uma vez por pasta do jogo: se a pessoa
# apagar, só volta se o jogo for instalado em outra pasta. Se existir, é refeito toda vez (pasta e ícone em dia).
function Atualizar-Atalho {
    $launcher = Join-Path $Coop 'launcher.ps1'
    if (-not (Test-Path $launcher)) { return }
    $atalho = Join-Path ([Environment]::GetFolderPath('Desktop')) 'SA-COOP Nostalgia.lnk'
    $marca = Join-Path $env:LOCALAPPDATA 'SA-COOP Nostalgia\atalho-criado.txt'
    if (-not (Test-Path $atalho) -and (Test-Path $marca) -and ((Get-Content -Raw $marca).Trim() -eq $launcher)) { return }
    $icone = Executavel-Do-Jogo  # o ícone do próprio jogo: só o nome do atalho diz que é o SA-COOP Nostalgia
    $shell = New-Object -ComObject WScript.Shell
    $lnk = $shell.CreateShortcut($atalho)
    $lnk.TargetPath = Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
    $lnk.Arguments = '-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "' + $launcher + '"'
    $lnk.WorkingDirectory = $Jogo
    $lnk.IconLocation = $icone + ',0'
    $lnk.WindowStyle = 7  # minimizado: a janela do PowerShell não aparece antes de sumir
    $lnk.Description = 'SA-COOP Nostalgia'
    $lnk.Save()
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $marca) | Out-Null
    Set-Content -Path $marca -Value $launcher -Encoding UTF8
}

if ($SoInstalar) {
    try {
        $release = $null
        try { $release = Buscar-Versao } catch { }
        Fazer-Tudo $release
        exit 0
    } catch {
        Write-Host ('Não deu para instalar (' + $_.Exception.Message + ').')
        Start-Sleep -Seconds 5
        exit 1
    }
}

if (-not (Test-Path (Executavel-Do-Jogo))) {
    Write-Host ('Esta pasta não é a do GTA (não tem o gta_sa.exe): ' + $Jogo)
    Write-Host 'Extraia o instalador dentro da pasta do GTA, onde fica o gta_sa.exe, e abra o Jogar SA-COOP.bat de lá.'
    if (-not $SemAbrir) { Read-Host 'Aperte Enter para fechar' | Out-Null }
    return
}

Write-Host 'Procurando atualização...'
$release = $null
try {
    try {
        $release = Buscar-Versao
    } catch {
        Write-Host 'Sem conexão com o GitHub agora: o jogo abre com a versão que já está instalada.'
    }
    $versaoNova = $release -and ($release.tag_name -ne (Versao-Instalada))
    $faltando = @(Arquivos-Faltando $release).Count -gt 0
    $limiteFalta = Falta-LimitAdjuster $release
    $iniAjustar = @(Inis-Para-Ajustar).Count -gt 0
    $modsFaltando = @(Mods-Para-Instalar).Count -gt 0
    if ($versaoNova -or $faltando -or $limiteFalta -or $iniAjustar -or $modsFaltando) {
        if ($versaoNova) { Write-Host ('Nova versão: ' + $release.tag_name + '. Instalando...') }
        if ($limiteFalta) { Write-Host 'Instalando o Open Limit Adjuster (limites para 4 jogadores)...' }
        $pastas = @($Jogo, $Coop, (Join-Path $Jogo 'scripts'), (Join-Path $Jogo 'modloader'))
        if (@($pastas | Where-Object { -not (Pode-Escrever $_) }).Count -eq 0) {
            Fazer-Tudo $release
        } else {
            Write-Host 'A pasta do jogo pede permissão de administrador: confirme na janela do Windows.'
            $argumentos = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', ('"' + $PSCommandPath + '"'), '-SoInstalar')
            $processo = Start-Process -FilePath 'powershell.exe' -Verb RunAs -Wait -PassThru -ArgumentList $argumentos
            if ($processo.ExitCode -ne 0) { throw 'a instalação como administrador não terminou' }
        }
    }
    $instalada = Versao-Instalada
    if ($instalada) { Write-Host ('Tudo pronto: versão ' + $instalada + '.') } else { Write-Host 'Tudo pronto.' }
} catch {
    Write-Host ('Não deu para atualizar agora (' + $_.Exception.Message + '). O jogo abre com a versão que já está instalada.')
    if (-not $SemAbrir) { Start-Sleep -Seconds 4 }
}
try { Atualizar-Atalho } catch { }

if ($SemAbrir) {
    if ($release) { Write-Output $release }  # o launcher mostra as novidades dela
    return
}
Write-Host 'Abrindo o GTA San Andreas...'
Start-Process -FilePath (Executavel-Do-Jogo) -WorkingDirectory $Jogo
