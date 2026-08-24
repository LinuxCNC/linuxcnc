#!/usr/bin/env python3
"""Directory-level include dependency report for the LinuxCNC src tree.

Resolves every #include the way the compiler does, aggregates the result to the
directory that builds each file, finds the strongly connected components, and
writes a markdown report.  The exported-header list is read out of SRCHEADERS in
src/Makefile, so the report follows what the build actually installs.

    ./scripts/include-dep-report.py [src] > report.md
"""

import os
import re
import sys
from collections import defaultdict

SRC = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "src")
SKIP_DIRS = {"objects", "autom4te.cache", "m4", "depends"}
EXTS = (".c", ".cc", ".cpp", ".h", ".hh", ".hpp", ".comp", ".icomp")
INC_RE = re.compile(r'^\s*#\s*include\s*([<"])([^>"]+)[>"]')
COMP_INC_RE = re.compile(r'^\s*include\s*([<"])([^>"]+)[>"]\s*;')
OBJS_RE = re.compile(r'^\s*[A-Za-z0-9_.-]+-objs\s*[:+]?=(.*)$')

# The -I list differs between the two compiles.  Userspace gets INCLUDE from
# src/Makefile, which is "." plus the "emc" added by src/emc/Submakefile.
# Realtime gets only what EXTRA_CFLAGS carries, "$(BASEPWD)" and the exported
# include/ directory, so an -Iemc form that compiles in userspace does not
# compile in a realtime module.  halcompile adds the .comp's own directory.
SEARCH = {"user": ["", "emc"], "rt": [""], "comp": [""]}


def read_srcheaders():
    out, collecting = [], False
    for line in open(os.path.join(SRC, "Makefile"), encoding="utf-8", errors="replace"):
        if not collecting:
            if re.match(r'^\s*SRCHEADERS\s*:?=', line):
                collecting = True
                line = line.split("=", 1)[1]
            else:
                continue
        cont = line.rstrip("\n").endswith("\\")
        out += [t for t in line.replace("\\", " ").split() if t.endswith((".h", ".hh"))]
        if not cont:
            break
    return out


def walk_sources():
    for dirpath, dirnames, filenames in os.walk(SRC):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
        for fn in filenames:
            if fn.endswith(EXTS):
                yield os.path.join(dirpath, fn)


def read_rt_sources():
    """Sources the realtime rule compiles, from the -objs lists in src/Makefile
    plus the .comp files that halcompile turns into realtime modules."""
    out = set()
    for line in open(os.path.join(SRC, "Makefile"), encoding="utf-8", errors="replace"):
        m = OBJS_RE.match(line)
        if not m:
            continue
        for tok in m.group(1).split():
            if tok.endswith(".o") and "$(" not in tok:
                out.add(tok[:-2])
    return out


def rt_source_files(stems, relfiles):
    """Map the -objs stems onto the source files they are built from, and add the
    .comp files halcompile turns into realtime modules."""
    out = set()
    for stem in stems:
        for ext in (".c", ".cc", ".cpp", ".comp"):
            if stem + ext in relfiles:
                out.add(stem + ext)
                break
    for rel in relfiles:
        if rel.endswith(".comp") and os.path.dirname(rel) in (
                "hal/components", "hal/drivers") and os.path.basename(rel) != "tpcomp.comp":
            out.add(rel)
    return out


def include_lines(path):
    """Includes of a source file.  A .comp only becomes C below its ;; line, but
    the declaration section above it has its own `include <foo.h>;` statement,
    which halcompile copies into the generated C, so both are read."""
    comp = path.endswith((".comp", ".icomp"))
    body = not comp
    for lineno, line in enumerate(open(path, encoding="utf-8", errors="replace"), 1):
        if not body:
            if line.rstrip("\n") == ";;":
                body = True
                continue
            m = COMP_INC_RE.match(line)
            if m:
                yield lineno, m.group(1), m.group(2)
            continue
        m = INC_RE.match(line)
        if m:
            yield lineno, m.group(1), m.group(2)


