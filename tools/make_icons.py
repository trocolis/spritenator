#!/usr/bin/env python3
"""
make_icons.py - Gera todos os ícones do Spritenator (arte própria, pixel art).

Cada ícone é descrito aqui mesmo, como texto: mapas ASCII (um caractere por
pixel, ver PALETTE) ou figuras geradas por código (formas geométricas, grades
e os tons, lidos de src/core/Tone.h). Para mudar um ícone, edite o mapa e
rode o script de novo — o resultado é sempre o mesmo (reproduzível).

Saídas:
  resources/icons/<nome>.png            ícone normal
  resources/icons/<nome>_disabled.png   variante desabilitada (automática)
  resources/app_icon.ico                ícone do executável (Windows)
  resources/app_icon.icns               ícone do pacote (macOS)
  resources/linux/icons/<N>.png         ícone do aplicativo (Linux, hicolor)

Uso:
    python3 tools/make_icons.py            # gera tudo
    python3 tools/make_icons.py --preview prancha.png   # + prancha ampliada
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

from PIL import Image, ImageDraw

REPO_ROOT = Path(__file__).resolve().parent.parent
ICONS_DIR = REPO_ROOT / "resources" / "icons"
TONE_HEADER = REPO_ROOT / "src" / "core" / "Tone.h"

# -----------------------------------------------------------------------------
# Paleta única de todos os ícones (um caractere por cor; "." = transparente).
# -----------------------------------------------------------------------------
PALETTE = {
    ".": (0, 0, 0, 0),
    "K": (27, 29, 46, 255),      # contorno
    "d": (76, 82, 110, 255),     # cinza escuro
    "g": (138, 146, 170, 255),   # cinza médio
    "l": (200, 205, 220, 255),   # cinza claro
    "w": (255, 255, 255, 255),   # branco
    "B": (52, 116, 214, 255),    # azul
    "b": (142, 194, 255, 255),   # azul claro
    "R": (214, 69, 61, 255),     # vermelho
    "r": (255, 160, 150, 255),   # rosa (borracha)
    "Y": (242, 192, 52, 255),    # amarelo
    "y": (255, 236, 150, 255),   # amarelo claro
    "O": (226, 128, 50, 255),    # laranja
    "G": (60, 162, 78, 255),     # verde
    "P": (138, 82, 208, 255),    # roxo
    "N": (35, 38, 74, 255),      # azul-marinho (fundo do ícone do app)
    "n": (52, 57, 104, 255),     # linhas da grade do ícone do app
}


def color(key: str) -> tuple[int, int, int, int]:
    return PALETTE[key]


# -----------------------------------------------------------------------------
# Utilitários de desenho
# -----------------------------------------------------------------------------
def new(width: int, height: int) -> Image.Image:
    return Image.new("RGBA", (width, height), (0, 0, 0, 0))


def art(text: str, width: int | None = None, height: int | None = None) -> Image.Image:
    """Converte um mapa ASCII em imagem, conferindo as dimensões."""
    rows = [row.strip() for row in text.strip("\n").splitlines()]
    rows = [row for row in rows if row]
    w = len(rows[0])
    for number, row in enumerate(rows):
        if len(row) != w:
            raise ValueError(f"linha {number} com {len(row)} colunas (esperado {w}): {row}")
    if width is not None and w != width:
        raise ValueError(f"largura {w}, esperado {width}")
    if height is not None and len(rows) != height:
        raise ValueError(f"altura {len(rows)}, esperado {height}")
    image = new(w, len(rows))
    for y, row in enumerate(rows):
        for x, key in enumerate(row):
            image.putpixel((x, y), color(key))
    return image


def paste(base: Image.Image, overlay: Image.Image, x: int, y: int) -> Image.Image:
    """Sobrepõe "overlay" (pixels transparentes não cobrem a base)."""
    base.alpha_composite(overlay, (x, y))
    return base


def shape(image: Image.Image, inside, outline: str, fill: str | None) -> Image.Image:
    """Desenha uma forma dada por um predicado inside(x, y): pixels internos
    vizinhos de um externo viram contorno; os demais, preenchimento."""
    w, h = image.size
    for y in range(h):
        for x in range(w):
            if not inside(x, y):
                continue
            edge = any(not inside(x + dx, y + dy) for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
            if edge:
                image.putpixel((x, y), color(outline))
            elif fill is not None:
                image.putpixel((x, y), color(fill))
    return image


def rect_inside(x0, y0, x1, y1):
    return lambda x, y: x0 <= x <= x1 and y0 <= y <= y1


def round_rect_inside(x0, y0, x1, y1, radius):
    def inside(x, y):
        if not (x0 <= x <= x1 and y0 <= y <= y1):
            return False
        cx = min(max(x, x0 + radius), x1 - radius)
        cy = min(max(y, y0 + radius), y1 - radius)
        return (x - cx) ** 2 + (y - cy) ** 2 <= radius * radius + radius * 0.8
    return inside


def ellipse_inside(x0, y0, x1, y1):
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    rx, ry = (x1 - x0) / 2 + 0.5, (y1 - y0) / 2 + 0.5
    return lambda x, y: ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1.0


def line(image: Image.Image, x0: int, y0: int, x1: int, y1: int, key: str) -> None:
    """Reta de Bresenham (1 pixel)."""
    dx, dy = abs(x1 - x0), -abs(y1 - y0)
    sx, sy = (1 if x0 < x1 else -1), (1 if y0 < y1 else -1)
    err = dx + dy
    while True:
        image.putpixel((x0, y0), color(key))
        if x0 == x1 and y0 == y1:
            break
        e2 = 2 * err
        if e2 >= dy:
            err += dy
            x0 += sx
        if e2 <= dx:
            err += dx
            y0 += sy


def mirror(image: Image.Image) -> Image.Image:
    return image.transpose(Image.Transpose.FLIP_LEFT_RIGHT)


def flip(image: Image.Image) -> Image.Image:
    return image.transpose(Image.Transpose.FLIP_TOP_BOTTOM)


def rotate_cw(image: Image.Image) -> Image.Image:
    return image.transpose(Image.Transpose.ROTATE_270)


# -----------------------------------------------------------------------------
# Peças reutilizadas
# -----------------------------------------------------------------------------
# Bloco de amostras de cor (13x9): a "paleta" dos ícones do menu Paleta.
SWATCHES = """
KKKKKKKKKKKKK
KRRRKYYYKGGGK
KRRRKYYYKGGGK
KRRRKYYYKGGGK
KKKKKKKKKKKKK
KBBBKPPPKOOOK
KBBBKPPPKOOOK
KBBBKPPPKOOOK
KKKKKKKKKKKKK
"""

MINI_FOLDER = """
.KKK....
KyyyKKK.
KyyyyyyK
KYYYYYYK
KYYYYYYK
KYYYYYYK
KKKKKKKK
"""

MINI_DISK = """
KKKKKKK.
KBwwwBKK
KBwwwBBK
KBBBBBBK
KBKKKKBK
KBKllKBK
KBKllKBK
KKKKKKKK
"""

MINI_PENCIL = """
.....KK.
....KrrK
...KYKKK
..KYYYK.
.KYYYK..
KwYYK...
KKwK....
KKK.....
"""

MINI_LOCK = """
..KKK..
.K...K.
.K...K.
KKKKKKK
KYYYYYK
KYYKYYK
KOOOOOK
KKKKKKK
"""

MINI_LOCK_OPEN = """
....KKK.
...K...K
...K....
KKKKKKK.
KlllllK.
KlldllK.
KgggggK.
KKKKKKK.
"""

# Cadeado grande (12x14).
LOCK = """
...KKKKKK...
..KggggggK..
.KgKKKKKKgK.
.KgK....KgK.
.KgK....KgK.
KKKKKKKKKKKK
KYyyyyyyyyYK
KYYYYYYYYYYK
KYYYYKKYYYYK
KYYYYKKYYYYK
KYYYYYKYYYYK
KYYYYYYYYYYK
KOOOOOOOOOOK
KKKKKKKKKKKK
"""


def swatch_icon(overlay: str | None) -> Image.Image:
    image = new(16, 16)
    paste(image, art(SWATCHES), 0, 1)
    if overlay is not None:
        mini = art(overlay)
        paste(image, mini, 16 - mini.width, 16 - mini.height)
    return image


# -----------------------------------------------------------------------------
# Ícones 16x16: arquivo, edição, exibição, paleta e ajuda
# -----------------------------------------------------------------------------
def file_new():
    return art("""
