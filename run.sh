#!/bin/sh
set -e

python3 -m midi2svara \
  --octave 1200 \
  --single-letter 1 \
  --sthayi-span 2 \
  --sthayi-marks '^=1' '_=-1' \
  --max-gap 12 \
  --smoothing 0.5 \
  --min-points 4 \
  --min-length 5 \
  --max-nan-fraction 0.3 \
  --lengths 1 2 4 \
  --svara-names 'S=Sa' 'R=Ri' 'G=Ga' 'M=Ma' 'P=Pa' 'D=Da' 'N=Ni' \
  --cache-dir .cache \
  --plots-dir plots \
  --output-dir outputs \
  "$@"
