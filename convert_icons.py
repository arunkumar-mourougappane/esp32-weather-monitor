#!/usr/bin/env python3
"""Convert PNG weather icons to C XBM byte arrays for M5GFX drawXBitmap."""

from PIL import Image
import os

PNG_DIR = "weather_icons/PNG"
OUT_FILE = "lib/Display/weather_bitmaps.h"
ICON_SIZE = 32  # target size in pixels; icons are already 32x32

# Map: (c_name, png_filename, threshold)
# Threshold: alpha > threshold → dark (foreground) pixel
ICONS = [
    ("icon_clear",            "SUN.png",               30),
    ("icon_cloudy",           "CLOUDY.png",             30),
    ("icon_partly_cloudy",    "PARTLY_CLOUDY.png",      30),
    ("icon_mostly_cloudy",    "MOSTLY_CLOUDY.png",      30),
    ("icon_rain",             "RAIN.png",               30),
    ("icon_drizzle",          "DRIZZLE.png",            30),
    ("icon_heavy_showers",    "HEAVY_SHOWERS.png",      30),
    ("icon_freezing_rain",    "FREEZING_RAIN.png",      30),
    ("icon_thunder",          "THUNDERSTORMS.png",      30),
    ("icon_thundershowers",   "THUNDERSHOWERS.png",     30),
    ("icon_snow",             "SNOW.png",               30),
    ("icon_heavy_snow",       "HEAVY_SNOW.png",         30),
    ("icon_sleet",            "SLEET.png",              30),
    ("icon_blowing_snow",     "BLOWING_SNOW.png",       30),
    ("icon_hail",             "HAIL.png",               30),
    ("icon_fog",              "FOG.png",                30),
    ("icon_foggy",            "FOGGY.png",              30),
    ("icon_wind",             "WIND.png",               30),
    ("icon_windy",            "WINDY.png",              30),
    ("icon_tornado",          "TORNADO.png",            30),
    ("icon_hurricane",        "HURRICANE.png",          30),
    ("icon_blustery",         "BLUSTERY.png",           30),
    ("icon_haze",             "HAZE.png",               30),
    ("icon_smoky",            "SMOKY.png",              30),
    ("icon_not_available",    "NOT_AVAILABLE.png",      30),
]

def png_to_xbm_bytes(path: str, threshold: int = 30) -> list[int]:
    """Return list of ints (LSB-first XBM row bytes) for the icon at 'path'."""
    img = Image.open(path).convert("RGBA")
    if img.size != (ICON_SIZE, ICON_SIZE):
        img = img.resize((ICON_SIZE, ICON_SIZE), Image.LANCZOS)
    
    W, H = ICON_SIZE, ICON_SIZE
    bytes_per_row = (W + 7) // 8  # = 4 for ICON_SIZE=32
    result = []
    
    for y in range(H):
        row_bytes = [0] * bytes_per_row
        for x in range(W):
            r, g, b, a = img.getpixel((x, y))
            # A sufficiently opaque dark pixel → 1 (foreground)
            is_dark = (a > threshold) and ((r + g + b) < 600)
            if is_dark:
                byte_idx = x // 8
                bit_idx  = x % 8          # LSB first
                row_bytes[byte_idx] |= (1 << bit_idx)
        result.extend(row_bytes)
    
    return result


lines = []
lines.append("#ifndef WEATHER_BITMAPS_H")
lines.append("#define WEATHER_BITMAPS_H")
lines.append("")
lines.append("#include <Arduino.h>")
lines.append("")
lines.append(f"// Auto-generated {ICON_SIZE}x{ICON_SIZE} XBM icon bitmaps (LSB-first, 1=dark pixel)")
lines.append(f"// Source: weather_icons/PNG/ (IcoMoon icon set)")
lines.append("")

generated = []
for c_name, png_name, thresh in ICONS:
    path = os.path.join(PNG_DIR, png_name)
    if not os.path.exists(path):
        print(f"  SKIP (not found): {png_name}")
        continue
    try:
        bmp_bytes = png_to_xbm_bytes(path, thresh)
    except Exception as e:
        print(f"  SKIP ({e}): {png_name}")
        continue

    hex_vals = ", ".join(f"0x{b:02x}" for b in bmp_bytes)
    lines.append(f"// {png_name}")
    lines.append(f"static const uint8_t PROGMEM {c_name}_bmp[] = {{")
    # Break into rows of 4 bytes (one row of 32 pixels each)
    for row in range(ICON_SIZE):
        row_start = row * 4
        row_hex = ", ".join(f"0x{bmp_bytes[row_start + i]:02x}" for i in range(4))
        lines.append(f"    {row_hex},")
    # Remove trailing comma on last row already handled — just strip last
    lines[-1] = lines[-1].rstrip(",")
    lines.append("};")
    lines.append("")
    generated.append(c_name)
    print(f"  OK: {c_name} <- {png_name}")

lines.append("#endif // WEATHER_BITMAPS_H")
lines.append("")

with open(OUT_FILE, "w") as f:
    f.write("\n".join(lines))

print(f"\nWrote {OUT_FILE} with {len(generated)} icons.")
print("Icons:", ", ".join(generated))
