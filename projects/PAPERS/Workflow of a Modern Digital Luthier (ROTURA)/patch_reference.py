#!/usr/bin/env python3
"""
Patch pandoc's default reference.docx to ROTURA's editorial norms:
  - Times New Roman throughout
  - 12pt body text
  - Single line spacing, no paragraph space-after
  - A4 page size

Regenerates reference.docx in place. Safe to re-run.
"""

import re
import shutil
import subprocess
import zipfile
from pathlib import Path

HERE = Path(__file__).parent
REF = HERE / "reference.docx"
WORK = HERE / ".refwork"

# Word measures font size in half-points and spacing in twentieths of a point.
BODY_HALF_POINTS = "24"  # 12pt
A4_WIDTH_TWIPS = "11906"
A4_HEIGHT_TWIPS = "16838"
MARGIN_TWIPS = "1701"  # 3 cm


def fresh_reference():
    """Get a clean copy of pandoc's default reference doc."""
    if WORK.exists():
        shutil.rmtree(WORK)
    WORK.mkdir()
    with open(REF, "wb") as fh:
        subprocess.run(
            ["pandoc", "--print-default-data-file", "reference.docx"],
            stdout=fh,
            check=True,
        )
    with zipfile.ZipFile(REF) as zf:
        zf.extractall(WORK)


def patch_styles():
    path = WORK / "word" / "styles.xml"
    xml = path.read_text(encoding="utf-8")

    # Force Times New Roman on every font reference.
    xml = re.sub(
        r'w:ascii="[^"]*"\s+w:hAnsi="[^"]*"',
        'w:ascii="Times New Roman" w:hAnsi="Times New Roman"',
        xml,
    )
    xml = re.sub(r'w:cs="[^"]*"', 'w:cs="Times New Roman"', xml)
    xml = re.sub(r'w:eastAsia="[^"]*"', 'w:eastAsia="Times New Roman"', xml)

    # Normalise all declared sizes to 12pt, then restore heading hierarchy below.
    xml = re.sub(r'<w:sz w:val="\d+"\s*/>', f'<w:sz w:val="{BODY_HALF_POINTS}"/>', xml)
    xml = re.sub(
        r'<w:szCs w:val="\d+"\s*/>', f'<w:szCs w:val="{BODY_HALF_POINTS}"/>', xml
    )

    # Single spacing, no space before/after paragraphs.
    xml = re.sub(
        r'<w:spacing[^/>]*/>',
        '<w:spacing w:before="0" w:after="0" w:line="240" w:lineRule="auto"/>',
        xml,
    )

    path.write_text(xml, encoding="utf-8")


def patch_page_size():
    path = WORK / "word" / "document.xml"
    xml = path.read_text(encoding="utf-8")

    xml = re.sub(
        r'<w:pgSz[^/>]*/>',
        f'<w:pgSz w:w="{A4_WIDTH_TWIPS}" w:h="{A4_HEIGHT_TWIPS}"/>',
        xml,
    )
    xml = re.sub(
        r'<w:pgMar[^/>]*/>',
        f'<w:pgMar w:top="{MARGIN_TWIPS}" w:right="{MARGIN_TWIPS}" '
        f'w:bottom="{MARGIN_TWIPS}" w:left="{MARGIN_TWIPS}" '
        f'w:header="708" w:footer="708" w:gutter="0"/>',
        xml,
    )

    # Pandoc reads page setup from the FIRST sectPr in the body. The default
    # template ships one that declares only footnote behaviour, so the page
    # dimensions must be inserted into it rather than appended as a second
    # sectPr — Word permits only one final sectPr per body, and pandoc would
    # ignore the extra one regardless.
    if "<w:pgSz" not in xml:
        page_setup = (
            f'<w:pgSz w:w="{A4_WIDTH_TWIPS}" w:h="{A4_HEIGHT_TWIPS}"/>'
            f'<w:pgMar w:top="{MARGIN_TWIPS}" w:right="{MARGIN_TWIPS}" '
            f'w:bottom="{MARGIN_TWIPS}" w:left="{MARGIN_TWIPS}" '
            'w:header="708" w:footer="708" w:gutter="0"/>'
        )
        xml = xml.replace("<w:sectPr>", "<w:sectPr>" + page_setup, 1)

    if "<w:pgSz" not in xml:
        raise SystemExit("patch_reference: failed to set A4 page size")

    path.write_text(xml, encoding="utf-8")


