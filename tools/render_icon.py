#!/usr/bin/env python3
from PIL import Image, ImageDraw, ImageFont, ImageFilter
import os

OUT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "resources", "icons"))


def rounded(size, radius, color):
    im = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((0, 0, size - 1, size - 1), radius=radius, fill=color)
    return im


def lerp(c1, c2, t):
    return tuple(int(a + (b - a) * t) for a, b in zip(c1, c2))


def render(size):
    s = float(size)
    im = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    # blue bezel via gradient-ish fill
    bezel = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    bd = ImageDraw.Draw(bezel)
    r = max(3, int(round(22 * s / 128)))
    for y in range(size):
        t = y / max(1, size - 1)
        c = lerp((26, 109, 255, 255), (11, 61, 145, 255), t)
        bd.line([(0, y), (size - 1, y)], fill=c)
    mask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, size - 1, size - 1), radius=r, fill=255)
    bezel.putalpha(mask)
    im.alpha_composite(bezel)

    inset = max(2, int(round(5 * s / 128)))
    inner_r = max(2, int(round(18 * s / 128)))
    body = (inset, inset, size - 1 - inset, size - 1 - inset)
    d = ImageDraw.Draw(im)
    d.rounded_rectangle(body, radius=inner_r, fill=(11, 18, 32, 255))

    title_h = max(8, int(round(22 * s / 128)))
    # title bar: rounded top + rectangle to flatten bottom
    d.rounded_rectangle(
        (inset, inset, size - 1 - inset, inset + title_h + inner_r),
        radius=inner_r,
        fill=(22, 50, 92, 255),
    )
    d.rectangle(
        (inset, inset + title_h // 2, size - 1 - inset, inset + title_h),
        fill=(22, 50, 92, 255),
    )
    d.rounded_rectangle(
        (inset, inset + title_h, size - 1 - inset, size - 1 - inset),
        radius=inner_r,
        fill=(11, 18, 32, 255),
    )
    # keep title strip
    d.rectangle(
        (inset, inset + max(1, title_h - 2), size - 1 - inset, inset + title_h),
        fill=(22, 50, 92, 255),
    )

    cy = inset + title_h // 2
    cr = max(2, int(round(4 * s / 128)))
    gap = max(3, int(round(12 * s / 128)))
    cx0 = inset + max(6, int(round(15 * s / 128)))
    for i, col in enumerate(((255, 95, 86, 255), (255, 189, 46, 255), (39, 201, 63, 255))):
        d.ellipse((cx0 + i * gap - cr, cy - cr, cx0 + i * gap + cr, cy + cr), fill=col)

    # text sits in the black pane, not the title bar
    body_top = inset + title_h
    body_h = size - inset - body_top
    text_y = body_top + int(body_h * 0.52)
    dollar_x = inset + max(6, int(round(10 * s / 128)))
    lin_x = dollar_x + max(12, int(round(22 * s / 128)))
    font_d = max(9, int(round(26 * s / 128)))
    font_l = max(9, int(round(24 * s / 128)))
    font = None
    for name in ("consola.ttf", "CascadiaMono.ttf", "cour.ttf", "arial.ttf"):
        try:
            font = ImageFont.truetype(name, font_d)
            font2 = ImageFont.truetype("segoeui.ttf" if name != "arial.ttf" else "arialbd.ttf", font_l)
            break
        except OSError:
            font = None
    if font is None:
        font = ImageFont.load_default()
        font2 = font
    d.text((dollar_x, text_y - font_d), "$", fill=(126, 231, 135, 255), font=font)
    d.text((lin_x, text_y - font_l), "Lin", fill=(230, 237, 243, 255), font=font2)

    # underscore
    y = min(size - inset - max(4, sw := max(2, int(round(5 * s / 128)))), text_y + max(8, int(round(16 * s / 128))))
    x1 = dollar_x
    x2 = dollar_x + max(28, int(round(52 * s / 128)))
    d.line((x1, y, x2, y), fill=(26, 109, 255, 255), width=sw)
    return im


def main():
    images = []
    for size, name in ((128, "linhub.png"), (64, "linhub-64.png"), (48, "linhub-48.png")):
        im = render(size)
        path = os.path.join(OUT, name)
        im.save(path, "PNG")
        print("wrote", path, os.path.getsize(path))
        images.append(im)
    ico = os.path.join(OUT, "linhub.ico")
    icons = [render(16), render(32), render(48), render(64), render(128)]
    icons[-1].save(ico, format="ICO", append_images=icons[:-1],
                   sizes=[(16, 16), (32, 32), (48, 48), (64, 64), (128, 128)])
    print("wrote", ico, os.path.getsize(ico))


if __name__ == "__main__":
    main()
