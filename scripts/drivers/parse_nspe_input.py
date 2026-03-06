#!/usr/bin/env python3
"""Parse NSPE input files and estimate computational complexity.

Extracts key parameters from .in files and computes a first-order
estimate of the PE computational grid size and relative complexity.

Usage:
    parse_nspe_input.py <file.in> [file2.in ...]
    parse_nspe_input.py <directory>
    parse_nspe_input.py <directory> --timing <_timing.log>
    parse_nspe_input.py <directory> --tsv

Physics basis (RAM PE solver):
    c0      = 1485 m/s (reference sound speed)
    lambda  = c0 / freq
    dz      = lambda * dzfact(speedDial)
    dr      = lambda * drfact(speedDial)
    N_z     = bmax / dz          (vertical grid points in water column)
    N_r     = Rmax / dr          (range steps)
    work    ~ N_r * N_z          (proportional to total floating-point ops)

SpeedDial grid factors (from setspeed.f):
    SD=1:  drfact=1.25,  dzfact=1/20   (finest)
    SD=5:  drfact=17.0,  dzfact=1/3.5  (default breakpoint)
    SD=10: drfact=50.0,  dzfact=1/3.0  (coarsest)
"""

import argparse
import math
import os
import sys

C0 = 1485.0  # reference sound speed (m/s)


def speeddial_factors(sd):
    """Return (drfact, dzfact) for a given speedDial value.

    Reproduces the piecewise-linear interpolation in setspeed.f.
    Does not include bathyTweak or freqTweak adjustments.
    """
    if sd <= 0:
        # Manual mode; return nominal SD=5 values as placeholder
        return 17.0, 1.0 / 3.5
    if sd > 10:
        return 40.0, 1.0 / 3.0

    breakR, breakZ = 17.0, 3.5

    if sd <= 5.0:
        si = 1.0
        rs = (breakR - 1.25) / (5.0 - 1.0)
        ri = 1.25
        zs = (breakZ - 20.0) / (5.0 - 1.0)
        zi = 20.0
    else:
        si = 5.0
        rs = (50.0 - breakR) / (10.0 - 5.0)
        ri = breakR
        zs = (3.0 - breakZ) / (10.0 - 5.0)
        zi = breakZ

    drfact = ri + rs * (sd - si)
    dzfact = 1.0 / (zi + zs * (sd - si))
    return drfact, dzfact


