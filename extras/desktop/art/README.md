# GUI artwork

`kyleos-icons.png` is a clean-room icon sheet generated for KyleOS. It is not
extracted from macOS and contains no Apple logos or other Apple artwork. The
visual brief calls for original, glossy, photorealistic desktop objects in the
late-1990s/early-2000s Aqua era, laid out on a 3x3 grid for the asset builder.

The source generation output is 1254x1254 RGBA. `scripts/build-gui-assets.sh`
crops it into nine 64x64 KIMG files in reading order: system monitor, read-me,
game, trash, editor/paint, terminal, mines, solitaire, and calculator.

`kyleos-home.png` and `kyleos-city.png` are matching clean-room icon masters
for System Disk and City Builder. The asset builder trims and scales these into
`system-disk.kimg` and `citybuilder.kimg`; System Monitor retains the original
disc artwork as `sysmon.kimg`.

`kyleos-paint.png` and `kyleos-cide.png` are dedicated matching masters for
Paint and the C IDE. They build into `paint.kimg` and `cide.kimg` rather than
reusing the generic editor artwork.

`kyleos-tetris.png` is the falling-block game's application icon.
`kyleos-tetris-block.png` is a glossy source tile; the asset builder derives
seven colored 16x16 KIMG tiles from it for the live board renderer.

Generated with OpenAI image generation on 2026-08-20 for this repository. The
artwork is intended to be distributed under the repository's license.

## Aqua desktop picture

`aqua-blue.jpg` is the original 1024x768 `Aqua Blue.jpg` distributed with Mac
OS X 10.0 Cheetah. It was downloaded from the Wallpapers Wiki archival copy at
the user's request and is used only by KyleOS's Aqua environment. It remains
Apple artwork and is not covered by the repository's license. The asset builder
preserves the source and creates a center-cropped widescreen KIMG derivative.

## Fonts

`scripts/build-gui-assets.sh` rasterizes the host's DejaVu Sans, DejaVu Sans
Condensed, and DejaVu Sans Mono fonts into KyleOS KFONT alpha atlases. DejaVu
fonts are derived from Bitstream Vera and are redistributable under their
permissive font license. The generated atlases retain that font attribution;
they are not original Apple or Microsoft font files.
