# SA-COOP Nostalgia

Versão modificada do [SA:COOP](https://github.com/Karimov-Akbar/sa-coop) (mod cooperativo de GTA San Andreas, de
Karimov-Akbar) para jogar com os amigos. Baseada no SA:COOP **0.3.2**.

## O que tem a mais

- **Até 4 jogadores** no servidor (`max_players = 4` no `coop\server.ini`).
- **Rádio compartilhada no carro:** todos no mesmo carro ouvem a mesma estação e a mesma música, no mesmo ponto.
  Qualquer um no carro pode trocar a estação.
- **Rádio igual para todos:** na mesma estação, todos ouvem a mesma música, mesmo em carros diferentes (a de quem ouve a
  estação há mais tempo).
- **Fone de ouvido:** a pé, **J** liga e desliga o rádio no fone; segurando **J**, a rodinha troca a estação. Um MP3 no
  canto direito mostra a estação. A tecla muda em `coop\coop.ini` (`headphones` em `[keys]`).
- **Fone na cabeça:** um modelo próprio de fone (`tools/headset_model.py`), desenhado no osso da cabeça de quem está de
  fone, também nas cópias dos outros jogadores. `/fone cor <cor>` escolhe as cores; `/fone x y z rx ry rz` ajusta a
  posição num teste.
- **Caído em vez de morto** (estilo Left 4 Dead): quem "morre" a pé cai ferido por 25 s. Um amigo segura **F** por
  4 s ao lado para levantá-lo. Sem ajuda, ou todos caídos, morre e vai para o hospital como sempre.
- **Segurar no teto do carro:** em cima de um carro, **X** prende o personagem no lugar enquanto o carro anda.
  **X** de novo ou pular solta.
- **Proteção contra o crash** de personagem sem animação (`0x4D1750`).
- **Pegar dinheiro e armas com o veículo:** o motorista pega o que passa por cima; os passageiros, não.
- **Cópias mais suaves** para quem não é o host: trem, caixas e objetos de missão e carros dos outros.
- **Registro de rede** no `coop\coop.log` a cada 10 s (ping, perda, FPS, atrasos, teleportes).
- **Open Limit Adjuster** com 500 pedestres e 500 carros (o jogo tem 140 e 110), contra o crash `0x544BC8` com 4
  jogadores. O atualizador só instala se você ainda não tiver um limit adjuster (na pasta do jogo ou em `scripts`); se
  já tiver o Open Limit Adjuster, ele sobe no `.ini` os limites que estiverem abaixo disso.
- **Launcher** com a logo do grupo (atalho **SA-COOP Nostalgia** na área de trabalho): atualiza sozinho, mostra as
  novidades da versão e abre o jogo no botão JOGAR.

## Instalação (primeira vez)

1. Tenha o GTA San Andreas 1.0 US com o **SA:COOP 0.3.2** oficial instalado
   ([releases do SA:COOP](https://github.com/Karimov-Akbar/sa-coop/releases)).
2. Baixe o `SA-COOP-Nostalgia-instalador.zip` da [última versão](https://github.com/Filemom5/sa-coop-nostalgia/releases/latest), coloque o zip
   **dentro da pasta do GTA** (onde fica o `gta_sa.exe`) e use **Extrair aqui**. O "Extrair tudo" do Windows cria uma
   subpasta com o nome do zip, e aí o atualizador avisa que não está na pasta do GTA. O zip traz o `Jogar SA-COOP.bat`
   e a pasta `coop` com o atualizador e o launcher.
3. Abra uma vez pelo **`Jogar SA-COOP.bat`**: ele instala a versão mais nova, cria o atalho **SA-COOP Nostalgia** na
   área de trabalho e abre o GTA. Depois, abra sempre pelo atalho (o `.bat` continua funcionando).

Os arquivos substituídos ficam guardados como `.original` (por exemplo `coop.asi.original`). Para voltar ao SA:COOP
oficial, é só renomeá-los de volta.

Se o GTA estiver em `C:\Program Files (x86)`, o Windows pede permissão de administrador quando há atualização. Para
evitar isso, mova a pasta do jogo para outro lugar, por exemplo `C:\Jogos\GTA San Andreas`.

## Para o host

O `coop-server.exe` desta versão aceita até 4 jogadores. Se existir `coop\server.ini`, deixe `max_players = 4`.

## Código e licença

O código-fonte é o do SA:COOP com as mudanças acima (veja o histórico de commits). Compilação: `tools/build.py`
(llvm-mingw). Licença MIT, a mesma do projeto original: veja [LICENSE](LICENSE). O
[Open Limit Adjuster](https://github.com/GTAmodding/III.VC.SA.LimitAdjuster) (v1.7.2, MIT) vem da versão oficial dele;
o `.ini` ajustado está em `nostalgia/limit-adjuster`. Leia-me original:
[русский](README.ru.md), [English](README.en.md).
