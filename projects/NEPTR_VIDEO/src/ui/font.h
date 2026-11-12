// A 5x7 bitmap font, built into an atlas at startup.
//
// No font library, no file to ship, no dependency. The panel needs to print
// parameter names and numbers and nothing more, and a classic 5x7 cell is
// legible down to very small panels.
//
// The atlas carries one extra cell past the printable range that is entirely
// solid, so filled rectangles (slider bars, bar backgrounds) can be drawn from
// the same texture, in the same draw call, with the same shader as the text.
#pragma once

#include <cstdint>
#include <vector>

namespace nv {
namespace font {

constexpr int kGlyphW = 5;
constexpr int kGlyphH = 7;
constexpr int kCellW = 6;   // one column of spacing
constexpr int kCellH = 8;
constexpr int kFirst = 32;  // space
// Through '_' (95) rather than stopping at 'Z': sensor and parameter ids are
// full of underscores, and folding one to a space makes two different ids
// look identical on the monitor.
constexpr int kLast = 95;
constexpr int kCount = kLast - kFirst + 1;
constexpr int kSolidCell = kCount;  // the all-on cell used for rectangles
constexpr int kCells = kCount + 1;

constexpr int atlasWidth() { return kCells * kCellW; }
constexpr int atlasHeight() { return kCellH; }

// Single-channel, one byte per pixel, 0 or 255.
std::vector<uint8_t> buildAtlas();

// Cell index for a character. Lowercase folds to uppercase; anything the font
// does not carry becomes a space rather than a missing-glyph box.
int cellFor(char c);

}  // namespace font
}  // namespace nv
