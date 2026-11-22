#!/usr/bin/env python3
"""Build the five NUT documentation diagrams (and the template sheet).

Usage (from docs/images/):
    python3 diagram-source/build.py            # write *.svg into docs/images/
    python3 diagram-source/build.py --png      # also export 2400 px PNGs with rsvg-convert
"""

import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import nutdiag as nd  # noqa: E402
from nutdiag import Diagram, M, W, T, C, UPS_W, UPS_H, UPS_COMM_DY, prog, driver  # noqa: E402

OUT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Shared column grid for the three-host diagrams (advanced, bizarre)
CARD_W = 240
COLS = (360, 640, 920)          # card x for host 1..3 (gap 40, right edge 1160)
TCP_RISER_X = 620               # vertical TCP run in the gap between host 1 and 2


# --------------------------------------------------------------------------
def simple():
    d = Diagram("simple", "One UPS, one computer", 576,
                "A UPS supplies power to one computer and is connected to it by USB or serial. "
                "The computer runs the NUT driver, upsd and upsmon as primary.")
    d.title()
    card = d.host(760, 132, 270, "Computer",
                  [[driver()], [prog("upsd")], [prog("upsmon", "primary")]])
    drv = card.pills[0]
    ups = d.ups(150, drv.cy - UPS_COMM_DY, "UPS")

    d.comm([ups["comm_right"], drv.left()])
    d.wire_label((ups["comm_right"][0] + card.x) / 2, drv.cy - 14, "USB / serial")

    inlet = d.inlet(card.cx, card.y + card.h, "bottom")
    bus_y = 446
    d.power([ups["bottom"], (ups["cx"], bus_y), (card.cx, bus_y), inlet])
    d.wire_label((ups["cx"] + card.cx) / 2, bus_y + 32, "Power")

    d.legend(540)
    return d


# --------------------------------------------------------------------------
def advanced():
    d = Diagram("advanced", "One UPS, several computers", 840,
                "Power: one UPS feeds Server, Client 1 and Client 2. Communication: the UPS USB or serial "
                "link goes only to the driver on Server, which also runs upsd and upsmon as primary. "
                "upsmon on Client 1 and Client 2 runs as secondary and connects over TCP to upsd on Server.")
    d.title()
    names = ("Server", "Client 1", "Client 2")

    # ---- power view
    d.section(136, "Power", "power")
    top = 262
    ups = d.ups(56, 192, "UPS")
    bus_y = 226
    ends = []
    for x, n in zip(COLS, names):
        c = d.host(x, top, CARD_W, n)
        ends.append(d.inlet(c.cx, c.y, "top"))
    ox, oy = ups["power_right"]
    d.power([(ox, oy), (300, oy), (300, bus_y), (ends[-1][0], bus_y), ends[-1]])
    for e in ends[:-1]:
        d.power([(e[0], bus_y), e], start_dot=False)
        d.junction(e[0], bus_y, "power")

    d.divider(392)

    # ---- communication view
    d.section(440, "Communication", "comm")
    top = 480
    srv = d.host(COLS[0], top, CARD_W, "Server",
                 [[driver()], [prog("upsd")], [prog("upsmon", "primary")]])
    c1 = d.host(COLS[1], top, CARD_W, "Client 1", [[prog("upsmon", "secondary")]])
    c2 = d.host(COLS[2], top, CARD_W, "Client 2", [[prog("upsmon", "secondary")]])
    drv, upsd = srv.pills[0], srv.pills[1]
    ups = d.ups(56, drv.cy - UPS_COMM_DY, "UPS")
    d.comm([ups["comm_right"], drv.left()])
    d.wire_label((ups["comm_right"][0] + srv.x) / 2, drv.cy - 14, "USB / serial")

    bus_y = srv.y + srv.h + 38
    p2, p1 = c2.pills[0].bottom(), c1.pills[0].bottom()
    d.comm([p2, (p2[0], bus_y), (TCP_RISER_X, bus_y), (TCP_RISER_X, upsd.cy), upsd.right()], end="arrow")
    d.comm([p1, (p1[0], bus_y)], end=None)
    d.junction(p1[0], bus_y, "comm")
    d.wire_label((TCP_RISER_X + p2[0]) / 2 + 10, bus_y + 32, "TCP to upsd on Server")
    return d


