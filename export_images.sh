#!/usr/bin/env bash
set -euo pipefail

KICAD_CLI="/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"
PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
SCH="$PROJECT_DIR/PCB/hairtag/hairtag.kicad_sch"
PCB="$PROJECT_DIR/PCB/hairtag/hairtag.kicad_pcb"
OUT_DIR="$PROJECT_DIR/doc"

mkdir -p "$OUT_DIR"

echo "Exporting schematic SVG..."
"$KICAD_CLI" sch export svg -o "$OUT_DIR" "$SCH"

if [ -f "$OUT_DIR/hairtag.svg" ]; then
  mv "$OUT_DIR/hairtag.svg" "$OUT_DIR/schematic.svg"
  echo "  -> doc/schematic.svg"
fi

echo "Exporting PCB composite SVG..."
"$KICAD_CLI" pcb export svg \
  --output "$OUT_DIR/pcb.svg" \
  --layers "F.Cu,B.Cu,F.Silkscreen,B.Silkscreen,Edge.Cuts" \
  --page-size-mode 2 \
  --fit-page-to-board \
  --exclude-drawing-sheet \
  "$PCB"

# Add margin (5mm on each side) by expanding the viewBox
# SVG units are mm in KiCad exports
MARGIN=5
VIEWBOX_LINE=$(grep -o 'viewBox="[^"]*"' "$OUT_DIR/pcb.svg")
VX=$(echo "$VIEWBOX_LINE" | sed 's/viewBox="\([^ ]*\) \([^ ]*\) \([^ ]*\) \([^"]*\)"/\1/')
VY=$(echo "$VIEWBOX_LINE" | sed 's/viewBox="\([^ ]*\) \([^ ]*\) \([^ ]*\) \([^"]*\)"/\2/')
VW=$(echo "$VIEWBOX_LINE" | sed 's/viewBox="\([^ ]*\) \([^ ]*\) \([^ ]*\) \([^"]*\)"/\3/')
VH=$(echo "$VIEWBOX_LINE" | sed 's/viewBox="\([^ ]*\) \([^ ]*\) \([^ ]*\) \([^"]*\)"/\4/')
NEW_VX=$(echo "$VX - $MARGIN" | bc)
NEW_VY=$(echo "$VY - $MARGIN" | bc)
NEW_VW=$(echo "$VW + 2 * $MARGIN" | bc)
NEW_VH=$(echo "$VH + 2 * $MARGIN" | bc)
sed -i '' "s/viewBox=\"[^\"]*\"/viewBox=\"$NEW_VX $NEW_VY $NEW_VW $NEW_VH\"/" "$OUT_DIR/pcb.svg"

# Make copper layers semi-transparent so both sides are visible
sed -i '' 's/fill:#4D7FC4; fill-opacity:1.0000/fill:#4D7FC4; fill-opacity:0.6/g' "$OUT_DIR/pcb.svg"
sed -i '' 's/fill:#C83434; fill-opacity:1.0000/fill:#C83434; fill-opacity:0.6/g' "$OUT_DIR/pcb.svg"

# Reorder SVG groups so layers render in correct z-order (back to front):
# B.Cu -> B.Silkscreen -> F.Cu -> F.Silkscreen -> Edge.Cuts -> Pads/Vias
# KiCad does not guarantee this order in its SVG export
python3 - "$OUT_DIR/pcb.svg" <<'PYEOF'
import sys, re

path = sys.argv[1]
with open(path, 'r') as f:
    content = f.read()

header_match = re.search(r'^(.*?<svg[^>]*>)', content, re.DOTALL)
footer_match = re.search(r'(</svg>\s*)$', content)
if not header_match or not footer_match:
    sys.exit(0)

header = header_match.group(1)
footer = footer_match.group(1)
body = content[header_match.end():footer_match.start()]

# Extract top-level <g> groups (depth-1 children of <svg>)
groups = []
depth = 0
current = []
for line in body.split('\n'):
    s = line.strip()
    depth += len(re.findall(r'<g[\s>]', s)) - s.count('</g>')
    current.append(line)
    if depth == 0 and current:
        groups.append('\n'.join(current))
        current = []

