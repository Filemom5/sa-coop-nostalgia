# Casa do Charlie Harper na praia

A casa da série *Two and a Half Men* em escala 1x1, como parte do mapa: vista de fora, com o deck virado para o mar, e
dá para entrar e sair andando, sem teleporte.

## Como fica no jogo

- **Lugar:** Santa Maria Beach, no lugar do bloco de casas `santahouse04_law2` (o prédio laranja e o bege de 3
  andares, em 265,97, -1757,58). O bloco some para todos que têm o mod; uma palmeira e um arbusto do mapa que ficariam
  dentro da casa também.
- **Modelo:** o `model.dae` (SketchUp, já em metros: 36,7 x 22,1 m, 7 m de altura), virado com o deck e as portas de
  vidro para o mar e a porta da frente para a rua. O piso fica a 3,6 m, 25 cm acima de uma placa de areia do tamanho do
  bloco antigo. As folhas das portas saem: os vãos ficam abertos, dá para entrar andando. O portão da garagem fica.
- **Cores:** sem texturas por enquanto, nas cores dos vértices (dia e noite): por fora paredes salmão e telhado
  terracota, como as fotos de Malibu da série; por dentro paredes creme, piso terracota, vigas de madeira e vidro. A luz
  de cada canto vem de quanto céu ele vê, então os cantos e o interior ficam mais escuros de dia e com luz quente à noite.
- **De longe** (mais de 80 m), o jogo mostra o LOD: só a parte de fora da casa.
- **Chuva:** não chove debaixo dos telhados (um mapa de 0,5 m de onde tem telhado).
- **Outro mod** que tenha trocado esse bloco: o mod deixa o dele.

## Etapas

1. **Escolher o terreno:** `/lote` grava `coop\lote.txt` (o ponto, o chão e o mar em volta, e os prédios do mapa num
   raio de 70 m). Feito: santahouse04_law2.
2. **Estrutura:** paredes, telhado, deck e garagem no terreno, com a colisão; o bloco antigo some. Feito (1.23.0).
3. **Interior:** texturas (piso, paredes, teto de vigas, escada com azulejos), cortinas e os móveis nas medidas da
   planta.

## Código e ferramentas

- `src/client/features/charlie/Lot.cpp`: o `/lote`.
- `src/client/features/charlie/Charlie.cpp`: troca o modelo e a colisão do bloco (por `shared/game/ModelSwap`, que a
  placa NOSTALGIA também usa), esconde a palmeira e o arbusto, e tira a chuva debaixo do telhado.
- `tools/charlie/build.py model.dae src/client/features/charlie/CharlieData.h`: o conversor (DAE → DFF, LOD, COL3 e o
  mapa do telhado). Grava também `tools/charlie/out/` (o .dff, o .col e `preview.npz`); `preview.py` desenha uma vista
  dele (`python3 preview.py out/preview.npz vista.png "(14,-42,3)" "(0,0,-3)" 99 50`).
- O modelo 3D não está no repositório: é o arquivo que o Vinicius recebeu (ver a licença dele antes de distribuir).
