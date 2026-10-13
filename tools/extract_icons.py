#!/usr/bin/env python3
"""
extract_icons.py - Gera os ícones provisórios do Spritenator a partir dos
bitmaps extraídos do Character Maker 1999.

As ImageLists do Delphi foram exportadas como dois bitmaps: as cores
(``*.Bitmap.0.bmp``) e a máscara monocromática (``*.Bitmap.1.bmp``, onde
branco = transparente). Este script recorta cada célula, aplica a máscara
como canal alfa e grava um PNG com nome lógico em ``resources/icons``.

Os ícones são PROVISÓRIOS: a ideia é substituí-los futuramente por arte
própria. Para isso basta sobrescrever os PNGs mantendo os mesmos nomes; o
código da aplicação referencia apenas os nomes lógicos (ver IconProvider).

Uso:
    python3 tools/extract_icons.py [diretório_assets] [diretório_saída]
"""

from __future__ import annotations

import sys
from pathlib import Path

from PIL import Image

# Raiz do repositório (este arquivo fica em <raiz>/tools/).
REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_ASSETS = REPO_ROOT / "original" / "extracted" / "forms" / "assets"
DEFAULT_OUTPUT = REPO_ROOT / "resources" / "icons"

# Cada folha: (prefixo do arquivo, largura da célula, altura da célula,
#              colunas, lista de nomes lógicos na ordem do ImageIndex,
#              sufixo opcional acrescentado ao nome — p.ex. "_disabled").
SHEETS = [
    # Barra de ferramentas principal (ícones habilitados).
    ("TCharacterRaiserMainForm.ImageListEx1", 16, 16, 4, [
        "file_new", "file_open", "file_save", "edit_undo",
        "edit_redo", "edit_cut", "edit_copy", "edit_paste",
        "view_chara_size", "view_main_grid", "view_sub_grid", "zoom_1",
        "zoom_2", "zoom_4", "zoom_6", "zoom_8",
        "view_animation", "edit_draw_text", "canvas_resize", "palette_load",
        "palette_save", "palette_edit", "palette_transparent", "help_contents",
        "zoom_12", "mask_clear", "mask_invert", "mask_auto",
    ], ""),
    # Mesma folha, versão desabilitada (acinzentada).
    ("TCharacterRaiserMainForm.ImageListEx2", 16, 16, 4, [
        "file_new", "file_open", "file_save", "edit_undo",
        "edit_redo", "edit_cut", "edit_copy", "edit_paste",
        "view_chara_size", "view_main_grid", "view_sub_grid", "zoom_1",
        "zoom_2", "zoom_4", "zoom_6", "zoom_8",
        "view_animation", "edit_draw_text", "canvas_resize", "palette_load",
        "palette_save", "palette_edit", "palette_transparent", "help_contents",
        "zoom_12", "mask_clear", "mask_invert", "mask_auto",
    ], "_disabled"),
    # Ferramentas de desenho do Integrator.
    ("TIntegrator.ImageListEx3", 20, 20, 4, [
        "tool_select", "tool_zoom", "tool_fill", "tool_pen_thin",
        "tool_pen_thick", "tool_line", "tool_rect", "tool_round_rect",
        "tool_ellipse", "tool_rect_filled", "tool_round_rect_filled",
        "tool_ellipse_filled",
    ], ""),
    ("TIntegrator.ImageListEx4", 20, 20, 4, [
        "tool_select", "tool_zoom", "tool_fill", "tool_pen_thin",
        "tool_pen_thick", "tool_line", "tool_rect", "tool_round_rect",
        "tool_ellipse", "tool_rect_filled", "tool_round_rect_filled",
        "tool_ellipse_filled",
    ], "_disabled"),
    # Padrões de tom (retícula) A..H e o botão de deslocamento de tom.
    ("TIntegrator.ImageListEx5", 20, 16, 4, [
        "tone_a", "tone_b", "tone_c", "tone_d",
        "tone_e", "tone_f", "tone_g", "tone_h",
        "tone_shift",
    ], ""),
    # Transformações da área de desenho.
    ("TIntegrator.ImageListEx6", 20, 20, 4, [
        "shift_up", "shift_right", "shift_down", "shift_left",
        "flip_vertical", "flip_horizontal", "mirror_vertical", "mirror_horizontal",
        "rotate_cw", "rotate_ccw", "rotate_free", "stretch",
    ], ""),
    ("TIntegrator.ImageListEx7", 20, 20, 4, [
        "shift_up", "shift_right", "shift_down", "shift_left",
        "flip_vertical", "flip_horizontal", "mirror_vertical", "mirror_horizontal",
        "rotate_cw", "rotate_ccw", "rotate_free", "stretch",
    ], "_disabled"),
]