def parse_input_file(filepath):
    """Parse an NSPE .in file and extract key parameters.

    Returns a dict with extracted and derived values.
    """
    result = {
        "file": os.path.basename(filepath),
        "title": "",
        "speeddial": None,
        "freq": None,
        "src_depth": None,
        "rmax": None,
        "bathy_max": None,
        "bathy_min": None,
        "bathy_points": 0,
        "bottom_type": "",
        "surface_type": "",
        "n_tl_depths": 0,
        "source_type": "",
    }

    with open(filepath, "r") as f:
        lines = f.readlines()

    # State machine to parse the sequential blocks
    state = "title"
    bathy_depths = []
    i = 0

    while i < len(lines):
        line = lines[i].strip()
        llow = line.lower()

        if state == "title":
            if i == 0:
                result["title"] = line.rstrip()
            state = "scan"
            i += 1
            continue

        if state == "scan":
            if llow == "nspe":
                state = "nspe"
            elif llow.startswith("range"):
                state = "range"
            elif llow.startswith("source"):
                state = "source"
            elif llow.startswith("bathymetry"):
                state = "bathymetry"
            elif llow == "svp":
                state = "svp"
            elif llow.startswith("bottom"):
                state = "bottom"
            elif llow.startswith("surface"):
                state = "surface"
            elif llow.startswith("output"):
                state = "output"
            elif llow == "end":
                break
            i += 1
            continue

        if state == "nspe":
            # Next non-blank line is the speeddial value
            if (
                line
                and not llow.startswith("earth")
                and not llow.startswith("cfill")
                and not llow.startswith("crit")
                and not llow.startswith("blam")
                and not llow.startswith("layers")
                and not llow.startswith("cz")
                and not llow.startswith("metric")
                and not llow.startswith("high")
            ):
                try:
                    result["speeddial"] = float(line.split()[0])
                    state = "nspe_opts"
                except ValueError:
                    pass
            i += 1
            continue

        if state == "nspe_opts":
            # Skip optional nspe block keywords until next block
            if llow in (
                "range",
                "source",
                "bathymetry",
                "svp",
                "bottom",
                "surface",
                "output",
                "end",
            ) or llow.startswith("range"):
                state = "scan"
                continue
            i += 1
            continue

        if state == "range":
            if line:
                try:
                    result["rmax"] = float(line.split()[0])
                    state = "scan"
                except ValueError:
                    pass
            i += 1
            continue

        if state == "source":
            # First line after 'source' is the source type keyword
            if line:
                result["source_type"] = line.split()[0].lower()
                state = "source_params"
            i += 1
            continue

        if state == "source_params":
            # First data line has freq and source_depth
            if line and not line.startswith("-"):
                parts = line.split()
                if len(parts) >= 2:
                    try:
                        result["freq"] = float(parts[0])
                        result["src_depth"] = float(parts[1])
                    except ValueError:
                        pass
                state = "scan"
            i += 1
            continue

        if state == "bathymetry":
            if line.startswith("-1"):
                state = "scan"
                i += 1
                continue
            parts = line.split()
            if len(parts) >= 2:
                try:
                    depth = float(parts[1])
                    bathy_depths.append(depth)
                except ValueError:
                    pass
            i += 1
            continue

        if state == "svp":
            # Skip through SVP block until we hit the block terminator
            # SVP ends with -1 (after the -1 -1 for profiles and -1 for ranges)
            if line.startswith("-1") and len(line.split()) == 1:
                state = "scan"
            i += 1
            continue

        if state == "bottom":
            if line:
                result["bottom_type"] = line.split()[0]
                state = "bottom_skip"
            i += 1
            continue

        if state == "bottom_skip":
            # Skip until next block keyword
            if llow in ("surface", "output", "end"):
                state = "scan"
                continue
            i += 1
            continue

        if state == "surface":
            # Capture surface type(s)
            if not line:
                state = "scan"
                i += 1
                continue
            if llow in ("output", "end"):
                state = "scan"
                continue
            stype = line.split()[0].lower()
            if stype in ("wind", "ice", "eice", "table", "blam", "aux"):
                if result["surface_type"]:
                    if stype not in result["surface_type"]:
                        result["surface_type"] += "+" + stype
                else:
                    result["surface_type"] = stype
            # Skip surface data lines (read until next keyword or blank)
            i += 1
            continue

        if state == "output":
            if llow == "tl" or llow == "rtl":
                state = "tl_depths"
            elif llow in ("end", "field"):
                state = "scan"
            i += 1
            continue

        if state == "tl_depths":
            if line.startswith("-"):
                state = "output"
                i += 1
                continue
            try:
                float(line.split()[0])
                result["n_tl_depths"] += 1
            except (ValueError, IndexError):
                state = "output"
                continue
            i += 1
            continue

        i += 1

    # Compute bathymetry statistics
    if bathy_depths:
        result["bathy_max"] = max(bathy_depths)
        result["bathy_min"] = min(bathy_depths)
        result["bathy_points"] = len(bathy_depths)

    # Compute derived grid parameters
    compute_complexity(result)

    return result


def compute_complexity(r):
    """Compute derived grid and complexity parameters."""
    if r["freq"] is None or r["freq"] <= 0:
        return
    if r["rmax"] is None or r["rmax"] <= 0:
        return
    if r["bathy_max"] is None or r["bathy_max"] <= 0:
        return

    freq = r["freq"]
    rmax = r["rmax"]
    bmax = r["bathy_max"]
    sd = r["speeddial"] if r["speeddial"] is not None else 5.0

    wavelength = C0 / freq
    drfact, dzfact = speeddial_factors(sd)

    dz = wavelength * dzfact
    dr = wavelength * drfact

    nz = int(bmax / dz + 5)
    nr = int(rmax / dr + 0.5)

    r["wavelength"] = wavelength
    r["dz"] = dz
    r["dr"] = dr
    r["nz"] = nz
    r["nr"] = nr
    r["grid_work"] = nr * nz  # total tridiagonal solves * grid size


