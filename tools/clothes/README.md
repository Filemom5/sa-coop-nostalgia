# The jackets: the group's clothes

`src/client/features/clothes` paints our jackets over the game's own as their textures load, and puts them in every
wardrobe:

- SubUrban's "coach" jacket: Grn Windbreaker → lilac "Corta-vento Matunas", Blk Windbreaker → black and purple
  "Corta-vento Nostalgia";
- Victim's "hoodjack" (Beige Hooded Jacket) → "Jaqueta Sunset Nostalgia": indigo to red, a synthwave sunset with palms
  and NOSTALGIA on the back (after a "Los Angeles" jacket).

The designs are 1024 x 1024. The game's textures are 256 x 256: every 4th texel goes on them as they load, and the
dressed player's texture (the game makes it from the jacket's in `CClothesBuilder::ConstructTextures`) is made anew at
1024 x 1024 with mipmaps, so the logo is sharp.

These tools make the mod's data (`ClothesData.h`) from the game's files, which are not in this repository: `coach.dff`,
`coach.txd`, `coachsemi.txd`, `hoodjack.dff` and `hoodjackbeige.txd` from `models/player.img`, in `orig/`.

1. `python3 design.py`: the designs (`out/<texture>.png`), painted by the 3D place of each texel (`uvmap.py`), and
   `out/*_final.png`, the HD textures as the game will show them. `logo.png` is the patch (1024 x 1024),
   `matunas.png` the lilac one's lettering, `fonts/Anton-Regular.ttf` NOSTALGIA's letters (SIL Open Font License,
   `fonts/OFL.txt`).
2. `python3 export.py ../../src/client/features/clothes/ClothesData.h`
3. Test: `g++ -O2 -std=c++17 -I../../src/client paint_test.cpp ../../src/client/features/clothes/JacketPaint.cpp` and
   run it here: it paints as the mod does (the textures as they load, the HD one and its mipmaps) and compares with
   export.py's.
