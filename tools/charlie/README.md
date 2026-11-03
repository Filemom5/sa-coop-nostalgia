# Charlie Harper's beach house

`src/client/features/charlie` puts Charlie Harper's house (Two and a Half Men) at 1:1 on Santa Maria Beach, in place of
the block of houses `santahouse04_law2` (docs/casa-charlie.md). These tools make its data (`CharlieData.h`) from the
SketchUp model of the house (`model.dae`, in metres), which is not in this repository.

- `python3 build.py model.dae ../../src/client/features/charlie/CharlieData.h`: the model (`santahouse04_law2` near,
  `LODtahouse04_law2` far, the outside only), the collision and the map of the roofs (no rain under them). Also
  `out/`: the .dff, the .col and `preview.npz`.
- `python3 preview.py out/preview.npz view.png "(14,-42,3)" "(0,0,-3)" 99 50 [night]`: a view of it (eye, target, in
  the block's axes; the faces under a height; the field of view), with the colours of day or night.
