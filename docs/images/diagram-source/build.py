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
def nut_layering():
    d = Diagram("nut_layering", "How NUT fits together", 880,
                "A power device talks to a NUT driver using its own device protocol. The driver and upsd "
                "run on the same host and talk over local IPC (Unix domain sockets or Windows named pipes). "
                "upsd keeps a data cache and answers client requests. Clients on this or another host use "
                "the NUT network protocol over TCP, including over localhost: upsmon for shutdown, upsc and "
                "upslog to read and log, upsrw and upscmd for permitted changes.")
    d.title()

    host = d.boundary_card(314, 150, W - M - 314, 290, "Driver / server host")
    cy_top, comp_h, comp_w = 264, 140, 230
    upsd_cx = 978
    drv = d.component(373, cy_top, comp_w, comp_h, "nut-glyph-driver", "Driver",
                      ["Device readings", "and controls"])
    srv = d.component(upsd_cx - comp_w / 2, cy_top, comp_w, comp_h, "nut-glyph-cache", "upsd",
                      ["Data cache", "Client requests"], mono_title=True)
    line_y = drv.cy

    ups = d.ups(80, line_y - UPS_COMM_DY, "Power device", label_pos="below")
    d.comm([ups["comm_right"], drv.left()])
    lx = (ups["comm_right"][0] + host.x) / 2
    d.wire_label(lx, line_y - 40, "Device")
    d.wire_label(lx, line_y - 14, "protocol")

    d.comm([drv.right(), srv.left()])
    gx = (drv.x + drv.w + srv.x) / 2
    d.wire_label(gx, line_y - 14, "Local IPC")
    gap = srv.x - (drv.x + drv.w)
    d.text(gx, line_y + 30, "Unix domain sockets", size=T["note"], color=C["muted"], max_w=gap - 8)
    d.text(gx, line_y + 54, "or Windows named pipes", size=T["note"], color=C["muted"], max_w=gap - 8)

    d.section(500, "Clients on this host or another host")
    gy, gh = 600, 170
    gw = (W - 2 * M - 2 * 30) / 3
    specs = [("nut-glyph-shutdown", "Shutdown", "upsmon", "Shuts down its own host"),
             ("nut-glyph-log", "Read and log", "upsc, upslog", "Readings and history"),
             ("nut-glyph-control", "Control", "upsrw, upscmd", "Permitted changes")]
    groups = [d.group(M + i * (gw + 30), gy, gw, gh, *s) for i, s in enumerate(specs)]

    bus_y = 540
    g3 = groups[2].top()
    assert abs(g3[0] - upsd_cx) < 0.5, "group 3 drop must line up under upsd"
    d.comm([g3, srv.bottom()], end="arrow")
    g1 = groups[0].top()
    d.comm([g1, (g1[0], bus_y), (g3[0], bus_y)], end=None)
    g2 = groups[1].top()
    d.comm([g2, (g2[0], bus_y)], end=None)
    d.junction(g2[0], bus_y, "comm")
    d.junction(g3[0], bus_y, "comm")
    d.wire_label((g2[0] + g3[0]) / 2, bus_y + 32, "NUT network protocol over TCP")

    d.note(818, "Clients on the driver / server host also connect to upsd over TCP (localhost).")
    d.note(846, "Only communication is shown here; no power wiring is depicted.")
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
