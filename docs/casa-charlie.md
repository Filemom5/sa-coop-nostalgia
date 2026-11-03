# Casa do Charlie Harper na praia

A casa da série *Two and a Half Men* em escala 1x1, como parte do mapa: vista de fora, com o deck virado para o mar, e
dá para entrar e sair andando, sem teleporte.

## Como fica no jogo

- **Lugar:** Santa Maria Beach, no lugar do bloco de casas `santahouse04_law2` (o prédio laranja e o bege de 3
  andares, em 265,97, -1757,58) e de parte do bloco do lado leste, `santahouse02_law2`, que vira areia. O lado oeste
  encosta no vizinho, a frente fica onde era a do bloco antigo e o resto avança para a praia. Palmeiras, arbustos e a
  boia do mapa que ficariam dentro da casa somem.
- **Tamanho:** 1,6x o do modelo, porque as pessoas e as portas do jogo são maiores que as de verdade: as portas ficam
  com 3,2 m, como as portas grandes do próprio jogo, e a casa com 58,7 x 35,4 m.
- **Modelo:** o `model.dae` (SketchUp, em metros), virado com o deck e as portas de vidro para o mar e a porta da frente
  para a rua. O piso fica a 3,85 m, acima da areia e da calçada, e a base desce para dentro da areia. As folhas das
  portas saem, então os vãos ficam abertos e dá para entrar andando. O portão da garagem fica.
- **Texturas (nossas, `tools/charlie/textures.py`):** reboco terracota por fora, telhado de telha de barro, piso de
  lajota, teto de tábuas claras com vigas escuras, madeira escura nos caixilhos e no guarda-corpo, azulejos espanhóis
  nos degraus e na borda do piso elevado, como nas fotos da série. Cada lado de cada face tem a sua, conforme para onde
  olha: a parte de baixo de um telhado é teto, o lado de dentro de uma parede de fora é parede de sala.
- **Luz:** nas cores dos vértices, de dia e de noite, como nos prédios do jogo. O jogo dobra essas cores, então 128 é a
  textura como ela é. Por isso as cores da 1.23.0 saíam estouradas. A luz de cada canto vem de quanto céu ele vê: os
  cantos e o interior ficam mais escuros de dia, e à noite tem luz quente dentro.
- **Móveis:** objetos do próprio jogo (sofás, poltronas, camas, criados-mudos, a cozinha com geladeira de inox e
  fogão, a mesa redonda, os banheiros, a mesa e as espreguiçadeiras do deck, a prancha de surfe do Charlie...) nos
  lugares da planta da série (`tools/charlie/furniture.py`). Cada jogo cria os seus quando o jogador chega a 160 m da
  casa e tira depois de 220 m; o save do jogo não guarda. O piano de cauda o jogo não tem: é nosso, feito no modelo da
  casa, no piso elevado perto das portas do deck.
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
5. **Vizinhos:** repintar as casas ao redor e o muro de madeira com escada na areia.

## Código e ferramentas

- `src/client/features/charlie/Lot.cpp`: o `/lote`.
- `src/client/features/charlie/Charlie.cpp`: troca os modelos e as colisões dos dois blocos (por
  `shared/game/ModelSwap`, que a placa NOSTALGIA também usa), põe as texturas nos dicionários dos blocos quando eles
  carregam (por `shared/game/TxdLoad`, que as jaquetas também usam), esconde o que estava no lugar e tira a chuva
  debaixo do telhado.
- `tools/charlie/build.py model.dae lote.txt src/client/features/charlie/CharlieData.h`: o conversor (DAE → DFF, LOD,
  COL3, as texturas em DXT1, o mapa do telhado e a lista do que esconder). Grava também `tools/charlie/out/`.
- `tools/charlie/furniture.py`: os móveis (modelo do jogo, lugar na planta, para onde olha), a planta casada com as
  paredes do modelo, e o piano.
- `tools/charlie/textures.py`: as texturas (`out/textures.png` para ver). `preview.py`: uma vista da casa como o jogo
  desenha (textura x cor do vértice x 2).
- O modelo 3D não está no repositório: é o arquivo que o Vinicius recebeu (ver a licença dele antes de distribuir).
