#!/usr/bin/env python3
"""Build reproducible Nunito bitmap atlases used by Sprout SDL surfaces."""

from pathlib import Path
from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
FONT_PATH = ROOT / "shared" / "assets" / "fonts" / "Nunito-Variable.ttf"
OUTPUT_ROOT = ROOT / "shared" / "assets" / "fonts"
METRICS_PATH = ROOT / "shared" / "include" / "sprout" / "ui" / "font_metrics.hpp"
FIRST, LAST = 32, 126
SIZE, CELL, COLUMNS, PADDING = 48, 64, 16, 4


def build(weight: str, filename: str):
    font = ImageFont.truetype(FONT_PATH, SIZE)
    font.set_variation_by_name(weight)
    rows = (LAST - FIRST + 1 + COLUMNS - 1) // COLUMNS
    atlas = Image.new("RGBA", (COLUMNS * CELL, rows * CELL), (255, 255, 255, 0))
    draw = ImageDraw.Draw(atlas)
    metrics = []
    for codepoint in range(FIRST, LAST + 1):
        character = chr(codepoint)
        index = codepoint - FIRST
        cell_x = (index % COLUMNS) * CELL
        cell_y = (index // COLUMNS) * CELL
        left, top, right, bottom = font.getbbox(character, anchor="ls")
        width, height = max(0, right - left), max(0, bottom - top)
        advance = round(font.getlength(character))
        if width and height:
            draw.text(
                (cell_x + PADDING - left, cell_y + PADDING - top),
                character,
                font=font,
                fill=(255, 255, 255, 255),
                anchor="ls",
            )
        metrics.append((cell_x + PADDING, cell_y + PADDING, width, height, left, top, advance))
    atlas.save(OUTPUT_ROOT / filename, optimize=True)
    return metrics, font.getmetrics()[0]


regular, regular_ascent = build("SemiBold", "nunito-semibold.png")
heading, heading_ascent = build("ExtraBold", "nunito-extrabold.png")


def rows(name: str, values):
    body = ",\n".join(
        f"    {{{sx}, {sy}, {width}, {height}, {left}, {top}, {advance}}}"
        for sx, sy, width, height, left, top, advance in values
    )
    return f"inline constexpr std::array<UiGlyphMetric, {len(values)}> {name}{{{{\n{body}\n}}}};"


METRICS_PATH.write_text(
    """#pragma once

#include <array>

namespace sprout::ui {

struct UiGlyphMetric {
  int source_x;
  int source_y;
  int width;
  int height;
  int bearing_x;
  int bearing_top;
  int advance;
};

inline constexpr int kUiFontSourceSize = 48;
inline constexpr int kUiFontFirstCodepoint = 32;
inline constexpr int kUiFontLastCodepoint = 126;
inline constexpr int kUiFontRegularAscent = %d;
inline constexpr int kUiFontHeadingAscent = %d;

%s

%s

}  // namespace sprout::ui
"""
    % (regular_ascent, heading_ascent, rows("kUiFontRegularMetrics", regular), rows("kUiFontHeadingMetrics", heading)),
    encoding="utf-8",
)
