// The instrument's colours, in one place.
//
// These are NEPTR's palette, lifted from the on-device UI
// (`NEPTR phase4/neptrPhase4_UI.py`) rather than invented here. The video
// engine is a MODE of the same instrument, so it has no business having a
// look of its own -- two surfaces of one instrument that disagree about their
// colours read as two programs sharing a screen.
//
// It is a system, not a set. EVERY colour is derived from hue 296 degrees at
// varying saturation and value; ALERT is the single deliberate exception that
// leaves the purple, which is exactly what makes it read as an alarm. So if a
// new shade is ever needed, derive it at H=296 -- introducing a second hue
// would spend the one signal ALERT has.
//
// The bitmap font and the pixel grid stay: those are the instrument's texture,
// and only the palette was ever wrong.
#pragma once

namespace nv {
namespace theme {

struct Colour {
    float r, g, b;
};

// H296 S100 V12 -- the anchor everything else is measured against.
constexpr Colour kBg          {0.110f, 0.000f, 0.118f};
constexpr Colour kBgTop       {0.078f, 0.000f, 0.094f};
constexpr Colour kBgBot       {0.165f, 0.008f, 0.196f};

constexpr Colour kFg          {1.000f, 1.000f, 1.000f};
constexpr Colour kOffOutline  {0.443f, 0.251f, 0.459f};  // H296 S43 V46
constexpr Colour kOffText     {0.608f, 0.447f, 0.620f};  // H296 S27 V61
constexpr Colour kEmptyOutline{0.216f, 0.098f, 0.243f};  // H296 S60 V24

constexpr Colour kTrackActive {0.863f, 0.729f, 0.871f};  // near-white, warm
constexpr Colour kTrackDim    {0.361f, 0.227f, 0.369f};

constexpr Colour kAccent      {0.792f, 0.165f, 0.922f};  // violet-magenta glow
constexpr Colour kAccentDim   {0.337f, 0.094f, 0.384f};

// The ONE non-purple. Governor throttling, failed launches, missing config --
// and nothing else, or it stops meaning anything.
constexpr Colour kAlert       {1.000f, 0.329f, 0.471f};

}  // namespace theme
}  // namespace nv
