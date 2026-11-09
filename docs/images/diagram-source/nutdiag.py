"""nutdiag - shared drawing kit for Network UPS Tools documentation diagrams.

Everything a diagram needs lives here: palette, type scale, authored symbols
(UPS, computer, purpose glyphs), host cards with process "pills", wires with
rounded orthogonal routing, ports, junctions, labels, section headers and a
legend. Diagram files only place things; they never restyle them.

Conventions (see ../README-diagrams.txt for the reasoning):
  * Canvas is always 1200 user units wide; PNG export is 2400 px (2x).
  * Power  = solid red-orange line, ends on an inlet tab on a host *card edge*.
  * Communication = dashed blue line, ends on a *process pill* (or component).
    USB / serial and local IPC links have round ports at both ends;
    network (TCP) links end in an arrow at upsd, pointing client -> server.
  * Text stays as <text>; every connector is a single editable <path>.
  * Presentation attributes are inline (no CSS) for the widest renderer support.
"""

from xml.sax.saxutils import escape

# --------------------------------------------------------------------------
# Design tokens
# --------------------------------------------------------------------------
W = 1200            # canvas width in user units (export at 2x = 2400 px)
M = 48              # outer margin

C = dict(
    ink="#17212b",          # primary text
    muted="#536273",        # secondary text, section labels
    faint="#8a98a8",
    hair="#d6dee7",         # dividers
    card_stroke="#8293a6",
    head_fill="#edf2f7",
    pill_fill="#f6f9fc",
    pill_stroke="#b3c0ce",
    power="#c4372b",        # power wiring
    comm="#1d66c6",         # communication
    bg="#ffffff",
    # UPS symbol
    ups_body="#2a3746", ups_edge="#131c26", ups_panel="#1a2531",
    ups_trim="#4a5d73", ups_socket="#3a4b5e", bolt="#ffc23d", led="#3ccf78",
    # computer symbol
    pc_body="#eef2f6", pc_edge="#37475a", pc_bay="#d2dbe5", pc_vent="#9aa8b7",
)

SANS = "'Noto Sans', 'DejaVu Sans', 'Liberation Sans', Arial, Helvetica, sans-serif"
MONO = "'Noto Sans Mono', 'DejaVu Sans Mono', 'Liberation Mono', Menlo, Consolas, monospace"

# Type scale (user units; at 640 px display multiply by 0.533)
T = dict(title=34, section=18, name=24, label=21, pill=21, note=19, comp=24, body=20)

# Wire geometry
POWER_W = 4.5
COMM_W = 3
COMM_DASH = "9 6"
CORNER = 14
PORT_R = 4.5
JUNCTION_R = 5.5
ARROW_BACK = 8      # path is shortened by this much so the arrow tip lands on target

# Symbol sizes (native)
UPS_W, UPS_H = 104, 136
UPS_COMM_DY = 40          # comm port height on UPS side
UPS_POWER_DY = 102        # outlet height on UPS side
PC_W, PC_H = 48, 64

# Host card geometry
HEAD = 86
PAD = 14
PILL_H = 38
PILL_GAP = 10


def card_height(rows):
    if rows == 0:
        return HEAD
    return HEAD + PAD + rows * PILL_H + (rows - 1) * PILL_GAP + PAD


def pill_row_y(card_y, i):
    return card_y + HEAD + PAD + i * (PILL_H + PILL_GAP)


# --------------------------------------------------------------------------
# Rough text metrics, used only to warn about overflow at build time.
# Deliberately pessimistic (close to DejaVu Sans, the widest likely fallback).
# --------------------------------------------------------------------------
def text_width(s, size, mono=False, bold=False):
    if mono:
        return len(s) * 0.602 * size
    narrow = set("iljtfr.,:;'|!I \xb7/()")
    wide = set("mwMWOQGDH")
    w = 0.0
    for ch in s:
        if ch in narrow:
            w += 0.36
        elif ch in wide:
            w += 0.86
        elif ch.isupper() or ch.isdigit():
            w += 0.70
        else:
            w += 0.61
    return w * size * (1.07 if bold else 1.0)


WARNINGS = []


def warn(msg):
    WARNINGS.append(msg)


