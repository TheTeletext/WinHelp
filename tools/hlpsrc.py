#!/usr/bin/env python3
# hlpsrc.py - The Teletext WinHelp: plain-text help source -> RTF + HPJ for
# the Microsoft Help Compiler 3.10.505 (WinHelp 3.1, WHAG conventions).
# Copyright 2026 The Teletext. LGPL 2.1 or later (COPYING.LIB).
#
# The source is meant for editors, not RTF: one file of topics.
#
#   %title  Help file title (caption)          %contents  ctx of the contents topic
#   %helpon NAME.HLP (SetHelpOnFile)           %copyright text for About
#   %bitmap FILE.BMP ...                       %map ctx number  (help context ids)
#   %config macro                              (one [CONFIG] line)
#   %seqicon seq ICON.BMP  the icon of every topic in browse sequence seq
#
#   @topic ctx | Title | key1;key2 | browse:seq | ICON.BMP   (browse, icon optional;
#          the icon goes before the title, in the non-scrolling band)
#   @popup ctx | Term                                 (a glossary popup topic)
#   paragraphs, separated by blank lines; a line starting "- " is a bullet,
#   "1. " a numbered step (continuation lines indented two spaces),
#   a line starting "> " is a code/example line (Courier), "= " a subheading.
#   inline: [[ctx|text]] jump (classic green, double underline),
#           {{ctx|text}} popup (green, dotted), **bold**, ``code``,
#           {bmc FILE.BMP} a bitmap (as in WinHelp).
#
# Browse sequences: "+" footnotes "seq:NNN" in source order per sequence.
# Checks: every jump/popup target exists, every context string is unique,
# keyword lists are not empty for jump topics, no em dash, no "Usenet".
import re, sys, os

GREEN = r"\cf2"     # colour table index of RGB(0,128,0)

def rtf_escape(s):
    out = []
    for ch in s:
        o = ord(ch)
        if ch in "\\{}":
            out.append("\\" + ch)
        elif o < 128:
            out.append(ch)
        elif o < 256:
            out.append("\\'%02x" % o)
        else:
            raise ValueError("character outside Windows-1252: %r" % ch)
    return "".join(out)

def inline(s, topics, used):
    """markup -> RTF; records targets in used"""
    parts = []
    pos = 0
    pat = re.compile(r"\[\[([^|\]]+)\|([^\]]+)\]\]|\{\{([^|}]+)\|([^}]+)\}\}|\*\*(.+?)\*\*|``(.+?)``|\{bmc ([^}]+)\}")
    for m in pat.finditer(s):
        parts.append(rtf_escape(s[pos:m.start()]))
        if m.group(1):
            used.append(("jump", m.group(1).strip()))
            parts.append("{\\uldb%s %s}{\\v %s}" % (GREEN, rtf_escape(m.group(2)), m.group(1).strip()))
        elif m.group(3):
            used.append(("popup", m.group(3).strip()))
            parts.append("{\\ul%s %s}{\\v %s}" % (GREEN, rtf_escape(m.group(4)), m.group(3).strip()))
        elif m.group(5):
            parts.append("{\\b %s}" % rtf_escape(m.group(5)))
        elif m.group(6):
            parts.append("{\\f2 %s}" % rtf_escape(m.group(6)))
        else:
            parts.append("\\{bmc %s\\}" % m.group(7).strip())
        pos = m.end()
    parts.append(rtf_escape(s[pos:]))
    return "".join(parts)

def source_lines(paths):
    for path in paths:
        for n, raw in enumerate(open(path, encoding="utf-8"), 1):
            yield path, n, raw