def folded_dirs():
    """Subdirectories the build does not treat as units of their own.

    A directory whose sources are listed by its parent's Submakefile compiles and
    links as part of the parent, so an include crossing that boundary crosses
    nothing.  libnml is the case in the tree: libnml/Submakefile lists every
    source under its six subdirectories and links them into one libnml.so.
    Reporting those as separate nodes invents a cycle out of a directory layout.
    Subdirectories the top-level Makefile builds on their own, emc/tp into tpmod
    for one, are not folded.
    """
    fold = {}
    for cur, dirs, unused_files in os.walk(SRC):
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS and not d.startswith(".")]
        rel = os.path.relpath(cur, SRC)
        if rel == ".":
            continue
        parent = os.path.dirname(rel)
        pmake = os.path.join(SRC, parent, "Submakefile")
        if not os.path.exists(pmake):
            continue
        with open(pmake, errors="replace") as f:
            text = f.read()
        pat = r"(^|[\s/])" + re.escape(os.path.basename(rel)) + r"/[\w.-]+\.(c|cc|cpp)\b"
        if re.search(pat, text, re.M):
            fold[rel] = parent
    return fold


FOLD = folded_dirs()


def module_of(rel):
    parts = rel.split("/")
    if len(parts) == 1:
        return "src"
    if parts[0] in ("emc", "hal", "libnml", "rtapi") and len(parts) > 2:
        mod = "/".join(parts[:2])
    else:
        return parts[0]
    while mod in FOLD:
        mod = FOLD[mod]
    return mod


def sccs(adj, nodes):
    index, low, on, st, out, c = {}, {}, {}, [], [], [0]
    for root in sorted(nodes):
        if root in index:
            continue
        work = [(root, iter(sorted(adj[root])))]
        index[root] = low[root] = c[0]; c[0] += 1
        st.append(root); on[root] = True
        while work:
            n, it = work[-1]; adv = False
            for w in it:
                if w not in index:
                    index[w] = low[w] = c[0]; c[0] += 1
                    st.append(w); on[w] = True
                    work.append((w, iter(sorted(adj[w])))); adv = True; break
                if on.get(w):
                    low[n] = min(low[n], index[w])
            if adv:
                continue
            work.pop()
            if work:
                low[work[-1][0]] = min(low[work[-1][0]], low[n])
            if low[n] == index[n]:
                comp = []
                while True:
                    w = st.pop(); on[w] = False; comp.append(w)
                    if w == n:
                        break
                if len(comp) > 1:
                    out.append(sorted(comp))
    return out


def analyse():
    exported = {os.path.basename(h): h for h in read_srcheaders()}
    relfiles = {os.path.relpath(f, SRC): f for f in walk_sources()}
    by_name = defaultdict(list)
    for rel in relfiles:
        by_name[os.path.basename(rel)].append(rel)
    rt_sources = rt_source_files(read_rt_sources(), relfiles)

    def resolve(name, style, incdir, mode):
        """Return (target, via) the way the compiler for `mode` would find it."""
        if style == '"':
            cand = os.path.normpath(os.path.join(incdir, name)) if incdir else name
            if cand in relfiles:
                return cand, "own directory"
        for base in SEARCH[mode]:
            cand = os.path.normpath(os.path.join(base, name)) if base else name
            if cand in relfiles:
                return cand, ("-I." if not base else "-I" + base)
        b = os.path.basename(name)
        if b in exported:
            return exported[b], "include/"
        if style == '"' and len(by_name.get(b, [])) == 1:
            return by_name[b][0], "unique basename"
        return None, None

    edges = defaultdict(set)
    exported_users = defaultdict(set)
    rt_only_via_emc = []          # would not compile in a realtime module
    includers = defaultdict(set)  # header -> set of includers

    for rel, full in sorted(relfiles.items()):
        d, src_mod = os.path.dirname(rel), module_of(rel)
        if rel.endswith((".comp", ".icomp")):
            modes = ["comp"]
        elif rel in rt_sources:
            modes = ["rt", "user"] if not rel.endswith((".h", ".hh")) else ["rt"]
        else:
            modes = ["user"]
        for lineno, style, name in include_lines(full):
            seen = {}
            for mode in modes:
                seen[mode] = resolve(name, style, d, mode)
            target, via = seen[modes[0]]
            if "rt" in seen and seen["rt"][0] is None and seen.get("user", (None,))[0]:
                rt_only_via_emc.append((rel, lineno, name, seen["user"][1]))
                target, via = seen["user"]
            if target is None:
                for mode in modes:
                    if seen[mode][0]:
                        target, via = seen[mode]
                        break
            if target is None:
                continue
            includers[target].add(rel)
            if os.path.basename(target) in exported:
                exported_users[exported[os.path.basename(target)]].add(rel)
            dst_mod = module_of(target)
            if dst_mod != src_mod:
                edges[(src_mod, dst_mod)].add((rel, lineno, target, style))

    # A file is realtime-reachable if a realtime compile pulls it in, directly or
    # through another header; likewise for userspace.  A header in both sets is
    # one whose contents have to serve both, which is where "needed to build" and
    # "needed to interface" stop being the same question.
    def reach(roots):
        out, work = set(roots), list(roots)
        while work:
            cur = work.pop()
            for h, users in includers.items():
                if h in out:
                    continue
                if cur in users:
                    out.add(h)
                    work.append(h)
        return out

    rt_reach = reach(rt_sources)
    user_roots = {r for r in relfiles
                  if r not in rt_sources and not r.endswith((".h", ".hh", ".hpp"))}
    user_reach = reach(user_roots)
    return {
        "edges": edges, "exported": exported, "exported_users": exported_users,
        "rt_only_via_emc": rt_only_via_emc, "rt_sources": rt_sources,
        "rt_reach": rt_reach, "user_reach": user_reach, "relfiles": relfiles,
    }