# Layer priority: lower = renders behind (painted first in SVG)
# B.Cu(red) < B.Silk(pink) < F.Cu(blue) < F.Silk(yellow) < Edge.Cuts < Pads
# Classify by the group's opening <g> style (not scanning all child content,
# since KiCad sometimes embeds stray elements from other layers inside a group)
def layer_priority(g):
    # Extract the opening <g ...> tag and its style
    g_tag = ''
    for line in g.split('\n'):
        g_tag += line + ' '
        if '>' in line:
            break

    style_m = re.search(r'style="([^"]*)"', g_tag)
    style = style_m.group(1) if style_m else ''

    # Check fill/stroke colors in the group's own style
    fill_m = re.search(r'fill:(#[0-9A-Fa-f]{6})', style)
    stroke_m = re.search(r'stroke:(#[0-9A-Fa-f]{6})', style)
    fill_color = fill_m.group(1) if fill_m else None
    stroke_color = stroke_m.group(1) if stroke_m else None

    # Also scan for distinctive colors that only appear in specific layers
    has_e8b2a7 = '#E8B2A7' in g   # B.Silkscreen text fill
    has_f2eda1 = '#F2EDA1' in g   # F.Silkscreen text stroke
    has_d0d2cd = '#D0D2CD' in g   # Edge.Cuts
    has_white  = '#FFFFFF' in g    # Pads/Vias

    # Classify by fill color first (copper zones/pads), then stroke (traces)
    if fill_color == '#C83434':
        return 0  # B.Cu
    if fill_color == '#4D7FC4':
        return 2  # F.Cu
    if stroke_color == '#C83434':
        return 0  # B.Cu traces
    if stroke_color == '#4D7FC4':
        return 2  # F.Cu traces
    if has_e8b2a7:
        return 1  # B.Silkscreen
    if has_f2eda1:
        return 3  # F.Silkscreen
    if has_d0d2cd:
        return 4  # Edge.Cuts
    if has_white:
        return 5  # Pads/Vias
    return -1     # Unknown

# Compute priorities and check if reordering is needed
priorities = [layer_priority(g) for g in groups]
needs_reorder = any(
    priorities[i] > priorities[j]
    for i in range(len(priorities))
    for j in range(i+1, len(priorities))
    if priorities[i] >= 0 and priorities[j] >= 0
)

if needs_reorder:
    tagged = [(priorities[i], i, g) for i, g in enumerate(groups)]
    tagged.sort(key=lambda t: (t[0], t[1]))
    reordered = [t[2] for t in tagged]
    with open(path, 'w') as f:
        f.write(header + '\n'.join(reordered) + footer)
    counts = {}
    for p in priorities:
        names = {-1:'other', 0:'B.Cu', 1:'B.Silk', 2:'F.Cu', 3:'F.Silk', 4:'Edge.Cuts', 5:'Pads'}
        n = names[p]
        counts[n] = counts.get(n, 0) + 1
    print(f"  Reordered layers: {dict(sorted(counts.items()))}")
else:
    print("  Layers already in correct order")
PYEOF

# Add dark background
sed -i '' "/<desc>Image generated by PCBNEW <\/desc>/a\\
<!-- Background rectangle -->\\
<rect x=\"$NEW_VX\" y=\"$NEW_VY\" width=\"$NEW_VW\" height=\"$NEW_VH\" fill=\"#001124\"/>
" "$OUT_DIR/pcb.svg"
echo "  -> doc/pcb.svg"

echo "Rendering PCB 3D top view..."
"$KICAD_CLI" pcb render \
  --side top \
  --quality high \
  --width 2000 \
  --height 2000 \
  -o "$OUT_DIR/pcb_3d_top.png" \
  "$PCB"
echo "  -> doc/pcb_3d_top.png"

echo "Done! Exported to doc/"
