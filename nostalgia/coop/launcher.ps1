# SA-COOP Nostalgia - launcher. (Salvo em UTF-8 com BOM: o PowerShell 5 só lê os acentos assim.)
# Uma janela com a logo do grupo, as novidades da versão e o botão JOGAR. Ao abrir, roda o atualizador
# (atualizador.ps1 -SemAbrir) em segundo plano e mostra o que ele está fazendo; JOGAR abre o GTA e fecha o launcher.
# O atalho "SA-COOP Nostalgia" da área de trabalho (criado pelo atualizador) abre este arquivo.
# -Previa <pasta>: não mostra a janela, só salva duas imagens dela (antes e depois da atualização) para conferir.
param([string]$Previa)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName PresentationFramework, PresentationCore, WindowsBase

$Coop = $PSScriptRoot
$Jogo = Split-Path -Parent $Coop
$PastaLauncher = Join-Path $Coop 'launcher'
$Atualizador = Join-Path $Coop 'atualizador.ps1'
$ArquivoVersao = Join-Path $Coop 'versao-nostalgia.txt'
$Guardado = Join-Path $env:LOCALAPPDATA 'SA-COOP Nostalgia'
$NovidadesGuardadas = Join-Path $Guardado 'novidades.txt'

$xaml = @'
<Window xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
        xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
        Title="SA-COOP Nostalgia" Width="900" Height="540" WindowStyle="None" ResizeMode="CanMinimize"
        WindowStartupLocation="CenterScreen" Background="#170C36" BorderBrush="#7A3FC4" BorderThickness="1"
        FontFamily="Segoe UI" Foreground="White">
  <Window.Resources>
    <Style TargetType="ScrollBar">
      <Setter Property="Width" Value="6"/>
      <Setter Property="MinWidth" Value="6"/>
      <Setter Property="Template">
        <Setter.Value>
          <ControlTemplate TargetType="ScrollBar">
            <Track x:Name="PART_Track" IsDirectionReversed="True">
              <Track.Thumb>
                <Thumb>
                  <Thumb.Template>
                    <ControlTemplate TargetType="Thumb">
                      <Border Background="#88FF71CE" CornerRadius="3"/>
                    </ControlTemplate>
                  </Thumb.Template>
                </Thumb>
              </Track.Thumb>
            </Track>
          </ControlTemplate>
        </Setter.Value>
      </Setter>
    </Style>
    <Style x:Key="BotaoJogar" TargetType="Button">
      <Setter Property="Foreground" Value="White"/>
      <Setter Property="FontFamily" Value="Bahnschrift SemiBold, Segoe UI"/>
      <Setter Property="FontSize" Value="26"/>
      <Setter Property="Cursor" Value="Hand"/>
      <Setter Property="Template">
        <Setter.Value>
          <ControlTemplate TargetType="Button">
            <Border x:Name="Fundo" CornerRadius="8">
              <Border.Background>
                <LinearGradientBrush StartPoint="0,0" EndPoint="1,1">
                  <GradientStop Color="#FF4FA3" Offset="0"/>
                  <GradientStop Color="#8A4DFF" Offset="1"/>
                </LinearGradientBrush>
              </Border.Background>
              <Border.Effect>
                <DropShadowEffect x:Name="Brilho" Color="#FF71CE" BlurRadius="24" ShadowDepth="0" Opacity="0.6"/>
              </Border.Effect>
              <ContentPresenter HorizontalAlignment="Center" VerticalAlignment="Center"/>
            </Border>
            <ControlTemplate.Triggers>
              <Trigger Property="IsMouseOver" Value="True">
                <Setter TargetName="Fundo" Property="Opacity" Value="0.9"/>
              </Trigger>
              <Trigger Property="IsPressed" Value="True">
                <Setter TargetName="Fundo" Property="Opacity" Value="0.75"/>
              </Trigger>
              <Trigger Property="IsEnabled" Value="False">
                <Setter TargetName="Fundo" Property="Background" Value="#3A2F5E"/>
                <Setter TargetName="Fundo" Property="Effect" Value="{x:Null}"/>
                <Setter Property="Foreground" Value="#9A90B8"/>
              </Trigger>
            </ControlTemplate.Triggers>
          </ControlTemplate>
        </Setter.Value>
      </Setter>
    </Style>
    <Style x:Key="BotaoJanela" TargetType="Button">
      <Setter Property="Foreground" Value="#C9B8FF"/>
      <Setter Property="FontSize" Value="13"/>
      <Setter Property="Width" Value="46"/>
      <Setter Property="Height" Value="34"/>
      <Setter Property="Template">
        <Setter.Value>
          <ControlTemplate TargetType="Button">
            <Border x:Name="Fundo" Background="Transparent">
              <ContentPresenter HorizontalAlignment="Center" VerticalAlignment="Center"/>
            </Border>
            <ControlTemplate.Triggers>
              <Trigger Property="IsMouseOver" Value="True">
                <Setter TargetName="Fundo" Property="Background" Value="#33FFFFFF"/>
              </Trigger>
            </ControlTemplate.Triggers>
          </ControlTemplate>
        </Setter.Value>
      </Setter>
    </Style>
  </Window.Resources>
  <Grid>
    <Grid.Background>
      <LinearGradientBrush StartPoint="0,0" EndPoint="1,1">
        <GradientStop Color="#160B33" Offset="0"/>
        <GradientStop Color="#2A1260" Offset="0.65"/>
        <GradientStop Color="#45166B" Offset="1"/>
      </LinearGradientBrush>
    </Grid.Background>
    <Grid.RowDefinitions>
      <RowDefinition Height="34"/>
      <RowDefinition Height="*"/>
      <RowDefinition Height="Auto"/>
    </Grid.RowDefinitions>

    <Grid x:Name="Titulo" Grid.Row="0" Background="Transparent">
      <TextBlock Text="SA-COOP NOSTALGIA" Margin="16,0" VerticalAlignment="Center"
                 FontFamily="Bahnschrift SemiBold, Segoe UI" FontSize="12" Foreground="#C9B8FF"/>
      <StackPanel Orientation="Horizontal" HorizontalAlignment="Right">
        <Button x:Name="Minimizar" Style="{StaticResource BotaoJanela}" Content="&#x2014;"/>
        <Button x:Name="Fechar" Style="{StaticResource BotaoJanela}" Content="&#x2715;"/>
      </StackPanel>
    </Grid>

    <Grid Grid.Row="1" Margin="28,10,28,0">
      <Grid.ColumnDefinitions>
        <ColumnDefinition Width="330"/>
        <ColumnDefinition Width="28"/>
        <ColumnDefinition Width="*"/>
      </Grid.ColumnDefinitions>
      <Border x:Name="MolduraLogo" Grid.Column="0" Width="330" Height="330" VerticalAlignment="Top"
              CornerRadius="12" BorderBrush="#FF71CE" BorderThickness="2" Background="#2A1A55">
        <Border.Effect>
          <DropShadowEffect Color="#FF71CE" BlurRadius="30" ShadowDepth="0" Opacity="0.55"/>
        </Border.Effect>
        <TextBlock x:Name="LogoTexto" Text="SA-COOP&#10;NOSTALGIA" TextAlignment="Center" VerticalAlignment="Center"
                   FontFamily="Bahnschrift SemiBold, Segoe UI" FontSize="40" Foreground="#FF71CE"/>
      </Border>
      <Grid Grid.Column="2">
        <Grid.RowDefinitions>
          <RowDefinition Height="Auto"/>
          <RowDefinition Height="Auto"/>
          <RowDefinition Height="*"/>
        </Grid.RowDefinitions>
        <TextBlock Grid.Row="0" Text="NOVIDADES" FontFamily="Bahnschrift SemiBold, Segoe UI" FontSize="22" Foreground="#FF71CE"/>
        <TextBlock x:Name="Versao" Grid.Row="1" FontSize="12.5" Foreground="#9FE8FF" Margin="0,2,0,10"/>
        <Border Grid.Row="2" Background="#1AFFFFFF" CornerRadius="10" BorderBrush="#33FFFFFF" BorderThickness="1" Padding="18,14">
          <ScrollViewer VerticalScrollBarVisibility="Auto">
            <TextBlock x:Name="Novidades" TextWrapping="Wrap" FontSize="13.5" Foreground="#EDE7FF" LineHeight="21" Margin="0,0,12,0"/>
          </ScrollViewer>
        </Border>
      </Grid>
    </Grid>

    <Grid Grid.Row="2" Margin="28,22,28,24">
      <Grid.ColumnDefinitions>
        <ColumnDefinition Width="*"/>
        <ColumnDefinition Width="28"/>
        <ColumnDefinition Width="240"/>
      </Grid.ColumnDefinitions>
      <StackPanel Grid.Column="0" VerticalAlignment="Center">
        <TextBlock x:Name="Status" Text="Procurando atualização..." FontSize="14" Foreground="#EDE7FF" TextWrapping="Wrap"/>
        <ProgressBar x:Name="Barra" Height="4" Margin="0,10,0,0" IsIndeterminate="True"
                     Foreground="#01CDFE" Background="#26FFFFFF" BorderThickness="0"/>
      </StackPanel>
      <Button x:Name="Jogar" Grid.Column="2" Height="60" Content="JOGAR" Style="{StaticResource BotaoJogar}" IsEnabled="False"/>
    </Grid>
  </Grid>
