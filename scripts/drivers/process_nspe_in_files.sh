#!/bin/bash
# Usage: ./process_in_files.sh <mode> [directory] [options]
# Aug 2025 JCL
# =============================
# EXPLANATION SECTION
#
# This script processes .in files in a directory using the specified executable.
# Note: Input files are copied to a temporary file (default: nspe.in) before execution
# (for compatibility with executables that only read a specific input name).
# Output files are then renamed to match the original input file basename.
#
# Input options (first argument):
#   make - Runs the executable only (no file operations)
#   copy - Copies existing output files to reference names (.tl, .rtl, .ftl)
#   test - Runs the executable, logs output, and compares results to reference files
#   diff - Compares existing output files to reference files (does not run the executable)
#
# PROMPT: To process files, run:
#   ./process_in_files.sh <mode> [directory] [options]
# Where <mode> is 'make', 'copy', 'test', or 'diff'. Directory defaults to 'std'.
#
# Additional options:
#   --exe <path>        Specify executable path (default: auto-detected)
#   --pattern <glob>    Include files matching glob pattern (can be used multiple times)
#   --exclude <glob>    Exclude files matching glob pattern (can be used multiple times)
#   --skip-existing     Skip files where outputs already exist
#   --skip-newer        Skip files where outputs are newer than input
#   --force             Override skip options and process all matched files
#   --keep-bin          Keep all binary and extra output files (default: only keep ASCII files with references)
#   --diff-level <N>    Force diff level: 1=diff only, 2=max tldiff, 3=force uband_diff (default: auto hierarchy)
#   --dry-run           Show what would be processed without running
#   --debug             Show detailed file filtering information
#   -h, --help          Show this help message
#
# Dependencies:
#   - lib_diff_utils.sh (must be in same directory or PATH)
#   - Executable (auto-detected or specified via --exe)
#   - Optional: tldiff, uband_diff for advanced comparisons
#
# Replaces functionality of:
#
#   script             | mode
#   -------------------+------
#   std/copy_std.bat   | COPY
#   std/mkstd          | MAKE
#   std/testram        | TEST
#   std/testram_getarg | TEST
#
# =============================

usage() {
  cat <<EOF
Usage: $0 <mode> [directory] [options]

Modes:
  make    Run executable only (no file operations)
  copy    Copy existing output files to reference names (.tl, .rtl, .ftl)
  test    Run executable and compare outputs to reference files
  diff    Compare existing output files to reference files (no program execution)

Options:
  --exe <path>        Specify executable path (default: auto-detected)
  --input-name <name> Specify input filename to copy .in files to (default: nspe.in)
  --pattern <glob>    Include files matching glob pattern (can be used multiple times)
  --exclude <glob>    Exclude files matching glob pattern (can be used multiple times)
  --skip-existing     Skip files where outputs already exist
  --skip-newer        Skip files where outputs are newer than input
  --force             Override skip options and process all matched files
  --keep-bin          Keep all binary and extra output files (default: only keep ASCII files with references)
  --diff-level <N>    Force diff level: 1=diff only, 2=max tldiff, 3=force uband_diff (default: auto hierarchy)
  --stop-on-error     Stop processing remaining files if an error occurs
  --no-make           Skip automatic 'make' command before running (for make/test modes)
  --dry-run           Show what would be processed without running
  --debug             Show detailed file filtering information
  -h, --help          Show this help message

Examples:
  $0 test std --exe ./prog.x --pattern 'case*'
  $0 test std --pattern 'case*' --exclude 'case_old*'
  $0 make . --skip-existing --debug
  $0 diff std --pattern 'test1*' --pattern 'test2*'
  $0 test std --stop-on-error --pattern 'case*'
  $0 test std --input-name pf_flyer.in --pattern 'case*'

Environment variables:
  NSPE_EXE            Alternative way to specify executable path
EOF
}

# Parse command line arguments
patterns=()
excludes=()
skip_existing=false
skip_newer=false
force=false
dry_run=false
debug=false
keep_bin=false
cli_exe=""
diff_level=0  # 0 = auto (default), 1 = diff only, 2 = max tldiff, 3 = force uband_diff
no_make=false
input_filename="nspe.in"  # Default input filename
stop_on_error=false

# Handle help first
if [[ $# -eq 0 ]]; then
    echo "Error: Mode must be specified as first argument" >&2
    usage
    exit 1
fi

if [[ "$1" == "-h" || "$1" == "--help" ]]; then
    usage
    exit 0
fi

# First argument must be mode
if [[ "$1" =~ ^- ]]; then
    echo "Error: Mode must be specified as first argument" >&2
    usage
    exit 1
fi

mode="$1"
shift

# Check if second argument is directory or an option
if [[ $# -gt 0 && ! "$1" =~ ^- ]]; then
    # Second argument is directory
    directory="$1"
    shift
else
    # No directory specified, use default
    directory="std"
fi

# Parse remaining options
while [[ $# -gt 0 ]]; do
    case "$1" in
        --exe) cli_exe="$2"; shift 2;;
        --input-name) input_filename="$2"; shift 2;;
        --pattern) patterns+=("$2"); shift 2;;
        --exclude) excludes+=("$2"); shift 2;;
        --skip-existing) skip_existing=true; shift;;
        --skip-newer) skip_newer=true; shift;;
        --force) force=true; shift;;
        --keep-bin) keep_bin=true; shift;;
        --diff-level) diff_level="$2"; shift 2;;
        --stop-on-error) stop_on_error=true; shift;;
        --dry-run) dry_run=true; shift;;
        --debug) debug=true; shift;;
        --no-make) no_make=true; shift;;
        -h|--help) usage; exit 0;;
        *) echo "Unknown option: $1" >&2; usage; exit 1;;
    esac
done

# Source the utility library
SCRIPT_DIR="$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")"
LIBRARY_FILE="${SCRIPT_DIR}/lib_diff_utils.sh"

if [[ -f "$LIBRARY_FILE" ]]; then
    source "$LIBRARY_FILE"
else
    echo -e "\e[31mError: Required library file '$LIBRARY_FILE' not found.\e[0m" >&2
    echo "Please ensure lib_diff_utils.sh is in the same directory as this script." >&2
    exit 1
fi

set -e

# Validate diff_level
if [[ ! "$diff_level" =~ ^[0-3]$ ]]; then
    echo -e "\e[31mError: Invalid diff level '$diff_level'.\e[0m"
    echo "Valid diff levels are:"
    echo "  0 - Auto (default hierarchical behavior)"
    echo "  1 - diff only (no fallback to tldiff or uband_diff)"
    echo "  2 - max tldiff (stop at tldiff, don't try uband_diff)"
    echo "  3 - force uband_diff (always run all three diff tools)"
    exit 1
fi

# Set default directory if not provided
directory="${directory:-std}"

# Remove trailing slash from directory to avoid double slashes in file paths
directory="${directory%/}"

# Validate mode
if [[ -z "$mode" ]]; then
    echo -e "\e[31mError: No mode specified.\e[0m"
    echo "Usage: $0 <mode> [directory]"
    echo "mode: make, copy, test, or diff"
    echo "directory: defaults to 'std' if not specified"
    exit 1
fi

