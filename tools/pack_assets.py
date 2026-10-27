#!/usr/bin/env python3
"""
Empacota os sprites do zip original em texturas leves (RGBA5551) para o PSP.

Uso:
    python3 tools/pack_assets.py "<pasta_do_zip_extraido>" data

Gera em data/:
    chr_N.dat   atlas + tabela de animações de cada lutador
    port_N.dat  retrato 128x128 (tela de seleção)
    stage_N.dat céu 256x256 (RGB565) + chão 512x256 (RGBA5551)
    font.dat    fonte 256x128 (RGBA4444)
    ICON0.png   ícone do jogo no XMB
Requer: Pillow
"""
import os, re, sys, glob, struct, hashlib, collections
from PIL import Image, ImageDraw, ImageFont

SRC = sys.argv[1] if len(sys.argv) > 1 else "."
OUT = sys.argv[2] if len(sys.argv) > 2 else "data"
os.makedirs(OUT, exist_ok=True)

PAGE = 512
NANIM = 16
(A_IDLE, A_WALK, A_JUMP, A_GUARD, A_DASH, A_ATK1, A_ATK2, A_ATK3, A_BLAST,
 A_SUPER, A_HIT, A_HIT2, A_FALL, A_LIE, A_WIN, A_AIR) = range(NANIM)

# (nome, pasta, cor do ki 0xAABBGGRR é feita no C; aqui só pasta)
SHEETS = "sprites 3ds"
SHEET_PREFIX = "3DS - Dragon Ball Z_ Extreme Butoden - Fighters - "
SHEET_CHARS = [
    ("Goku", "Goku"), ("Goku SSJ", "Goku (Super Saiyan)"),
    ("Goku SSJ God", "Goku (Super Saiyan God)"), ("Goku SSJ Blue", "Goku (Super Saiyan Blue)"),
    ("Vegeta", "Vegeta"), ("Vegeta SSJ", "Vegeta (Super Saiyan)"),
    ("Teen Gohan", "Teen Gohan"), ("Gohan SSJ2", "Teen Gohan (Super Saiyan 2)"),
    ("Gohan Adulto", "Young Adult Gohan"), ("Future Trunks", "Future Trunks"),
    ("Piccolo", "Piccolo"), ("Krillin", "Krillin"), ("Frieza", "Frieza"),
    ("Cell", "Cell"), ("Majin Buu", "Mister Buu"), ("Android 18", "Android 18"),
    ("Nappa", "Nappa"), ("Raditz", "Raditz"), ("Bardock", "Bardock"),
    ("Broly", "Broly"), ("Beerus", "Beerus"),
]
MUGEN_CHARS = [
    ("Evil Goku",   "Evil Goku EB"),
    ("Goku Black",  "Goku Black EB"),
    ("Vegetto",     "Vegetto EB/Vegetto Blue EB"),
    ("Goku SSJ4",   "Goku GT EB/Goku SSJ4 EB"),
    ("Ize",         "Ize AF EB"),
    ("Kefla",       "Kefla EB"),
    ("Hitto",       "Hitto EB"),
]