# --------------------------------------------------------------------------
def bigbox():
    d = Diagram("bigbox", "One server, two UPS feeds", 580,
                "UPS 1 and UPS 2 each supply a separate power feed, A and B, of one server. Each UPS "
                "has its own USB or serial link to its own driver on the server, which runs two drivers, "
                "one upsd and upsmon as primary for both UPSes.")
    d.title()
    card = d.host(410, 150, 380, "Server",
                  [[driver("UPS 1"), driver("UPS 2")], [prog("upsd")],
                   [prog("upsmon", "primary for both")]])
    d1, d2 = card.pills[0], card.pills[1]
    u1 = d.ups(70, d1.cy - UPS_COMM_DY, "UPS 1")
    u2 = d.ups(W - 70 - UPS_W, d2.cy - UPS_COMM_DY, "UPS 2")

    d.comm([u1["comm_right"], d1.left()])
    d.wire_label((u1["comm_right"][0] + card.x) / 2, d1.cy - 14, "USB / serial")
    d.comm([u2["comm_left"], d2.right()])
    d.wire_label((u2["comm_left"][0] + card.x + card.w) / 2, d2.cy - 14, "USB / serial")

    by = card.y + card.h
    a = d.inlet(card.x + 110, by, "bottom", letter="A")
    b = d.inlet(card.x + card.w - 110, by, "bottom", letter="B")
    run_y = 474
    d.power([u1["bottom"], (u1["cx"], run_y), (a[0], run_y), a])
    d.power([u2["bottom"], (u2["cx"], run_y), (b[0], run_y), b])
    d.wire_label((u1["cx"] + a[0]) / 2, run_y - 14, "Power feed A")
    d.wire_label((u2["cx"] + b[0]) / 2, run_y - 14, "Power feed B")

    d.legend(540)
    return d


# --------------------------------------------------------------------------
def bizarre():
    d = Diagram("bizarre", "Power and communication can differ", 1130,
                "Power: UPS 1 feeds Server only; UPS 2 feeds Client 1 and Client 2 only. Communication: the USB "
                "or serial links of both UPSes go to two drivers on Server, which also runs upsd. upsmon on "
                "Server runs as primary for UPS 1. upsmon on Client 1 and Client 2 monitors UPS 2 "
                "as secondary through upsd on Server over TCP.")
    d.title()
    names = ("Server", "Client 1", "Client 2")

    # ---- power view
    d.section(136, "Power", "power")
    top = 400
    cards = [d.host(x, top, CARD_W, n) for x, n in zip(COLS, names)]
    ins = [d.inlet(c.cx, c.y, "top") for c in cards]
    u1 = d.ups(cards[0].cx - UPS_W / 2, 196, "UPS 1")
    mid = (cards[1].cx + cards[2].cx) / 2
    u2 = d.ups(mid - UPS_W / 2, 196, "UPS 2")
    d.power([u1["bottom"], ins[0]])
    split_y = 364
    d.power([u2["bottom"], (mid, split_y)])
    d.power([(mid, split_y), (ins[1][0], split_y), ins[1]], start_dot=False)
    d.power([(mid, split_y), (ins[2][0], split_y), ins[2]], start_dot=False)
    d.junction(mid, split_y, "power")

    d.divider(530)

    # ---- communication view
    d.section(578, "Communication", "comm")
    top = 618
    ha = d.host(COLS[0], top, CARD_W, "Server",
                [[driver("UPS 1")], [driver("UPS 2")], [prog("upsd")], [prog("upsmon", "primary")]])
    hb = d.host(COLS[1], top, CARD_W, "Client 1", [[prog("upsmon", "secondary")]])
    hc = d.host(COLS[2], top, CARD_W, "Client 2", [[prog("upsmon", "secondary")]])
    dr1, dr2, upsd = ha.pills[0], ha.pills[1], ha.pills[2]

    v1 = d.ups(56, dr1.cy - UPS_COMM_DY, "UPS 1")
    v2 = d.ups(56, v1["top"][1] + UPS_H + 62, "UPS 2")
    d.comm([v1["comm_right"], dr1.left()])
    d.wire_label((v1["comm_right"][0] + ha.x) / 2, dr1.cy - 14, "USB / serial")
    jog_x = 300
    s2 = v2["comm_right"]
    d.comm([s2, (jog_x, s2[1]), (jog_x, dr2.cy), dr2.left()])
    d.wire_label((s2[0] + jog_x) / 2 - 2, s2[1] + 32, "USB / serial")

    bus_y = ha.y + ha.h + 38
    pc, pb = hc.pills[0].bottom(), hb.pills[0].bottom()
    d.comm([pc, (pc[0], bus_y), (TCP_RISER_X, bus_y), (TCP_RISER_X, upsd.cy), upsd.right()], end="arrow")
    d.comm([pb, (pb[0], bus_y)], end=None)
    d.junction(pb[0], bus_y, "comm")
    d.wire_label((TCP_RISER_X + pc[0]) / 2 + 10, bus_y + 32, "TCP to upsd on Server")

    d.note(1066, "Server runs upsmon as primary for UPS 1.")
    d.note(1094, "Clients 1 and 2 run upsmon as secondary for UPS 2, through upsd on Server.")
    return d