# Validate that mode is one of the accepted values
if [[ "$mode" != "make" && "$mode" != "copy" && "$mode" != "test" && "$mode" != "diff" ]]; then
    echo -e "\e[31mError: Invalid mode '$mode'.\e[0m"
    echo "Valid modes are: make, copy, test, or diff"
    echo ""
    echo "Mode descriptions:"
    echo "  make - Run executable only (no file operations) [mkstd]"
    echo "  copy - Copy existing output files to reference files [copy_std.bat]"
    echo "  test - Time and run executable and compare outputs to reference files [testram or testram_getarg]"
    echo "  diff - Compare existing output files to reference files (no program execution)"
    exit 1
fi

if [[ ! -d "$directory" ]]; then
    echo "Error: Directory '$directory' does not exist. Exiting."
    exit 1
fi

# Intelligent program detection - handles various executable names or makefile targets

detect_program() {
    local candidates=("./nspe.x" "./nspe.exe" "nspe.x" "nspe.exe")
    local makefile_targets=()

    # Check for existing executables first
    for prog in "${candidates[@]}"; do
        if [[ -f "$prog" && -x "$prog" ]]; then
            echo "Found existing executable: $prog" >&2
            echo "$prog"
            return 0
        fi
    done

    # If no executable found, check makefile for available targets
    if [[ -f "makefile" ]] || [[ -f "Makefile" ]]; then
        echo "No executable found. Checking makefile for executable targets..." >&2

        # Look for executable targets in makefile (case insensitive)
        if command -v make >/dev/null 2>&1; then
            # Try to get makefile targets (this works with GNU make)
            local make_targets
            make_targets=$(make -qp 2>/dev/null | grep -E '^[^.%#[:space:]][^=]*:' | cut -d: -f1 | grep -i nspe || true)

            if [[ -n "$make_targets" ]]; then
                echo "Found makefile targets containing 'nspe':" >&2
                echo "$make_targets" | sed 's/^/  /' >&2

                # Prefer .x over .exe, then first match
                for target in nspe.x nspe.exe $make_targets; do
                    if echo "$make_targets" | grep -q "^$target$"; then
                        echo "Selected target: $target" >&2
                        if [[ "$target" == nspe.* ]]; then
                            echo "./$target"
                        else
                            echo "$target"
                        fi
                        return 0
                    fi
                done

                # If no exact match, use first target found
                local first_target
                first_target=$(echo "$make_targets" | head -n1)
                echo "Using first available target: $first_target" >&2
                if [[ "$first_target" == nspe.* ]]; then
                    echo "./$first_target"
                else
                    echo "$first_target"
                fi
                return 0
            fi
        fi
    fi

    # Default fallback
    echo "No executable or makefile target found. Using default: ./nspe.x" >&2
    echo "./nspe.x"
    return 0
}
PROG_OUTPUT_COLOR="\x1B[38;5;71m" # Light green color for PROG output
PROG_OUTPUT_COLOR="\x1B[0m" # Reset color for PROG output

# Run 'make' unless --no-make flag is set (for make and test modes)
if [[ "$no_make" = false && ("$mode" == "make" || "$mode" == "test") ]]; then
    echo "Running 'make' to ensure executables are up to date..."
    if ! make; then
        echo -e "\e[31mError: 'make' command failed. Exiting.\e[0m"
        exit 1
    fi
    echo "'make' completed successfully."
fi

# Detect the appropriate executable (skip for diff and copy modes)
if [[ "$mode" != "diff" && "$mode" != "copy" ]]; then
    # Set the program executable
    # Priority: command line --exe > NSPE_EXE environment variable > auto-detect
    if [[ -n "$cli_exe" ]]; then
        PROG="$cli_exe"
        elif [[ -n "$NSPE_EXE" ]]; then
        PROG="$NSPE_EXE"
    else
        PROG=$(detect_program)
    fi
    echo "Selected program: $PROG"

    # Check if the program exists, and build it if not
    if [[ ! -f "$PROG" ]]; then
        echo "Program $PROG not found. Attempting to build it..."
        if command -v make >/dev/null 2>&1; then
            # Extract just the target name for make (remove ./ prefix)
            make_target="${PROG#./}"
            echo "Running: make $make_target"
            echo -en "${PROG_OUTPUT_COLOR}"
            if make "$make_target"; then
                echo -en "\x1B[0m" # Reset text color
                echo "Successfully built $PROG"
            else
                echo -e "\e[31mError: Failed to build $PROG. Please check your makefile and dependencies.\e[0m"
                exit 1
            fi
        else
            echo -e "\e[31mError: make command not found and $PROG does not exist.\e[0m"
            echo "Please build $PROG manually or install make."
            exit 1
        fi
    fi

    # Verify the program is now executable
    if [[ ! -x "$PROG" ]]; then
        echo -e "\e[31mError: $PROG exists but is not executable.\e[0m"
        echo "Making it executable..."
        chmod +x "$PROG"
    fi
else
    echo "Copy/Diff mode: Skipping $PROG program detection and execution"
fi

# Create array of files to process, sorted numerically by filename
mapfile -t infiles < <(find "$directory" -maxdepth 1 -type f -name '*.in' -print0 | xargs -0 ls -1 | sort -V)

