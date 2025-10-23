#!/usr/bin/env bash
# Batch runner for pf_ram-like executable over all *.in files under std/
# For each input file:
#   1. Copy to ./pf_flyer.in (as required by pf_ram)
#   2. Execute target executable (default ./bin/pf_ram.exe or via --exe/ PF_RAM_EXE)
#   3. Rename produced outputs tl.grid, tl.line, p.vert to
#        <inputbase>_<exe_tag>_<originaloutputfilename>
#      and move them into the same directory as the input file.
# Example:
#   std/case1.in with pf_ram.exe -> std/case1_pf_ram_tl.line (and ..._tl.grid, ..._p.vert)
#   std/case1.in with my_alt.exe -> std/case1_my_alt_tl.line etc.
#
# Supports optional --dry-run to preview actions without executing pf_ram.
# By default processes every case. Use --skip-existing to skip cases whose three
# renamed output files already exist. Use --force to override skipping.

# Note: Avoid 'set -e' so one failing case doesn't abort the loop.
set -uo pipefail
IFS=$'\n\t'

usage() {
  cat <<EOF
Usage: $0 [--dry-run] [--force] [--rebuild] [--skip-existing] [--exe <path>] [--pattern <glob>] [--max-depth N] [--input-dir <dir>] [--debug]

Options:
  --dry-run     Show what would be done without running the executable.
  --force       Re-run even if all output files already exist for an input.
    --rebuild       Invoke 'make bin/pf_ram.exe' before processing.
    --exe PATH      Explicit path to executable (overrides PF_RAM_EXE env).
  --skip-existing Skip cases where all three renamed outputs already exist.
  --pattern G     Additional shell glob (without path) to filter *.in files (e.g. 'case1*').
  --max-depth N   Limit directory descent under std (default: unlimited).
  --input-dir DIR Root directory to search for .in files (default: std).
  --debug         Print the list of resolved input files.
  -h, --help    Show this help.

Environment variables:
  PF_RAM_EXE     Path to pf_ram executable (default: ./bin/pf_ram.exe)
  INPUT_ROOT     Root directory to search (default: std)
EOF
}

dry_run=false
force=false
rebuild=false
skip_existing=false
pattern=""
max_depth=""
debug=false
cli_exe=""
cli_input_dir=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --dry-run) dry_run=true; shift;;
        --force) force=true; shift;;
        --rebuild) rebuild=true; shift;;
        --skip-existing) skip_existing=true; shift;;
        --exe) cli_exe="$2"; shift 2;;
        --pattern) pattern="$2"; shift 2;;
        --max-depth) max_depth="$2"; shift 2;;
        --input-dir) cli_input_dir="$2"; shift 2;;
        --debug) debug=true; shift;;
        -h|--help) usage; exit 0;;
        *) echo "Unknown argument: $1" >&2; usage; exit 1;;
    esac
done

PF_RAM_EXE=${PF_RAM_EXE:-./bin/pf_ram.exe}
if [[ -n "$cli_exe" ]]; then
    PF_RAM_EXE="$cli_exe"
fi
INPUT_ROOT=${INPUT_ROOT:-std}
if [[ -n "$cli_input_dir" ]]; then
    INPUT_ROOT="$cli_input_dir"
fi

if [[ ! -d "$INPUT_ROOT" ]]; then
    echo "ERROR: Input root '$INPUT_ROOT' not found" >&2
    exit 1
fi

if $rebuild; then
    echo "[INFO] Rebuilding pf_ram executable..."
    if $dry_run; then
        echo "DRY-RUN: make bin/pf_ram.exe"
    else
        make bin/pf_ram.exe
    fi
fi

if [[ ! -x "$PF_RAM_EXE" ]]; then
    echo "[INFO] Executable $PF_RAM_EXE not found or not executable; attempting build..."
    if $dry_run; then
        echo "DRY-RUN: make ${$PF_RAM_EXE}"
    else
        make ${PF_RAM_EXE} || true
    fi
fi

if [[ ! -x "$PF_RAM_EXE" ]]; then
    echo "ERROR: Cannot locate executable $PF_RAM_EXE after build attempt" >&2
    exit 2
fi

