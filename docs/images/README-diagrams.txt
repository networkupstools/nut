NUT documentation diagrams
==========================

The five figures are `simple`, `advanced`, `bigbox`, `bizarre` and
`nut_layering`. Each has a self-contained SVG and a 2400-pixel
PNG export. The documentation build uses the PNGs and needs no graphics
renderer. Figures display at 640 pixels in HTML and 160 mm in PDF.

Shared source
-------------

`diagram-source/nutdiag.py` defines the palette, typography, original
vector symbols, host boundaries and connectors. `diagram-source/build.py`
places the components for each figure. `diagram-source/template.svg`
shows the shared symbols, labels and connection styles for future figures.
The Python files are the authoritative source. Edit them and regenerate
the SVGs and PNGs together; regeneration overwrites direct SVG edits.

A computer boundary contains its NUT processes. Solid red power lines end
at the computer's power inlet; dashed blue communication lines end at the
relevant driver or server. TCP arrows point from clients toward upsd.
USB/serial and local IPC links use round endpoints. These are connection
diagrams, not a shutdown sequence.

SVG text remains selectable and editable. The font stack uses Noto Sans and Noto Sans Mono,
with DejaVu and Liberation fallbacks. The SVGs contain no raster images
or external references. The symbols and layout were authored with
Claude Opus 5.5 assistance.

Regeneration
------------

The drawing workstation needs Python 3.6 or later, librsvg's
`rsvg-convert`, and Noto Sans / Noto Sans Mono for the default appearance.
From this directory, run:

----
python3 diagram-source/build.py --png
----

This writes all five SVGs and PNGs, plus the template sheet. PNG bytes can
vary with installed fonts and the librsvg version. To export an
individual SVG:

----
rsvg-convert --width=2400 --output=simple.png simple.svg
----

Keep the exports with their source. Inspect changes at normal HTML and PDF
sizes, and trace the power and communication paths against the adjacent
text. Use the same symbols, type scale and line meanings in future figures.
Detailed device and driver inventories belong in the surrounding prose.
