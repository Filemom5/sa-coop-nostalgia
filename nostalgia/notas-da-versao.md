## Novo nesta versão

- **Basquete no estilo NBA Street:**
  - **Enterrada em terceira pessoa:** a câmera não pula mais para a primeira pessoa. Você vê a enterrada inteira.
  - **Regras de rua:** até 21, 1 ponto de dentro do arco e 2 de fora, ganha com 2 de vantagem. `/basquete 11` muda o
    alvo, e `/basquete nba` joga com 2 e 3 pontos.
  - **Turbo:** o Espaço corre gastando uma barra que enche sozinha. Com turbo, a enterrada sai de mais longe.
  - **Truques com Shift:** crossover andando, giro correndo com Espaço e firula parado. Um truque bem na frente do
    defensor pode **quebrar o tornozelo** dele: ele cai e você passa.
  - **Finta com C:** começa o arremesso e desiste. Se o defensor pular para o toco, ele cai na finta. C no ar cancela o
    arremesso.
  - **Passe no botão direito:** para o colega. Com Espaço: **ponte aérea** para o colega perto do aro, passe quicando na
    cabeça do defensor, ou tabela para você mesmo (no 1x1 e no treino, a tabela vira enterrada se você estiver perto).
  - **Defesa:** botão esquerdo rouba (melhor durante um truque ou firula), Shift dá toco (e tapinha perto do aro), e
    segurar o botão direito põe na postura de defesa, com A e D para os lados.
  - **Estilo e Gamebreaker:** truques seguidos fazem combo. O estilo só conta quando a cesta cai e enche a barra do time.
    Barra cheia: a próxima cesta vale +1 e tira 1 ponto do outro time.
  - **Relógio de 24 s** para arremessar. Passes podem ser interceptados, e arremessos marcados de perto caem menos.
  - `/basquete comandos` mostra tudo isso no meio da tela.

## Da versão 1.20.2

- **Basquete, ajustes do primeiro teste:**
  - A bola agora fica na mão do CJ e acompanha o drible. Antes ela ficava no meio do corpo.
  - **Arremesso no botão esquerdo do mouse:** aperte para pular e solte quando a barra chegar na marca, ou dê um toque
    e aperte de novo na marca. Antes só valia segurar e soltar, então quase todo arremesso saía "Bad".
  - O **Espaço** voltou a ser correr: segurando, o CJ corre com a bola. A enterrada é o botão esquerdo correndo.
  - **`/basquete comandos`** abre no meio da tela um quadro com todos os comandos, as teclas e as regras.

## Da versão 1.20.1

- **Basquete sem crash:** o jogo fechava logo depois do `/basquete`. A bola recebia a animação antes de o jogo terminar
  de criá-la, e agora ela espera o modelo aparecer. O drible também não trava mais na primeira animação.
- **Cesta mais fácil de achar:** o `/basquete` agora funciona a até 25 m da cesta. Mais longe que isso, o chat diz a
  distância para você chegar perto.

## Da versão 1.20.0

- **Basquete (primeira parte):** perto de qualquer cesta do mapa, digite **/basquete** no chat. Quem está perto recebe o
  convite e aceita com **Y**. Os times são azul e vermelho, e ganha quem chegar a 11 (ou a outro número: `/basquete 21`).
  - Com a bola, o CJ dribla com as animações de basquete do próprio jogo.
  - **Arremesso:** segure **Espaço** e solte no alto do pulo, como no minigame original. A barra mostra o ponto certo.
  - **Enterrada:** aperte **Espaço** correndo de frente para o aro.
  - De dentro do arco vale 2 pontos e de fora vale 3. Rebote é de quem chegar primeiro na bola.
  - Depois de cesta ou rebote da defesa, o time precisa levar a bola para fora do arco antes de arremessar.
  - **Treino sozinho:** `/basquete treino`. Para sair: `/basquete sair`.
  - Ainda não tem passe, roubo nem toco. Isso vem nas próximas partes.

## Da versão 1.19.0

- **Jaquetas em HD:** as jaquetas do grupo agora usam uma textura de 1024×1024 no lugar da de 256×256 (16 vezes mais
  pixels). A logo do Nostalgia ficou nítida, com as letras legíveis, e um pouco maior no peito, do tamanho das jaquetas
  de verdade.
- **Jaqueta nova, a Sunset Nostalgia:** fica no lugar da Beige Hooded Jacket da Victim e já está no guarda-roupa de todo
  mundo. Tem degradê do azul-escuro ao vermelho, a logo no peito, "NOSTALGIA" na manga e, nas costas, um pôr do sol
  synthwave com palmeiras.

## Da versão 1.18.2

- **Fim do quadrado vermelho no mapa:** uma marcação de missão que bugou nos primeiros testes ficou presa no save, e o
  GPS levava todo mundo para lá ao entrar num carro. Agora, quando o jogo carrega, ele apaga as marcações de missão que
  sobraram (as das entregas do caminhão também). Depois de carregar, salve o jogo uma vez para ela sair do save.