# anim: (id, [grupos candidatos], max_frames, ticks_fixos | duração_alvo, exclusivo)
# ticks: se int -> ticks por frame; se ("dur", N) -> N frames de duração total
ANIMS = [
    (A_IDLE,  [0, 1],                          8, 8,           False),
    (A_WALK,  [20, 21, 11, 0],                 8, 6,           False),
    (A_JUMP,  [40, 41, 42, 0],                 8, 6,           False),
    (A_GUARD, [121, 120, 122, 130, 110, 0],    3, 6,           False),
    (A_DASH,  [100, 105, 110, 20, 0],          3, 4,           False),
    (A_ATK1,  [200, 210, 230, 201, 300],       8, ("dur", 20), True),
    (A_ATK2,  [205, 210, 220, 230, 240, 250, 201, 1000], 8, ("dur", 26), True),
    (A_ATK3,  [250, 300, 310, 220, 400, 410, 230, 240, 1100], 8, ("dur", 32), True),
    (A_BLAST, [1500, 1200, 1000, 6001, 6000, 1610, 1100, 1300, 700, 2010, 720, 800, 901], 8, ("dur", 30), False),
    (A_SUPER, [3700, 3000, 3705, 3610, 1700, 6002, 2000, 1200, 1500, 1100, 700, 901, 800, 2010], 9, ("dur", 72), False),
    (A_HIT,   [5000, 5010, 5020, 5030],        3, 6,           False),
    (A_HIT2,  [5030, 5020, 5010, 5000],        3, 6,           False),
    (A_FALL,  [5040, 5120, 5030, 5001, 5000],  5, 8,           False),
    (A_LIE,   [5300, 5120, 5040, 5000],        4, 10,          False),
    (A_WIN,   [170, 180, 190, 191, 0],         8, 8,           False),
    (A_AIR,   [610, 600, 620, 640, 630, 650, 200], 6, ("dur", 22), False),
]
ANIM_NAMES = ["IDLE", "WALK", "JUMP", "GUARD", "DASH", "ATK1", "ATK2", "ATK3",
              "BLAST", "SUPER", "HIT", "HIT2", "FALL", "LIE", "WIN", "AIR"]


def scan_groups(folder):
    groups = collections.defaultdict(list)
    for f in glob.glob(os.path.join(folder, "*.png")):
        m = re.match(r"^(\d+)-(\d+)\.png$", os.path.basename(f))
        if m:
            groups[int(m.group(1))].append((int(m.group(2)), f))
    for g in groups:
        groups[g].sort()
    return groups