# --------------------------------------------------------------------------
# NUT layering: helpers used only by nut_layering()
# --------------------------------------------------------------------------
def _layering_symbols():
    """Register the extra device icons and glyphs this one figure uses.

    They are added to the shared symbol table only when nut_layering() runs,
    and a symbol is emitted into an SVG only if that SVG uses it, so the other
    figures are unaffected.
    """
    c = C
    dark = dict(fill=c["ups_body"], edge=c["ups_edge"], panel=c["ups_panel"],
                trim=c["ups_trim"], sock=c["ups_socket"])

    def pdu():
        outlets = "".join(
            f'<rect x="{x}" y="15" width="12" height="15" rx="3" fill="{dark["sock"]}" '
            f'stroke="{dark["trim"]}" stroke-width="1.2"/>'
            f'<rect x="{x+3}" y="19" width="2" height="5" fill="{dark["edge"]}"/>'
            f'<rect x="{x+7}" y="19" width="2" height="5" fill="{dark["edge"]}"/>'
            for x in (34, 50, 66, 82))
        return (f'<symbol id="nut-dev-pdu" viewBox="0 0 104 46"><title>PDU</title>'
                f'<rect x="1" y="12" width="8" height="22" rx="2" fill="{dark["edge"]}"/>'
                f'<rect x="95" y="12" width="8" height="22" rx="2" fill="{dark["edge"]}"/>'
                f'<rect x="6" y="7" width="92" height="32" rx="6" fill="{dark["fill"]}" '
                f'stroke="{dark["edge"]}" stroke-width="2"/>'
                f'<rect x="13" y="14" width="15" height="18" rx="3" fill="{dark["panel"]}" '
                f'stroke="{dark["trim"]}" stroke-width="1.2"/>'
                f'<circle cx="20.5" cy="23" r="3" fill="{c["led"]}"/>' + outlets + '</symbol>')

    def solar():
        cells = "".join(
            f'<path d="M{x0} 6 L{x1} 32" stroke="#9cc0e6" stroke-width="1.6"/>'
            for x0, x1 in ((26, 34), (44, 52)))
        return (f'<symbol id="nut-dev-solar" viewBox="0 0 92 62"><title>Solar controller</title>'
                f'<path d="M8 6 H62 L72 32 H18 Z" fill="#24476e" stroke="{dark["edge"]}" '
                f'stroke-width="2" stroke-linejoin="round"/>'
                + cells +
                f'<path d="M12.5 19 H67" stroke="#9cc0e6" stroke-width="1.6"/>'
                f'<path d="M45 32 V40 H58" fill="none" stroke="{dark["trim"]}" stroke-width="2.4"/>'
                f'<rect x="56" y="34" width="34" height="26" rx="4" fill="{dark["fill"]}" '
                f'stroke="{dark["edge"]}" stroke-width="2"/>'
                f'<rect x="61" y="39" width="17" height="10" rx="2" fill="{dark["panel"]}" '
                f'stroke="{dark["trim"]}" stroke-width="1"/>'
                f'<path d="M71.5 40.5 L66 45.5 H69.5 L67.5 48.5 L73 43.5 H69.5 Z" fill="{c["bolt"]}"/>'
                f'<circle cx="83" cy="44" r="2.4" fill="{c["led"]}"/>'
                '</symbol>')

    def psu():
        blades = "".join(
            f'<path d="M22 27 L{x} {y}" stroke="{c["pc_edge"]}" stroke-width="1.6" stroke-linecap="round"/>'
            for x, y in ((22, 17), (31, 30), (13, 30)))
        return (f'<symbol id="nut-dev-psu" viewBox="0 0 92 54"><title>Server PSU</title>'
                f'<rect x="2" y="7" width="80" height="40" rx="4" fill="{c["pc_body"]}" '
                f'stroke="{c["pc_edge"]}" stroke-width="2.2"/>'
                f'<circle cx="22" cy="27" r="13" fill="{c["pc_bay"]}" stroke="{c["pc_edge"]}" stroke-width="1.6"/>'
                + blades +
                f'<circle cx="22" cy="27" r="3" fill="{c["pc_edge"]}"/>'
                f'<rect x="44" y="18" width="20" height="17" rx="3" fill="{c["pc_bay"]}" '
                f'stroke="{c["pc_edge"]}" stroke-width="1.6"/>'
                f'<rect x="48" y="23" width="2.6" height="7" rx="1" fill="{c["pc_edge"]}"/>'
                f'<rect x="52.7" y="23" width="2.6" height="7" rx="1" fill="{c["pc_edge"]}"/>'
                f'<rect x="57.4" y="23" width="2.6" height="7" rx="1" fill="{c["pc_edge"]}"/>'
                f'<circle cx="73" cy="40" r="2.6" fill="{c["led"]}"/>'
                f'<path d="M82 17 H88 Q90 17 90 19 V35 Q90 37 88 37 H82" fill="none" '
                f'stroke="{c["pc_edge"]}" stroke-width="2.2"/>'
                '</symbol>')

    def other():
        return (f'<symbol id="nut-dev-other" viewBox="0 0 72 54"><title>Other devices</title>'
                f'<rect x="6" y="5" width="60" height="44" rx="8" fill="#ffffff" stroke="{c["faint"]}" '
                f'stroke-width="2.2" stroke-dasharray="6 5"/>'
                f'<circle cx="24" cy="27" r="3.6" fill="{c["muted"]}"/>'
                f'<circle cx="36" cy="27" r="3.6" fill="{c["muted"]}"/>'
                f'<circle cx="48" cy="27" r="3.6" fill="{c["muted"]}"/>'
                '</symbol>')

    extra = {
        "nut-dev-pdu": pdu, "nut-dev-solar": solar, "nut-dev-psu": psu, "nut-dev-other": other,
        "nut-glyph-apps": lambda: nd._glyph(
            "nut-glyph-apps", "Integrations",
            '<rect x="4" y="4" width="8.5" height="8.5" rx="2"/>'
            '<rect x="15.5" y="4" width="8.5" height="8.5" rx="2"/>'
            '<rect x="4" y="15.5" width="8.5" height="8.5" rx="2"/>'
            '<path d="M19.75 15.5 V24 M15.5 19.75 H24"/>'),
        "nut-glyph-server": lambda: nd._glyph(
            "nut-glyph-server", "Data server",
            '<rect x="4" y="4.5" width="20" height="8" rx="2"/>'
            '<rect x="4" y="15.5" width="20" height="8" rx="2"/>'
            '<path d="M8.5 8.5 H9 M8.5 19.5 H9 M14 8.5 H19.5 M14 19.5 H19.5"/>'),
        "nut-glyph-vars": lambda: nd._glyph(
            "nut-glyph-vars", "Driver variables",
            '<rect x="3.5" y="4.5" width="21" height="19" rx="3"/>'
            '<path d="M3.5 11 H24.5 M3.5 17 H24.5 M11 4.5 V23.5"/>'),
    }
    for k, fn in extra.items():
        nd.SYMBOLS.setdefault(k, fn)