## Da versão 1.18.1

- **Entrar de novo só com código:** o "entrar sem código" da 1.18.0 saiu. Ele mandava o código do servidor (que leva o
  IP de quem cria) para um canal público na internet, e como o código do mod é aberto, qualquer um podia achar. Voltou
  a ser como antes: quem cria o servidor passa o código para os amigos.

## Da versão 1.18.0

- **FPS e ping na tela (Ctrl+F):** como no League of Legends, **Ctrl+F** mostra e esconde o FPS e o ping no canto
  superior direito. O jogo lembra a escolha.
- **Entrar sem código:** quem cria o servidor avisa o grupo sozinho. Na aba "Join a game" do F9 aparece
  "Servidor de Fulano" com o número de jogadores e um botão **Join**: um clique e pronto, sem copiar código. Se o
  servidor fechar, some da lista em até 11 minutos.
- **Menos peso com o grupo espalhado:** carros e pessoas dos outros jogadores que estão longe de você (mais de 300 m
  para carros, 160 m para pessoas) só aparecem no seu jogo quando você chega perto. Antes, cada jogo carregava a rua de
  todo mundo, mesmo do outro lado do mapa.
- **Menos internet:** carros longe de todos os outros jogadores mandam a posição menos vezes por segundo (as pessoas
  já faziam isso).

## Da versão 1.17.1

- **Fim da chuva de carros:** quando um jogo apagava um carro perto de vocês, cada um dos outros jogos ficava com uma
  cópia dele, todas no mesmo lugar. Com 4 jogadores isso virava uma pilha (a torre de Bobcats em Angel Pine), carros
  explodindo do nada, 1 FPS e crash. Agora só um jogo fica com o carro, e nunca se já houver outro carro naquele lugar.
- **Carros e pessoas divididos entre os jogos:** onde vários jogadores estão juntos, só um jogo cria o trânsito e as
  pessoas (antes isso só valia perto do host). Separados, cada jogo cria perto do seu jogador, mas conta os carros e as
  pessoas dos outros jogos como se fossem seus. A rua fica com a quantidade normal de um jogo só, não 4 vezes mais.
- **Um carro extra na missão, não um por jogador:** quando não há lugar no carro do host, só um parceiro ganha um veículo
  igual, e os outros entram de carona nele. Outro veículo só aparece se não couber mais ninguém.

## Da versão 1.17.0

- **Até 6 jogadores:** o servidor agora aceita 6 jogadores (eram 4). Quem cria o servidor pelo F9 já abre com 6 vagas,
  mesmo que o `server.ini` antigo diga 4. Ainda em teste: se algo estranhar com 5 ou 6, avisem.

## Da versão 1.16.0

- **Corta-ventos do grupo:** o **Grn Windbreaker** virou o corta-vento lilás **Matunas** (emblema Nostalgia no peito e
  "Matunas" nas costas), e o **Blk Windbreaker** virou o preto e roxo **Nostalgia** (faixa degradê com grade, punhos e
  barra listrados). Os dois já aparecem no guarda-roupa de todas as casas, sem precisar comprar, com os nomes
  "Corta-vento Matunas" e "Corta-vento Nostalgia". Os amigos veem quem estiver usando.

## Da versão 1.15.1

- **Marcação na mira:** a marcação agora vai para onde a mira aparece quando você mira com uma arma (e para o meio da tela
  em primeira pessoa, na luneta e dentro de veículos). Antes ela saía em linha reta da cabeça do CJ.
- **Marcação fixa em carros e pessoas:** marcando um carro ou uma pessoa (inclusive os da rua), a marcação fica presa
  neles e acompanha para onde forem, para todos. Pessoas perto da mira também contam, não precisa acertar em cheio.
- **Nome na cor do jogador:** o nome de quem marcou aparece na cor que ele escolheu no F9 (a mesma do radar).

## Da versão 1.15.0

- **Marcação rápida (Z):** aperte **Z** olhando para um lugar, um carro ou uma pessoa, e todos veem a marcação por 20 s, na
  cor do seu ícone no radar, com o seu nome e a distância. Se estiver fora da tela, ela fica na borda com uma seta. Também
  aparece no radar e no mapa. Carro ou pessoa marcados são seguidos pela marcação. **Z** duas vezes rápido tira a sua.
  A tecla muda em `coop\coop.ini` (`ping` em `[keys]`).
- **Mais memória para o mundo:** o jogo reservava só 50 MB para os prédios, carros e pessoas ao redor (um valor de
  2004). Agora reserva até 512 MB, conforme o PC, então o mundo some e aparece atrasado bem menos. Dá para mudar em `coop\coop.ini`
  (`streaming_memory` em `[game]`, em MB).
- **Versões com número novo:** a partir de agora as versões são como 1.15.0 (esta é a 15ª).