def main():
    data = analyse()
    edges = data["edges"]
    exported = data["exported"]
    exported_users = data["exported_users"]
    nodes = {m for e in edges for m in e}
    adj = defaultdict(set)
    for (a, b) in edges:
        adj[a].add(b)
    comps = sorted(sccs(adj, nodes), key=len, reverse=True)
    incycle = {m for c in comps for m in c}
    intra = {k: v for k, v in edges.items()
             if any(k[0] in c and k[1] in c for c in comps)}

    w = sys.stdout.write
    total_sites = sum(len(v) for v in edges.values())
    intra_sites = sum(len(v) for v in intra.values())
    thin = {k: v for k, v in intra.items() if len(v) == 1}

    w("# src/ directory dependency report\n\n")
    w("Generated by `scripts/include-dep-report.py`, which resolves every `#include` "
      "in the tree the way the compiler that sees it would, buckets each file into the "
      "directory that builds it, and reports the edges between those buckets. A "
      "subdirectory whose sources the parent's `Submakefile` lists compiles and links "
      "as part of the parent, so it is folded into it; a subdirectory the top-level "
      "`Makefile` builds on its own is not.\n\n")
    w("The two compiles do not get the same `-I`. Userspace gets `INCLUDE` from "
      "`src/Makefile`, which is `.` plus the `emc` added by `src/emc/Submakefile`. "
      "Realtime gets only what `EXTRA_CFLAGS` carries, `$(BASEPWD)` and the exported "
      "`include/`, so a quoted `emc/...` form that compiles in userspace does not "
      "compile in a realtime module. Each file is resolved under the rules of the "
      "compile it actually goes through, taken from the `-objs` lists in `src/Makefile`; "
      "`.comp` sources are scanned below their `;;` line and resolved the way "
      "halcompile does, with the component's own directory ahead of the rest. The "
      "exported-header set is read out of `SRCHEADERS`, so it follows what the build "
      "installs rather than a list of its own. Includes that resolve outside the tree "
      "are dropped.\n\n")

    w("## Summary\n\n")
    w(f"- {len(nodes)} directories, {len(edges)} directory-level edges, "
      f"{total_sites} include sites between directories.\n")
    if comps:
        w(f"- {len(comps)} dependency cycle{'s' if len(comps) != 1 else ''} covering "
          f"{len(incycle)} directories, {len(intra)} edges, {intra_sites} include sites:\n")
        for c in comps:
            w(f"  - {', '.join('`%s`' % m for m in c)}\n")
        w(f"- {len(thin)} of those {len(intra)} cycle edges "
          f"{'is' if len(thin) == 1 else 'are'} **a single `#include`**.\n")
    else:
        w("- No dependency cycles.\n")
    w(f"- {len(exported)} exported headers.\n\n")

    if comps:
        w("## The cycles\n\n```mermaid\ngraph LR\n")
        ids = {n: "n%d" % i for i, n in enumerate(sorted(incycle))}
        for n in sorted(incycle):
            w(f'  {ids[n]}["{n}"]\n')
        for (a, b), v in sorted(intra.items()):
            w(f"  {ids[a]} -->|{len(v)}| {ids[b]}\n")
        w("```\n\n")

    if thin:
        w("## Cycle edges that are one include\n\n")
        w("Each of these is a single line. Cutting it removes a directory-level "
          "dependency outright.\n\n")
        w("| edge | site | include |\n|---|---|---|\n")
        for (a, b), v in sorted(thin.items()):
            rel, ln, tgt, style = sorted(v)[0]
            close = '"' if style == '"' else '>'
            w(f"| `{a}` -> `{b}` | `src/{rel}:{ln}` | "
              f"`{style}{os.path.basename(tgt)}{close}` = `src/{tgt}` |\n")
        w("\n")

    heavy = [(k, v) for k, v in intra.items() if len(v) > 1]
    if heavy:
        w("## The heavier knots\n\n<details><summary>Cycle edges carrying two or more "
          "includes</summary>\n\n")
        w("| edge | sites | headers |\n|---|---|---|\n")
        for (a, b), v in sorted(heavy, key=lambda kv: -len(kv[1])):
            hdrs = defaultdict(int)
            for s_ in v:
                hdrs[os.path.basename(s_[2])] += 1
            txt = ", ".join(f"`{h}`" + (f" x{c}" if c > 1 else "")
                            for h, c in sorted(hdrs.items(), key=lambda x: (-x[1], x[0])))
            w(f"| `{a}` -> `{b}` | {len(v)} | {txt} |\n")
        w("\n</details>\n\n")

    rtbad = data["rt_only_via_emc"]
    w("## Includes that would not resolve in a realtime compile\n\n")
    if not rtbad:
        w("None. No realtime-compiled file reaches a header by a form that only the "
          "userspace `-I` list provides.\n\n")
    else:
        w("| file | line | include | resolved in userspace by |\n|---|---|---|---|\n")
        for rel, ln, name, via in sorted(rtbad):
            w(f"| `src/{rel}` | {ln} | `{name}` | {via} |\n")
        w("\n")

    both = sorted(h for h in data["rt_reach"] & data["user_reach"]
                  if h.endswith((".h", ".hh", ".hpp")))
    allh = [f for f in data["relfiles"] if f.endswith((".h", ".hh", ".hpp"))]
    w("## Headers reached from both compiles\n\n")
    w(f"{len(both)} of {len(allh)} headers are pulled in by a realtime compile and by a "
      "userspace one, following includes transitively from the sources each compile "
      "starts at. A header in both sets has to serve both, which is where \"what this "
      "code needs to build\" and \"what other code needs to interface\" stop being the "
      "same question. The graph above records the include either way and cannot tell "
      "the two apart, so this is the list to read beside it.\n\n")
    bydir = defaultdict(list)
    for h in both:
        bydir[module_of(h)].append(h)
    w("| directory | headers | |\n|---|---|---|\n")
    for d in sorted(bydir, key=lambda x: (-len(bydir[x]), x)):
        names = ", ".join("`%s`" % os.path.basename(h) for h in sorted(bydir[d]))
        w(f"| `{d}` | {len(bydir[d])} | {names} |\n")
    w("\n")

    w("## Exported headers with no in-tree user outside their own directory\n\n")
    w("A header reached only from its own directory is a candidate for coming off "
      "`SRCHEADERS`, but not automatically: a header included by its umbrella beside "
      "it, or by code in a subdirectory that falls in the same bucket, shows up here "
      "too.\n\n")
    w("| header | users elsewhere |\n|---|---|\n")
    for h in sorted(exported.values()):
        own = module_of(h)
        outside = sorted({u for u in exported_users.get(h, set()) if module_of(u) != own})
        if len(outside) > 2:
            continue
        cell = ", ".join(f"`src/{u}`" for u in outside) or "none"
        w(f"| `{h}` | {cell} |\n")
    w("\n")

    w("<details><summary>Full directory edge list</summary>\n\n")
    w("| from | to | sites |\n|---|---|---|\n")
    for (a, b), v in sorted(edges.items(), key=lambda kv: (kv[0][0], -len(kv[1]))):
        mark = " (in cycle)" if (a, b) in intra else ""
        w(f"| `{a}` | `{b}`{mark} | {len(v)} |\n")
    w("\n</details>\n")


if __name__ == "__main__":
    main()