................
..KKKKKKKK......
..KwwwwwwKK.....
..KwwwwwwKlK....
..KwwwwwwKllK...
..KwwwwwwKKKKK..
..KwwwwwwwwwwK..
..KwwwwwwwwwwK..
..KwwwwwwwwwwK..
..KwwwwwwwwwwK..
..KwwwwwwwwwwK..
..KwwwwwwwwwKYK.
..KwwwwwwwwKYyYK
..KKKKKKKKKKKYK.
................
................
""", 16, 16)


def file_open():
    return art("""
................
................
..KKKK..........
.KYYYYK.........
.KYYYYYKKKKKK...
.KYYYYYYYYYYYK..
.KYYKKKKKKKKKKKK
.KYKyyyyyyyyyyyK
.KYKyyyyyyyyyyK.
.KKyyyyyyyyyyyK.
.KKyyyyyyyyyyK..
.KyyyyyyyyyyyK..
.KKKKKKKKKKKKK..
................
................
................
""", 16, 16)


def file_save():
    return art("""
................
.KKKKKKKKKKKKK..
.KBKwwwwwwwKBBK.
.KBKwwwwwwwKBBBK
.KBKwwwwwwwKBBBK
.KBKKKKKKKKKBBBK
.KBBBBBBBBBBBBBK
.KBBKKKKKKKKKBBK
.KBBKlllllllKBBK
.KBBKlKKKKKlKBBK
.KBBKlllllllKBBK
.KBBKlKKKKKlKBBK
.KBBKlllllllKBBK
.KKKKKKKKKKKKKKK
................
................
""", 16, 16)


def edit_undo():
    return art("""
