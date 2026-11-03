# Charlie Harper's beach house

`src/client/features/charlie` puts Charlie Harper's house (Two and a Half Men), 1.6 times life size, on Santa Maria
Beach in place of the block of houses `santahouse04_law2` and over part of `santahouse02_law2`, which becomes sand
(docs/casa-charlie.md). These tools make its data (`CharlieData.h`) from the SketchUp model of the house
(`model.dae`, in metres), which is not in this repository, and `coop\lote.txt` (`/lote` in the game, by the house's
place: what of the map stands there).

- `python3 build.py model.dae lote.txt ../../src/client/features/charlie/CharlieData.h`: the models (`santahouse04_law2`
  near, `LODtahouse04_law2` far, the outside only; the east block's sand), the collisions (with ramps over the
  stairs), the textures, the map of the roofs (no rain under them) and what to hide. Also `out/`: the .dff, the .col
  and `preview.npz`.
- `python3 textures.py`: the textures, made here (`out/textures.png`).
- `python3 preview.py view.png "(eye)" "(target)" [height] [fov] [night]`: a view as the game draws it (texture x
  vertex colour x 2), in the block's axes (the floor at z -5.06).