# --------------------------------------------------------------------------
# Symbols (all authored for this kit)
# --------------------------------------------------------------------------
def _sym_ups():
    c = C
    sockets = []
    for cx in (32, 72):
        sockets.append(
            f'<rect x="{cx-14}" y="88" width="28" height="30" rx="6" fill="{c["ups_socket"]}" '
            f'stroke="{c["ups_trim"]}" stroke-width="1.5"/>'
            f'<rect x="{cx-7}" y="95" width="3.2" height="9" rx="1" fill="{c["ups_edge"]}"/>'
            f'<rect x="{cx+3.8}" y="95" width="3.2" height="9" rx="1" fill="{c["ups_edge"]}"/>'
            f'<circle cx="{cx}" cy="111" r="2.6" fill="{c["ups_edge"]}"/>')
    return (
        f'<symbol id="nut-ups" viewBox="0 0 {UPS_W} {UPS_H}">'
        f'<title>UPS</title>'
        f'<rect x="12" y="126" width="20" height="9" rx="2.5" fill="{c["ups_edge"]}"/>'
        f'<rect x="72" y="126" width="20" height="9" rx="2.5" fill="{c["ups_edge"]}"/>'
        f'<rect x="2" y="2" width="100" height="128" rx="11" fill="{c["ups_body"]}" '
        f'stroke="{c["ups_edge"]}" stroke-width="2"/>'
        f'<path d="M8 13 Q8 7 14 7 H90 Q96 7 96 13" fill="none" stroke="{c["ups_trim"]}" '
        f'stroke-width="2" stroke-linecap="round"/>'
        # status panel with battery-and-bolt emblem
        f'<rect x="14" y="17" width="76" height="44" rx="6" fill="{c["ups_panel"]}" '
        f'stroke="{c["ups_trim"]}" stroke-width="1.5"/>'
        f'<rect x="27" y="28" width="43" height="22" rx="4" fill="none" stroke="#e9eff5" stroke-width="3"/>'
        f'<rect x="70" y="34" width="5" height="10" rx="1.5" fill="#e9eff5"/>'
        f'<path d="M51.5 30 L41 41 H48 L44.5 48.5 L56 37 H49 L53 30 Z" fill="{c["bolt"]}"/>'
        # status LEDs and button
        f'<circle cx="22" cy="73" r="3.6" fill="{c["led"]}"/>'
        f'<circle cx="34" cy="73" r="3.6" fill="#55687d"/>'
        f'<circle cx="46" cy="73" r="3.6" fill="#55687d"/>'
        f'<rect x="66" y="68" width="24" height="10" rx="5" fill="#55687d"/>'
        + "".join(sockets) +
        '</symbol>')


def _sym_computer():
    c = C
    vents = "".join(
        f'<line x1="12" y1="{y}" x2="36" y2="{y}" stroke="{c["pc_vent"]}" stroke-width="1.8" stroke-linecap="round"/>'
        for y in (47, 52, 57))
    return (
        f'<symbol id="nut-computer" viewBox="0 0 {PC_W} {PC_H}">'
        f'<title>Computer</title>'
        f'<rect x="1.5" y="1.5" width="45" height="61" rx="5.5" fill="{c["pc_body"]}" '
        f'stroke="{c["pc_edge"]}" stroke-width="2.2"/>'
        f'<rect x="8" y="9" width="32" height="7.5" rx="2" fill="{c["pc_bay"]}" stroke="{c["pc_edge"]}" stroke-width="1.4"/>'
        f'<rect x="8" y="20" width="32" height="7.5" rx="2" fill="{c["pc_bay"]}" stroke="{c["pc_edge"]}" stroke-width="1.4"/>'
        f'<circle cx="24" cy="36.5" r="3.4" fill="{c["comm"]}"/>'
        + vents + '</symbol>')


def _glyph(gid, title, body):
    return (f'<symbol id="{gid}" viewBox="0 0 28 28"><title>{title}</title>'
            f'<g fill="none" stroke="{C["comm"]}" stroke-width="2.4" stroke-linecap="round" '
            f'stroke-linejoin="round">{body}</g></symbol>')