................
................
.....K..........
....KBK.........
...KBBKKKKKKK...
..KBBBBBBBBBBK..
...KBBKKKKKKBBK.
....KBK.....KBBK
.....K.......KBK
.............KBK
............KBBK
.......KKKKKBBK.
.......KBBBBBK..
.......KKKKKK...
................
................
""", 16, 16)


def edit_redo():
    return mirror(edit_undo())


def edit_cut():
    return art("""
................
....K.....K.....
....KK...KK.....
....KlK.KlK.....
.....KlKlK......
.....KlKlK......
......KdK.......
......KdK.......
.....KK.KK......
....RRK.KRR.....
...R..R.R..R....
...R..R.R..R....
...R..R.R..R....
....RR...RR.....
................
................
""", 16, 16)


def edit_copy():
    return art("""
................
.KKKKKKK........
.KwwwwwK........
.KwllllK........
.KwwwwKKKKKKK...
.KwlllKwwwwwK...
.KwwwwKwllllK...
.KwlllKwwwwwK...
.KwwwwKwllllK...
.KKKKKKwwwwwK...
......KwllllK...
......KwwwwwK...
......KKKKKKK...
................
................
................
""", 16, 16)


def edit_paste():
    return art("""
................
.....KKKKK......
..KKKKgggKKKK...
..KOOKKKKKOOK...
..KOOOOOOOOOK...
..KOOKKKKKKKKK..
..KOOKwwwwwwwK..
..KOOKwllllwwK..
..KOOKwwwwwwwK..
..KOOKwlllllwK..
..KOOKwwwwwwwK..
..KOOKwllllwwK..
..KKKKwwwwwwwK..
.....KKKKKKKKK..
................
................
""", 16, 16)


def edit_draw_text():
    return art("""
................
.......KK.......
......KBBK......
......KBBK......
.....KBKKBK.....
.....KBKKBK.....
....KBK..KBK....
....KBKKKKBK....
...KBBBBBBBBK...
...KBKKKKKKBK...
..KBK......KBK..
..KBK......KBK..
.KKKK......KKKK.
................
................
................
""", 16, 16)


def grid_icon(step: int, line_key: str, cell_key: str | None = None) -> Image.Image:
    """Quadro 14x14 com linhas de grade a cada "step" pixels."""
    image = new(16, 16)
    shape(image, rect_inside(1, 1, 14, 14), "K", "w")
    for k in range(1 + step, 14, step):
        for t in range(2, 14):
            image.putpixel((k, t), color(line_key))
            image.putpixel((t, k), color(line_key))
    if cell_key is not None:  # célula destacada (canto superior esquerdo)
        for y in range(2, 1 + step):
            for x in range(2, 1 + step):
                image.putpixel((x, y), color(cell_key))
    return image


def view_main_grid():
    return grid_icon(3, "g")


def view_sub_grid():
    return grid_icon(6, "B", "b")


def view_grids():
    # Diálogo "Grades": grade fina + sub-grade.
    image = grid_icon(2, "l")
    for k in (7,):
        for t in range(2, 14):
            image.putpixel((k, t), color("B"))
            image.putpixel((t, k), color("B"))
    return image


def view_animation():
    image = art("""