def _lay_band(d, x, y, w, h, label, note=None):
    """One NUT layer: a light band with its name at top left."""
    d.add("cards", f'<rect x="{nd.fmt(x)}" y="{nd.fmt(y)}" width="{nd.fmt(w)}" height="{nd.fmt(h)}" '
                   f'rx="12" fill="{C["pill_fill"]}" stroke="{C["pill_stroke"]}" stroke-width="1.5"/>')
    d.text(x + 18, y + 33, label.upper(), size=T["section"], weight=700, color=C["muted"],
           anchor="start", spacing="1.6")
    lx = x + 18 + nd.text_width(label.upper(), T["section"], bold=True) + 1.6 * len(label) + 22
    if note:
        d.text(lx, y + 33, note, size=T["note"], color=C["muted"], anchor="start",
               max_w=x + w - 18 - lx)
    return lx   # x where text after the layer name may start


def _lay_host(d, x, y, w, h, name, note):
    """Host boundary as in Diagram.boundary_card, with a muted remark after the name."""
    r, hd = 14, nd.HEAD
    d.add("cards",
          f'<g id="host-boundary">'
          f'<rect x="{nd.fmt(x)}" y="{nd.fmt(y)}" width="{nd.fmt(w)}" height="{nd.fmt(h)}" rx="{r}" fill="#ffffff"/>'
          f'<path d="M{nd.fmt(x)} {nd.fmt(y+hd)} V{nd.fmt(y+r)} A{r} {r} 0 0 1 {nd.fmt(x+r)} {nd.fmt(y)} '
          f'H{nd.fmt(x+w-r)} A{r} {r} 0 0 1 {nd.fmt(x+w)} {nd.fmt(y+r)} V{nd.fmt(y+hd)} Z" fill="{C["head_fill"]}"/>'
          f'<path d="M{nd.fmt(x)} {nd.fmt(y+hd)} H{nd.fmt(x+w)}" stroke="{C["pill_stroke"]}" stroke-width="1.2"/>'
          f'<rect x="{nd.fmt(x)}" y="{nd.fmt(y)}" width="{nd.fmt(w)}" height="{nd.fmt(h)}" rx="{r}" fill="none" '
          f'stroke="{C["card_stroke"]}" stroke-width="2"/></g>')
    d.use("nut-computer", x + 18, y + (hd - nd.PC_H) / 2, nd.PC_W, nd.PC_H)
    tx = x + 18 + nd.PC_W + 16
    d.rich(tx, y + hd / 2 + 8.5, [(name, "bold"), ("  " + note, "muted")], size=T["name"],
           anchor="start", layer="labels", max_w=w - (tx - x) - 12)