SYMBOLS = {
    "nut-ups": _sym_ups,
    "nut-computer": _sym_computer,
    "nut-glyph-shutdown": lambda: _glyph(
        "nut-glyph-shutdown", "Shutdown",
        '<path d="M9 8.6 A9 9 0 1 0 19 8.6"/><path d="M14 4 V14"/>'),
    "nut-glyph-log": lambda: _glyph(
        "nut-glyph-log", "Read and log",
        '<rect x="3.5" y="3.5" width="21" height="21" rx="3.5"/>'
        '<path d="M7.5 18.5 L11.5 13 L15 16 L20.5 9"/>'),
    "nut-glyph-control": lambda: _glyph(
        "nut-glyph-control", "Control",
        '<path d="M4 7 H24 M4 14 H24 M4 21 H24"/>'
        '<circle cx="10" cy="7" r="2.9" fill="#ffffff"/>'
        '<circle cx="19" cy="14" r="2.9" fill="#ffffff"/>'
        '<circle cx="13" cy="21" r="2.9" fill="#ffffff"/>'),
    "nut-glyph-driver": lambda: _glyph(
        "nut-glyph-driver", "Driver",
        '<rect x="7.5" y="7.5" width="13" height="13" rx="2"/>'
        '<path d="M11 3.5 V7.5 M17 3.5 V7.5 M11 20.5 V24.5 M17 20.5 V24.5 '
        'M3.5 11 H7.5 M3.5 17 H7.5 M20.5 11 H24.5 M20.5 17 H24.5"/>'),
    "nut-glyph-cache": lambda: _glyph(
        "nut-glyph-cache", "Data cache",
        '<ellipse cx="14" cy="7" rx="9" ry="3.5"/>'
        '<path d="M5 7 V21 A9 3.5 0 0 0 23 21 V7"/><path d="M5 14 A9 3.5 0 0 0 23 14"/>'),
}


def _markers():
    return (
        f'<marker id="nut-arrow-comm" viewBox="0 0 14 12" refX="6" refY="6" markerWidth="14" '
        f'markerHeight="12" markerUnits="userSpaceOnUse" orient="auto">'
        f'<path d="M0 0 L14 6 L0 12 Z" fill="{C["comm"]}"/></marker>')


# --------------------------------------------------------------------------
# Geometry helpers
# --------------------------------------------------------------------------
def fmt(v):
    v = round(v, 2)
    return str(int(v)) if v == int(v) else str(v)


def rounded_path(pts, r=CORNER):
    """Orthogonal polyline -> SVG path with arc-rounded corners."""
    if len(pts) < 2:
        raise ValueError("need two points")
    d = [f"M{fmt(pts[0][0])} {fmt(pts[0][1])}"]
    for i in range(1, len(pts) - 1):
        (x0, y0), (x1, y1), (x2, y2) = pts[i - 1], pts[i], pts[i + 1]
        l1 = abs(x1 - x0) + abs(y1 - y0)
        l2 = abs(x2 - x1) + abs(y2 - y1)
        rr = min(r, l1 / 2, l2 / 2)
        ux0, uy0 = _unit(x0, y0, x1, y1)
        ux1, uy1 = _unit(x1, y1, x2, y2)
        ax, ay = x1 - ux0 * rr, y1 - uy0 * rr
        bx, by = x1 + ux1 * rr, y1 + uy1 * rr
        sweep = 1 if (ux0 * uy1 - uy0 * ux1) > 0 else 0
        d.append(f"L{fmt(ax)} {fmt(ay)}")
        d.append(f"A{fmt(rr)} {fmt(rr)} 0 0 {sweep} {fmt(bx)} {fmt(by)}")
    d.append(f"L{fmt(pts[-1][0])} {fmt(pts[-1][1])}")
    return " ".join(d)


def _unit(x0, y0, x1, y1):
    dx, dy = x1 - x0, y1 - y0
    n = (dx * dx + dy * dy) ** 0.5 or 1
    return dx / n, dy / n