# Glifos de TSpeedButton: o Delphi usa a cor do pixel inferior esquerdo como
# cor transparente. Cada arquivo traz 2 glifos lado a lado (normal/desabilitado).
GLYPHS = [
    ("TIntegrator.SpeedButtonGradation.Glyph.Data.bmp", "palette_gradation"),
    ("TIntegrator.SpeedButtonMask.Glyph.Data.bmp", "palette_mask_mode"),
]


def extract_sheet(assets: Path, output: Path, prefix: str, cell_w: int,
                  cell_h: int, columns: int, names: list[str], suffix: str) -> int:
    """Recorta uma ImageList inteira; devolve a quantidade de ícones gravados."""
    colors = Image.open(assets / f"{prefix}.Bitmap.0.bmp").convert("RGB")
    mask = Image.open(assets / f"{prefix}.Bitmap.1.bmp").convert("L")

    # Na máscara do Windows, branco (255) = transparente; invertemos para alfa.
    alpha = mask.point(lambda value: 0 if value > 127 else 255)
    rgba = colors.copy()
    rgba.putalpha(alpha)

    for index, name in enumerate(names):
        left = (index % columns) * cell_w
        top = (index // columns) * cell_h
        tile = rgba.crop((left, top, left + cell_w, top + cell_h))
        tile.save(output / f"{name}{suffix}.png")
    return len(names)


def extract_glyph(assets: Path, output: Path, file_name: str, name: str) -> None:
    """Separa um glifo duplo de TSpeedButton em versões normal e desabilitada."""
    glyph = Image.open(assets / file_name).convert("RGBA")
    width, height = glyph.size
    key = glyph.getpixel((0, height - 1))  # cor transparente do Delphi

    # Zera o alfa de todo pixel com a cor-chave (acesso direto, pixel a pixel).
    access = glyph.load()
    for y in range(height):
        for x in range(width):
            if access[x, y] == key:
                r, g, b, _ = access[x, y]
                access[x, y] = (r, g, b, 0)

    half = width // 2
    glyph.crop((0, 0, half, height)).save(output / f"{name}.png")
    glyph.crop((half, 0, width, height)).save(output / f"{name}_disabled.png")


def main(argv: list[str]) -> int:
    assets = Path(argv[1]) if len(argv) > 1 else DEFAULT_ASSETS
    output = Path(argv[2]) if len(argv) > 2 else DEFAULT_OUTPUT
    output.mkdir(parents=True, exist_ok=True)

    total = 0
    for prefix, cell_w, cell_h, columns, names, suffix in SHEETS:
        total += extract_sheet(assets, output, prefix, cell_w, cell_h,
                               columns, names, suffix)
    for file_name, name in GLYPHS:
        extract_glyph(assets, output, file_name, name)
        total += 2

    # Ícone da aplicação (janela "Sobre" do original): PNG para a interface e
    # .ico (cópia do original, com 16x16 e 32x32) para o executável do Windows.
    icon_path = assets / "TFormAbout.Image1.Picture.Data.ico"
    Image.open(icon_path).save(output / "app_icon.png")
    (output.parent / "app_icon.ico").write_bytes(icon_path.read_bytes())
    total += 2

    print(f"{total} ícones gravados em {output}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