................
................
KKKKKKKKKKKKKKKK
KwKwKwKwKwKwKwKK
KKKKKKKKKKKKKKKK
KbbbbKbbbbKbbbbK
KbbbbKbbbbKbRRbK
KbbbbKbRRbKbRRbK
KbbbbKbRRbKbbbbK
KRRbbKbbbbKbbbbK
KRRbbKbbbbKbbbbK
KKKKKKKKKKKKKKKK
KwKwKwKwKwKwKwKK
KKKKKKKKKKKKKKKK
................
................
""", 16, 16)
    return image


def canvas_resize():
    image = new(16, 16)
    # Moldura tracejada (tamanho novo) e quadro branco (tamanho atual).
    for t in range(1, 15):
        if t % 2:
            for x, y in ((t, 1), (t, 14), (1, t), (14, t)):
                image.putpixel((x, y), color("g"))
    shape(image, rect_inside(1, 1, 8, 8), "K", "w")
    paste(image, art("""
..KKKK
...KBK
..KBBK
.KBKBK
KBK.KK
.K....
"""), 9, 9)
    return image


def palette_load():
    return swatch_icon(MINI_FOLDER)


def palette_save():
    return swatch_icon(MINI_DISK)


def palette_edit():
    return swatch_icon(MINI_PENCIL)


def palette_mask_mode():
    return swatch_icon(MINI_LOCK)


def palette_transparent():
    image = new(16, 16)
    shape(image, rect_inside(1, 1, 14, 14), "K", "w")
    for y in range(2, 14):
        for x in range(2, 14):
            if ((x - 2) // 3 + (y - 2) // 3) % 2:
                image.putpixel((x, y), color("l"))
    # A "cor de fundo", que não é colada: quadrado vermelho riscado.
    shape(image, rect_inside(4, 4, 11, 11), "K", "R")
    line(image, 5, 10, 10, 5, "w")
    return image


def palette_gradation():
    image = new(16, 16)
    start, end = (35, 70, 150), (190, 225, 255)
    shape(image, rect_inside(0, 3, 15, 12), "K", None)
    for x in range(1, 15):
        t = (x - 1) / 13
        rgb = tuple(round(a + (b - a) * t) for a, b in zip(start, end))
        for y in range(4, 12):
            image.putpixel((x, y), rgb + (255,))
    return image


def help_contents():
    image = new(16, 16)
    shape(image, ellipse_inside(0, 0, 15, 15), "K", "B")
    paste(image, art("""
.www.
ww.ww
...ww
..ww.
..ww.
.....
..ww.
..ww.
"""), 5, 3)
    return image


def lock_icon(text: str) -> Image.Image:
    image = new(16, 16)
    paste(image, art(text), 2, 1)
    return image


def mask_auto():
    # Cadeado sem a fechadura, com um "A" (automática) gravado no corpo.
    image = lock_icon(LOCK.replace("KYYYYKKYYYYK", "KYYYYYYYYYYK").replace(
        "KYYYYYKYYYYK", "KYYYYYYYYYYK"))
    paste(image, art("""
YKKKY
KYYYK
KKKKK
KYYYK
KYYYK
"""), 6, 8)
    return image


def mask_clear():
    # Cadeado aberto: a haste sobe e solta de um lado.
    return lock_icon("""
....KKKKKK..
...KggggggK.
...KgKKKKgK.
...KgK..KgK.
...KgK..KgK.
...KgK......
KKKKKKKKKKKK
KYyyyyyyyyYK
KYYYYKKYYYYK
KYYYYKKYYYYK
KYYYYYKYYYYK
KYYYYYYYYYYK
KOOOOOOOOOOK
KKKKKKKKKKKK
""")


def mask_invert():
    # Dois cadeados pequenos trocando de estado: um fechado e um aberto.
    image = new(16, 16)
    paste(image, art(MINI_LOCK), 0, 0)
    paste(image, art(MINI_LOCK_OPEN), 8, 8)
    paste(image, art("""
..K
.KK
KKK
"""), 10, 2)
    paste(image, art("""