def format_table(results, show_grid=True):
    """Format results as a human-readable table."""
    # Header
    cols = [
        ("FILE", 40, "l"),
        ("SD", 4, "r"),
        ("FREQ", 8, "r"),
        ("RMAX_km", 8, "r"),
        ("BMAX_m", 8, "r"),
        ("BOTTOM", 12, "l"),
        ("SURFACE", 8, "l"),
        ("SRC", 7, "l"),
        ("nTL", 4, "r"),
    ]
    if show_grid:
        cols += [
            ("dz_m", 7, "r"),
            ("dr_m", 7, "r"),
            ("Nz", 8, "r"),
            ("Nr", 8, "r"),
            ("WORK", 12, "r"),
        ]

    # Build format strings
    header = ""
    sep = ""
    for name, width, align in cols:
        if align == "l":
            header += f"{name:<{width}}  "
        else:
            header += f"{name:>{width}}  "
        sep += "-" * width + "  "

    lines = [header.rstrip(), sep.rstrip()]

    for r in results:
        parts = []
        parts.append(f"{r['file']:<40}")
        parts.append(f"{r['speeddial'] or '-':>4}")
        parts.append(f"{r['freq'] or '-':>8}")
        rmax_km = f"{r['rmax']/1000:.1f}" if r["rmax"] else "-"
        parts.append(f"{rmax_km:>8}")
        parts.append(f"{r['bathy_max'] or '-':>8}")
        parts.append(f"{r['bottom_type'][:12]:<12}")
        parts.append(f"{r['surface_type'][:8] or '-':<8}")
        parts.append(f"{r['source_type'][:7]:<7}")
        parts.append(f"{r['n_tl_depths']:>4}")

        if show_grid and "nz" in r:
            parts.append(f"{r['dz']:.2f}" if "dz" in r else "-")
            parts.append(f"{r['dr']:.1f}" if "dr" in r else "-")
            parts.append(f"{r['nz']:>8}" if "nz" in r else "-")
            parts.append(f"{r['nr']:>8}" if "nr" in r else "-")
            work = r.get("grid_work")
            if work:
                if work >= 1e9:
                    parts.append(f"{work:.3e}")
                else:
                    parts.append(f"{work:>12}")
            else:
                parts.append(f"{'-':>12}")

        line = "  ".join(parts)
        lines.append(line)

    return "\n".join(lines)


def format_tsv(results):
    """Format results as TSV for easy import to spreadsheets or joining with timing."""
    header = "\t".join(
        [
            "file",
            "speeddial",
            "freq",
            "rmax",
            "bathy_max",
            "bathy_min",
            "bottom_type",
            "surface_type",
            "source_type",
            "n_tl_depths",
            "wavelength",
            "dz",
            "dr",
            "nz",
            "nr",
            "grid_work",
        ]
    )
    lines = [header]
    for r in results:
        vals = [
            r["file"],
            f"{r['speeddial']}" if r["speeddial"] is not None else "",
            f"{r['freq']}" if r["freq"] is not None else "",
            f"{r['rmax']}" if r["rmax"] is not None else "",
            f"{r['bathy_max']}" if r["bathy_max"] is not None else "",
            f"{r['bathy_min']}" if r["bathy_min"] is not None else "",
            r["bottom_type"],
            r["surface_type"],
            r["source_type"],
            str(r["n_tl_depths"]),
            f"{r['wavelength']:.4f}" if "wavelength" in r else "",
            f"{r['dz']:.4f}" if "dz" in r else "",
            f"{r['dr']:.4f}" if "dr" in r else "",
            str(r.get("nz", "")),
            str(r.get("nr", "")),
            str(r.get("grid_work", "")),
        ]
        lines.append("\t".join(vals))
    return "\n".join(lines)


