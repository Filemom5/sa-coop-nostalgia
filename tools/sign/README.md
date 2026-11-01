# The sign: NOSTALGIA in place of VINEWOOD

`src/client/features/sign` makes the sign on the hill anew from the game's own models as they load. These tools make
its data (`SignData.h`) from the game's files, which are not in this repository:

1. `extrair-placa.ps1 -Jogo "C:\...\GTA San Andreas"` copies them into `orig/` (the sign's models, its textures, the
   hill's collision and placements).
2. `python3 build.py` makes the three models and the collision in `out/` (`preview.py` renders them: `views()`).
3. `python3 recipe.py ../../src/client/features/sign/SignData.h` writes the recipe the mod applies.
4. Test: `g++ -std=c++17 -I../../src/client compose_test.cpp ../../src/client/features/sign/SignRecipe.cpp` and run it
   here: it makes the models as the mod does (`test/`, the same bytes as `out/`).

`font.py` has the letters (the sign's typeface, measured from VINEWOOD); `build.py` the layout (`GAPS`, `LIFT`: the
letters' bottoms above the hill), which frame each new letter copies and how they are coloured.