KKK
KK.
K..
"""), 2, 11)
    return image


# -----------------------------------------------------------------------------
# Ícones 20x20: ferramentas
# -----------------------------------------------------------------------------
def tool_select():
    image = new(20, 20)
    x0, y0, x1, y1 = 2, 3, 17, 16
    border = [(x, y0) for x in range(x0, x1 + 1)] + [(x1, y) for y in range(y0 + 1, y1 + 1)]
    border += [(x, y1) for x in range(x1 - 1, x0 - 1, -1)] + [(x0, y) for y in range(y1 - 1, y0, -1)]
    for index, point in enumerate(border):  # "formigas": tracejado preto e branco
        image.putpixel(point, color("K" if (index // 2) % 2 == 0 else "w"))
    return image


def tool_zoom():
    image = new(20, 20)
    shape(image, ellipse_inside(1, 1, 12, 12), "K", "b")
    shape(image, ellipse_inside(3, 3, 10, 10), "K", "b")
    for x, y in ((5, 5), (6, 5), (5, 6)):  # reflexo
        image.putpixel((x, y), color("w"))
    # Ponto interno do anel duplo: preenche o espaço entre as elipses.
    outer, inner = ellipse_inside(1, 1, 12, 12), ellipse_inside(3, 3, 10, 10)
    for y in range(20):
        for x in range(20):
            if outer(x, y) and not inner(x, y):
                edge = any(not outer(x + dx, y + dy) for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
                if not edge and image.getpixel((x, y)) != color("K"):
                    image.putpixel((x, y), color("g"))
    for k in range(11, 18):  # cabo
        for dx, key in ((-1, "K"), (0, "d"), (1, "d"), (2, "K")):
            if 0 <= k + dx < 20:
                image.putpixel((k + dx, k), color(key))
    image.putpixel((17, 18), color("K"))
    image.putpixel((18, 17), color("K"))
    return image


def tool_fill():
    return art("""
