# Synthetic AOI test pair

These images are original synthetic assets created specifically for AOI Vision
Lab Qt. They contain no third-party product, brand, design file or production
data.

## Files

- `pcb-reference-good.png`: known-good reference board.
- `pcb-inspection-defective.png`: matching inspection board with controlled
  anomalies.

The two misalignment scenarios are generated deterministically at runtime from
the good and defective images. A fixed projective transform introduces rotation,
translation and perspective without inventing or altering board details.

## Intended anomalies

1. A displaced SMD component near the upper edge.
2. A reversed diode in the left-side diode column.
3. A missing SMD component and exposed pads to the left of the central IC.
4. A solder bridge between adjacent lower pins of the central QFP.
5. A tilted white connector on the right.

The second image deliberately retains the same resolution, camera position,
background and fiducials so the initial prototype can focus on registration and
difference localisation.
