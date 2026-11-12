#!/bin/bash
# Build the ROTURA submission .docx from the Markdown source.
# Enforces the journal's editorial norms: Times New Roman 12pt, single-spaced, A4.
#
# Usage:  ./build_docx.sh
# Output: rotura_submission.docx

set -e
cd "$(dirname "$0")"

echo "Patching reference template (TNR 12pt, single-spaced, A4)..."
python3 patch_reference.py

echo "Generating .docx..."
pandoc rotura_submission.md \
    --reference-doc=reference.docx \
    --from=markdown \
    --to=docx \
    --output=rotura_submission.docx

SIZE=$(stat -f%z rotura_submission.docx)
SIZE_MB=$(echo "scale=2; $SIZE/1048576" | bc)
echo "Built: rotura_submission.docx (${SIZE_MB} MB)"

if [ "$SIZE" -gt 2097152 ]; then
    echo "WARNING: exceeds ROTURA's 2 MB limit. Downsample the photographs."
else
    echo "Within ROTURA's 2 MB limit."
fi

WORDS=$(pandoc rotura_submission.md --to=plain | wc -w | tr -d ' ')
echo "Total word count (incl. abstracts + references): ${WORDS}"