....................
....................
.......KKKK.........
......K....K........
.....K......K.......
....KKKKKKKKKKK.....
....KlwwwwwwwlKBB...
...KlwwwwwwwwlKBBK..
...KlllllllllgKBBBK.
...KllllllllggK.BBK.
...KlllllllgggK.BBK.
....KlllllgggK..BBK.
....KllllggggK...KK.
.....KllgggK........
.....KKKKKKK........
....................
...............b....
..............KBK...
..............KBK...
...............K....
""", 20, 20)


PENCIL = """
............KK...
...........KrrK..
..........KrrrrK.
.........KKrrrK..
........KYKKKK...
.......KYYYK.....
......KYYYK......
.....KYYYK.......
....KYYYK........
...KwYYK.........
...KwwK..........
..KwwK...........
..KKK............
..KK.............
"""


def tool_pen_thin():
    image = new(20, 20)
    paste(image, art(PENCIL), 2, 0)
    line(image, 2, 17, 4, 15, "B")
    line(image, 4, 15, 9, 17, "B")
    line(image, 9, 17, 16, 15, "B")
    return image


def tool_pen_thick():
    image = new(20, 20)
    paste(image, art(PENCIL), 2, 0)
    for dy in (0, 1, 2):
        line(image, 2, 16 + dy, 5, 14 + dy, "B")
        line(image, 5, 14 + dy, 10, 16 + dy, "B")
        line(image, 10, 16 + dy, 17, 14 + dy, "B")
    return image


def tool_line():
    image = new(20, 20)
    line(image, 3, 16, 16, 3, "K")
    for x, y in ((2, 16), (3, 17), (2, 17), (16, 2), (17, 3), (17, 2)):
        image.putpixel((x, y), color("B"))
    return image


def tool_rect():
    return shape(new(20, 20), rect_inside(2, 4, 17, 15), "K", None)


def tool_rect_filled():
    return shape(new(20, 20), rect_inside(2, 4, 17, 15), "K", "b")


def tool_round_rect():
    return shape(new(20, 20), round_rect_inside(2, 4, 17, 15, 3), "K", None)


def tool_round_rect_filled():
    return shape(new(20, 20), round_rect_inside(2, 4, 17, 15, 3), "K", "b")


def tool_ellipse():
    return shape(new(20, 20), ellipse_inside(1, 4, 18, 15), "K", None)


def tool_ellipse_filled():
    return shape(new(20, 20), ellipse_inside(1, 4, 18, 15), "K", "b")


# -----------------------------------------------------------------------------
# Ícones 20x20: transformações
# -----------------------------------------------------------------------------
ARROW_UP = """
....................
.........KK.........
........KBBK........
.......KBBBBK.......
......KBBBBBBK......
.....KBBBBBBBBK.....
....KBBBBBBBBBBK....
....KKKKBBBBKKKK....
.......KBBBBK.......
.......KBBBBK.......
.......KBBBBK.......
.......KbbbbK.......
.......KKKKKK.......
....................
...K.K.K.K.K.K.K....
....................
....................
....................
....................
....................
"""


def shift_up():
    return art(ARROW_UP, 20, 20)


def shift_right():
    return rotate_cw(shift_up())


def shift_down():
    return flip(shift_up())


def shift_left():
    return mirror(shift_right())


def axis_icon(bottom_fill: str | None, arrow: str) -> Image.Image:
    """Triângulo de cima e o seu reflexo embaixo, separados por um eixo."""
    image = new(20, 20)
    top = lambda x, y: 1 <= y <= 8 and abs(x - 9.5) <= (y - 1) * 0.95 + 0.5
    shape(image, top, "K", "B")
    shape(image, lambda x, y: top(x, 18 - y), "K", bottom_fill)
    for x in range(0, 20, 2):  # eixo tracejado
        image.putpixel((x, 9), color("g"))
    paste(image, art(arrow), 15, 4)
    return image


ARROW_BOTH = """
.K.
KBK
.B.
.B.
.B.
.B.
.B.
.B.
.B.
KBK
.K.
"""

ARROW_ONE = """
.B.
.B.
.B.
.B.
.B.
.B.
.B.
.B.
KBK
.K.
.K.
"""


def flip_vertical():
    return axis_icon(None, ARROW_BOTH)


def flip_horizontal():
    return rotate_cw(flip_vertical())


def mirror_vertical():
    return axis_icon("b", ARROW_ONE)


def mirror_horizontal():
    return mirror(rotate_cw(flip(mirror_vertical())))


ROTATE = """
....................
....................
.......KKKKKK.......
.....KKBBBBBBKK.....
....KBBBKKKKBBBK....
...KBBKK....KKBBK...
...KBK........KBK...
..KBBK........KBBK..
..KBK..........KBK..
..KBK..........KBK..
..KBK......KKKKBBBKK
..KBK.......KBBBBBK.
..KBBK.......KBBBK..
...KBK........KBK...
...KBBKK.......K....
....KBBBKKKK........
.....KKBBBBBK.......
.......KKKKK........
....................
....................
"""


def rotate_cw_icon():
    return art(ROTATE, 20, 20)


def rotate_ccw_icon():
    return mirror(rotate_cw_icon())


def rotate_free():
    image = rotate_cw_icon()
    paste(image, art("""
...K...
..KYK..
.KYYYK.
KYYyYYK
.KYYYK.
..KYK..
...K...
"""), 6, 6)
    return image


def stretch():
    image = new(20, 20)
    for t in range(1, 19):  # tamanho novo (tracejado)
        if t % 2:
            for x, y in ((t, 1), (t, 18), (1, t), (18, t)):
                image.putpixel((x, y), color("g"))
    shape(image, rect_inside(1, 1, 9, 9), "K", "b")
    paste(image, art("""
...KKKKK
....KBBK
...KBBBK
..KBBKBK
.KBBK.KK
KBBK....
.KK.....
"""), 10, 10)
    return image


# -----------------------------------------------------------------------------
# Tons (20x16): o padrão 4x4 real de cada tom, lido de src/core/Tone.h
# -----------------------------------------------------------------------------
def tone_patterns() -> list[list[int]]:
    text = TONE_HEADER.read_text(encoding="utf-8")
    rows = re.findall(r"\{\s*(0b[01]{4}),\s*(0b[01]{4}),\s*(0b[01]{4}),\s*(0b[01]{4})\s*\}", text)
    if len(rows) != 8:
        raise RuntimeError("não encontrei os 8 padrões de tom em Tone.h")
    return [[int(value, 2) for value in row] for row in rows]


def tone_icon(pattern: list[int], shift: int = 0) -> Image.Image:
    image = new(20, 16)
    shape(image, rect_inside(0, 0, 19, 15), "K", "w")
    for y in range(1, 15):
        for x in range(1, 19):
            row = pattern[(y - 1 - shift // 4) % 4]
            bit = 3 - (x - 1 - shift % 4) % 4
            if row >> bit & 1:
                image.putpixel((x, y), color("d"))
    return image


def tone_shift():
    image = tone_icon(tone_patterns()[4])
    paste(image, art("""