def load_timing(timing_path):
    """Load timing log and return dict of {filename: [elapsed_s, ...]}."""
    timing = {}
    with open(timing_path, "r") as f:
        for line in f:
            parts = line.strip().split("\t")
            if len(parts) < 7 or parts[0] == "hostname":
                continue
            fname = parts[4]  # input_file
            status = parts[5]
            if status != "PASS":
                continue
            try:
                elapsed = float(parts[6])
            except (ValueError, IndexError):
                continue
            timing.setdefault(fname, []).append(elapsed)
    return timing


def format_combined(results, timing):
    """Format results combined with timing data."""
    header = (
        f"{'FILE':<40}  {'SD':>4}  {'FREQ':>8}  {'RMAX_km':>8}  "
        f"{'BMAX_m':>8}  {'BOTTOM':<12}  "
        f"{'WORK':>12}  {'T_mean':>8}  {'T_std':>8}  {'N':>3}  "
        f"{'WORK/T':>12}"
    )
    sep = "-" * len(header)
    lines = [header, sep]

    for r in results:
        fname = r["file"]
        tdata = timing.get(fname, [])
        work = r.get("grid_work")

        if tdata:
            t_mean = sum(tdata) / len(tdata)
            t_std = (
                sum((t - t_mean) ** 2 for t in tdata) / max(len(tdata) - 1, 1)
            ) ** 0.5
            n_runs = len(tdata)
            work_per_t = f"{work / t_mean:.0f}" if work and t_mean > 0 else "-"
        else:
            t_mean = t_std = 0
            n_runs = 0
            work_per_t = "-"

        rmax_km = f"{r['rmax']/1000:.1f}" if r["rmax"] else "-"
        work_s = f"{work:>12}" if work else f"{'-':>12}"
        t_mean_s = f"{t_mean:.4f}" if n_runs else "-"
        t_std_s = f"{t_std:.4f}" if n_runs > 1 else "-"

        line = (
            f"{fname:<40}  {r['speeddial'] or '-':>4}  "
            f"{r['freq'] or '-':>8}  {rmax_km:>8}  "
            f"{r['bathy_max'] or '-':>8}  {r['bottom_type'][:12]:<12}  "
            f"{work_s}  {t_mean_s:>8}  {t_std_s:>8}  {n_runs:>3}  "
            f"{work_per_t:>12}"
        )
        lines.append(line)

    return "\n".join(lines)


def format_combined_tsv(results, timing):
    """Format combined results + timing as TSV."""
    header = "\t".join(
        [
            "file",
            "speeddial",
            "freq",
            "rmax",
            "bathy_max",
            "bathy_min",
            "bottom_type",
            "surface_type",
            "source_type",
            "n_tl_depths",
            "wavelength",
            "dz",
            "dr",
            "nz",
            "nr",
            "grid_work",
            "timing_mean",
            "timing_std",
            "timing_n",
            "work_per_sec",
        ]
    )
    lines = [header]
    for r in results:
        fname = r["file"]
        tdata = timing.get(fname, [])
        if tdata:
            t_mean = sum(tdata) / len(tdata)
            t_std = (
                sum((t - t_mean) ** 2 for t in tdata) / max(len(tdata) - 1, 1)
            ) ** 0.5
            work = r.get("grid_work")
            wps = f"{work / t_mean:.0f}" if work and t_mean > 0 else ""
        else:
            t_mean = t_std = 0
            wps = ""

        vals = [
            r["file"],
            f"{r['speeddial']}" if r["speeddial"] is not None else "",
            f"{r['freq']}" if r["freq"] is not None else "",
            f"{r['rmax']}" if r["rmax"] is not None else "",
            f"{r['bathy_max']}" if r["bathy_max"] is not None else "",
            f"{r['bathy_min']}" if r["bathy_min"] is not None else "",
            r["bottom_type"],
            r["surface_type"],
            r["source_type"],
            str(r["n_tl_depths"]),
            f"{r['wavelength']:.4f}" if "wavelength" in r else "",
            f"{r['dz']:.4f}" if "dz" in r else "",
            f"{r['dr']:.4f}" if "dr" in r else "",
            str(r.get("nz", "")),
            str(r.get("nr", "")),
            str(r.get("grid_work", "")),
            f"{t_mean:.6f}" if tdata else "",
            f"{t_std:.6f}" if len(tdata) > 1 else "",
            str(len(tdata)) if tdata else "",
            wps,
        ]
        lines.append("\t".join(vals))
    return "\n".join(lines)