def _lay_client(d, x, y, w, h, glyph, title, progs, lines):
    d.add("nodes", f'<rect x="{nd.fmt(x)}" y="{nd.fmt(y)}" width="{nd.fmt(w)}" height="{nd.fmt(h)}" '
                   f'rx="10" fill="#ffffff" stroke="{C["pill_stroke"]}" stroke-width="1.5"/>')
    d.use(glyph, x + 16, y + 15, 28, 28)
    d.text(x + 54, y + 37, title, size=T["label"], weight=700, anchor="start", max_w=w - 64)
    d.add("nodes", f'<path d="M{nd.fmt(x+16)} {nd.fmt(y+56)} H{nd.fmt(x+w-16)}" '
                   f'stroke="{C["hair"]}" stroke-width="1.5"/>')
    d.text(x + 16, y + 88, progs, size=20, weight=600, anchor="start", mono=True, max_w=w - 32)
    for i, ln in enumerate(lines):
        d.text(x + 16, y + 117 + i * 25, ln, size=T["note"], anchor="start", max_w=w - 32)
    return nd.Box(x, y, w, h)


LAY_PILL_H, LAY_PILL_GAP, LAY_PAD = 38, 8, 10


def _lay_drivers(d, x, y, w, names):
    """A group of driver programs that share one kind of device link."""
    h = 2 * LAY_PAD + len(names) * LAY_PILL_H + (len(names) - 1) * LAY_PILL_GAP
    d.add("nodes", f'<rect x="{nd.fmt(x)}" y="{nd.fmt(y)}" width="{nd.fmt(w)}" height="{nd.fmt(h)}" '
                   f'rx="10" fill="#ffffff" stroke="{C["card_stroke"]}" stroke-width="1.5"/>')
    for i, n in enumerate(names):
        py = y + LAY_PAD + i * (LAY_PILL_H + LAY_PILL_GAP)
        px, pw = x + LAY_PAD, w - 2 * LAY_PAD
        if n is None:   # placeholder for drivers not shown
            d.add("nodes", f'<rect x="{nd.fmt(px)}" y="{nd.fmt(py)}" width="{nd.fmt(pw)}" height="{LAY_PILL_H}" '
                           f'rx="9" fill="#ffffff" stroke="{C["faint"]}" stroke-width="1.5" stroke-dasharray="5 4"/>')
            d.text(px + pw / 2, py + LAY_PILL_H / 2 + 7, "more drivers", size=T["note"],
                   color=C["muted"], layer="nodes", max_w=pw - 12)
            continue
        d.add("nodes", f'<rect x="{nd.fmt(px)}" y="{nd.fmt(py)}" width="{nd.fmt(pw)}" height="{LAY_PILL_H}" '
                       f'rx="9" fill="{C["pill_fill"]}" stroke="{C["pill_stroke"]}" stroke-width="1.5"/>')
        d.text(px + pw / 2, py + LAY_PILL_H / 2 + 7, n, size=20, weight=600, mono=True,
               layer="nodes", max_w=pw - 12)
    return nd.Box(x, y, w, h)


def _lay_device(d, cx, base_y, sym, sw, sh, name, sub=None):
    """Device icon standing on base_y, name and optional remark below."""
    d.use(sym, cx - sw / 2, base_y - sh, sw, sh)
    d.text(cx, base_y + 30, name, size=T["label"], weight=700, max_w=190)
    if sub:
        d.text(cx, base_y + 54, sub, size=T["note"], color=C["muted"], max_w=200)