def to5551(im):
    """RGBA -> bytes RGBA5551 (formato GU_PSM_5551 do PSP, little endian)."""
    im = im.convert("RGBA")
    px = im.tobytes()
    out = bytearray(len(px) // 2)
    j = 0
    for i in range(0, len(px), 4):
        r, g, b, a = px[i], px[i + 1], px[i + 2], px[i + 3]
        if a >= 128:
            v = (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10) | 0x8000
        else:
            v = 0
        out[j] = v & 255
        out[j + 1] = v >> 8
        j += 2
    return bytes(out)


def to565(im):
    im = im.convert("RGB")
    px = im.tobytes()
    out = bytearray(len(px) // 3 * 2)
    j = 0
    for i in range(0, len(px), 3):
        r, g, b = px[i], px[i + 1], px[i + 2]
        v = (r >> 3) | ((g >> 2) << 5) | ((b >> 3) << 11)
        out[j] = v & 255
        out[j + 1] = v >> 8
        j += 2
    return bytes(out)


def to4444(im):
    im = im.convert("RGBA")
    px = im.tobytes()
    out = bytearray(len(px) // 2)
    j = 0
    for i in range(0, len(px), 4):
        r, g, b, a = px[i], px[i + 1], px[i + 2], px[i + 3]
        v = (r >> 4) | ((g >> 4) << 4) | ((b >> 4) << 8) | ((a >> 4) << 12)
        out[j] = v & 255
        out[j + 1] = v >> 8
        j += 2
    return bytes(out)


class Atlas:
    def __init__(self):
        self.pages = []      # lista de Image RGBA PAGExPAGE
        self.shelves = []    # por página: lista [y, h, xcursor]
        self.cache = {}      # hash -> (page,x,y,w,h)

    def _new_page(self):
        self.pages.append(Image.new("RGBA", (PAGE, PAGE), (0, 0, 0, 0)))
        self.shelves.append([])

    def add(self, im):
        im = im.convert("RGBA")
        bb = im.getchannel("A").point(lambda v: 255 if v >= 128 else 0).getbbox()
        if bb is None:
            im = Image.new("RGBA", (2, 2), (0, 0, 0, 0))
            bb = (0, 0, 2, 2)
        else:
            im = im.crop(bb)
        w, h = im.size
        # ancora X = centro de massa horizontal dos pixels opacos
        a = im.getchannel("A").point(lambda v: 1 if v >= 128 else 0)
        cols = [0] * w
        pa = a.load()
        for yy in range(h):
            for xx in range(w):
                cols[xx] += pa[xx, yy]
        tot = sum(cols) or 1
        cx = sum(i * c for i, c in enumerate(cols)) / tot + 0.5
        ax = int(round(min(max(cx, w * 0.25), w * 0.75)))
        key = hashlib.md5(im.tobytes() + bytes(str(im.size), "ascii")).hexdigest()
        if key in self.cache:
            return self.cache[key]
        pw, ph = w + 1, h + 1
        assert pw <= PAGE and ph <= PAGE, "frame grande demais"
        for pi in range(len(self.pages)):
            for sh in self.shelves[pi]:
                if sh[1] >= ph and sh[2] + pw <= PAGE and sh[1] - ph <= 24:
                    x, y = sh[2], sh[0]
                    sh[2] += pw
                    self.pages[pi].paste(im, (x, y))
                    r = (pi, x, y, w, h, ax)
                    self.cache[key] = r
                    return r
            used = sum(s_[1] for s_ in self.shelves[pi])
            if used + ph <= PAGE:
                y = used
                self.shelves[pi].append([y, ph, pw])
                self.pages[pi].paste(im, (0, y))
                r = (pi, 0, y, w, h, ax)
                self.cache[key] = r
                return r
        self._new_page()
        self.shelves[-1].append([0, ph, pw])
        self.pages[-1].paste(im, (0, 0))
        r = (len(self.pages) - 1, 0, 0, w, h, ax)
        self.cache[key] = r
        return r


def write_character(idx, name, anim_images, portrait=None):
    """anim_images: aid -> lista de PIL.Image (ou None)."""
    idle = anim_images[A_IDLE]
    assert idle, f"{name}: sem frames de idle"
    atlas = Atlas()
    recs = []
    anims = []
    for (aid, chain, maxf, ticks, excl) in ANIMS:
        imgs = anim_images[aid] or idle[:1]
        start = len(recs)
        for im in imgs:
            recs.append(atlas.add(im))
        n = len(imgs)
        if isinstance(ticks, tuple):
            t = max(2, min(12, round(ticks[1] / n)))
        else:
            t = ticks
        anims.append((start, n, t))
    npages = len(atlas.pages)
    meta_len = 8 + NANIM * 4 + len(recs) * 12
    pad = (-meta_len) % 64
    blob = bytearray()
    blob += b"DBCH" + struct.pack("<HH", npages, len(recs))
    for (s_, n, t) in anims:
        blob += struct.pack("<HBB", s_, n, t)
    for (pi, x, y, w, h, ax) in recs:
        blob += struct.pack("<HHHHHH", pi, x, y, w, h, ax)
    blob += b"\0" * pad
    for pg in atlas.pages:
        blob += to5551(pg)
    with open(os.path.join(OUT, f"chr_{idx}.dat"), "wb") as fo:
        fo.write(blob)
    if portrait is None:
        im = idle[0].convert("RGBA")
        bb = im.getchannel("A").getbbox()
        if bb: im = im.crop(bb)
        sc = min(1.0, 124 / im.width, 124 / im.height)
        if sc < 1.0:
            im = im.resize((max(1, int(im.width * sc)), max(1, int(im.height * sc))), Image.NEAREST)
        portrait = Image.new("RGBA", (128, 128), (0, 0, 0, 0))
        portrait.paste(im, ((128 - im.width) // 2, 126 - im.height), im)
    with open(os.path.join(OUT, f"port_{idx}.dat"), "wb") as fo:
        fo.write(to5551(portrait))
    print(f"[{idx:2d}] {name:<14} frames={len(recs):3d} paginas={npages} ({len(blob)//1024} KB)")


def pack_mugen(idx, name, folder):
    groups = scan_groups(os.path.join(SRC, folder))
    used = set()
    anim_images = {}
    for (aid, chain, maxf, ticks, excl) in ANIMS:
        chosen = None
        for g in chain:
            if g in groups and (not excl or g not in used):
                chosen = g
                break
        if chosen is None:
            anim_images[aid] = None
            continue
        if excl:
            used.add(chosen)
        anim_images[aid] = [Image.open(f).convert("RGBA") for (_, f) in groups[chosen]][:maxf]
    write_character(idx, name, anim_images)
    return anim_images


def key_green(im):
    im = im.convert("RGBA")
    px = im.load()
    for y in range(im.height):
        for x in range(im.width):
            r, g, b, a = px[x, y]
            if r <= 24 and g >= 232 and 50 <= b <= 115:
                px[x, y] = (0, 0, 0, 0)
    return im


def pack_sheet(idx, name, fname):
    from scipy import ndimage
    import numpy as np
    path = os.path.join(SRC, SHEETS, SHEET_PREFIX + fname + ".png")
    im = Image.open(path).convert("RGBA")
    a = np.array(im)
    bg = (a[:, :, 0] == 0) & (a[:, :, 1] == 128) & (a[:, :, 2] == 128)
    lab, _ = ndimage.label(~bg)
    cells = []
    for sl in ndimage.find_objects(lab):
        y0, y1, x0, x1 = sl[0].start, sl[0].stop, sl[1].start, sl[1].stop
        if (x1 - x0) * (y1 - y0) >= 900:
            cells.append((y0, x0, y1, x1))
    cells.sort()
    rows = []
    for c in cells:
        if rows and abs(rows[-1][0][0] - c[0]) <= 3:
            rows[-1].append(c)
        else:
            rows.append([c])
    for r in rows:
        r.sort(key=lambda t: t[1])
    # acha o bloco "3,3,3" (guarda/agachar) -> linha de pulo = j+3
    lens = [len(r) for r in rows]
    j = next(i for i in range(4, len(rows) - 3) if lens[i] == 3 and lens[i + 1] == 3 and lens[i + 2] == 3)
    J = j + 3

    def row(i, maxf):
        if i < 1 or i >= len(rows):
            return None
        out = []
        for (y0, x0, y1, x1) in rows[i][:maxf]:
            out.append(key_green(im.crop((x0, y0, x1, y1))))
        return out

    idle_i = 2 if (j == 5 and lens[2] >= 3) else J - 4
    m = {
        A_IDLE: row(idle_i, 8), A_WALK: row(J - 4, 8), A_JUMP: row(J, 8), A_GUARD: row(J - 3, 3),
        A_DASH: row(J + 10, 3), A_ATK1: row(J + 6, 8), A_ATK2: row(J + 8, 8), A_ATK3: row(J + 7, 8),
        A_BLAST: row(J + 17, 8), A_SUPER: row(J + 25, 9), A_HIT: row(J + 1, 3), A_HIT2: row(J + 3, 3),
        A_FALL: row(J + 2, 5), A_LIE: row(J + 4, 4), A_WIN: row(J + 5, 8), A_AIR: row(J + 12, 6),
    }
    # retrato: close do rosto (celula 1 da primeira linha)
    pc = rows[0][1] if len(rows[0]) > 1 else rows[0][0]
    face = im.crop((pc[1], pc[0], pc[3], pc[2])).convert("RGBA")
    side = min(face.width, face.height)
    face = face.crop(((face.width - side) // 2, 0, (face.width - side) // 2 + side, side)).resize((128, 128), Image.LANCZOS)
    write_character(idx, name, m, face)
    return m


STAGES = [1, 2, 3, 5, 6, 7, 8, 10]


def pack_stages():
    fn = None
    for f in glob.glob(os.path.join(SRC, "*Stage Backgrounds.png")):
        fn = f
    if not fn:
        print("AVISO: sheet de cenários não encontrado")
        return
    sheet = Image.open(fn).convert("RGB")
    for n, k in enumerate(STAGES):
        y0 = 2 + 258 * k
        ground = sheet.crop((2, y0, 514, y0 + 256)).convert("RGBA")
        gp = ground.load()
        for y in range(256):
            for x in range(512):
                r, g, b, a = gp[x, y]
                if (r, g, b) == (255, 0, 255):
                    gp[x, y] = (0, 0, 0, 0)
        sky = sheet.crop((516, y0, 772, y0 + 256))
        with open(os.path.join(OUT, f"stage_{n}.dat"), "wb") as fo:
            fo.write(to565(sky))
            fo.write(to5551(ground))
    print(f"{len(STAGES)} cenários empacotados")


def pack_font():
    try:
        font = ImageFont.load_default(size=14)
    except TypeError:
        font = ImageFont.load_default()
    atlas = Image.new("RGBA", (256, 128), (0, 0, 0, 0))
    d = ImageDraw.Draw(atlas)
    for c in range(32, 128):
        i = c - 32
        cx, cy = (i % 16) * 16, (i // 16) * 16
        # contorno + preenchimento branco
        ch = chr(c)
        d.text((cx + 1, cy), ch, font=font, fill=(255, 255, 255, 255))
    with open(os.path.join(OUT, "font.dat"), "wb") as fo:
        fo.write(to4444(atlas))
    atlas.save(os.path.join(OUT, "_font_preview.png"))
    print("fonte ok")


def pack_icon(first_frames):
    sheet = None
    for f in glob.glob(os.path.join(SRC, "*Stage Backgrounds.png")):
        sheet = Image.open(f).convert("RGB")
    icon = Image.new("RGB", (144, 80), (20, 20, 60))
    if sheet:
        k = 2
        y0 = 2 + 258 * k
        sky = sheet.crop((516, y0, 772, y0 + 256)).resize((144, 80))
        icon.paste(sky, (0, 0))
        ground = sheet.crop((2, y0, 514, y0 + 256)).convert("RGBA")
        gp = ground.load()
        for y in range(256):
            for x in range(512):
                if gp[x, y][:3] == (255, 0, 255):
                    gp[x, y] = (0, 0, 0, 0)
        g2 = ground.resize((int(512 * 0.5), 128), Image.NEAREST)
        icon.paste(g2.crop((0, 40, 144, 120)), (0, 0), g2.crop((0, 40, 144, 120)))
    icon = icon.convert("RGBA")
    def put(im, x, flip):
        im = im.convert("RGBA")
        bb = im.getchannel("A").getbbox()
        if bb: im = im.crop(bb)
        s = 62 / im.height
        im = im.resize((int(im.width * s), 62), Image.NEAREST)
        if flip:
            im = im.transpose(Image.FLIP_LEFT_RIGHT)
        icon.paste(im, (x - im.width // 2, 76 - im.height), im)
    put(first_frames[0][A_IDLE][0], 38, False)
    put(first_frames[4][A_IDLE][0], 106, True)
    d = ImageDraw.Draw(icon)
    try:
        f = ImageFont.load_default(size=13)
    except TypeError:
        f = ImageFont.load_default()
    d.text((73, 4), "DRAGON BALL", font=f, fill=(255, 220, 40, 255), anchor="mt",
           stroke_width=2, stroke_fill=(120, 30, 0, 255))
    icon.convert("RGB").save(os.path.join(OUT, "ICON0.png"))
    print("ICON0.png ok")


def main():
    frames = []
    idx = 0
    for (name, fname) in SHEET_CHARS:
        frames.append(pack_sheet(idx, name, fname))
        idx += 1
    for (name, folder) in MUGEN_CHARS:
        frames.append(pack_mugen(idx, name, folder))
        idx += 1
    print("total de personagens:", idx)
    pack_stages()
    pack_font()
    pack_icon(frames)


if __name__ == "__main__":
    main()