</Window>
'@

$janela = [Windows.Markup.XamlReader]::Parse($xaml)
$c = @{}
foreach ($nome in @('Titulo', 'Minimizar', 'Fechar', 'MolduraLogo', 'LogoTexto', 'Versao', 'Novidades', 'Status', 'Barra', 'Jogar')) {
    $c[$nome] = $janela.FindName($nome)
}

# uma imagem lida inteira para a memória: o arquivo não fica preso e o atualizador pode trocá-lo
function Ler-Imagem([string]$caminho) {
    if (-not (Test-Path $caminho)) { return $null }
    try {
        $bytes = [IO.File]::ReadAllBytes($caminho)
        $imagem = New-Object Windows.Media.Imaging.BitmapImage
        $imagem.BeginInit()
        $imagem.StreamSource = New-Object IO.MemoryStream(, $bytes)
        $imagem.CacheOption = [Windows.Media.Imaging.BitmapCacheOption]::OnLoad
        $imagem.EndInit()
        $imagem.Freeze()
        return $imagem
    } catch {
        return $null
    }
}

function Mostrar-Logo {
    $logo = Ler-Imagem (Join-Path $PastaLauncher 'logo.png')
    if (-not $logo) { return }
    $pincel = New-Object Windows.Media.ImageBrush($logo)
    $pincel.Stretch = [Windows.Media.Stretch]::UniformToFill
    $c.MolduraLogo.Background = $pincel
    $c.LogoTexto.Visibility = [Windows.Visibility]::Collapsed
    $janela.Icon = $logo
}