# --------------------------------------------------------------------------
def nut_layering():
    _layering_symbols()
    d = Diagram("nut_layering", "NUT layers and interfaces", 1436,
                "Four layers, top to bottom. Clients (upsmon shuts down its own host; upsc and upslog read "
                "and log; upsrw and upscmd make permitted changes and commands; NUT-Monitor, Nagios, Zenoss "
                "and other integrations) use the NUT network protocol over TCP, remotely or via localhost, to "
                "reach upsd. upsd keeps an in-memory cache of driver data. On the same server host, upsd "
                "talks to each driver with the NUT socket protocol over Unix domain sockets or Windows named "
                "pipes. Each driver keeps its own variables and auxiliary data. Drivers by device link: "
                "serial (bcmxcp, apcsmart, genericups, mge-shut) and USB (usbhid-ups) are point to point; "
                "network (snmp-ups for SNMP, netxml-ups for XML/HTTP); local IPMI through /dev/ipmi0 "
                "(nut-ipmipsu); and other drivers. Devices include UPSes, PDUs, solar controllers (SCD), "
                "server PSUs and other power devices; a device may offer several links. Power wiring "
                "is not shown.")
    d.title()
    X0, X1 = M, W - M
    spine_x = 640                     # TCP arrow into upsd and socket link out of it

    # ---- Layer 1: clients ------------------------------------------------
    cb_y, card_h = 104, 164
    cb_h = 50 + card_h + 18
    _lay_band(d, X0, cb_y, X1 - X0, cb_h, "Clients",
              "on the server host (via localhost) or on other hosts")
    pad, gap = 16, 16
    cw = (X1 - X0 - 2 * pad - 3 * gap) / 4
    specs = [("nut-glyph-shutdown", "Shutdown", "upsmon", ["Shuts down", "its own host"]),
             ("nut-glyph-log", "Read and log", "upsc, upslog", ["Read values,", "log over time"]),
             ("nut-glyph-control", "Control", "upsrw, upscmd", ["Permitted changes", "and instant commands"]),
             ("nut-glyph-apps", "Integrations", "NUT-Monitor", ["Nagios, Zenoss", "and others"])]
    cards = [_lay_client(d, X0 + pad + i * (cw + gap), cb_y + 50, cw, card_h, *s)
             for i, s in enumerate(specs)]

    # ---- Interface: NUT network protocol (TCP) ---------------------------
    tcp_bus = cb_y + cb_h + 30
    host_y = tcp_bus + 80
    sb_y = host_y + nd.HEAD + 16
    box_y, box_h = sb_y + 50, 112
    upsd_top = box_y

    for c in cards:
        b = c.bottom()
        d.comm([b, (b[0], tcp_bus)], end=None)
        if b[0] not in (cards[0].cx, cards[-1].cx):
            d.junction(b[0], tcp_bus, "comm")
    d.comm([(cards[0].cx, tcp_bus), (cards[-1].cx, tcp_bus)], start=None, end=None)
    d.comm([(spine_x, tcp_bus), (spine_x, upsd_top)], start=None, end="arrow")
    d.junction(spine_x, tcp_bus, "comm")
    d.wire_label(spine_x - 18, tcp_bus + 36, "NUT network protocol over TCP", anchor="end")
    d.text(spine_x + 18, tcp_bus + 36, "remote, or localhost", size=T["note"], color=C["muted"],
           anchor="start", halo=True)

    # ---- Server host frame -----------------------------------------------
    sb_h = 50 + box_h + 16
    sock_top = sb_y + sb_h
    db_y = sock_top + 92
    dcard_y = db_y + 52
    col_w, col_gap = 182, 32
    col_x0 = X0 + 33
    groups = [["bcmxcp", "apcsmart", "genericups", "mge-shut"],
              ["usbhid-ups"],
              ["snmp-ups", "netxml-ups"],
              ["nut-ipmipsu"],
              [None]]
    tallest = max(len(g) for g in groups)
    dcard_h = 2 * LAY_PAD + tallest * LAY_PILL_H + (tallest - 1) * LAY_PILL_GAP
    db_h = (dcard_y - db_y) + dcard_h + 18
    host_h = (db_y + db_h + 16) - host_y
    _lay_host(d, X0, host_y, X1 - X0, host_h, "Server host", "runs the drivers and upsd")

    # ---- Layer 2: data server --------------------------------------------
    _lay_band(d, X0 + 16, sb_y, X1 - X0 - 32, sb_h, "Data server")
    bx, bw = 290, 720
    d.add("nodes", f'<rect x="{bx}" y="{nd.fmt(box_y)}" width="{bw}" height="{box_h}" rx="10" '
                   f'fill="#ffffff" stroke="{C["comm"]}" stroke-width="2"/>')
    d.use("nut-glyph-server", bx + 18, box_y + 17, 28, 28)
    d.text(bx + 56, box_y + 40, "upsd", size=T["comp"], weight=700, anchor="start", mono=True)
    d.text(bx + 18, box_y + 74, "Answers client requests;", size=T["note"], anchor="start",
           max_w=spine_x - bx - 30)
    d.text(bx + 18, box_y + 98, "relays permitted changes", size=T["note"], anchor="start",
           max_w=spine_x - bx - 30)
    cx_, cw_ = spine_x + 30, bx + bw - 14 - (spine_x + 30)
    d.add("nodes", f'<rect x="{cx_}" y="{nd.fmt(box_y + 14)}" width="{nd.fmt(cw_)}" height="{box_h - 28}" '
                   f'rx="8" fill="{C["pill_fill"]}" stroke="{C["pill_stroke"]}" stroke-width="1.5"/>')
    d.use("nut-glyph-cache", cx_ + 16, box_y + 28, 28, 28)
    d.text(cx_ + 54, box_y + 49, "In-memory cache", size=T["label"], weight=700, anchor="start",
           max_w=cw_ - 64)
    d.text(cx_ + 54, box_y + 76, "copy of every driver's data", size=T["note"], anchor="start",
           max_w=cw_ - 64)

    # ---- Interface: NUT socket protocol (local) ---------------------------
    d.comm([(spine_x, box_y + box_h), (spine_x, db_y)])
    mid = sock_top + 92 / 2
    d.wire_label(spine_x - 18, mid - 4, "NUT socket protocol, local only", anchor="end")
    d.text(spine_x - 18, mid + 22, "Unix domain sockets or Windows named pipes", size=T["note"],
           color=C["muted"], anchor="end", halo=True)
    d.text(spine_x + 18, mid - 4, "one connection", size=T["note"], color=C["muted"], anchor="start", halo=True)
    d.text(spine_x + 18, mid + 22, "per driver", size=T["note"], color=C["muted"], anchor="start", halo=True)

    # ---- Layer 3: drivers -------------------------------------------------
    _lay_band(d, X0 + 16, db_y, X1 - X0 - 32, db_h, "Drivers")
    # Driver-local state, kept clear of the socket port on the band edge.
    vx = spine_x + 30
    d.use("nut-glyph-vars", vx, db_y + 13, 28, 28)
    d.text(vx + 38, db_y + 33, "Per-driver variables and auxiliary data", size=T["note"],
           color=C["muted"], anchor="start", max_w=X1 - 16 - 14 - (vx + 38))
    cols = [_lay_drivers(d, col_x0 + i * (col_w + col_gap), dcard_y, col_w, g)
            for i, g in enumerate(groups)]

    # ---- Interface: device links -------------------------------------------
    host_bottom = host_y + host_h
    dv_y = host_bottom + 96
    links = [("Serial", "point to point", False),
             ("USB", "point to point", False),
             ("Network", "SNMP, XML/HTTP", False),
             ("Local IPMI", "/dev/ipmi0", True),
             ("Other links", "per driver", False)]
    ly = host_bottom + 40
    for col, (name, sub, mono_sub) in zip(cols, links):
        d.comm([col.bottom(), (col.cx, dv_y)])
        # Labels sit beside the link, clear of the next one, like the family's wire labels.
        d.wire_label(col.cx + 12, ly, name, anchor="start")
        d.text(col.cx + 12, ly + 25, sub, size=T["note"], color=C["muted"], mono=mono_sub,
               anchor="start", halo=True, max_w=col_w + col_gap - 24)

    # ---- Layer 4: devices ---------------------------------------------------
    dv_h = 186
    _lay_band(d, X0, dv_y, X1 - X0, dv_h, "Power devices",
              "a device may offer several links; the link and protocol select the driver")
    base = dv_y + 118
    devices = [
        ("nut-ups", 50, 65, "UPS", None),
        ("nut-dev-pdu", 96, 42, "PDU", None),
        ("nut-dev-solar", 88, 59, "SCD", "solar controller"),
        ("nut-dev-psu", 86, 50, "Server PSU", "of the server host"),
        ("nut-dev-other", 66, 50, "Other", "power devices"),
    ]
    for col, device in zip(cols, devices):
        _lay_device(d, col.cx, base, *device)

    # ---- Legend ----------------------------------------------------------------
    ly = dv_y + dv_h + 46
    x = X0
    d.swatch(x, ly - 7, 46, "comm")
    d.text(x + 58, ly, "Communication", size=T["note"], anchor="start")
    x += 58 + nd.text_width("Communication", T["note"]) + 40
    d.add("labels", f'<path d="M{x} {ly - 7} H{x + 38}" stroke="{C["comm"]}" stroke-width="{nd.COMM_W}" '
                    f'stroke-dasharray="{nd.COMM_DASH}" marker-end="url(#nut-arrow-comm)"/>')
    d.text(x + 60, ly, "TCP, client to upsd", size=T["note"], anchor="start")
    x += 60 + nd.text_width("TCP, client to upsd", T["note"]) + 40
    d.port(x + 6, ly - 7)
    d.text(x + 22, ly, "Link endpoint", size=T["note"], anchor="start")
    d.text(X1, ly, "Power wiring is not shown.", size=T["note"], color=C["muted"], anchor="end")
    return d