def print_summary(results):
    """Print aggregate statistics."""
    freqs = sorted(set(r["freq"] for r in results if r["freq"]))
    sds = sorted(set(r["speeddial"] for r in results if r["speeddial"] is not None))
    bottoms = sorted(set(r["bottom_type"] for r in results if r["bottom_type"]))
    works = [r["grid_work"] for r in results if r.get("grid_work")]

    print(f"\nInput File Analysis Summary")
    print(f"{'=' * 40}")
    print(f"  Files:        {len(results)}")
    print(f"  Frequencies:  {', '.join(str(f) for f in freqs)}")
    print(f"  SpeedDials:   {', '.join(str(s) for s in sds)}")
    print(f"  Bottom types: {', '.join(bottoms)}")
    if works:
        print(f"  Work range:   {min(works):,.0f} — {max(works):,.0f}")
        print(f"  Work ratio:   {max(works) / max(min(works), 1):.1f}x")


def main():
    parser = argparse.ArgumentParser(
        description="Parse NSPE input files and estimate computational complexity.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument(
        "inputs", nargs="+", help="Input file(s) or directory containing .in files"
    )
    parser.add_argument(
        "--tsv", action="store_true", help="Output tab-separated values"
    )
    parser.add_argument(
        "--timing", metavar="LOG", help="Timing log file to join with analysis"
    )
    parser.add_argument(
        "--sort",
        metavar="FIELD",
        default=None,
        help="Sort by field (freq, rmax, bathy_max, work, file)",
    )
    parser.add_argument(
        "--no-grid", action="store_true", help="Omit grid/complexity columns"
    )
    parser.add_argument(
        "--summary", action="store_true", help="Print aggregate summary"
    )

    args = parser.parse_args()

    # Collect input files
    files = []
    for inp in args.inputs:
        if os.path.isdir(inp):
            for f in sorted(os.listdir(inp)):
                if f.endswith(".in"):
                    files.append(os.path.join(inp, f))
        elif os.path.isfile(inp):
            files.append(inp)
        else:
            print(f"Warning: {inp} not found, skipping", file=sys.stderr)

    if not files:
        print("No .in files found.", file=sys.stderr)
        sys.exit(1)

    # Parse all files
    results = []
    for f in files:
        try:
            results.append(parse_input_file(f))
        except Exception as e:
            print(f"Warning: error parsing {f}: {e}", file=sys.stderr)

    # Sort if requested
    sort_keys = {
        "freq": lambda r: r["freq"] or 0,
        "rmax": lambda r: r["rmax"] or 0,
        "bathy_max": lambda r: r["bathy_max"] or 0,
        "work": lambda r: r.get("grid_work", 0) or 0,
        "file": lambda r: r["file"],
        "sd": lambda r: r["speeddial"] or 0,
        "speeddial": lambda r: r["speeddial"] or 0,
    }
    if args.sort and args.sort in sort_keys:
        results.sort(key=sort_keys[args.sort])

    # Output
    timing = None
    if args.timing:
        timing = load_timing(args.timing)

    if timing:
        if args.tsv:
            print(format_combined_tsv(results, timing))
        else:
            print(format_combined(results, timing))
    else:
        if args.tsv:
            print(format_tsv(results))
        else:
            print(format_table(results, show_grid=not args.no_grid))

    if args.summary:
        print_summary(results)


if __name__ == "__main__":
    main()
