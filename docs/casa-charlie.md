# Casa do Charlie Harper na praia

A casa da série *Two and a Half Men* em escala 1x1, como parte do mapa: vista de fora, com o deck virado para o mar, e
dá para entrar e sair andando, sem teleporte.

## Como fica no jogo

- **Lugar:** um terreno da praia (a ideia é Santa Maria Beach, a "Malibu" de Los Santos). As casas do mapa que estão
  ali, sem uso na história, somem para todos os jogadores quando aquela parte do mapa carrega.
- **Modelo:** a base é o `model.dae` (SketchUp, já em metros: 36,7 x 22,3 m, 7 m de altura), com paredes, cômodos,
  portas, janelas, escada, deck e garagem. O acabamento e os móveis seguem a planta e as fotos da série.
- **Arquivos:** modelo, colisão e texturas novos, carregados pelo modloader e distribuídos pelo launcher.
- **Chuva:** uma zona sem chuva no lugar da casa, para não chover dentro.

## Etapas

1. **Escolher o terreno:** na praia, no lugar da frente da casa, olhando para o mar, digitar `/lote`. O mod grava
   `coop\lote.txt` (o ponto, o chão e o mar em volta, e os prédios do mapa num raio de 70 m).
2. **Estrutura:** paredes, telhado, deck e garagem no terreno, com a colisão; as casas antigas somem.
3. **Interior:** pisos, paredes, teto de vigas, escada com azulejos, cortinas, e os móveis nas medidas da planta.

## Código e ferramentas

- `src/client/features/charlie/Lot.cpp`: o `/lote`.
