# The jackets: the group's windbreakers

`src/client/features/clothes` paints our two windbreakers over the game's "coach" jacket (SubUrban's Grn and Blk
Windbreaker) as its textures load, and puts both in every wardrobe. These tools make its data (`ClothesData.h`) from
the game's files, which are not in this repository: `coach.dff`, `coach.txd` and `coachsemi.txd` from
`models/player.img`, in `orig/`.

1. `python3 design.py`: the designs (`out/coach.png`: lilac "Matunas"; `out/coachsemi.png`: black and purple), painted
   by the 3D place of each texel (`uvmap.py`); `logo.png` is the patch, `matunas.png` the back's lettering.
2. `python3 export.py ../../src/client/features/clothes/ClothesData.h`
3. Test: `g++ -std=c++17 -I../../src/client paint_test.cpp ../../src/client/features/clothes/JacketPaint.cpp` and run it
   here: it paints as the mod does and compares with export.py's.