# --------------------------------------------------------------------------
# Diagram
# --------------------------------------------------------------------------
class Diagram:
    LAYERS = ("cards", "wires", "nodes", "ports", "labels")

    def __init__(self, slug, title, height, desc):
        self.slug, self.title_text, self.h, self.desc = slug, title, height, desc
        self.layers = {k: [] for k in self.LAYERS}
        self.used = set()
        self._ids = {}

    # -- bookkeeping ------------------------------------------------------
    def add(self, layer, s):
        self.layers[layer].append(s)

    def uid(self, base):
        n = self._ids.get(base, 0) + 1
        self._ids[base] = n
        return base if n == 1 else f"{base}-{n}"

    def use(self, sym, x, y, w, h, ident=None, layer="nodes"):
        self.used.add(sym)
        i = f' id="{ident}"' if ident else ""
        self.add(layer, f'<use{i} href="#{sym}" xlink:href="#{sym}" x="{fmt(x)}" y="{fmt(y)}" '
                        f'width="{fmt(w)}" height="{fmt(h)}"/>')

    # -- text -------------------------------------------------------------
    def text(self, x, y, s, size=None, weight=400, color=None, anchor="middle",
             mono=False, halo=False, layer="labels", spacing=None, max_w=None, ident=None):
        size = size or T["label"]
        color = color or C["ink"]
        if max_w and text_width(s, size, mono, weight >= 600) > max_w:
            warn(f"{self.slug}: text may overflow {max_w:.0f}u: {s!r}")
        attrs = [f'x="{fmt(x)}"', f'y="{fmt(y)}"', f'font-size="{size}"', f'fill="{color}"']
        if ident:
            attrs.insert(0, f'id="{ident}"')
        if weight != 400:
            attrs.append(f'font-weight="{weight}"')
        if anchor != "start":
            attrs.append(f'text-anchor="{anchor}"')
        if mono:
            attrs.append(f'font-family="{MONO}"')
        if spacing:
            attrs.append(f'letter-spacing="{spacing}"')
        if halo:
            attrs.append(f'stroke="{C["bg"]}" stroke-width="6" stroke-linejoin="round" paint-order="stroke"')
        self.add(layer, f'<text {" ".join(attrs)}>{escape(s)}</text>')

    def rich(self, x, y, parts, size=None, anchor="middle", layer="labels", max_w=None):
        """parts: [(text, 'mono'|'sans'|'muted'|'bold')]."""
        size = size or T["pill"]
        if max_w:
            est = sum(text_width(t, size, k == "mono", k == "bold") for t, k in parts)
            if est > max_w:
                warn(f"{self.slug}: pill text may overflow {max_w:.0f}u: "
                     f"{''.join(t for t, _ in parts)!r} (~{est:.0f}u)")
        spans = []
        for t, k in parts:
            if k == "mono":
                spans.append(f'<tspan font-family="{MONO}" font-weight="600">{escape(t)}</tspan>')
            elif k == "muted":
                spans.append(f'<tspan fill="{C["muted"]}">{escape(t)}</tspan>')
            elif k == "bold":
                spans.append(f'<tspan font-weight="700">{escape(t)}</tspan>')
            else:
                spans.append(f'<tspan>{escape(t)}</tspan>')
        a = f' text-anchor="{anchor}"' if anchor != "start" else ""
        self.add(layer, f'<text x="{fmt(x)}" y="{fmt(y)}" font-size="{size}" fill="{C["ink"]}"{a} '
                        f'xml:space="preserve">{"".join(spans)}</text>')

    # -- page furniture ---------------------------------------------------
    def title(self):
        self.text(M, 72, self.title_text, size=T["title"], weight=700, anchor="start", ident="title-text")

    def section(self, y, label, kind=None):
        x = M
        if kind:
            self.swatch(x, y - 6, 34, kind)
            x += 48
        self.text(x, y, label.upper(), size=T["section"], weight=700, color=C["muted"],
                  anchor="start", spacing="1.6")

    def divider(self, y):
        self.add("cards", f'<path d="M{M} {fmt(y)} H{W-M}" stroke="{C["hair"]}" stroke-width="1.5"/>')

    def swatch(self, x, y, length, kind):
        if kind == "power":
            self.add("labels", f'<path d="M{fmt(x)} {fmt(y)} H{fmt(x+length)}" stroke="{C["power"]}" '
                               f'stroke-width="{POWER_W}" stroke-linecap="round"/>')
        else:
            self.add("labels", f'<path d="M{fmt(x)} {fmt(y)} H{fmt(x+length)}" stroke="{C["comm"]}" '
                               f'stroke-width="{COMM_W}" stroke-dasharray="{COMM_DASH}"/>')

    def legend(self, y, items=(("power", "Power"), ("comm", "Communication"))):
        x = M
        for kind, label in items:
            self.swatch(x, y - 7, 46, kind)
            self.text(x + 60, y, label, size=T["label"], anchor="start", color=C["ink"])
            x += 60 + text_width(label, T["label"]) + 56

    def note(self, y, s, x=M, anchor="start"):
        self.text(x, y, s, size=T["note"], color=C["muted"], anchor=anchor)

    # -- equipment --------------------------------------------------------
    def ups(self, x, y, label, label_pos="above"):
        """Place a UPS symbol with its top-left at (x, y). Returns anchor dict."""
        self.use("nut-ups", x, y, UPS_W, UPS_H, ident=self.uid("ups-" + label.lower().replace(" ", "")))
        cx = x + UPS_W / 2
        if label_pos == "above":
            self.text(cx, y - 14, label, size=T["name"], weight=700)
        elif label_pos == "below":
            self.text(cx, y + UPS_H + 30, label, size=T["name"], weight=700)
        return dict(
            comm_right=(x + UPS_W, y + UPS_COMM_DY), comm_left=(x, y + UPS_COMM_DY),
            power_right=(x + UPS_W, y + UPS_POWER_DY), power_left=(x, y + UPS_POWER_DY),
            bottom=(cx, y + UPS_H - 4), top=(cx, y), cx=cx)

    def host(self, x, y, w, name, rows=(), ident=None):
        """Host card: header (computer icon + name) and stacked process pills.

        rows: list of rows; each row is a list of pills; each pill is a list of
        (text, kind) parts for Diagram.rich. Returns a Card.
        """
        n = len(rows)
        h = card_height(n)
        gid = ident or self.uid("host-" + name.lower().replace(" ", "-").replace("/", ""))
        r = 12
        head_h = HEAD if n else h
        self.add("cards",
                 f'<g id="{gid}">'
                 f'<rect x="{fmt(x)}" y="{fmt(y)}" width="{fmt(w)}" height="{fmt(h)}" rx="{r}" fill="#ffffff"/>'
                 + (f'<path d="M{fmt(x)} {fmt(y+head_h)} V{fmt(y+r)} A{r} {r} 0 0 1 {fmt(x+r)} {fmt(y)} '
                    f'H{fmt(x+w-r)} A{r} {r} 0 0 1 {fmt(x+w)} {fmt(y+r)} V{fmt(y+head_h)} Z" fill="{C["head_fill"]}"/>'
                    f'<path d="M{fmt(x)} {fmt(y+head_h)} H{fmt(x+w)}" stroke="{C["pill_stroke"]}" stroke-width="1.2"/>'
                    if n else
                    f'<rect x="{fmt(x)}" y="{fmt(y)}" width="{fmt(w)}" height="{fmt(h)}" rx="{r}" fill="{C["head_fill"]}"/>')
                 + f'<rect x="{fmt(x)}" y="{fmt(y)}" width="{fmt(w)}" height="{fmt(h)}" rx="{r}" fill="none" '
                   f'stroke="{C["card_stroke"]}" stroke-width="2"/>'
                 '</g>')
        iy = y + (HEAD - PC_H) / 2
        self.use("nut-computer", x + 18, iy, PC_W, PC_H)
        self.text(x + 18 + PC_W + 16, y + HEAD / 2 + 8.5, name, size=T["name"], weight=700,
                  anchor="start", max_w=w - (18 + PC_W + 16) - 12)
        card = Card(x, y, w, h)
        for i, row in enumerate(rows):
            py = pill_row_y(y, i)
            k = len(row)
            pw = (w - 2 * PAD - (k - 1) * PILL_GAP) / k
            for j, parts in enumerate(row):
                px = x + PAD + j * (pw + PILL_GAP)
                self.add("nodes", f'<rect x="{fmt(px)}" y="{fmt(py)}" width="{fmt(pw)}" height="{PILL_H}" '
                                  f'rx="9" fill="{C["pill_fill"]}" stroke="{C["pill_stroke"]}" stroke-width="1.5"/>')
                self.rich(px + pw / 2, py + PILL_H / 2 + 7.5, parts, layer="nodes", max_w=pw - 14)
                card.pills.append(Box(px, py, pw, PILL_H))
        return card

    def component(self, x, y, w, h, glyph, title, lines, mono_title=False):
        """Large process box used for the layering diagram."""
        self.add("nodes", f'<rect x="{fmt(x)}" y="{fmt(y)}" width="{fmt(w)}" height="{fmt(h)}" rx="10" '
                          f'fill="#ffffff" stroke="{C["comm"]}" stroke-width="2"/>')
        self.add("nodes", f'<path d="M{fmt(x+16)} {fmt(y+60)} H{fmt(x+w-16)}" stroke="{C["hair"]}" stroke-width="1.5"/>')
        self.use(glyph, x + 18, y + 16, 28, 28)
        self.text(x + 58, y + 39, title, size=T["comp"], weight=700, anchor="start", mono=mono_title)
        for i, ln in enumerate(lines):
            self.text(x + 18, y + 94 + i * 28, ln, size=T["body"], anchor="start", max_w=w - 30)
        return Box(x, y, w, h)

    def group(self, x, y, w, h, glyph, title, programs, purpose):
        """Client-purpose group card (layering diagram)."""
        self.add("cards", f'<rect x="{fmt(x)}" y="{fmt(y)}" width="{fmt(w)}" height="{fmt(h)}" rx="12" '
                          f'fill="{C["pill_fill"]}" stroke="{C["pill_stroke"]}" stroke-width="1.5"/>')
        self.add("cards", f'<path d="M{fmt(x+18)} {fmt(y+68)} H{fmt(x+w-18)}" stroke="{C["hair"]}" stroke-width="1.5"/>')
        self.use(glyph, x + 20, y + 22, 28, 28)
        self.text(x + 60, y + 45, title, size=T["name"], weight=700, anchor="start", max_w=w - 70)
        self.text(x + 20, y + 106, programs, size=22, weight=600, anchor="start", mono=True, max_w=w - 36)
        self.text(x + 20, y + 144, purpose, size=T["body"], anchor="start", color=C["ink"], max_w=w - 36)
        return Box(x, y, w, h)

    def boundary_card(self, x, y, w, h, name):
        """Host boundary that contains components rather than pills."""
        r = 14
        self.add("cards",
                 f'<g id="{self.uid("host-boundary")}">'
                 f'<rect x="{fmt(x)}" y="{fmt(y)}" width="{fmt(w)}" height="{fmt(h)}" rx="{r}" fill="#ffffff"/>'
                 f'<path d="M{fmt(x)} {fmt(y+HEAD)} V{fmt(y+r)} A{r} {r} 0 0 1 {fmt(x+r)} {fmt(y)} '
                 f'H{fmt(x+w-r)} A{r} {r} 0 0 1 {fmt(x+w)} {fmt(y+r)} V{fmt(y+HEAD)} Z" fill="{C["head_fill"]}"/>'
                 f'<path d="M{fmt(x)} {fmt(y+HEAD)} H{fmt(x+w)}" stroke="{C["pill_stroke"]}" stroke-width="1.2"/>'
                 f'<rect x="{fmt(x)}" y="{fmt(y)}" width="{fmt(w)}" height="{fmt(h)}" rx="{r}" fill="none" '
                 f'stroke="{C["card_stroke"]}" stroke-width="2"/></g>')
        self.use("nut-computer", x + 18, y + (HEAD - PC_H) / 2, PC_W, PC_H)
        self.text(x + 18 + PC_W + 16, y + HEAD / 2 + 8.5, name, size=T["name"], weight=700, anchor="start")
        return Box(x, y, w, h)

    # -- wiring -----------------------------------------------------------
    def power(self, pts, start_dot=True, ident=None):
        i = f' id="{ident}"' if ident else f' id="{self.uid("power")}"'
        self.add("wires", f'<path{i} d="{rounded_path(pts)}" fill="none" stroke="{C["power"]}" '
                          f'stroke-width="{POWER_W}" stroke-linecap="round" stroke-linejoin="round"/>')
        if start_dot:
            self.add("ports", f'<circle cx="{fmt(pts[0][0])}" cy="{fmt(pts[0][1])}" r="5" fill="{C["power"]}"/>')

    def inlet(self, x, y, edge="top", letter=None):
        """Power inlet tab straddling a card edge. Returns the wire end point."""
        if letter:
            w, h = 30, 26
        else:
            w, h = (24, 12) if edge in ("top", "bottom") else (12, 24)
        self.add("ports", f'<rect x="{fmt(x-w/2)}" y="{fmt(y-h/2)}" width="{w}" height="{h}" rx="4" '
                          f'fill="{C["power"]}" stroke="#ffffff" stroke-width="1.5"/>')
        if letter:
            self.text(x, y + 6.5, letter, size=18, weight=700, color="#ffffff", layer="ports")
        off = {"top": (0, -h / 2), "bottom": (0, h / 2), "left": (-w / 2, 0), "right": (w / 2, 0)}[edge]
        return (x + off[0], y + off[1])

    def comm(self, pts, start="port", end="port", ident=None):
        pts = [tuple(p) for p in pts]
        if end == "arrow":
            (x0, y0), (x1, y1) = pts[-2], pts[-1]
            ux, uy = _unit(x0, y0, x1, y1)
            pts[-1] = (x1 - ux * ARROW_BACK, y1 - uy * ARROW_BACK)
        m = ' marker-end="url(#nut-arrow-comm)"' if end == "arrow" else ""
        i = f' id="{ident}"' if ident else f' id="{self.uid("comm")}"'
        self.add("wires", f'<path{i} d="{rounded_path(pts)}" fill="none" stroke="{C["comm"]}" '
                          f'stroke-width="{COMM_W}" stroke-dasharray="{COMM_DASH}" stroke-linejoin="round"{m}/>')
        if start == "port":
            self.port(*pts[0])
        if end == "port":
            self.port(*pts[-1])

    def port(self, x, y):
        self.add("ports", f'<circle cx="{fmt(x)}" cy="{fmt(y)}" r="{PORT_R}" fill="#ffffff" '
                          f'stroke="{C["comm"]}" stroke-width="2.5"/>')

    def junction(self, x, y, kind):
        col = C["power"] if kind == "power" else C["comm"]
        self.add("ports", f'<circle cx="{fmt(x)}" cy="{fmt(y)}" r="{JUNCTION_R}" fill="{col}"/>')

    def wire_label(self, x, y, s, anchor="middle", size=None, color=None):
        self.text(x, y, s, size=size or T["label"], anchor=anchor, halo=True, color=color)

    # -- output -----------------------------------------------------------
    def svg(self):
        defs = "".join(SYMBOLS[k]() for k in sorted(self.used)) + _markers()
        body = "\n".join(
            f'<g id="layer-{k}">\n' + "\n".join(self.layers[k]) + "\n</g>" for k in self.LAYERS)
        return (
            '<?xml version="1.0" encoding="UTF-8"?>\n'
            '<!-- Generated by diagram-source/build.py; edit the Python source to retain changes. -->\n'
            f'<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" '
            f'width="{W}" height="{self.h}" viewBox="0 0 {W} {self.h}" role="img" '
            f'aria-labelledby="{self.slug}-title {self.slug}-desc" '
            f'font-family="{SANS}">\n'
            f'<title id="{self.slug}-title">{escape(self.title_text)}</title>\n'
            f'<desc id="{self.slug}-desc">{escape(self.desc)}</desc>\n'
            f'<defs>{defs}</defs>\n'
            f'<rect id="background" width="{W}" height="{self.h}" fill="{C["bg"]}"/>\n'
            f'{body}\n</svg>\n')


class Box:
    def __init__(self, x, y, w, h):
        self.x, self.y, self.w, self.h = x, y, w, h

    @property
    def cx(self):
        return self.x + self.w / 2

    @property
    def cy(self):
        return self.y + self.h / 2

    def left(self):
        return (self.x, self.cy)

    def right(self):
        return (self.x + self.w, self.cy)

    def top(self, dx=None):
        return (self.x + (self.w / 2 if dx is None else dx), self.y)

    def bottom(self, dx=None):
        return (self.x + (self.w / 2 if dx is None else dx), self.y + self.h)


class Card(Box):
    def __init__(self, x, y, w, h):
        super().__init__(x, y, w, h)
        self.pills = []


# Pill shorthands -----------------------------------------------------------
def prog(name, qualifier=None):
    """A process pill: program name in mono, optional qualifier in muted sans."""
    parts = [(name, "mono")]
    if qualifier:
        parts.append((" " + qualifier, "muted"))
    return parts


def driver(qualifier=None):
    """Driver pill. 'driver' is a role, not a literal program name, so it is
    set in bold sans rather than mono."""
    parts = [("driver", "bold")]
    if qualifier:
        parts.append((" " + qualifier, "muted"))
    return parts


def plain(s):
    return [(s, "sans")]
