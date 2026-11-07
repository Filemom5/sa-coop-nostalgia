# Casa do Charlie Harper na praia

A casa da série *Two and a Half Men* em escala 1x1, como parte do mapa: vista de fora, com o deck virado para o mar, e
dá para entrar e sair andando, sem teleporte.

## Como fica no jogo

- **Lugar:** Santa Maria Beach, no lugar do bloco de casas `santahouse04_law2` (o prédio laranja e o bege de 3
  andares, em 265,97, -1757,58) e das duas casas do bloco do lado leste (`santahouse02_law2`) que ficam a oeste da casa
  de salvar da praia. Esse bloco é o do jogo, cortado em x = 308 quando carrega (o modelo, o LOD e a colisão:
  `Cut.cpp`); a casa de salvar (a porta em 315,7, -1769,8) fica como é. O lado oeste encosta no vizinho e a frente fica
  onde era a do bloco antigo. Palmeiras, arbustos e a boia do mapa que ficariam dentro da casa somem.
- **O terreno, como as casas de Malibu:** a casa e o quintal ficam num terreno elevado, com o piso a 5,3 m (1,7 m acima
  da rua e 2,1 m acima da areia), calçado de pedra. O terreno recuou 4,5 m da rua (1.28.0): o muro da rua fica na linha da frente da casa de salvar vizinha, sem comer a calçada (a faixa que sobrou é calçada nossa, logo abaixo da do jogo). A casa fica 3,8 m atrás do muro da rua, com uma calçada larga na
  frente, vasos de plantas e lanternas. A parte da casa que avança sobre a praia fica sobre estacas de madeira, aberta
  embaixo, como nas imagens de referência. Em volta do quintal (rua, vizinhos e praia), um muro de reboco com capa de
  terracota. Da rua sobem: os degraus até o pátio da porta da frente (portão verde no alto, pilares com lanternas) e a
  rampa dos carros, no lado oeste (portão verde), que segue reta até as duas portas da garagem. A escada do deck chega
  na areia.
- **Portas:** as folhas das portas voltaram, abertas a 90° para o lado que tem espaço (dá para passar). As portas da
  garagem ficam enroladas debaixo do teto, abertas para os carros. As de vidro de correr ficam com uma folha corrida.
- **Telhados:** telhado de quatro águas de telha de barro em todas as partes de teto plano do modelo (cada parte é
  coberta pelos telhados dos seus maiores retângulos, um sobre o outro, `tools/charlie/roofs.py`), menos os dois
  terraços do andar de cima, que ficam com piso de lajota e um muro com capa em volta. O beiral sai da parede onde o
  ar é aberto.
- **1.29.0:** portas do andar de cima abertas (para dentro dos cômodos), corredor do quarto do Charlie mais largo e janela do pátio no meio (`tools/charlie/edits.py`); escada com degraus de lajota; banheiro do Charlie azul com azulejo ocre; base de pedra retirada (piscava com o rodapé); frente da rampa fechada até a calçada; móveis do jogo posicionados pela caixa real (sem atravessar paredes); folhas, tapetes e quadros sem colisão.
- **Por fora (1.28.0, `tools/charlie/exterior.py`):** base de pedra nas paredes do térreo, peitoris de terracota e vergas de madeira escura nas janelas, venezianas de madeira nas janelas da frente, viga de madeira escura no nível do andar de cima, pilastras de pedra no muro da rua, canteiros com arbustos floridos na frente da casa e buganvília subindo no pergolado e no pátio. Calçada do terreno em concreto cinza, reboco creme mais quente e luz do sol mais marcada (menos chapado).
- **Decoração nossa (1.28.0, `tools/charlie/decor.py`):** no modelo, como o piano. Sala: lareira com cornija e pôster em cima, estante embutida com livros, sofá verde de frente para a lareira, aparador com baú atrás, mesa de tambor, poltronas de couro e creme, tapetes, escrivaninha perto das portas do deck, pôsteres de jazz na parede da escada e quadros. Cozinha: armários brancos, bancada e azulejo amarelos, coifa de cobre, geladeira de inox, carrinho de madeira, mesa redonda com cadeiras brancas e tapete azul. Sala de TV: sofá cinza, tapete listrado, pufe vermelho, armário de madeira, poltrona de palhinha, aparador e quadros. Os quadros dos outros cômodos também são nossos (os do jogo não apareciam). Pôsteres desenhados aqui, no estilo dos da série.
- **Detalhes:** pergolado de madeira escura no pátio da porta da frente; lanternas de ferro nas paredes ao lado das
  portas e nos pilares dos portões, acesas à noite (luzes 2dfx no modelo e a luz delas nas cores de noite);
  corrimão de ferro reto na escada da sala, seguindo a inclinação dos degraus; rodapé de madeira nas paredes de dentro; vidro escuro nas janelas.
- **Como casa do `/casa`:** `/casa marcar` em qualquer ponto do terreno marca esta casa: a própria casa é o interior
  (entra-se pelas portas dela, sem teleporte), o disquete de salvar fica dentro, ao lado da porta da frente, e o
  `/home` leva para o pátio da porta. Só um jogador do grupo pode tê-la, como os interiores.
- **Tamanho:** 1,6x o do modelo, porque as pessoas e as portas do jogo são maiores que as de verdade: as portas ficam
  com 3,2 m, como as portas grandes do próprio jogo, e a casa com 58,7 x 35,4 m.
- **Modelo:** o `model.dae` (SketchUp, em metros), virado com o deck e as portas de vidro para o mar e a porta da frente
  para a rua. As folhas das portas saem, então os vãos ficam abertos e dá para entrar andando. As portas de vidro de
  correr ficam, com uma folha corrida sobre a outra (meio vão aberto). O portão da garagem fica.