def parse(paths):
    meta = {"bitmaps": [], "config": [], "map": [], "seqicon": {}}
    topics = []
    cur = None
    for path, n, raw in source_lines(paths):
        line = raw.rstrip("\n").rstrip()
        if line.startswith("#!") or line.startswith(";;"):
            continue
        if line.startswith("%"):
            k, _, v = line[1:].partition(" ")
            v = v.strip()
            if k in ("bitmap",): meta["bitmaps"] += v.split()
            elif k == "config": meta["config"].append(v)
            elif k == "map": meta["map"].append(v.split())
            elif k == "seqicon":
                a, b = v.split(); meta["seqicon"][a] = b
            else: meta[k] = v
            continue
        if line.startswith("@topic") or line.startswith("@popup"):
            kind = line[1:6]
            f = [x.strip() for x in line[6:].split("|")]
            cur = {"kind": kind, "ctx": f[0], "title": f[1], "line": "%s:%d" % (os.path.basename(path), n),
                   "keys": f[2] if len(f) > 2 and kind == "topic" else "",
                   "browse": f[3] if len(f) > 3 else "",
                   "icon": f[4] if len(f) > 4 else "", "body": []}
            topics.append(cur)
            continue
        if cur is None:
            if line: sys.exit("%s:%d: text outside a topic" % (path, n))
            continue
        cur["body"].append(line)
    return meta, topics

def blocks(lines):
    """group lines into paragraphs: ('p', text) ('li', text) ('code', text) ('h', text)"""
    out, para = [], []
    def flush():
        if para: out.append(("p", " ".join(para))); para.clear()
    for l in lines:
        if not l.strip():
            flush(); continue
        if l.startswith("- "):
            flush(); out.append(("li", l[2:].strip())); continue
        mnum = re.match(r"(\d+)\. (.*)", l)
        if mnum:
            flush(); out.append(("num", mnum.group(1) + ".", mnum.group(2).strip())); continue
        if l.startswith("> "):
            flush(); out.append(("code", l[2:])); continue
        if l == ">":
            flush(); out.append(("code", "")); continue
        if l.startswith("= "):
            flush(); out.append(("h", l[2:].strip())); continue
        if out and out[-1][0] in ("li", "num") and not para and l.startswith("  "):
            out[-1] = out[-1][:-1] + (out[-1][-1] + " " + l.strip(),); continue
        para.append(l.strip())
    flush()
    return out