KKKKKKKKK
KwwwwwwwK
KwBwwwwwK
KwwBwwBwK
KwwwBwBwK
KwwwwBBwK
KwwBBBBwK
KwwwwwwwK
KKKKKKKKK
"""), 10, 6)
    return image


# -----------------------------------------------------------------------------
# Ícone do aplicativo: um "S" de blocos de pixel sobre uma grade, com um lápis
# -----------------------------------------------------------------------------
# "S" desenhado à mão para cada tamanho (traço contínuo de 3 e de 2 px).
S_GLYPHS = {
    32: [
        "....########..",
        "..###########.",
        ".####......###",
        "####..........",
        "###...........",
        "###...........",
        "####..........",
        ".#########....",
        "..##########..",
        "....#########.",
        "..........####",
        "...........###",
        "...........###",
        "..........####",
        "###......####.",
        ".###########..",
        "..########....",
    ],
    16: [
        ".######.",
        "##....##",
        "##......",
        "##......",
        ".#####..",
        "..#####.",
        "......##",
        "......##",
        "##....##",
        ".######.",
    ],
}


def app_icon(size: int) -> Image.Image:
    """Versões desenhadas: 16 (sem lápis e sem grade) e 32 (completa)."""
    image = new(size, size)
    radius = 3 if size == 16 else 5
    shape(image, round_rect_inside(0, 0, size - 1, size - 1, radius), "K", "N")
    glyph = S_GLYPHS[size]
    left, top = (4, 3) if size == 16 else (5, 6)
    if size == 32:  # grade de pixels ao fundo
        for k in range(1, size, 3):
            for t in range(2, size - 2):
                if image.getpixel((k, t)) == color("N"):
                    image.putpixel((k, t), color("n"))
                if image.getpixel((t, k)) == color("N"):
                    image.putpixel((t, k), color("n"))
    letter = {
        (left + column, top + row)
        for row, text in enumerate(glyph)
        for column, key in enumerate(text) if key == "#"
    }
    for x, y in letter:  # sombra projetada (1 px para baixo e para a direita)
        if (x + 1, y + 1) not in letter:
            image.putpixel((x + 1, y + 1), color("K"))
    for x, y in letter:  # borda inferior/direita em laranja dá volume
        edge = (x + 1, y) not in letter or (x, y + 1) not in letter
        image.putpixel((x, y), color("O" if edge else "Y"))
    if size == 32:
        paste(image, art("""