function Mostrar-Versao {
    $versao = ''
    if (Test-Path $ArquivoVersao) { $versao = (Get-Content -Raw $ArquivoVersao).Trim() }
    if ($versao) { $c.Versao.Text = 'Versão instalada: ' + $versao } else { $c.Versao.Text = '' }
}

# o texto da versão no GitHub (Markdown) em texto simples: títulos em maiúsculas, itens com marcador, linhas quebradas
# do mesmo parágrafo juntas
function Texto-Das-Novidades([string]$markdown) {
    $saida = New-Object System.Collections.Generic.List[string]
    $continua = $false
    foreach ($linha in ($markdown -split "`r?`n")) {
        $texto = ($linha -replace '\*\*', '' -replace '`', '').TrimEnd()
        if ($texto.Trim() -eq '') {
            $saida.Add('')
            $continua = $false
        } elseif ($texto -match '^\s*#+\s*(.*)$') {
            $saida.Add($Matches[1].ToUpper())
            $continua = $false
        } elseif ($texto -match '^\s*[-*]\s+(.*)$') {
            $saida.Add([string][char]0x2022 + '  ' + $Matches[1])
            $continua = $true
        } elseif ($continua -and $saida.Count -gt 0) {
            $saida[$saida.Count - 1] += ' ' + $texto.Trim()
        } else {
            $saida.Add($texto.Trim())
            $continua = $true
        }
    }
    return (($saida -join "`n") -replace "`n{3,}", "`n`n").Trim()
}

function Mostrar-Novidades([string]$texto) {
    if ($texto) { $c.Novidades.Text = $texto }
}

function Iniciar-Atualizacao {
    $estado = [Management.Automation.Runspaces.InitialSessionState]::CreateDefault()
    $estado.ExecutionPolicy = [Microsoft.PowerShell.ExecutionPolicy]::Bypass
    $script:espaco = [Management.Automation.Runspaces.RunspaceFactory]::CreateRunspace($estado)
    $script:espaco.Open()
    $script:ps = [PowerShell]::Create()
    $script:ps.Runspace = $script:espaco
    [void]$script:ps.AddCommand($Atualizador).AddParameter('SemAbrir')
    $script:tarefa = $script:ps.BeginInvoke()
}