def build(srcs, outdir, base):
    meta, topics = parse(srcs)
    ctxs = {}
    errors = []
    for t in topics:
        if t["ctx"] in ctxs: errors.append("duplicate context %s (%s)" % (t["ctx"], t["line"]))
        ctxs[t["ctx"]] = t
        if not re.fullmatch(r"[A-Za-z0-9_.]+", t["ctx"]): errors.append("bad context %s" % t["ctx"])
    text_all = "".join(open(p, encoding="utf-8").read() for p in srcs)
    if "—" in text_all: errors.append("em dash in the source")
    if re.search(r"(?i)usenet", text_all): errors.append('"Usenet" in the source (use Netnews or NNTP)')
    seqcount = {}
    rtf = [r"{\rtf1\ansi \deff0",
           r"{\fonttbl{\f0\fswiss Helv;}{\f1\froman Tms Rmn;}{\f2\fmodern Courier;}{\f3\ftech Symbol;}}",
           r"{\colortbl;\red0\green0\blue0;\red0\green128\blue0;\red0\green0\blue128;}"]
    used = []
    first = True
    for t in topics:
        if not first: rtf.append(r"\page")
        first = False
        rtf.append(r"\pard\plain\f0\fs20")
        rtf.append("#{\\footnote %s}" % t["ctx"])
        rtf.append("${\\footnote %s}" % rtf_escape(t["title"]))
        if t["kind"] == "topic":
            if not t["keys"]: errors.append("topic %s has no keywords" % t["ctx"])
            rtf.append("K{\\footnote %s}" % rtf_escape(t["keys"]))
            if t["browse"]:
                seq = t["browse"].split(":")[0]
                seqcount[seq] = seqcount.get(seq, 0) + 10
                rtf.append("+{\\footnote %s:%03d}" % (seq, seqcount[seq]))
            # the title in the non-scrolling region (\keepn)
            # an icon before the title, as the Chameleon help files have their services' icons
            name = t["icon"] or meta["seqicon"].get(t["browse"].split(":")[0] if t["browse"] else "", "")
            icon = ("\\{bmc %s\\} " % name) if name else ""
            rtf.append("{\\keepn\\sb60\\sa120\\f0\\fs28\\b %s%s\\par}" % (icon, rtf_escape(t["title"])))
        else:
            # a glossary popup: no keywords, so Search lists only full topics
            rtf.append("\\pard\\sb60\\sa60\\f0\\fs20 {\\b %s}\\par" % rtf_escape(t["title"]))
        bl = blocks(t["body"])
        for i, b in enumerate(bl):
            kind, s = b[0], b[-1]
            last = (i == len(bl) - 1)
            sa = "\\sa120" if last else ""
            if kind == "p":
                rtf.append("\\pard\\sb80%s\\f0\\fs20 %s\\par" % (sa, inline(s, ctxs, used)))
            elif kind == "li":
                # the bullet in the Symbol font, as Windows 3.1's own help files have it
                rtf.append("\\pard\\sb40%s\\li360\\fi-240\\f0\\fs20 {\\f3\\'b7}\\tab %s\\par" % (sa, inline(s, ctxs, used)))
            elif kind == "num":
                rtf.append("\\pard\\sb40%s\\li360\\fi-240\\f0\\fs20 %s\\tab %s\\par" % (sa, b[1], inline(s, ctxs, used)))
            elif kind == "h":
                rtf.append("\\pard\\sb160%s\\f0\\fs20 {\\b %s}\\par" % (sa, inline(s, ctxs, used)))
            else:
                rtf.append("\\pard\\li360%s\\f2\\fs20 %s\\par" % (sa, rtf_escape(s)))
    rtf.append("}")
    for kind, target in used:
        if target not in ctxs:
            errors.append("%s to missing topic %s" % (kind, target))
        elif kind == "popup" and ctxs[target]["kind"] != "popup":
            pass   # a popup of a full topic is legal WinHelp
    for m in meta["map"]:
        if m[0] not in ctxs: errors.append("[MAP] names missing topic %s" % m[0])
    if errors:
        for e in errors: print("ERROR:", e)
        sys.exit(1)
    rtfname = base + ".RTF"
    with open(os.path.join(outdir, rtfname), "w", encoding="cp1252", newline="\r\n") as f:
        f.write("\n".join(rtf) + "\n")
    hpj = ["; generated by hlpsrc.py from %s; edit the source, not this file" % ", ".join(os.path.basename(p) for p in srcs),
           "[OPTIONS]", "TITLE=%s" % meta.get("title", base),
           "CONTENTS=%s" % meta.get("contents", topics[0]["ctx"]),
           "COMPRESS=FALSE", "WARNING=3", "REPORT=ON"]
    if "copyright" in meta: hpj.append("COPYRIGHT=%s" % meta["copyright"])
    hpj += ["", "[FILES]", rtfname]
    if meta["bitmaps"]: hpj += ["", "[BITMAPS]"] + meta["bitmaps"]
    cfg = list(meta["config"])
    if "helpon" in meta: cfg.insert(0, 'SetHelpOnFile("%s")' % meta["helpon"])
    if cfg: hpj += ["", "[CONFIG]"] + cfg
    if meta["map"]: hpj += ["", "[MAP]"] + ["%-24s %s" % (a, b) for a, b in meta["map"]]
    with open(os.path.join(outdir, base + ".HPJ"), "w", encoding="ascii", newline="\r\n") as f:
        f.write("\n".join(hpj) + "\n")
    njump = sum(1 for k, _ in used if k == "jump"); npop = sum(1 for k, _ in used if k == "popup")
    print("%s: %d topics (%d popups), %d jumps, %d popup links, browse %s, map %d" % (
        base, len(topics), sum(1 for t in topics if t["kind"] == "popup"), njump, npop,
        ",".join("%s=%d" % (k, v // 10) for k, v in sorted(seqcount.items())), len(meta["map"])))

if __name__ == "__main__":
    if len(sys.argv) < 4:
        sys.exit("usage: hlpsrc.py OUTDIR BASENAME SOURCE.txt [SOURCE.txt ...]")
    build(sys.argv[3:], sys.argv[1], sys.argv[2])