# --------------------------------------------------------------------------
def template():
    """Specimen / starter sheet: every symbol, connector and text style."""
    d = Diagram("template", "Diagram title (34 bold)", 760,
                "NUT diagram kit specimen: symbols, connectors and text styles.")
    d.title()
    d.section(136, "Section label with power swatch", "power")
    card = d.host(330, 170, 270, "Host name",
                  [[driver()], [prog("upsd")], [prog("upsmon", "qualifier")]])
    u = d.ups(64, card.pills[0].cy - UPS_COMM_DY, "UPS")
    d.comm([u["comm_right"], card.pills[0].left()])
    d.wire_label((u["comm_right"][0] + card.x) / 2, card.pills[0].cy - 14, "Wire label")
    inl = d.inlet(card.cx, card.y + card.h, "bottom")
    d.power([u["bottom"], (u["cx"], 470), (card.cx, 470), inl])
    d.wire_label((u["cx"] + card.cx) / 2, 502, "Power label")
    hdr = d.host(700, 140, 240, "Compact")
    d.inlet(hdr.cx, hdr.y, "top")
    d.inlet(1060, 184, "bottom", letter="A")
    d.text(1060, 238, "Lettered inlet", size=T["note"], color=C["muted"])
    c1 = d.host(680, 270, 240, "Client 1", [[prog("upsmon", "secondary")]])
    c2 = d.host(940, 270, 212, "Client 2", [[prog("upsmon")]])
    bus_y = 450
    riser_x = card.x + card.w + 40
    p2, p1 = c2.pills[0].bottom(), c1.pills[0].bottom()
    d.comm([p2, (p2[0], bus_y), (riser_x, bus_y), (riser_x, card.pills[1].cy), card.pills[1].right()],
           end="arrow")
    d.comm([p1, (p1[0], bus_y)], end=None)
    d.junction(p1[0], bus_y, "comm")
    d.wire_label((riser_x + p2[0]) / 2 + 10, bus_y + 32, "TCP label (arrow points to upsd)")
    for i, (g, n) in enumerate([("nut-glyph-shutdown", "shutdown"), ("nut-glyph-log", "log"),
                                ("nut-glyph-control", "control"), ("nut-glyph-driver", "driver"),
                                ("nut-glyph-cache", "cache")]):
        x = 64 + i * 170
        d.use(g, x, 570, 28, 28)
        d.text(x + 40, 591, n, size=T["note"], anchor="start", color=C["muted"])
    d.note(650, "Note text (19, muted). Canvas 1200 u wide; export 2400 px.")
    d.legend(712)
    return d


DIAGRAMS = [simple, advanced, bigbox, bizarre, nut_layering]


def main():
    png = "--png" in sys.argv
    stress = None
    if "--stress" in sys.argv:
        # Fallback-font stress test: force the wider DejaVu faces and write the
        # results to a scratch directory instead of the deliverables.
        stress = sys.argv[sys.argv.index("--stress") + 1]
        os.makedirs(stress, exist_ok=True)
        nd.SANS = "'DejaVu Sans', sans-serif"
        nd.MONO = "'DejaVu Sans Mono', monospace"
    built = []
    for fn in DIAGRAMS + [template]:
        dg = fn()
        if stress:
            target = os.path.join(stress, f"{dg.slug}.svg")
        else:
            target = os.path.join(OUT, "diagram-source" if fn is template else "", f"{dg.slug}.svg")
        with open(target, "w", encoding="utf-8") as fh:
            fh.write(dg.svg())
        built.append(target)
        if png and fn is not template:
            subprocess.run(["rsvg-convert", "-w", "2400", "-o", target[:-4] + ".png", target], check=True)
    for b in built:
        print("wrote", os.path.relpath(b, OUT))
    for w in nd.WARNINGS:
        print("WARNING", w)


if __name__ == "__main__":
    main()