if [[ ${#infiles[@]} -eq 0 ]]; then
    echo
    echo -e "\e[31mNo input files (*.in) found in directory: $directory\e[0m"
    exit 1
fi

# Exclude the temporary input filename from processing (it's created by the script)
filtered=()
for f in "${infiles[@]}"; do
    b=$(basename "$f")
    if [[ "$b" != "$input_filename" ]]; then
        filtered+=("$f")
    fi
done
infiles=("${filtered[@]}")

if [[ ${#infiles[@]} -eq 0 ]]; then
    echo
    echo -e "\e[31mNo input files (*.in) found in directory: $directory (after excluding $input_filename)\e[0m"
    exit 1
fi

# Apply pattern filtering
if [[ ${#patterns[@]} -gt 0 ]]; then
    if $debug; then
        printf "[DEBUG] Applying include patterns: %s\n" "${patterns[*]}"
    fi
    filtered=()
    for f in "${infiles[@]}"; do
        b=$(basename "$f")
        matched=false
        for pattern in "${patterns[@]}"; do
            if [[ $b == $pattern ]]; then
                matched=true
                break
            fi
        done
        if $matched; then
            filtered+=("$f")
        fi
    done
    infiles=("${filtered[@]}")
    if $debug; then
        printf "[DEBUG] After include filtering: %d files remain\n" "${#infiles[@]}"
    fi
fi

# Apply exclude pattern filtering
if [[ ${#excludes[@]} -gt 0 ]]; then
    if $debug; then
        printf "[DEBUG] Applying exclude patterns: %s\n" "${excludes[*]}"
    fi
    filtered=()
    for f in "${infiles[@]}"; do
        b=$(basename "$f")
        excluded=false
        for exclude in "${excludes[@]}"; do
            if [[ $b == $exclude ]]; then
                excluded=true
                break
            fi
        done
        if ! $excluded; then
            filtered+=("$f")
        fi
    done
    infiles=("${filtered[@]}")
    if $debug; then
        printf "[DEBUG] After exclude filtering: %d files remain\n" "${#infiles[@]}"
    fi
fi

if [[ ${#infiles[@]} -eq 0 ]]; then
    echo
    echo -e "\e[33mNo input files match the specified patterns in directory: $directory\e[0m"
    exit 0
fi
pass_files=()
fail_files=()
missing_reference_files=()
missing_output_files=()
empty_files=()
empty_output_files=()
processed_files=()
skipped_files=()
exec_success_files=()
exec_fail_files=()
missing_exec_output_files=()
simple_diff_fail_files=()
tldiff_fail_files=()
uband_diff_fail_files=()

# Helper function to add file to array only if not already present
add_to_array_if_not_present() {
    local array_name="$1"
    local file="$2"
    local -n array_ref="$array_name"

    # Check if file is already in array
    local item
    for item in "${array_ref[@]}"; do
        if [[ "$item" == "$file" ]]; then
            return 0  # File already present, don't add
        fi
    done

    # File not found, add it
    array_ref+=("$file")
}

# Print mode header once at the beginning
term_width=$(tput cols 2>/dev/null || echo 80)
line_len=$((term_width * 5 / 10))

printf '%*s\n' "$line_len" '' | tr '  ' '='
echo "Processing directory: $directory"
echo "Mode: $mode"
if [[ "$mode" == "make" ]]; then
    echo "Will run ${PROG} only (no file operations)"
    elif [[ "$mode" == "copy" ]]; then
    echo "Will copy existing output files (.asc) to reference files (.tl, .rtl, .ftl)"
    elif [[ "$mode" == "test" ]]; then
    echo "Will run ${PROG} and compare outputs to reference files"
    elif [[ "$mode" == "diff" ]]; then
    echo "Will compare existing output files to reference files (no ${PROG} execution)"
fi
# Show diff level if specified
if [[ $diff_level -ne 0 && ("$mode" == "test" || "$mode" == "diff") ]]; then
    case $diff_level in
        1) echo "Diff level: 1 (diff only - no fallback)" ;;
        2) echo "Diff level: 2 (max tldiff - stop at tldiff)" ;;
        3) echo "Diff level: 3 (force uband_diff - always run all tools)" ;;
    esac
fi
printf '%*s\n' "$line_len" '' | tr '  ' '='

for infile in "${infiles[@]}"; do
    # Check columns 76-80 on the first line for override behavior
    # Read the first line safely (handle empty files)
    first_line=""
    if IFS= read -r line < "$infile"; then
        first_line="$line"
    fi
    # Extract columns 76-80 (1-based). If line shorter than 80 chars, substring will be empty
    col76_80="${first_line:75:5}"
    # Trim trailing and leading whitespace
    col76_80_trimmed="$(echo -n "$col76_80" | awk '{gsub(/^\s+|\s+$/,"",$0); print $0}')"

    if [[ -z "$col76_80_trimmed" ]]; then
        echo "   Columns 76-80: [blank] — overriding content checks and proceeding"
        echo "   Running split-step Fourier method (PE)..."
        model_blank=true
    else
        echo "   Columns 76-80: '$col76_80_trimmed' — content found"
        echo "   Running split-step Pade method (RAM)..."
        model_blank=false
    fi

    # Check for required strings (case-insensitive). If columns 76-80 are blank, override and allow processing.
    if $model_blank || (grep -qi '^tl' "$infile" || grep -qi '^rtl' "$infile" || grep -Eiq '^hrfa|^hfra|^hari' "$infile"); then
        if $dry_run; then
            echo "[DRY-RUN] Would process: $infile"
            continue
        fi

        echo "Processing: $infile"
        LOG_FILE="$directory/$(basename "$infile" .in).log"
        basename_noext="$(basename "$infile" .in)"

        # Check if we should skip EXECUTION for make/test modes, or skip entirely for copy/make modes
        # In test mode with --skip-existing: skip execution but still do diff
        skip_execution=""
        skip_entirely=""

        if $debug; then
            echo "  [DEBUG] Checking skip conditions: skip_existing=$skip_existing, skip_newer=$skip_newer, force=$force, mode=$mode"
        fi

        if ($skip_existing || $skip_newer) && ! $force; then
            # Define primary output files (what gets generated by running PROG)
            primary_outputs=(
                "$directory/${basename_noext}_01.asc"
                "$directory/${basename_noext}_02.asc"
                "$directory/${basename_noext}_03.asc"
            )

            if $debug; then
                echo "  [DEBUG] Checking primary outputs: ${primary_outputs[*]}"
            fi

            if $skip_existing; then
                # For make/test modes: skip execution if primary outputs exist
                if [[ "$mode" == "make" || "$mode" == "test" ]]; then
                    if $debug; then
                        echo "  [DEBUG] In make/test mode, checking for existing output files..."
                    fi
                    # Check if any primary output exists
                    existing_count=0
                    for output in "${primary_outputs[@]}"; do
                        if [[ -f "$output" ]]; then
                            existing_count=$((existing_count + 1))
                            if $debug; then
                                echo "  [DEBUG] Found existing output: $output"
                            fi
                        fi
                    done
                    if $debug; then
                        echo "  [DEBUG] Found $existing_count existing output files"
                    fi
                    if [[ $existing_count -gt 0 ]]; then
                        skip_execution="output files exist"
                        # For make mode, skip entirely (no diff phase)
                        # For test mode, only skip execution, continue to diff
                        if [[ "$mode" == "make" ]]; then
                            skip_entirely="$skip_execution"
                        fi
                    fi
                elif [[ "$mode" == "copy" ]]; then
                    # For copy mode: skip entirely if reference files already exist
                    ref_files=(
                        "$directory/${basename_noext}.tl"
                        "$directory/${basename_noext}.rtl"
                        "$directory/${basename_noext}.ftl"
                    )
                    existing_count=0
                    for ref in "${ref_files[@]}"; do
                        if [[ -f "$ref" ]]; then
                            existing_count=$((existing_count + 1))
                        fi
                    done
                    if [[ $existing_count -gt 0 ]]; then
                        skip_entirely="reference files exist"
                    fi
                fi
            fi

            if $skip_newer && [[ -z "$skip_execution" ]]; then
                # Check if any primary output files are newer than input
                for output in "${primary_outputs[@]}"; do
                    if [[ -f "$output" && "$output" -nt "$infile" ]]; then
                        skip_execution="outputs newer than input"
                        if [[ "$mode" == "make" ]]; then
                            skip_entirely="$skip_execution"
                        fi
                        break
                    fi
                done
            fi
        fi

        # Skip entirely for make/copy modes, or if no files to process
        if [[ -n "$skip_entirely" ]]; then
            if $debug; then
                echo "  [DEBUG] Skipping entirely: $skip_entirely"
            fi
            echo "  [SKIP] $infile ($skip_entirely)"
            add_to_array_if_not_present "skipped_files" "$infile"
            continue
        fi

        if $debug; then
            echo "  [DEBUG] Not skipping, proceeding to execution check"
        fi

        # Run $PROG for 'make' and 'test' modes, but not for 'copy' or 'diff' mode
        # In test mode, can be skipped if --skip-existing and output already exists
        if [[ "$mode" == "make" || "$mode" == "test" ]]; then
            # Check if we should skip execution in test mode
            if [[ -n "$skip_execution" && "$mode" == "test" ]]; then
                echo "  [SKIP EXECUTION] $infile ($skip_execution) - will compare existing output"
            else
                # Check for existing output files and warn user
                basename_noext="$(basename "$infile" .in)"
                existing_files=()
                for suffix in 01 02 03; do
                    potential_output="$directory/${basename_noext}_${suffix}.asc"
                    if [[ -f "$potential_output" ]]; then
                        existing_files+=("$potential_output")
                    fi
                done

                # Also check for other common output files
                for ext in .003 _41.dat _42.dat .log; do
                    potential_output="$directory/${basename_noext}${ext}"
                    if [[ -f "$potential_output" ]]; then
                        existing_files+=("$potential_output")
                    fi
                done

                # Show warning if any existing files found
                if [[ ${#existing_files[@]} -gt 0 ]] && [[ "$mode" != "test" ]]; then
                    echo -e "   \e[90mWarning: Existing output files will be overwritten:\e[0m"
                    for existing_file in "${existing_files[@]}"; do
                        echo -e "     \e[90m- $(basename "$existing_file")\e[0m"
                    done
                fi

                # Copy input file to specified input filename in the same directory
            cp "$infile" "$directory/$input_filename"
            echo "   Copied $infile to $directory/$input_filename"

            # Get absolute path to executable for running from different directory
            if [[ "$PROG" = /* ]]; then
                # Already absolute path
                prog_path="$PROG"
            else
                # Convert relative path to absolute
                prog_path="$(readlink -f "$PROG")"
            fi

            echo -n "   Running: $PROG $input_filename... "
            echo -en "${PROG_OUTPUT_COLOR}" # Set text color to highlight PROG output (light green)
            set +e  # Temporarily disable exit on error to handle executable failures gracefully
            if [[ "$mode" == "test" ]]; then
                # Change to directory before running to ensure input file is found
                (cd "$directory" && { time "$prog_path" "$input_filename"; }) >> "$LOG_FILE" 2>&1
                RETVAL=$?
            else
                echo
                # Change to directory before running to ensure input file is found
                (cd "$directory" && "$prog_path" "$input_filename")
                RETVAL=$?
                echo -n "   "
            fi
            set -e  # Re-enable exit on error
            echo -en "\x1B[0m" # Reset text color
            # check PROG exit status
            if [[ $RETVAL -eq 0 ]]; then
                if [[ "$mode" != "test" ]]; then
                    echo -n "$PROG "
                fi
                echo -e "\e[32mOK\e[0m"
                if [[ "$mode" != "test" ]]; then
                    echo "   Success: ${PROG} completed successfully for $infile"
                fi
                exec_success_files+=("$infile")

                # Rename output files from nspe*.asc to basename_*.asc
                echo "   Renaming output files..."
                renamed_count=0
                for suffix in 01 02 03; do
                    # Try both formats: nspe_01.asc and nspe01.asc
                    for pattern in "nspe_${suffix}.asc" "nspe${suffix}.asc"; do
                        default_output="$directory/$pattern"
                        renamed_output="$directory/${basename_noext}_${suffix}.asc"
                        if [[ -f "$default_output" ]]; then
                            mv "$default_output" "$renamed_output"
                            echo "   Renamed $(basename "$default_output") to $(basename "$renamed_output")"
                            renamed_count=$((renamed_count + 1))

                            # Check if the renamed output file is empty
                            if [[ ! -s "$renamed_output" ]]; then
                                echo -e "   \e[33mWarning: Output file $renamed_output is empty\e[0m"
                                empty_output_files+=("$renamed_output")
                                # Note: We don't reclassify successful executions as failures
                                # An empty output file doesn't mean the executable failed
                            fi
                            break  # Don't try the other pattern if we found one
                        fi
                    done
                done

                # Rename other common output files (simple extensions)
                for ext in .prs .pulse; do
                    default_output="$directory/nspe${ext}"
                    renamed_output="$directory/${basename_noext}${ext}"
                    if [[ -f "$default_output" ]]; then
                        mv "$default_output" "$renamed_output"
                        echo "   Renamed $(basename "$default_output") to $(basename "$renamed_output")"
                        renamed_count=$((renamed_count + 1))
                    fi
                done

                # Special handling for for003.dat -> basename.003
                if [[ -f "$directory/for003.dat" ]]; then
                    mv "$directory/for003.dat" "$directory/${basename_noext}.003"
                    echo "   Renamed for003.dat to ${basename_noext}.003"
                    renamed_count=$((renamed_count + 1))
                fi

                # Rename nspeXX.bin files (nspe09.bin -> basename_09.bin)
                for nspefile in "$directory"/nspe[0-9][0-9].*; do
                    if [[ -f "$nspefile" ]]; then
                        # Extract the number and extension
                        suffix=$(basename "$nspefile" | sed 's/^nspe\([0-9][0-9]\).*/\1/')
                        ext=$(basename "$nspefile" | sed 's/^nspe[0-9][0-9]//')
                        renamed_output="$directory/${basename_noext}_${suffix}${ext}"
                        mv "$nspefile" "$renamed_output"
                        echo "   Renamed $(basename "$nspefile") to $(basename "$renamed_output")"
                        renamed_count=$((renamed_count + 1))
                    fi
                done

                # Rename _41.dat and _42.dat specifically (these use underscores)
                for suffix in 41 42; do
                    default_output="$directory/nspe_${suffix}.dat"
                    renamed_output="$directory/${basename_noext}_${suffix}.dat"
                    if [[ -f "$default_output" ]]; then
                        mv "$default_output" "$renamed_output"
                        echo "   Renamed $(basename "$default_output") to $(basename "$renamed_output")"
                        renamed_count=$((renamed_count + 1))
                    fi
                done

                # Rename angles.asc to basename_angles.asc
                if [[ -f "$directory/angles.asc" ]]; then
                    mv "$directory/angles.asc" "$directory/${basename_noext}_angles.asc"
                    echo "   Renamed angles.asc to ${basename_noext}_angles.asc"
                    renamed_count=$((renamed_count + 1))
                fi

                # Copy (not move) ram.in to basename_ram.in
                if [[ -f "$directory/ram.in" ]]; then
                    cp "$directory/ram.in" "$directory/${basename_noext}_ram.in"
                    echo "   Copied ram.in to ${basename_noext}_ram.in"
                    renamed_count=$((renamed_count + 1))
                fi

                # Rename Fortran unit files (for009.bin -> basename_09.bin, for041.dat -> basename_41.dat, etc.)
                # Note: for003.dat is skipped here because nspe.003 is handled separately above
                for forfile in "$directory"/for[0-9][0-9][0-9].*; do
                    if [[ -f "$forfile" ]]; then
                        basename_for=$(basename "$forfile")
                        # Skip for003.dat as it's handled via nspe.003 above
                        if [[ "$basename_for" == "for003.dat" ]]; then
                            continue
                        fi
                        # Extract the unit number
                        unit_num=$(basename "$forfile" | sed 's/^for\([0-9][0-9][0-9]\).*/\1/')
                        # Extract extension from filename (e.g., ".dat" from "for042.dat", ".bin" from "for009.bin")
                        ext=$(basename "$forfile" | sed 's/^for[0-9][0-9][0-9]//')
                        # Convert unit number to integer to drop leading zeros (009 → 9, 041 → 41, 042 → 42)
                        unit_num_int=$((10#$unit_num))
                        # Format with zero-padding for single digits (9 → 09)
                        if [[ $unit_num_int -lt 10 ]]; then
                            unit_formatted=$(printf "%02d" $unit_num_int)
                        else
                            unit_formatted=$unit_num_int
                        fi
                        renamed_output="$directory/${basename_noext}_${unit_formatted}${ext}"
                        mv "$forfile" "$renamed_output"
                        echo "   Renamed $(basename "$forfile") to $(basename "$renamed_output")"
                        renamed_count=$((renamed_count + 1))
                    fi
                done

                if [[ $renamed_count -eq 0 ]]; then
                    echo -e "   \e[33mWarning: No output files found to rename\e[0m"
                    # Track that executable ran but produced no output files
                    missing_exec_output_files+=("$infile")
                fi

                # If --keep-bin is not set, remove extra files that don't have corresponding reference files
                if [[ "$keep_bin" == false ]]; then
                    echo "   Cleaning up extra output files (only keeping files with references)..."

                    # Determine which reference files exist
                    has_tl=false
                    has_rtl=false
                    has_ftl=false
                    [[ -f "$directory/${basename_noext}.tl" ]] && has_tl=true
                    [[ -f "$directory/${basename_noext}.rtl" ]] && has_rtl=true
                    [[ -f "$directory/${basename_noext}.ftl" ]] && has_ftl=true

                    # Remove .asc files that don't have corresponding references
                    if [[ "$has_tl" == false && -f "$directory/${basename_noext}_01.asc" ]]; then
                        rm "$directory/${basename_noext}_01.asc"
                        echo "   Removed ${basename_noext}_01.asc (no .tl reference)"
                    fi
                    if [[ "$has_rtl" == false && -f "$directory/${basename_noext}_02.asc" ]]; then
                        rm "$directory/${basename_noext}_02.asc"
                        echo "   Removed ${basename_noext}_02.asc (no .rtl reference)"
                    fi
                    if [[ "$has_ftl" == false && -f "$directory/${basename_noext}_03.asc" ]]; then
                        rm "$directory/${basename_noext}_03.asc"
                        echo "   Removed ${basename_noext}_03.asc (no .ftl reference)"
                    fi

                    # Remove binary files and other extra outputs
                    for binfile in "$directory/${basename_noext}"_*.bin "$directory/${basename_noext}"_*.dat "$directory/${basename_noext}".003 "$directory/${basename_noext}".prs "$directory/${basename_noext}".pulse "$directory/${basename_noext}"_angles.asc; do
                        if [[ -f "$binfile" ]]; then
                            # Check if file is binary or ASCII
                            if file "$binfile" | grep -q "ASCII\|text"; then
                                rm "$binfile"
                                echo "   Removed $(basename "$binfile") (extra output)"
                            else
                                rm "$binfile"
                                echo "   Removed $(basename "$binfile") (binary output)"
                            fi
                        fi
                    done

                    # Keep log file only for case6 and case7
                    if [[ ! "$basename_noext" =~ case[67] ]]; then
                        if [[ -f "$directory/${basename_noext}.log" ]]; then
                            rm "$directory/${basename_noext}.log"
                            echo "   Removed ${basename_noext}.log (only kept for case6/case7)"
                        fi
                    fi
                fi

                # Clean up temporary input file and other temporary files after successful execution
                temp_files_cleaned=0
                # Clean up specific known temporary files (using variable for input filename)
                for temp_file in "$input_filename" nspe.log nspe.prs nspe.pulse angles.asc ram.in; do
                    if [[ -f "$directory/$temp_file" ]]; then
                        rm "$directory/$temp_file"
                        if [[ "$mode" != "test" ]]; then
                            echo "   Cleaned up $directory/$temp_file"
                        fi
                        temp_files_cleaned=$((temp_files_cleaned + 1))
                    fi
                done
                # Note: for*.* files are now renamed, not cleaned up
                if [[ "$mode" == "test" && $temp_files_cleaned -gt 0 ]]; then
                    echo "   Cleaned up $temp_files_cleaned temporary file(s)"
                fi
            else
                echo -e "\e[31mFAIL\e[0m"
                echo -e "   \e[31mError: ${PROG} failed with exit code $RETVAL for $infile\e[0m"
                echo "   Error: ${PROG} failed with exit code $RETVAL for $infile" >> "$LOG_FILE"
                exec_fail_files+=("$infile")

                # Clean up temporary input file and other temporary files after failed execution
                for temp_file in "$input_filename" nspe.log nspe.prs nspe.pulse angles.asc ram.in; do
                    if [[ -f "$directory/$temp_file" ]]; then
                        rm "$directory/$temp_file"
                    fi
                done
                # Clean up any for*.* files after failure
                for temp_file in "$directory"/for[0-9][0-9][0-9].*; do
                    if [[ -f "$temp_file" ]]; then
                        rm "$temp_file"
                    fi
                done

                # Stop processing if --stop-on-error flag is set
                if $stop_on_error; then
                    echo -e "\e[31m--stop-on-error flag set. Terminating processing loop.\e[0m"
                    break
                fi

                #echo "   Aborting..."; break
                echo "   Continuing..."; continue
            fi
            fi  # End of "if not skip_execution" block
        fi

        # For 'make' mode, we're done after running $PROG - skip file operations
        if [[ "$mode" == "make" ]]; then
            printf '%*s\n' "$line_len" '' | tr '  ' '='
            continue
        fi

        # Find the first available file pair by priority: 03, 02, 01
        found_files=false
        for suffix in 03 02 01; do
            test="$directory/$(basename "$infile" .in)_${suffix}.asc"
            case $suffix in
                01) ref="$directory/$(basename "$infile" .in).tl" ;;
                02) ref="$directory/$(basename "$infile" .in).rtl" ;;
                03) ref="$directory/$(basename "$infile" .in).ftl" ;;
            esac

            # For copy and test modes, check if test file exists
            # For diff mode, check if either test or ref exists to provide meaningful feedback
            if [[ "$mode" == "test" && -f "$test" && -s "$test" ]] || [[ "$mode" == "copy" && -f "$test" && -s "$test" ]] || [[ "$mode" == "diff" && (-f "$test" || -f "$ref") ]]; then
                # For diff mode, we need to check files even if test doesn't exist
                if [[ "$mode" == "diff" && ! -f "$test" ]]; then
                    # In diff mode, check if either test or ref files exist to provide meaningful feedback
                    if [[ -f "$ref" ]]; then
                        echo -e "\e[33m[[MISSING]]\e[0m\n   Output file '$test' does not exist"
                        echo -e "   \e[33mHint: Run 'make' mode first to generate output files\e[0m"
                        # In diff mode, only mark as missing, not as failed (since no comparison was attempted)
                        missing_output_files+=("$test")
                        add_to_array_if_not_present "skipped_files" "$infile"
                        found_files=true
                    else
                        echo -e "\e[33m[[MISSING]]\e[0m\n   Both output '$test' and reference '$ref' do not exist"
                        echo -e "   \e[33mHint: Run 'copy' mode first to generate reference files, then 'test' mode for output files\e[0m"
                        # In diff mode, only mark as missing, not as failed (since no comparison was attempted)
                        missing_output_files+=("$test")
                        missing_reference_files+=("$ref")
                        add_to_array_if_not_present "skipped_files" "$infile"
                        found_files=true
                    fi
                    break  # Exit the suffix loop since we handled this case
                fi

                # Continue with existing logic for cases where test file exists
                if [[ -f "$test" && -s "$test" ]]; then
                    if [[ "$mode" == "copy" ]]; then
                        # COPY mode: Only copy files, no comparison
                        cp -v "$test" "$ref"
                        echo "   Copied $test to $ref"
                        processed_files+=("$infile")
                        elif [[ "$mode" == "test" || "$mode" == "diff" ]]; then
                        # TEST and DIFF modes: Compare files
                        echo -n "   Comparing $test to $ref... "

                        # Check if reference file exists
                        if [[ ! -f "$ref" ]]; then
                            echo -e "\e[33m[[MISSING REFERENCE]]\e[0m\n   Reference file '$ref' does not exist"
                            if [[ "$mode" == "test" ]]; then
                                echo -e "\e[33m[[MISSING REFERENCE]]\e[0m\n   Reference file '$ref' does not exist" >> "$LOG_FILE"
                                echo "   Output was generated successfully but cannot be validated without reference" >> "$LOG_FILE"
                            fi
                            # Don't mark as failed - output was generated, just can't validate it
                            # This is a skip, not a failure
                            echo -e "   \e[33mHint: Run 'copy' mode first to generate reference files\e[0m"
                            missing_reference_files+=("$ref")
                            add_to_array_if_not_present "skipped_files" "$infile"
                            found_files=true  # Mark as processed to avoid "no files found" message
                            break  # Exit the suffix loop since we handled this case
                        fi

                        # Check if reference file is empty
                        if [[ ! -s "$ref" ]]; then
                            echo
                            echo -e "\e[33m[[EMPTY REFERENCE]]\e[0m"
                            echo "   Reference file '$ref' is empty"
                            if [[ "$mode" == "test" ]]; then
                                echo -e "\e[33m[[EMPTY REFERENCE]]\e[0m"
                                echo "   Reference file '$ref' is empty" >> "$LOG_FILE"
                                # In test mode, mark as failed since we were trying to validate the output
                                add_to_array_if_not_present "fail_files" "$infile"
                            fi
                            # In diff mode, only mark as empty, not as failed (since no comparison was attempted)
                            empty_files+=("$ref")
                            add_to_array_if_not_present "skipped_files" "$infile"
                            found_files=true  # Mark as processed to avoid "no files found" message
                            break  # Exit the suffix loop since we handled this case
                        fi

                        # For diff mode, also check if test file exists (since we're not running $PROG)
                        if [[ "$mode" == "diff" && ! -f "$test" ]]; then
                            echo -e "\e[33m[[MISSING OUTPUT]]\e[0m\n   Output file '$test' does not exist"
                            echo -e "   \e[33mHint: Run 'test' mode first to generate output files\e[0m"
                            # In diff mode, only mark as missing, not as failed (since no comparison was attempted)
                            missing_output_files+=("$test")
                            add_to_array_if_not_present "skipped_files" "$infile"
                            continue
                        fi

                        # For diff mode, check if test file is empty
                        if [[ "$mode" == "diff" && ! -s "$test" ]]; then
                            echo -e "\e[33m[[EMPTY OUTPUT]]\e[0m\n   Output file '$test' is empty"
                            # In diff mode, only mark as empty, not as failed (since no comparison was attempted)
                            empty_files+=("$test")
                            add_to_array_if_not_present "skipped_files" "$infile"
                            continue
                        fi

                        # Perform the actual comparison
                        if [[ "$mode" == "test" ]]; then
                            # TEST mode: Log comparison results
                            # Capture diff output to check which diff tool failed
                            diff_output_file=$(mktemp)
                            # Pass diff_level as 5th argument (after threshold1 and threshold2 which are empty/default)
                            if diff_files "$test" "$ref" "" "" "$diff_level" >> "$LOG_FILE" 2>&1; then
                                echo -e "\e[32m[[PASS]]\e[0m" # Print PASS to terminal
                                echo -e "\e[32m[[PASS]]\e[0m" >> "$LOG_FILE"
                                add_to_array_if_not_present "pass_files" "$infile"

                                # Even for passing files, track which intermediate diffs failed
                                if tail -100 "$LOG_FILE" | grep -q "diff FAILED"; then
                                    if tail -100 "$LOG_FILE" | grep -q "tldiff OK"; then
                                        # Simple diff failed but tldiff passed
                                        simple_diff_fail_files+=("$infile")
                                    elif tail -100 "$LOG_FILE" | grep -q "uband_diff OK"; then
                                        # tldiff also failed but uband_diff passed
                                        tldiff_fail_files+=("$infile")
                                    fi
                                fi
                            else
                                echo -e "\e[31m[[FAIL]]\e[0m" # Print FAIL to terminal
                                echo -e "\e[31m[[FAIL]]\e[0m" >> "$LOG_FILE"
                                add_to_array_if_not_present "fail_files" "$infile"

                                # Determine which diff tool failed by checking the log
                                if tail -100 "$LOG_FILE" | grep -q "diff FAILED"; then
                                    if tail -100 "$LOG_FILE" | grep -q "tldiff OK\|uband_diff OK"; then
                                        # Simple diff failed but advanced tools passed (shouldn't reach here)
                                        simple_diff_fail_files+=("$infile")
                                    elif tail -100 "$LOG_FILE" | grep -q "tldiff FAILED"; then
                                        if tail -100 "$LOG_FILE" | grep -q "uband_diff OK"; then
                                            # tldiff failed but uband_diff passed (shouldn't reach here)
                                            tldiff_fail_files+=("$infile")
                                        elif tail -100 "$LOG_FILE" | grep -q "uband_diff FAILED"; then
                                            # All diffs failed
                                            uband_diff_fail_files+=("$infile")
                                        else
                                            tldiff_fail_files+=("$infile")
                                        fi
                                    fi
                                fi
                            fi
                            rm -f "$diff_output_file"
                        else
                            # DIFF mode: Show comparison results to terminal only
                            # Capture diff output to check which diff tool failed while also showing to screen
                            diff_output_file=$(mktemp)
                            # Pass diff_level as 5th argument (after threshold1 and threshold2 which are empty/default)
                            diff_files "$test" "$ref" "" "" "$diff_level" 2>&1 | tee "$diff_output_file"
                            # Capture the exit code from diff_files (not from tee)
                            diff_exit_code=${PIPESTATUS[0]}
                            if [[ $diff_exit_code -eq 0 ]]; then
                                echo -e "$infile \e[32m[[PASS]]\e[0m" # Print PASS to terminal
                                add_to_array_if_not_present "pass_files" "$infile"

                                # Even for passing files, track which intermediate diffs failed
                                if grep -q "diff FAILED" "$diff_output_file"; then
                                    if grep -q "tldiff OK" "$diff_output_file"; then
                                        # Simple diff failed but tldiff passed
                                        simple_diff_fail_files+=("$infile")
                                    elif grep -q "uband_diff OK" "$diff_output_file"; then
                                        # tldiff also failed but uband_diff passed
                                        tldiff_fail_files+=("$infile")
                                    fi
                                fi
                            else
                                echo -e "$infile \e[31m[[FAIL]]\e[0m" # Print FAIL to terminal
                                add_to_array_if_not_present "fail_files" "$infile"

                                # Determine which diff tool failed by checking the output
                                if grep -q "diff FAILED" "$diff_output_file"; then
                                    if grep -q "tldiff OK\|uband_diff OK" "$diff_output_file"; then
                                        # Simple diff failed but advanced tools passed
                                        simple_diff_fail_files+=("$infile")
                                    elif grep -q "tldiff FAILED" "$diff_output_file"; then
                                        if grep -q "uband_diff OK" "$diff_output_file"; then
                                            # tldiff failed but uband_diff passed
                                            tldiff_fail_files+=("$infile")
                                        elif grep -q "uband_diff FAILED" "$diff_output_file"; then
                                            # All diffs failed
                                            uband_diff_fail_files+=("$infile")
                                        else
                                            tldiff_fail_files+=("$infile")
                                        fi
                                    fi
                                fi
                            fi
                            rm -f "$diff_output_file"
                        fi
                    else
                        echo "Unknown mode: $mode"
                        exit 1
                    fi
                    # After renaming/diffing, delete files with same base name except .in and selected reference
                    basename_noext="$(basename "$infile" .in)"
                    # Only remove files matching <basename>.<ext> or <basename>_<suffix>
                    shopt -s nullglob
                    files_to_clean=("$directory/${basename_noext}."* "$directory/${basename_noext}_"*)
                    for f in "${files_to_clean[@]}"; do
                        # Skip .in, selected reference, and test file
                        if [[ "$f" == "$infile" || "$f" == "$ref" || "$f" == "$test" ]]; then
                            continue
                        fi
                        # Skip log file in test mode
                        if [[ "$f" == "$LOG_FILE" && "$mode" == "test" ]]; then
                            continue
                        fi
                        # Skip if test file contains "case7" or "case6"
                        if [[ "$f" == "$LOG_FILE" ]] && [[ "$test" == *case7* || "$test" == *case6* ]]; then
                            continue
                        fi
                        # Never delete .in or .tl files
                        case "$f" in
                            *.in|*.tl)
                                continue
                                ;;
                        esac
                        # Only delete files
                        if [[ -f "$f" ]]; then
                            rm "$f"
                            if [[ "$mode" != "test" ]]; then
                                echo "   Deleted $f"
                            fi
                        fi
                    done
                    shopt -u nullglob
                    found_files=true
                    break
                fi  # End of the "if [[ -f "$test" && -s "$test" ]]; then" block
            fi  # End of the main suffix condition
        done

        # If no files were found, report it
        if [[ "$found_files" == false ]]; then
            if [[ "$mode" == "copy" ]]; then
                echo -e "   \e[33m[[MISSING]]\e[0m No output files (_01.asc, _02.asc, _03.asc) found to copy"
                echo -e "   \e[33mHint: Run 'make' mode first to generate output files\e[0m"
                skipped_files+=("$infile")
            elif [[ "$mode" == "test" ]]; then
                # Check if any reference files exist - if yes, this is a real error
                ref_exists=false
                for suffix in 01 02 03; do
                    case $suffix in
                        01) check_ref="$directory/$(basename "$infile" .in).tl" ;;
                        02) check_ref="$directory/$(basename "$infile" .in).rtl" ;;
                        03) check_ref="$directory/$(basename "$infile" .in).ftl" ;;
                    esac
                    if [[ -f "$check_ref" ]]; then
                        ref_exists=true
                        break
                    fi
                done
                
                if $ref_exists; then
                    # Reference exists but no output - this is a REAL ERROR (executable failed to produce output)
                    echo -e "   \e[31m[[ERROR]]\e[0m No output files found, but reference files exist"
                    echo -e "   \e[31mThis indicates the executable failed to generate expected output\e[0m"
                    echo -e "   \e[31m[[ERROR]]\e[0m No output files found, but reference files exist" >> "$LOG_FILE"
                    add_to_array_if_not_present "fail_files" "$infile"
                else
                    # No reference and no output - just skip (can't validate anyway)
                    echo -e "   \e[33m[[SKIPPED]]\e[0m No output or reference files found"
                    echo -e "   \e[33mHint: Run 'copy' mode to establish reference files first\e[0m"
                    echo -e "   \e[33m[[SKIPPED]]\e[0m No output or reference files found" >> "$LOG_FILE"
                    add_to_array_if_not_present "skipped_files" "$infile"
                fi
            elif [[ "$mode" == "make" ]]; then
                echo -e "   \e[36m[[INFO]]\e[0m Output files will be generated by ${PROG} for suffixes (01, 02, 03)"
            else
                echo -e "   \e[33m[[NO FILES]]\e[0m"
                echo "No output files (01, 02, 03) or reference files (tl, rtl, ftl) found for $basename_noext"
                echo -e "   \e[33mHint: Run 'make' mode to generate output files, and 'copy' mode to generate reference files\e[0m"
                # In diff mode, only mark as missing, not as failed (since no comparison was attempted)
                # No specific file to add to missing arrays since we don't know which suffix
                add_to_array_if_not_present "skipped_files" "$infile"
            fi
        fi
    else
        echo -e "\e[33mSkipping $infile (does not contain required strings)\e[0m"
    fi
    printf '\n%*s\n' "$line_len" '' | tr ' ' '='
done

# Print summary if in test, diff, copy, or make mode
if [[ "$mode" == "test" || "$mode" == "diff" || "$mode" == "copy" || "$mode" == "make" ]]; then
    if [[ "$mode" == "test" ]]; then
        echo "Test Summary:"
        elif [[ "$mode" == "copy" ]]; then
        echo "Copy Summary:"
        elif [[ "$mode" == "make" ]]; then
        echo "Make Summary:"
    else
        echo "Diff Summary:"
    fi
    printf '%*s\n' "$line_len" '' | tr ' ' '='

    # Execution Results Section (for modes that run executable)
    if [[ "$mode" == "test" || "$mode" == "make" ]]; then
        echo "Execution Results:"
        echo "=================="

        if [[ ${#exec_success_files[@]} -gt 0 ]]; then
            echo "Executable successful: ${#exec_success_files[@]}"
            for f in "${exec_success_files[@]}"; do
                echo -e "   \e[32mEXEC_OK\e[0m $f"
            done
        else
            echo "Executable successful: 0"
        fi

        if [[ ${#exec_fail_files[@]} -gt 0 ]]; then
            echo "Executable failed: ${#exec_fail_files[@]}"
            for f in "${exec_fail_files[@]}"; do
                echo -e "   \e[31mEXEC_FAIL\e[0m $f"
            done
        else
            echo "Executable failed: 0"
        fi

        if [[ ${#missing_exec_output_files[@]} -gt 0 ]]; then
            echo "Executable ran but no output: ${#missing_exec_output_files[@]}"
            for f in "${missing_exec_output_files[@]}"; do
                echo -e "   \e[33mNO_OUTPUT\e[0m $f"
            done
        fi

        if [[ ${#empty_output_files[@]} -gt 0 ]]; then
            echo "Empty output files: ${#empty_output_files[@]}"
            for f in "${empty_output_files[@]}"; do
                echo -e "   \e[33mEMPTY_OUTPUT\e[0m $(basename "$f")"
            done
        fi
        echo ""
    fi

    # Diff Results Section (for modes that compare files)
    if [[ "$mode" == "test" || "$mode" == "diff" ]]; then
        # Diff Details Section (show which diff tool failed) - FIRST
        if [[ ${#simple_diff_fail_files[@]} -gt 0 || ${#tldiff_fail_files[@]} -gt 0 || ${#uband_diff_fail_files[@]} -gt 0 ]]; then
            echo "Diff Details:"
            echo "-------------"

            if [[ ${#simple_diff_fail_files[@]} -gt 0 ]]; then
                echo "Files that passed with tldiff or uband_diff (simple diff failed): ${#simple_diff_fail_files[@]}"
                for f in "${simple_diff_fail_files[@]}"; do
                    echo -e "   \e[33mDIFF_FAIL\e[0m $f"
                done
            fi

            if [[ ${#tldiff_fail_files[@]} -gt 0 ]]; then
                echo "Files that passed with uband_diff (tldiff failed): ${#tldiff_fail_files[@]}"
                for f in "${tldiff_fail_files[@]}"; do
                    echo -e "   \e[33mTLDIFF_FAIL\e[0m $f"
                done
            fi

            if [[ ${#uband_diff_fail_files[@]} -gt 0 ]]; then
                echo "Files that failed all diff tools: ${#uband_diff_fail_files[@]}"
                for f in "${uband_diff_fail_files[@]}"; do
                    echo -e "   \e[31mUBAND_DIFF_FAIL\e[0m $f"
                done
            fi
            echo ""
        else
            # No diff failures at any level - all files are identical
            if [[ ${#pass_files[@]} -gt 0 ]]; then
                echo "Diff Details:"
                echo "-------------"
                echo -e "\e[1;32m*** All files are IDENTICAL - would pass simple 'diff' ***\e[0m"
                echo ""
            fi
        fi

        echo "Diff Results:"
        echo "============="
        if [[ ${#pass_files[@]} -gt 0 ]]; then
            echo "Passed files: ${#pass_files[@]}"
            for f in "${pass_files[@]}"; do
                echo -e "   \e[32mPASS\e[0m $f"
            done
        else
            echo "Passed files: 0"
        fi

        if [[ ${#fail_files[@]} -gt 0 ]]; then
            echo "Failed files: ${#fail_files[@]}"
            for f in "${fail_files[@]}"; do
                echo -e "   \e[31mFAIL\e[0m $f"
            done
        else
            echo "Failed files: 0"
        fi

        if [[ ${#skipped_files[@]} -gt 0 ]]; then
            echo "Skipped files: ${#skipped_files[@]}"
            for f in "${skipped_files[@]}"; do
                echo -e "   \e[33mSKIPPED\e[0m $f"
            done
        fi
        echo ""
    fi

    # Copy Results Section
    if [[ "$mode" == "copy" ]]; then
        echo "Copy Results:"
        echo "============="

        if [[ ${#processed_files[@]} -gt 0 ]]; then
            echo "Processed files: ${#processed_files[@]}"
            for f in "${processed_files[@]}"; do
                echo -e "   \e[32mCOPIED\e[0m $f"
            done
        else
            echo "Processed files: 0"
        fi

        if [[ ${#skipped_files[@]} -gt 0 ]]; then
            echo "Skipped files: ${#skipped_files[@]}"
            for f in "${skipped_files[@]}"; do
                echo -e "   \e[33mSKIPPED\e[0m $f"
            done
        fi

        if [[ ${#empty_output_files[@]} -gt 0 ]]; then
            echo "Empty output files detected: ${#empty_output_files[@]}"
            for f in "${empty_output_files[@]}"; do
                echo -e "   \e[33mEMPTY_OUTPUT\e[0m $(basename "$f")"
            done
        fi
        echo ""
    fi

    # File Status Section
    echo "File Status:"
    echo "============"

    if [[ ${#missing_reference_files[@]} -gt 0 ]]; then
        echo "Missing reference files (run 'copy' mode first): ${#missing_reference_files[@]}"
        for f in "${missing_reference_files[@]}"; do
            echo -e "   \e[33mMISSING\e[0m $f"
        done
    else
        echo "Missing reference files: 0"
    fi

    if [[ ${#missing_output_files[@]} -gt 0 ]]; then
        echo "Missing output files (run 'make' mode first): ${#missing_output_files[@]}"
        for f in "${missing_output_files[@]}"; do
            echo -e "   \e[33mMISSING\e[0m $f"
        done
    else
        echo "Missing output files: 0"
    fi

    if [[ ${#empty_files[@]} -gt 0 ]]; then
        echo "Empty files detected: ${#empty_files[@]}"
        for f in "${empty_files[@]}"; do
            echo -e "   \e[33mEMPTY\e[0m $f"
        done
    else
        echo "Empty files: 0"
    fi

        # Overall Status Assessment
    echo ""
    echo "Overall Status:"
    echo "==============="
    if [[ "$mode" == "copy" && ${#skipped_files[@]} -eq 0 && ${#empty_output_files[@]} -eq 0 ]]; then
        echo -e "\e[32mAll files processed successfully!\e[0m"
        elif [[ "$mode" == "copy" && ${#empty_output_files[@]} -gt 0 ]]; then
        echo -e "\e[33mSome output files are empty. Check copy results above.\e[0m"
        elif [[ "$mode" == "make" ]]; then
        if [[ ${#exec_fail_files[@]} -eq 0 && ${#empty_output_files[@]} -eq 0 && ${#missing_exec_output_files[@]} -eq 0 ]]; then
            echo -e "\e[32mAll files generated successfully!\e[0m"
        elif [[ ${#exec_fail_files[@]} -eq 0 && ${#empty_output_files[@]} -gt 0 ]]; then
            echo -e "\e[32mAll executables completed successfully!\e[0m"
            echo -e "\e[33mNote: Some output files are empty. Check execution results above.\e[0m"
        elif [[ ${#exec_fail_files[@]} -eq 0 && ${#missing_exec_output_files[@]} -gt 0 ]]; then
            echo -e "\e[32mAll executables completed successfully!\e[0m"
            echo -e "\e[33mNote: Some executables produced no output files. Check execution results above.\e[0m"
        else
            echo -e "\e[31mSome executables failed. Check execution errors above.\e[0m"
        fi
        elif [[ "$mode" == "test" ]]; then
        if [[ ${#exec_fail_files[@]} -eq 0 && ${#fail_files[@]} -eq 0 && ${#skipped_files[@]} -eq 0 && ${#empty_output_files[@]} -eq 0 ]]; then
            echo -e "\e[32mAll tests passed!\e[0m"
            elif [[ ${#exec_fail_files[@]} -gt 0 ]]; then
            echo -e "\e[31mSome executables failed. Check execution errors above.\e[0m"
            elif [[ ${#fail_files[@]} -gt 0 ]]; then
            echo -e "\e[31mSome diff comparisons failed. Check diff results above.\e[0m"
            elif [[ ${#empty_output_files[@]} -gt 0 ]]; then
            echo -e "\e[33mSome output files are empty. Check execution results above.\e[0m"
            elif [[ ${#skipped_files[@]} -gt 0 ]]; then
            echo -e "\e[33mSome files were skipped due to missing dependencies. Check file status above.\e[0m"
        fi
        elif [[ "$mode" == "diff" ]]; then
        if [[ ${#fail_files[@]} -eq 0 && ${#skipped_files[@]} -eq 0 ]]; then
            echo -e "\e[32mAll diffs passed!\e[0m"
            elif [[ ${#fail_files[@]} -gt 0 ]]; then
            echo -e "\e[31mSome diff comparisons failed. Check diff results above.\e[0m"
            elif [[ ${#skipped_files[@]} -gt 0 ]]; then
            echo -e "\e[33mSome files were skipped due to missing dependencies. Check file status above.\e[0m"
        fi
    fi
    echo "=============================="
fi

# Exit with appropriate code
if [[ "$mode" == "make" && ${#exec_fail_files[@]} -gt 0 ]]; then
    exit 1
    elif [[ "$mode" == "test" && (${#exec_fail_files[@]} -gt 0 || ${#fail_files[@]} -gt 0) ]]; then
    exit 1
    elif [[ "$mode" == "diff" && ${#fail_files[@]} -gt 0 ]]; then
    exit 1
else
    exit 0
fi
