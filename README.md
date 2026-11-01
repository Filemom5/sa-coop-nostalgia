# SA-COOP Nostalgia

Versão modificada do [SA:COOP](https://github.com/Karimov-Akbar/sa-coop) (mod cooperativo de GTA San Andreas, de
Karimov-Akbar) para jogar com os amigos. Baseada no SA:COOP **0.3.2**.

## O que tem a mais

- **Até 4 jogadores** no servidor (`max_players = 4` no `coop\server.ini`).
- **Rádio compartilhada no carro:** todos no mesmo carro ouvem a mesma estação e a mesma música, no mesmo ponto.
  Qualquer um no carro pode trocar a estação.
- **Caído em vez de morto** (estilo Left 4 Dead): quem "morre" a pé cai ferido por 25 s. Um amigo segura **F** por
  4 s ao lado para levantá-lo. Sem ajuda, ou todos caídos, morre e vai para o hospital como sempre.
- **Segurar no teto do carro:** em cima de um carro, **X** prende o personagem no lugar enquanto o carro anda.
  **X** de novo ou pular solta.
- **Proteção contra o crash** de personagem sem animação (`0x4D1750`).
- **Cópias mais suaves** para quem não é o host: trem, caixas e objetos de missão e carros dos outros.
- **Registro de rede** no `coop\coop.log` a cada 10 s (ping, perda, FPS, atrasos, teleportes).

## Instalação (primeira vez)

1. Tenha o GTA San Andreas 1.0 US com o **SA:COOP 0.3.2** oficial instalado
   ([releases do SA:COOP](https://github.com/Karimov-Akbar/sa-coop/releases)).
2. Baixe o `SA-COOP-Nostalgia-instalador.zip` da [última versão](../../releases/latest) e extraia **dentro da pasta
   do GTA** (onde fica o `gta_sa.exe`). Ele traz o `Jogar SA-COOP.bat` e o `coop\atualizador.ps1`.
3. Abra o jogo sempre pelo **`Jogar SA-COOP.bat`**. Ele baixa a versão mais nova (se houver) e abre o GTA.

Os arquivos substituídos ficam guardados como `.original` (por exemplo `coop.asi.original`). Para voltar ao SA:COOP
oficial, é só renomeá-los de volta.

Se o GTA estiver em `C:\Program Files (x86)`, o Windows pede permissão de administrador quando há atualização. Para
evitar isso, mova a pasta do jogo para outro lugar, por exemplo `C:\Jogos\GTA San Andreas`.

## Para o host

O `coop-server.exe` desta versão aceita até 4 jogadores. Se existir `coop\server.ini`, deixe `max_players = 4`.

## Código e licença

O código-fonte é o do SA:COOP com as mudanças acima (veja o histórico de commits). Compilação: `tools/build.py`
(llvm-mingw). Licença MIT, a mesma do projeto original: veja [LICENSE](LICENSE). Leia-me original:
[русский](README.ru.md), [English](README.en.md).