# chamado a cada 150 ms: mostra a última linha do atualizador e, quando ele termina, libera o JOGAR
function Acompanhar {
    foreach ($registro in $script:ps.Streams.Information.ReadAll()) {
        $texto = ([string]$registro.MessageData).Trim()
        if ($texto) { $c.Status.Text = $texto }
    }
    if (-not $script:tarefa.IsCompleted) { return }
    if ($script:relogio) { $script:relogio.Stop() }
    $release = $null
    try {
        $release = @($script:ps.EndInvoke($script:tarefa) | Where-Object { $_ -and $_.tag_name }) | Select-Object -Last 1
    } catch {
        $c.Status.Text = 'Não deu para atualizar agora: o jogo abre com a versão que já está instalada.'
    }
    foreach ($registro in $script:ps.Streams.Information.ReadAll()) {
        $texto = ([string]$registro.MessageData).Trim()
        if ($texto) { $c.Status.Text = $texto }
    }
    if ($release -and $release.body) {
        $texto = Texto-Das-Novidades ([string]$release.body)
        Mostrar-Novidades $texto
        try {
            New-Item -ItemType Directory -Force -Path $Guardado | Out-Null
            [IO.File]::WriteAllText($NovidadesGuardadas, $texto, (New-Object System.Text.UTF8Encoding $true))
        } catch { }
    }
    Mostrar-Logo
    Mostrar-Versao
    $c.Barra.Visibility = [Windows.Visibility]::Hidden
    if (Test-Path (Executavel-Do-Jogo)) {
        $c.Jogar.IsEnabled = $true
    } else {
        $c.Status.Text = 'Esta pasta não é a do GTA (não tem o gta_sa.exe). Extraia o instalador dentro da pasta do GTA e abra o Jogar SA-COOP.bat de lá.'
    }
    $script:ps.Dispose()
    $script:espaco.Close()
}

function Executavel-Do-Jogo {
    $executavel = Join-Path $Jogo 'gta_sa.exe'
    if (-not (Test-Path $executavel)) { $executavel = Join-Path $Jogo 'gta-sa.exe' }
    return $executavel
}

$c.Titulo.Add_MouseLeftButtonDown({ $janela.DragMove() })
$c.Minimizar.Add_Click({ $janela.WindowState = [Windows.WindowState]::Minimized })
$c.Fechar.Add_Click({ $janela.Close() })
$c.Jogar.Add_Click({
    try {
        Start-Process -FilePath (Executavel-Do-Jogo) -WorkingDirectory $Jogo
        $janela.Close()
    } catch {
        $c.Status.Text = 'Não achei o gta_sa.exe: o launcher precisa estar na pasta do GTA.'
    }
})

Mostrar-Logo
Mostrar-Versao
$c.Novidades.Text = 'Carregando as novidades...'
if (Test-Path $NovidadesGuardadas) {
    try { Mostrar-Novidades ([IO.File]::ReadAllText($NovidadesGuardadas)) } catch { }
}

if ($Previa) {
    New-Item -ItemType Directory -Force -Path $Previa | Out-Null
    function Salvar-Imagem([string]$arquivo) {
        $raiz = $janela.Content
        $tamanho = New-Object Windows.Size($janela.Width, $janela.Height)
        $raiz.Measure($tamanho)
        $raiz.Arrange((New-Object Windows.Rect($tamanho)))
        $raiz.UpdateLayout()
        $imagem = New-Object Windows.Media.Imaging.RenderTargetBitmap([int]$janela.Width, [int]$janela.Height, 96, 96, [Windows.Media.PixelFormats]::Pbgra32)
        $imagem.Render($raiz)
        $png = New-Object Windows.Media.Imaging.PngBitmapEncoder
        $png.Frames.Add([Windows.Media.Imaging.BitmapFrame]::Create($imagem))
        $arquivoPng = [IO.File]::Create((Join-Path $Previa $arquivo))
        $png.Save($arquivoPng)
        $arquivoPng.Close()
    }
    Salvar-Imagem 'launcher-1.png'
    Iniciar-Atualizacao
    while (-not $script:tarefa.IsCompleted) { Start-Sleep -Milliseconds 200 }
    Acompanhar
    Salvar-Imagem 'launcher-2.png'
    return
}

Iniciar-Atualizacao
$script:relogio = New-Object Windows.Threading.DispatcherTimer
$script:relogio.Interval = [TimeSpan]::FromMilliseconds(150)
$script:relogio.Add_Tick({ Acompanhar })
$script:relogio.Start()
[void]$janela.ShowDialog()