# Derive tag from executable name (basename without extension .exe)
exe_tag=$(basename "$PF_RAM_EXE")
exe_tag=${exe_tag%.exe}
exe_tag=${exe_tag// /_}

#############################################
# Build list of input files
#############################################
if [[ -n "$max_depth" ]]; then
    # -maxdepth must precede tests
    mapfile -t inputs < <(find "$INPUT_ROOT" -maxdepth "$max_depth" -type f -name '*.in' | sort)
else
    mapfile -t inputs < <(find "$INPUT_ROOT" -type f -name '*.in' | sort)
fi

# Optional pattern filtering (shell glob against basename)
if [[ -n "$pattern" ]]; then
    filtered=()
    for f in "${inputs[@]}"; do
        b=$(basename "$f")
        if [[ $b == $pattern ]]; then
            filtered+=("$f")
        fi
    done
    inputs=("${filtered[@]}")
fi

if [[ ${#inputs[@]} -eq 0 ]]; then
    echo "[INFO] No .in files found under $INPUT_ROOT"
    exit 0
fi

printf "[INFO] Found %d input file(s)\n" "${#inputs[@]}"
if $debug; then
    printf '  %s\n' "${inputs[@]}"
fi

fail_count=0
proc_count=0
start_epoch=$(date +%s)

for infile in "${inputs[@]}"; do
    rel_in="$infile"
    dir=$(dirname "$rel_in")
    basefile=$(basename "$rel_in")
    base=${basefile%.in}
    
    out_grid="${dir}/${base}_${exe_tag}_tl.grid"
    out_line="${dir}/${base}_${exe_tag}_tl.line"
    out_vert="${dir}/${base}_${exe_tag}_p.vert"
    
    if $skip_existing && ! $force && [[ -f "$out_grid" && -f "$out_line" && -f "$out_vert" ]]; then
        echo "[SKIP] $rel_in (all outputs already exist)"
        continue
    fi
    
    echo "[RUN ] $rel_in"
    # Copy to pf_flyer.in required by program
    if $dry_run; then
        echo "DRY-RUN: cp '$rel_in' ./pf_flyer.in"
    else
        cp "$rel_in" ./pf_flyer.in
    fi
    
    stdout_cap="${dir}/${base}_${exe_tag}.stdout"
    stderr_cap="${dir}/${base}_${exe_tag}.stderr"
    if $dry_run; then
        echo "DRY-RUN: $PF_RAM_EXE"
    else
        if ! "$PF_RAM_EXE" > "$stdout_cap" 2> "$stderr_cap"; then
            echo "[ERROR] Execution failed for $rel_in (see stdout/stderr captures)" >&2
            ((fail_count++))
            # Even on failure, remove any empty capture files
            [[ -f "$stdout_cap" && ! -s "$stdout_cap" ]] && rm -f "$stdout_cap"
            [[ -f "$stderr_cap" && ! -s "$stderr_cap" ]] && rm -f "$stderr_cap"
            continue
        fi
    fi
    
    # Move/rename outputs if they exist
    for produced in tl.grid tl.line p.vert; do
        if [[ -f "$produced" ]]; then
            case "$produced" in
                tl.grid) new_name="$out_grid";;
                tl.line) new_name="$out_line";;
                p.vert)  new_name="$out_vert";;
            esac
            if $dry_run; then
                echo "DRY-RUN: mv '$produced' '$new_name'"
            else
                mv -f "$produced" "$new_name"
            fi
        else
            echo "[WARN] Expected output '$produced' not found for $rel_in" >&2
        fi
    done
    # Remove empty stdout/stderr capture files (clutter cleanup)
    if ! $dry_run; then
        [[ -f "$stdout_cap" && ! -s "$stdout_cap" ]] && rm -f "$stdout_cap"
        [[ -f "$stderr_cap" && ! -s "$stderr_cap" ]] && rm -f "$stderr_cap"
    fi
    ((proc_count++))
    
done

elapsed=$(( $(date +%s) - start_epoch ))
echo "[DONE] Processed $proc_count input(s); failures: $fail_count; elapsed ${elapsed}s"
if $dry_run; then
    echo "[NOTE] Dry-run mode: no executions performed."
fi

exit $(( fail_count > 0 ? 3 : 0 ))