def add_custom_styles():
    """Add the paragraph styles the manuscript's fenced divs refer to.

    Pandoc maps `::: {custom-style="X"}` onto a Word paragraph style named X,
    but only applies it if the style exists in the reference document — a
    missing style fails silently and the content renders unstyled.
    """
    path = WORK / "word" / "styles.xml"
    xml = path.read_text(encoding="utf-8")

    def para_style(style_id, name, props):
        return (
            f'<w:style w:type="paragraph" w:customStyle="1" w:styleId="{style_id}">'
            f'<w:name w:val="{name}"/>'
            f'<w:basedOn w:val="BodyText"/>'
            f"<w:pPr>{props}</w:pPr>"
            f'<w:rPr><w:rFonts w:ascii="Times New Roman" w:hAnsi="Times New Roman"/>'
            f'<w:sz w:val="{BODY_HALF_POINTS}"/></w:rPr>'
            f"</w:style>"
        )

    # References: APA hanging indent, one clear line between entries.
    xml = xml.replace(
        "</w:styles>",
        para_style(
            "ReferenceEntry",
            "ReferenceEntry",
            '<w:ind w:left="720" w:hanging="720"/>'
            '<w:spacing w:before="0" w:after="160" w:line="240" w:lineRule="auto"/>',
        )
        + "</w:styles>",
    )

    # Pandoc emits images as its built-in "Figure" style and their captions as
    # "Image Caption". Centre both so figures sit on the page axis, and give
    # them vertical room so they do not crowd the surrounding text.
    # "CaptionedFigure" wraps an image that has a caption — that is the one
    # pandoc actually uses here; "Figure" covers a bare image with no caption.
    for style_id, spacing in (
        ("CaptionedFigure", '<w:spacing w:before="240" w:after="60"/>'),
        ("Figure", '<w:spacing w:before="240" w:after="120"/>'),
        ("ImageCaption", '<w:spacing w:before="0" w:after="240"/>'),
    ):
        pattern = re.compile(
            r'(<w:style w:type="paragraph"[^>]*w:styleId="' + style_id + r'">.*?)</w:style>',
            re.S,
        )
        match = pattern.search(xml)
        if not match:
            raise SystemExit(f"patch_reference: style {style_id!r} not found")
        block = match.group(1)
        # Drop any justification the base style carried, then centre it.
        block = re.sub(r"<w:jc[^/]*/>", "", block)
        if "<w:pPr>" in block:
            block = block.replace(
                "<w:pPr>", f'<w:pPr><w:jc w:val="center"/>{spacing}', 1
            )
        else:
            # OOXML fixes the child order of w:style: name, then basedOn, then
            # pPr. Inserting before w:name produces a file Word rejects as
            # corrupt, so append after the last of the preceding elements.
            new_ppr = f'<w:pPr><w:jc w:val="center"/>{spacing}</w:pPr>'
            # basedOn comes after name, so prefer it; a regex alternation would
            # match whichever appears first in the string, not the later one.
            anchor = re.search(r"<w:basedOn[^/]*/>", block) or re.search(
                r"<w:name[^/]*/>", block
            )
            if not anchor:
                raise SystemExit(
                    f"patch_reference: no anchor to place pPr in style {style_id!r}"
                )
            block = (
                block[: anchor.end()] + new_ppr + block[anchor.end():]
            )
        xml = xml.replace(match.group(1), block, 1)

    path.write_text(xml, encoding="utf-8")


def repack():
    REF.unlink(missing_ok=True)
    with zipfile.ZipFile(REF, "w", zipfile.ZIP_DEFLATED) as zf:
        for item in sorted(WORK.rglob("*")):
            if item.is_file():
                zf.write(item, item.relative_to(WORK))


def main():
    fresh_reference()
    patch_styles()
    add_custom_styles()
    patch_page_size()
    repack()
    shutil.rmtree(WORK)
    print(f"  reference.docx patched ({REF.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
