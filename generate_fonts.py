#!/usr/bin/env python3
"""Generate embedded TFT_eSPI/Seeed_GFX VLW fonts for Vector eInk Dashboard."""

from pathlib import Path
import struct
import sys

sys.path.insert(0, "/tmp/flintos-fontdeps")
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent
TTF = ROOT / "NotoSans-Variable.ttf"
LUNAR_TTF = Path("/System/Library/Fonts/Supplemental/Arial Unicode.ttf")

VIETNAMESE = (
    "ĂăÂâĐđÊêÔôƠơƯư"
    "ÀÁẢÃẠàáảãạẦẤẨẪẬầấẩẫậẰẮẲẴẶằắẳẵặ"
    "ÈÉẺẼẸèéẻẽẹỀẾỂỄỆềếểễệ"
    "ÌÍỈĨỊìíỉĩịÒÓỎÕỌòóỏõọỒỐỔỖỘồốổỗộỜỚỞỠỢờớởỡợ"
    "ÙÚỦŨỤùúủũụỪỨỬỮỰừứửữựỲÝỶỸỴỳýỷỹỵ"
)
EXTRA = "°€₫₹–—…•✓↑↓±%çÇñÑüÜöÖäÄßœŒæÆ¿¡ùûëïÿ"
CHARSET = sorted(set(chr(i) for i in range(0x20, 0x7F)) | set(VIETNAMESE) | set(EXTRA), key=ord)
LUNAR_TEXT = (
    "Âm lịch ngày mùng Tháng Năm nhuận Giáp Ất Bính Đinh Mậu Kỷ Canh Tân Nhâm Quý "
    "Tý Sửu Dần Mão Thìn Tỵ Ngọ Mùi Thân Dậu Tuất Hợi "
    "农历農曆年月日闰閏：甲乙丙丁戊己庚辛壬癸子丑寅卯辰巳午未申酉戌亥 "
    "음력년월일윤갑을병정무기경신임계자축인묘진사오미유술해 "
    "旧暦 Kalendar hari Bulan Tahun lompat Kuda Api Lunar day Month Year Leap "
    "Wood Fire Earth Metal Water Rat Ox Tiger Rabbit Dragon Snake Horse Goat Monkey Rooster Dog Pig"
)
LUNAR_CHARSET = sorted(set(chr(i) for i in range(0x20, 0x7F)) | set(LUNAR_TEXT), key=ord)

FONTS = [
    ("NotoSansMedium18", 18, "Medium"),
    ("NotoSansMedium24", 24, "Medium"),
    ("NotoSansSemiBold32", 32, "SemiBold"),
    ("NotoSansBold56", 56, "Bold"),
]


def i32(value: int) -> bytes:
    return struct.pack(">i", int(value))


def make_font(array_name: str, size: int, style: str, ttf: Path = TTF, charset=CHARSET) -> bytes:
    font = ImageFont.truetype(str(ttf), size=size)
    try:
        font.set_variation_by_name(style)
    except (OSError, ValueError):
        pass

    ascent, descent = font.getmetrics()
    metrics = bytearray()
    bitmaps = bytearray()
    glyph_count = 0

    for char in charset:
        stroke = 0
        bbox = font.getbbox(char, anchor="ls", stroke_width=stroke)
        if bbox is None:
            continue
        left, top, right, bottom = bbox
        width = max(0, right - left)
        height = max(0, bottom - top)
        advance = max(1, round(font.getlength(char)))

        if width and height:
            image = Image.new("L", (width, height), 0)
            draw = ImageDraw.Draw(image)
            draw.text((-left, -top), char, font=font, fill=255, stroke_width=stroke,
                      stroke_fill=255, anchor="ls")
            # E-paper is 1-bit: remove grey anti-alias pixels so thin edges do not vanish.
            image = image.point(lambda value: 255 if value >= 150 else 0)
            bitmaps.extend(image.tobytes())

        metrics.extend(i32(ord(char)))
        metrics.extend(i32(height))
        metrics.extend(i32(width))
        metrics.extend(i32(advance))
        metrics.extend(i32(-top))
        metrics.extend(i32(left))
        metrics.extend(i32(0))
        glyph_count += 1

    header = b"".join((i32(glyph_count), i32(11), i32(size), i32(0), i32(ascent), i32(descent)))
    name = array_name.encode("ascii")
    trailer = bytes((len(name),)) + name + b"\0" + bytes((len(name),)) + name + b"\0\1"
    return header + metrics + bitmaps + trailer


def write_header(array_name: str, payload: bytes) -> None:
    path = ROOT / f"{array_name}.h"
    guard = f"_{array_name.upper()}_H_"
    with path.open("w", encoding="ascii") as output:
        output.write(f"#ifndef {guard}\n#define {guard}\n\n#include <pgmspace.h>\n\n")
        output.write(f"const uint8_t {array_name}[] PROGMEM = {{\n")
        for offset in range(0, len(payload), 16):
            row = payload[offset : offset + 16]
            output.write("  " + ", ".join(f"0x{byte:02X}" for byte in row) + ",\n")
        output.write("};\n\n#endif\n")
    print(f"{path.name}: {len(payload):,} bytes")


if __name__ == "__main__":
    if "--lunar-only" in sys.argv:
        write_header("LunarText18", make_font("LunarText18", 18, "Regular", LUNAR_TTF, LUNAR_CHARSET))
    else:
        for name, size, style in FONTS:
            write_header(name, make_font(name, size, style))