.......KK.
......KrrK
.....KKrrK
....KYKKK.
...KYYYK..
..KYYYK...
.KwYYK....
.KwwK.....
KKKK......
KK........
"""), 20, 19)
    return image


def app_icon_scaled(size: int) -> Image.Image:
    """Tamanhos maiores: ampliação inteira (sem suavização) da versão 32."""
    base = app_icon(16 if size < 32 else 32)
    return base.resize((size, size), Image.Resampling.NEAREST)


# -----------------------------------------------------------------------------
# Catálogo: nome lógico -> função (ver IconProvider e ActionCatalog)
# -----------------------------------------------------------------------------
ICONS = {
    "file_new": file_new,
    "file_open": file_open,
    "file_save": file_save,
    "edit_undo": edit_undo,
    "edit_redo": edit_redo,
    "edit_cut": edit_cut,
    "edit_copy": edit_copy,
    "edit_paste": edit_paste,
    "edit_draw_text": edit_draw_text,
    "view_grids": view_grids,
    "view_main_grid": view_main_grid,
    "view_sub_grid": view_sub_grid,
    "view_animation": view_animation,
    "canvas_resize": canvas_resize,
    "palette_load": palette_load,
    "palette_save": palette_save,
    "palette_edit": palette_edit,
    "palette_transparent": palette_transparent,
    "palette_mask_mode": palette_mask_mode,
    "palette_gradation": palette_gradation,
    "mask_clear": mask_clear,
    "mask_invert": mask_invert,
    "mask_auto": mask_auto,
    "help_contents": help_contents,
    "tool_select": tool_select,
    "tool_zoom": tool_zoom,
    "tool_fill": tool_fill,
    "tool_pen_thin": tool_pen_thin,
    "tool_pen_thick": tool_pen_thick,
    "tool_line": tool_line,
    "tool_rect": tool_rect,
    "tool_round_rect": tool_round_rect,
    "tool_ellipse": tool_ellipse,
    "tool_rect_filled": tool_rect_filled,
    "tool_round_rect_filled": tool_round_rect_filled,
    "tool_ellipse_filled": tool_ellipse_filled,
    "shift_up": shift_up,
    "shift_right": shift_right,
    "shift_down": shift_down,
    "shift_left": shift_left,
    "flip_vertical": flip_vertical,
    "flip_horizontal": flip_horizontal,
    "mirror_vertical": mirror_vertical,
    "mirror_horizontal": mirror_horizontal,
    "rotate_cw": rotate_cw_icon,
    "rotate_ccw": rotate_ccw_icon,
    "rotate_free": rotate_free,
    "stretch": stretch,
    "tone_shift": tone_shift,
    "app_icon": lambda: app_icon(32),
}
for _index, _letter in enumerate("abcdefgh"):
    ICONS[f"tone_{_letter}"] = (lambda i: lambda: tone_icon(tone_patterns()[i]))(_index)


def disabled(image: Image.Image) -> Image.Image:
    """Variante desabilitada: cinza claro e meio transparente."""
    result = new(*image.size)
    for y in range(image.height):
        for x in range(image.width):
            r, g, b, a = image.getpixel((x, y))
            if a == 0:
                continue
            luminance = 0.299 * r + 0.587 * g + 0.114 * b
            gray = round(luminance * 0.45 + 140)
            result.putpixel((x, y), (gray, gray, gray, a * 6 // 10))
    return result


def preview(images: dict[str, Image.Image], path: Path, scale: int = 4) -> None:
    """Prancha com todos os ícones ampliados (normal e desabilitado)."""
    columns = 8
    cell = max(max(image.width, image.height) for image in images.values()) * scale + 8
    rows = (len(images) + columns - 1) // columns
    sheet = Image.new("RGBA", (columns * cell * 2, rows * (cell + 14)), (236, 238, 244, 255))
    draw = ImageDraw.Draw(sheet)
    for index, (name, image) in enumerate(images.items()):
        x = (index % columns) * cell * 2
        y = (index // columns) * (cell + 14)
        for offset, variant in ((0, image), (cell, disabled(image))):
            big = variant.resize((variant.width * scale, variant.height * scale),
                                 Image.Resampling.NEAREST)
            sheet.alpha_composite(big, (x + offset + 4, y + 4))
        draw.text((x + 4, y + image.height * scale + 8), name, fill=(40, 40, 60, 255))
    sheet.save(path)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[1])
    parser.add_argument("--preview", type=Path, help="grava também uma prancha ampliada")
    arguments = parser.parse_args()

    images = {name: build() for name, build in ICONS.items()}

    ICONS_DIR.mkdir(parents=True, exist_ok=True)
    for old in ICONS_DIR.glob("*.png"):  # remove ícones que não existem mais
        old.unlink()
    for name, image in images.items():
        image.save(ICONS_DIR / f"{name}.png")
        if name != "app_icon":
            disabled(image).save(ICONS_DIR / f"{name}_disabled.png")

    # Ícone do aplicativo em vários tamanhos (Windows, macOS e Linux).
    sizes = [16, 32, 48, 64, 128, 256]
    app = {size: app_icon(size) if size in (16, 32) else app_icon_scaled(size) for size in sizes}
    app[256].save(REPO_ROOT / "resources" / "app_icon.ico",
                  sizes=[(s, s) for s in sizes], append_images=[app[s] for s in sizes[:-1]])
    icns_sizes = [16, 32, 64, 128, 256, 512, 1024]
    icns = {s: app.get(s) or app_icon_scaled(s) for s in icns_sizes}
    icns[1024].save(REPO_ROOT / "resources" / "app_icon.icns",
                    append_images=[icns[s] for s in icns_sizes[:-1]])
    linux_dir = REPO_ROOT / "resources" / "linux" / "icons"
    linux_dir.mkdir(parents=True, exist_ok=True)
    for size in sizes:
        app[size].save(linux_dir / f"{size}.png")

    if arguments.preview:
        preview(images, arguments.preview)
    print(f"{len(images)} ícones gerados em {ICONS_DIR.relative_to(REPO_ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