## Da versão 1.14.0

- **Placa NOSTALGIA:** a placa VINEWOOD do morro agora diz NOSTALGIA. N, O e I são as letras originais. S, T, A, L e G
  foram desenhadas no mesmo tipo de letra, com as mesmas texturas, estruturas e luz de dia e de noite. De longe também
  aparece NOSTALGIA, e dá para subir e andar nas letras como antes.

## Da versão 1.13.0

- **Cursor da roda de armas corrigido:** o movimento para cima e para baixo estava invertido para quem não usa o mouse
  invertido no jogo (o jogo inverte o eixo vertical para a câmera, e a roda pegava esse valor). Agora o cursor anda
  igual ao mouse, e um pouco mais rápido.

## Da versão 1.12.0

- **Roda de armas com cursor:** agora aparece um cursor na roda, que anda como o do mouse no Windows. É só colocar em
  cima da arma e soltar o Q.
- **Fone na posição certa:** o seu ajuste (`/fone 0.003 -0.015 0`) agora é o padrão, e as conchas ficaram mais 5 mm
  para dentro.
- **Pegar com o veículo só passando por cima:** a área caiu de 2,5 m para 0,5 m em volta do veículo.

## Da versão 1.11.0

- **Roda de armas (como no GTA V):** a pé, segure **Q**. Aparece uma roda com as suas armas, uma por espaço (punho embaixo,
  pistolas em cima). Aponte com o mouse e solte o Q para pegar a arma. No meio aparecem o nome e a munição (pente / total).
  Enquanto a roda está aberta, a câmera para e não dá para atirar. O tempo não fica mais lento, porque no coop o tempo do
  mundo é o mesmo para todos. A tecla muda em `coop\coop.ini` (`weapon_wheel` em `[keys]`).

## Da versão 1.10.0

- **Pegar dinheiro e armas com o veículo:** quem está dirigindo pega o dinheiro e as armas do chão passando por cima,
  com qualquer veículo. Os passageiros não pegam nada (um por veículo).
- **Fone no espelho e nas fotos:** agora o fone também aparece no reflexo do espelho e nas fotos da câmera.
- **Conchas do fone mais fechadas:** 6 mm mais para dentro, encostadas nas orelhas.

## Da versão 1.9.0

- **Fone agora no J** (era H). No GTA, o H a pé é o comando "grupo: recuar" e travava o W enquanto estava apertado.
  Toque **J** para pôr e tirar; segurando **J**, a rodinha troca a estação.
- **Mesma música ao voltar para uma estação:** ao trocar de estação no fone, a anterior fica guardada como no rádio do
  carro, com o chiado de sintonia entre uma e outra.
- **Fone novo, mais detalhado:** conchas ovais com almofada, arco acolchoado mais alto (não entra mais na cabeça).
- **Cores do fone:** `/fone cor preto`, `branco`, `rosa`, `azul`, `vermelho` ou `cinza`. Os outros veem a sua cor.

## Da versão 1.8.0

- Crash ao pôr o fone corrigido.

## Da versão 1.7.0

- Rádio do fone sem pular, fone visível com o SilentPatch, nomes grandes no MP3 e atalho com o ícone do GTA.

## Das versões 5 e 6

- **Fone na cabeça:** ao pôr o fone, o personagem leva as mãos à cabeça e o fone aparece; os outros também veem.
- **Rádio igual para todo mundo** na mesma estação, mesmo em carros diferentes.
- **Fone de ouvido:** liga e desliga; segurando a tecla, a rodinha troca a estação.
- **MP3 na tela** com o nome da estação.

## Das versões anteriores

- Aviso quando o instalador está fora da pasta do GTA.
- **Launcher** com a logo do grupo: atualiza tudo sozinho e mostra as novidades.
- **Limites ajustados sozinhos** no `.ini` do Open Limit Adjuster (500 pedestres, 500 carros, 4000 EntryInfoNode).

## O que já tinha

- Até 4 jogadores no servidor.
- Rádio compartilhada no carro: mesma estação e mesma música, e qualquer um no carro troca a estação.
- Caído em vez de morto: 25 s para um amigo segurar **F** por 4 s e te levantar.
- Segurar no teto do carro com **X**.
- Proteção contra os crashes `0x4D1750` e `0x544BC8`.
- Trem, caixas e carros mais suaves para quem não é o host.
- Registro de rede no `coop\coop.log`.

## Como instalar

Primeira vez: tenha o SA:COOP 0.3.2 instalado, baixe o **SA-COOP-Nostalgia-instalador.zip** abaixo, coloque o zip
dentro da pasta do GTA (onde fica o `gta_sa.exe`) e use **Extrair aqui** (o "Extrair tudo" do Windows cria uma subpasta
e não serve). Abra uma vez pelo `Jogar SA-COOP.bat`: ele cria o atalho na área de trabalho.

Quem já tinha instalado: o atalho e o `.bat` atualizam sozinhos.