- **Texturas (nossas, `tools/charlie/textures.py`):** reboco bege por fora (como as imagens de referência), telhado de
  telha de barro, lajota terracota escura nos pisos, madeira escura no piso da sala rebaixada, teto de tábuas claras
  com vigas escuras, madeira escura nos caixilhos e no guarda-corpo, azulejos espanhóis nos degraus e na borda do piso
  elevado. Cada lado de cada face tem a sua, conforme para onde
  olha: a parte de baixo de um telhado é teto, o lado de dentro de uma parede de fora é parede de sala.
- **Luz:** nas cores dos vértices, de dia e de noite, como nos prédios do jogo. O jogo dobra essas cores, então 128 é a
  textura como ela é. Por isso as cores da 1.23.0 saíam estouradas. A luz de cada canto vem de quanto céu ele vê: os
  cantos e o interior ficam mais escuros de dia, e à noite tem luz quente dentro.
- **Móveis:** objetos do próprio jogo (sofás, poltronas, camas, criados-mudos, a cozinha com geladeira de inox e
  fogão, a mesa redonda, os banheiros, a mesa e as espreguiçadeiras do deck, a prancha de surfe do Charlie...) nos
  lugares da planta da série (`tools/charlie/furniture.py`). Cada jogo cria os seus quando o jogador chega a 160 m da
  casa e tira depois de 220 m; o save do jogo não guarda. O piano de cauda o jogo não tem: é nosso, feito no modelo da
  casa, no piso elevado perto das portas do deck.
- **As texturas no jogo:** o mod as cria quando os dicionários `bev_law2` e `welod_law2` carregam. Na 1.25 elas não
  entravam (a casa ficava branca): o mod procurava o dicionário antes de o jogo ter lido o `gta3.img`, e guardava o
  "não achei".
- **Escadas:** na colisão, uma rampa sobre os degraus (que têm 28 cm nesse tamanho), para subir andando.
- **De longe:** o jogo mostra o LOD, só a parte de fora. A casa de perto aparece até 170 m (o bloco do jogo: 80 m).
- **Chuva:** não chove debaixo dos telhados (um mapa de 0,5 m de onde tem telhado).
- **Outro mod** que tenha trocado esses blocos: o mod deixa o dele.

## Etapas

1. **Escolher o terreno:** `/lote` grava `coop\lote.txt` (o ponto, o chão e o mar em volta, e os prédios do mapa num
   raio de 70 m). Feito: santahouse04_law2.
2. **Estrutura:** paredes, telhado, deck e garagem no terreno, com a colisão; o bloco antigo some. Feito (1.23.0).
3. **Tamanho e texturas:** 1,6x, texturas e luz como as do jogo. Feito (1.24.0).
4. **Móveis:** objetos do próprio jogo nos lugares da planta, e o piano. Feito (1.25.0).
5. **Terreno elevado, estacas, muro de madeira, muro da rua, portas de vidro, cores das referências; a casa de salvar
   do lado de volta.** Feito (1.26.0).
6. **Casa pronta:** portões, rampa reta até a garagem, calçada larga, estacas abertas, portas, telhados, pergolado,
   lanternas, corrimão, decoração e o `/casa marcar` nela. Feito (1.27.0).
7. **Garagem que abre e fecha** como a do CJ (precisa de um modelo novo no jogo para a porta).
8. **Vizinhos:** repintar as casas ao redor, levá-las até a mesma linha da casa na praia, com sacadas, e o mesmo muro
   de madeira na frente delas.

## Código e ferramentas

- `src/client/features/charlie/Lot.cpp`: o `/lote` e o `/extrair` (copia de `models\gta3.img` para `coop\extrair\`
  os modelos e texturas das casas da praia, as colisões e os IPLs da área, para as ferramentas).
- `src/client/features/charlie/Cut.cpp`: corta o bloco do leste (modelo, LOD e colisão do jogo) quando carrega
  (`tools/charlie/cut_test.cpp` testa com os arquivos do jogo; `rwedit.py` faz o mesmo em Python, para ver).
- `src/client/features/charlie/Charlie.cpp`: troca os modelos e as colisões dos dois blocos (por
  `shared/game/ModelSwap`, que a placa NOSTALGIA também usa), põe as texturas nos dicionários dos blocos quando eles
  carregam (por `shared/game/TxdLoad`, que as jaquetas também usam), esconde o que estava no lugar e tira a chuva
  debaixo do telhado.
- `tools/charlie/build.py model.dae lote.txt src/client/features/charlie/CharlieData.h`: o conversor (DAE → DFF, LOD,
  COL3, as texturas em DXT1, o mapa do telhado e a lista do que esconder). Grava também `tools/charlie/out/`.
- `tools/charlie/lot.py`: o terreno elevado, as estacas, os muros, a rampa, os degraus, os portões e as lanternas dos
  pilares. `roofs.py`: os telhados e os muros dos terraços. `details.py`: as portas abertas, a garagem aberta e o
  corrimão.
- `src/client/shared/game/WalkIn.h`: a casa que é o próprio interior, que o `/casa` (features/home) usa.
- `tools/charlie/furniture.py`: os móveis (modelo do jogo, lugar na planta, para onde olha), a planta casada com as
  paredes do modelo, a decoração (quadros, tapetes, plantas, a mesa do terraço e do quintal) e o piano.
- `tools/charlie/textures.py`: as texturas (`out/textures.png` para ver). `preview.py`: uma vista da casa como o jogo
  desenha (textura x cor do vértice x 2).
- O modelo 3D não está no repositório: é o arquivo que o Vinicius recebeu (ver a licença dele antes de distribuir).
