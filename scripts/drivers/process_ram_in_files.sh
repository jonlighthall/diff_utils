#!/bin/bash
# Usage: ./process_ram_in_files.sh <mode> [directory] [options]
# Sep 2025 JCL
# =============================
# EXPLANATION SECTION
#
# This script processes .in files in a directory using the specified executable.
#
# Input options (first argument):
#   make - Runs the executable only (no file operations)
#   copy - Copies existing output files (.line) to reference names (.tl, .rtl, .ftl)
#   test - Runs the executable, logs output, and compares results to reference files
#   diff - Compares existing output files to reference files (does not run the executable)
#
# PROMPT: To process files, run:
#   ./process_in_files.sh <mode> [directory] [options]
# Where <mode> is 'make', 'copy', 'test', or 'diff'. Directory defaults to 'std'.
#
# Additional options:
#   --exe <path>        Specify executable path (default: bin/ram1.5.exe)
#   --pattern <glob>    Include files matching glob pattern (can be used multiple times)
#   --exclude <glob>    Exclude files matching glob pattern (can be used multiple times)
#   --skip-existing     Skip files where outputs already exist
#   --skip-newer        Skip files where outputs are newer than input
#   --force             Override skip options and process all matched files
#   --dry-run           Show what would be processed without running
#   --debug             Show detailed file filtering information
#   -h, --help          Show this help message
#
# Dependencies:
#   - lib_diff_utils.sh (must be in same directory or PATH)
#   - bin/ram1.5.exe
#   - Optional: tldiff, uband_diff for advanced comparisons
#
# =============================

usage() {
  cat <<EOF
Usage: $0 <mode> [directory] [options]

Modes:
  make    Run executable only (no file operations)
  copy    Copy existing output files (.line) to reference files (.tl)
  test    Run executable and compare outputs to reference files
  diff    Compare existing output files to reference files (no program execution)

Options:
  --exe <path>        Specify executable path (default: bin/ram1.5.exe)
  --pattern <glob>    Include files matching glob pattern (can be used multiple times)
  --exclude <glob>    Exclude files matching glob pattern (can be used multiple times)
  --skip-existing     Skip files where outputs already exist
  --skip-newer        Skip files where outputs are newer than input
  --force             Override skip options and process all matched files
  --dry-run           Show what would be processed without running
  --debug             Show detailed file filtering information
  -h, --help          Show this help message

Examples:
  $0 test std --exe ./prog.x --pattern 'case*'
  $0 test std --pattern 'case*' --exclude 'case_old*'
  $0 make . --skip-existing --debug
  $0 diff std --pattern 'test1*' --pattern 'test2*'

Environment variables:
  RAM_EXE             Alternative way to specify RAM executable path
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
cli_exe=""

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
        --pattern) patterns+=("$2"); shift 2;;
        --exclude) excludes+=("$2"); shift 2;;
        --skip-existing) skip_existing=true; shift;;
        --skip-newer) skip_newer=true; shift;;
        --force) force=true; shift;;
        --dry-run) dry_run=true; shift;;
        --debug) debug=true; shift;;
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
    echo "  make - Run executable only (no file operations)"
    echo "  copy - Copy existing output files to reference files"
    echo "  test - Time and run executable and compare outputs to reference files"
    echo "  diff - Compare existing output files to reference files (no program execution)"
    exit 1
fi

if [[ ! -d "$directory" ]]; then
    echo "Error: Directory '$directory' does not exist. Exiting."
    exit 1
fi

# Function to find the project root directory (where bin/ is located)
find_project_root() {
    local current_dir="$PWD"
    while [[ "$current_dir" != "/" ]]; do
        if [[ -d "$current_dir/bin" ]]; then
            echo "$current_dir"
            return 0
        fi
        current_dir="$(dirname "$current_dir")"
    done
    # If we can't find bin/, use current directory
    echo "$PWD"
}

# Find the project root directory
PROJECT_ROOT="$(find_project_root)"

# Set the program executable
# Priority: command line --exe > RAM_EXE environment variable > default
if [[ -n "$cli_exe" ]]; then
    PROG="$cli_exe"
    elif [[ -n "$RAM_EXE" ]]; then
    PROG="$RAM_EXE"
else
    PROG="bin/ram1.5.x"
fi
PROG_OUTPUT_COLOR="\x1B[38;5;71m" # Light green color for PROG output
PROG_OUTPUT_COLOR="\x1B[0m" # Reset color for PROG output

# Detect the appropriate executable (skip for diff and copy modes)
if [[ "$mode" != "diff" && "$mode" != "copy" ]]; then
    echo "Selected program: $PROG"

    # Check if the program exists, and build it if not
    if [[ ! -f "$PROG" ]]; then
        echo "Program $PROG not found. Attempting to build it..."
        make
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

# Create array of files to process, sorted by size
mapfile -t infiles < <(find "$directory" -maxdepth 1 -type f -name '*.in' -exec ls -lSr {} + | awk '{print $9}')

if [[ ${#infiles[@]} -eq 0 ]]; then
    echo
    echo -e "\e[31mNo input files (*.in) found in directory: $directory\e[0m"
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
    echo "Will copy existing output files (.line) to reference files (.tl)"
    elif [[ "$mode" == "test" ]]; then
    echo "Will run ${PROG} and compare outputs to reference files"
    elif [[ "$mode" == "diff" ]]; then
    echo "Will compare existing output files to reference files (no ${PROG} execution)"
fi
printf '%*s\n' "$line_len" '' | tr '  ' '='

for infile in "${infiles[@]}"; do
    if $dry_run; then
        echo "[DRY-RUN] Would process: $infile"
        continue
    fi

    echo "Processing: $infile"
    LOG_FILE="$directory/$(basename "$infile" .in).log"
    basename_noext="$(basename "$infile" .in)"
    parent_dir="$PROJECT_ROOT"

    # Check if we should skip this file
    skip_reason=""

    if ($skip_existing || $skip_newer) && ! $force; then
        # Define primary output file (what gets generated by running PROG)
        primary_output="$directory/${basename_noext}.line"

        # Define secondary/reference files (these are for comparison, not generated by PROG)
        secondary_outputs=(
            "$directory/${basename_noext}.tl"
            "$directory/${basename_noext}.grid"
            "$directory/${basename_noext}.vert"
            "$directory/${basename_noext}.check"
        )

        if $skip_existing; then
            # For make/test modes: only skip if primary output exists
            # For copy/diff modes: check secondary outputs too
            if [[ "$mode" == "make" || "$mode" == "test" ]]; then
                if [[ -f "$primary_output" ]]; then
                    skip_reason="output file exists"
                fi
            else
                # For copy/diff: check if any relevant files exist
                if [[ -f "$primary_output" ]]; then
                    skip_reason="output files exist"
                else
                    for output in "${secondary_outputs[@]}"; do
                        if [[ -f "$output" ]]; then
                            skip_reason="output files exist"
                            break
                        fi
                    done
                fi
            fi
        fi

        if $skip_newer && [[ -z "$skip_reason" ]]; then
            # Check if primary output is newer than input
            if [[ -f "$primary_output" && "$primary_output" -nt "$infile" ]]; then
                skip_reason="output newer than input"
            fi
        fi
    fi

    if [[ -n "$skip_reason" ]]; then
        echo "  [SKIP] $infile ($skip_reason)"
        add_to_array_if_not_present "skipped_files" "$infile"
        continue
    fi

    # Run $PROG for 'make' and 'test' modes, but not for 'copy' or 'diff' mode
    if [[ "$mode" == "make" || "$mode" == "test" ]]; then
        # Copy infile to ram.in in parent directory
        cp "$infile" "$parent_dir/ram.in"

        # Check for existing output files and warn user
        existing_files=()
        potential_output="$parent_dir/tl.line"
        if [[ -f "$potential_output" ]]; then
            existing_files+=("$potential_output")
        fi

        # Also check for other common output files
        for ext in .log; do
            potential_output="$parent_dir/${basename_noext}${ext}"
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

        echo -n "   Running: $PROG... "
        echo -en "${PROG_OUTPUT_COLOR}" # Set text color to highlight PROG output (light green)
        set +e  # Temporarily disable exit on error to handle executable failures gracefully
        if [[ "$mode" == "test" ]]; then
            { time "$PROG"; } >> "$LOG_FILE" 2>&1
            RETVAL=$?
        else
            echo
            "$PROG"
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
        else
            echo -e "\e[31mFAIL\e[0m"
            echo -e "   \e[31mError: ${PROG} failed with exit code $RETVAL for $infile\e[0m"
            echo "   Error: ${PROG} failed with exit code $RETVAL for $infile" >> "$LOG_FILE"
            exec_fail_files+=("$infile")
        fi

        # After running the executable, move the output files to the target directory
        test="$directory/${basename_noext}.line"
        grid="$directory/${basename_noext}.grid"
        vert="$directory/${basename_noext}.vert"
        check="$directory/${basename_noext}.check"
        # Move tl.line
        if [[ -f "$parent_dir/tl.line" ]]; then
            mv "$parent_dir/tl.line" "$test"
            echo "   Moved tl.line to $test"
            # Check if the moved output file is empty
            if [[ ! -s "$test" ]]; then
                echo -e "   \e[33mWarning: Output file $test is empty\e[0m"
                empty_output_files+=("$test")
                # Only reclassify from success to failure if not already in fail array
                # This prevents double-counting when execution failed AND produced empty output
                if [[ ! " ${exec_fail_files[*]} " =~ " $infile " ]]; then
                    # Remove from success array and add to fail array
                    temp_array=()
                    for f in "${exec_success_files[@]}"; do
                        if [[ "$f" != "$infile" ]]; then
                            temp_array+=("$f")
                        fi
                    done
                    exec_success_files=("${temp_array[@]}")
                    exec_fail_files+=("$infile")
                fi
            fi
        else
            echo -e "   \e[33mWarning: Expected output file tl.line not found in parent directory\e[0m"
        fi
        # Move tl.grid
        if [[ -f "$parent_dir/tl.grid" ]]; then
            mv "$parent_dir/tl.grid" "$grid"
            echo "   Moved tl.grid to $grid"
        else
            echo -e "   \e[33mWarning: Expected output file tl.grid not found in parent directory\e[0m"
        fi
        # Move p.vert
        if [[ -f "$parent_dir/p.vert" ]]; then
            mv "$parent_dir/p.vert" "$vert"
            echo "   Moved p.vert to $vert"
        else
            : #echo -e "   \e[33mWarning: Expected output file p.vert not found in parent directory\e[0m"
        fi
        # Move pade.check
        if [[ -f "$parent_dir/pade.check" ]]; then
            mv "$parent_dir/pade.check" "$check"
            echo "   Moved pade.check to $check"
        else
            : #echo -e "   \e[33mWarning: Expected output file pade.check not found in parent directory\e[0m"
        fi
    fi

    # For 'make' mode, we're done after running $PROG and moving output - skip file operations
    if [[ "$mode" == "make" ]]; then
        printf '%*s\n' "$line_len" '' | tr '  ' '='
        continue
    fi

    # Handle the output file
    test="$directory/${basename_noext}.line"
    ref="$directory/${basename_noext}.tl"

    # For make and test modes, check if test file exists (after moving from parent directory)
    # For copy mode, check if basename.line exists directly
    # For diff mode, check if either test or ref exists
    # For test mode, we also need to handle the case where executable ran but produced no output
    if [[ ("$mode" == "test") ]] || [[ "$mode" == "copy" && -f "$test" && -s "$test" ]] || [[ "$mode" == "diff" && (-f "$test" || -f "$ref") ]]; then
        # Handle test mode specially - check if output was produced and moved successfully
        if [[ "$mode" == "test" ]]; then
            if [[ ! -f "$test" ]]; then
                # Executable ran successfully but no output file was generated or moved
                echo -e "\e[33m[[MISSING OUTPUT]]\e[0m"
                echo -e "   \e[33mExecutable completed successfully but no output file 'tl.line' was generated\e[0m"
                echo -e "   \e[33mExecutable completed successfully but no output file 'tl.line' was generated\e[0m" >> "$LOG_FILE"
                missing_exec_output_files+=("$test")
                missing_output_files+=("$test")
                add_to_array_if_not_present "skipped_files" "$infile"
                printf '%*s\n' "$line_len" '' | tr '  ' '='
                continue
            fi
        fi

        # For diff mode, we need to check files even if test doesn't exist
        if [[ "$mode" == "diff" && ! -f "$test" ]]; then
            # In diff mode, check if ref exists
            if [[ -f "$ref" ]]; then
                echo -e "\e[33m[[MISSING OUTPUT]]\e[0m\n   Output file '$test' does not exist"
                echo -e "   \e[33mHint: Run 'make' mode first to generate output files\e[0m"
                # In diff mode, only mark as missing, not as failed (since no comparison was attempted)
                missing_output_files+=("$test")
                add_to_array_if_not_present "skipped_files" "$infile"
            else
                echo -e "\e[33m[[MISSING FILES]]\e[0m\n   Both output '$test' and reference '$ref' do not exist"
                echo -e "   \e[33mHint: Run 'copy' mode first to generate reference files, then 'test' mode for output files\e[0m"
                # In diff mode, only mark as missing, not as failed (since no comparison was attempted)
                missing_output_files+=("$test")
                missing_reference_files+=("$ref")
                add_to_array_if_not_present "skipped_files" "$infile"
            fi
            elif [[ -f "$test" && -s "$test" ]]; then
            if [[ "$mode" == "copy" ]]; then
                # COPY mode: Copy basename.line to .tl
                # Check if source file is empty before copying
                if [[ ! -s "$test" ]]; then
                    echo -e "   \e[33mWarning: Source file $test is empty\e[0m"
                    empty_output_files+=("$test")
                fi
                cp -v "$test" "$ref"
                echo "   Copied $test to $ref"
                processed_files+=("$infile")
                elif [[ "$mode" == "test" || "$mode" == "diff" ]]; then
                # TEST and DIFF modes: Compare files
                echo -n "   Comparing $test to $ref... "

                # Check if output file is empty
                if [[ ! -s "$test" ]]; then
                    echo -e "\e[33m[[EMPTY OUTPUT]]\e[0m"
                    echo "   Output file '$test' is empty"
                    empty_output_files+=("$test")
                    if [[ "$mode" == "test" ]]; then
                        echo -e "\e[33m[[EMPTY OUTPUT]]\e[0m" >> "$LOG_FILE"
                        echo "   Output file '$test' is empty" >> "$LOG_FILE"
                    fi
                    add_to_array_if_not_present "skipped_files" "$infile"
                # Check if reference file exists
                elif [[ ! -f "$ref" ]]; then
                    echo -e "\e[33m[[MISSING REFERENCE]]\e[0m\n   Reference file '$ref' does not exist"
                    if [[ "$mode" == "test" ]]; then
                        echo -e "\e[33m[[MISSING REFERENCE]]\e[0m\n   Reference file '$ref' does not exist" >> "$LOG_FILE"
                        add_to_array_if_not_present "skipped_files" "$infile"
                        elif [[ "$mode" == "diff" ]]; then
                        add_to_array_if_not_present "skipped_files" "$infile"
                    fi
                    echo -e "   \e[33mHint: Run 'copy' mode first to generate reference files\e[0m"
                    missing_reference_files+=("$ref")
                    # Check if reference file is empty
                    elif [[ ! -s "$ref" ]]; then
                    echo
                    echo -e "\e[33m[[EMPTY REFERENCE]]\e[0m"
                    echo "   Reference file '$ref' is empty"
                    if [[ "$mode" == "test" ]]; then
                        echo -e "\e[33m[[EMPTY REFERENCE]]\e[0m"
                        echo "   Reference file '$ref' is empty" >> "$LOG_FILE"
                        add_to_array_if_not_present "skipped_files" "$infile"
                        elif [[ "$mode" == "diff" ]]; then
                        add_to_array_if_not_present "skipped_files" "$infile"
                    fi
                    empty_files+=("$ref")
                else
                    # Perform the actual comparison
                    if [[ "$mode" == "test" ]]; then
                        # TEST mode: Log comparison results
                        # Capture diff output to check which diff tool failed
                        diff_output_file=$(mktemp)
                        if diff_files "$test" "$ref" >> "$LOG_FILE" 2>&1; then
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
                        diff_files "$test" "$ref" 2>&1 | tee "$diff_output_file"
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
                fi
            fi
        fi
    else
        if [[ "$mode" == "copy" ]]; then
            echo -e "   \e[33m[[SKIPPED]]\e[0m No $test file found to rename"
            echo -e "   \e[33mHint: Run 'make' or 'test' mode first to generate output files\e[0m"
            skipped_files+=("$infile")
            elif [[ "$mode" == "test" ]]; then
            echo -e "   \e[36m[[INFO]]\e[0m Output file tl.line will be generated by ram1.5.exe in parent directory"
        else
            echo -e "\e[33m[[NO FILES]]\e[0m"
            echo "No output file (basename.line) or reference file (.tl) found for $basename_noext"
            if [[ "$mode" == "diff" ]]; then
                echo -e "   \e[33mHint: Run 'make' mode to generate output files, and 'copy' mode to generate reference files\e[0m"
                add_to_array_if_not_present "skipped_files" "$infile"
            else
                fail_files+=("$infile")
            fi
        fi
    fi

    # Clean up extra files
    # Only remove files that are of the form: <basename>.<ext> or <basename>_<suffix>
    # This avoids matching other case names such as case10 when processing case1
    shopt -s nullglob
    files_to_clean=("$directory/${basename_noext}."* "$directory/${basename_noext}_"*)
    for f in "${files_to_clean[@]}"; do
        # Skip input, reference, output, and log
        if [[ "$f" == "$infile" || "$f" == "$ref" || "$f" == "$test" || "$f" == "$LOG_FILE" || "$f" == "$directory/${basename_noext}.in" ]]; then
            continue
        fi
        # Never delete .in or .tl files as a safety rule
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

    # Clean up ram.in from parent directory if it exists
    if [[ -f "$parent_dir/ram.in" ]]; then
        rm "$parent_dir/ram.in"
        if [[ "$mode" != "test" ]]; then
            echo "   Deleted $parent_dir/ram.in"
        fi
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
                echo -e "   \e[33mNO_OUTPUT\e[0m $(basename "$f" .line)"
            done
        fi

        if [[ ${#empty_output_files[@]} -gt 0 ]]; then
            echo "Empty output files: ${#empty_output_files[@]}"
            for f in "${empty_output_files[@]}"; do
                echo -e "   \e[33mEMPTY_OUTPUT\e[0m $(basename "$f" .line)"
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
                echo -e "   \e[33mEMPTY_OUTPUT\e[0m $(basename "$f" .line)"
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
        if [[ ${#exec_fail_files[@]} -eq 0 && ${#empty_output_files[@]} -eq 0 ]]; then
            echo -e "\e[32mAll files generated successfully!\e[0m"
        elif [[ ${#empty_output_files[@]} -gt 0 ]]; then
            echo -e "\e[33mSome output files are empty. Check execution results above.\e[0m"
        else
            echo -e "\e[31mSome executables failed. Check execution errors above.\e[0m"
        fi
        elif [[ "$mode" == "test" ]]; then
        if [[ ${#exec_fail_files[@]} -eq 0 && ${#fail_files[@]} -eq 0 && ${#skipped_files[@]} -eq 0 && ${#empty_output_files[@]} -eq 0 ]]; then
            echo -e "\e[32mAll tests passed!\e[0m"
            elif [[ ${#exec_fail_files[@]} -gt 0 ]]; then
            echo -e "\e[31mSome executables failed. Check execution errors above.\e[0m"
            elif [[ ${#empty_output_files[@]} -gt 0 ]]; then
            echo -e "\e[33mSome output files are empty. Check execution results above.\e[0m"
            elif [[ ${#fail_files[@]} -gt 0 ]]; then
            echo -e "\e[31mSome diff comparisons failed. Check diff results above.\e[0m"
            elif [[ ${#skipped_files[@]} -gt 0 ]]; then
            echo -e "\e[33mSome files were skipped due to missing dependencies. Check file status above.\e[0m"
        fi
        elif [[ "$mode" == "diff" ]]; then
        if [[ ${#fail_files[@]} -eq 0 && ${#skipped_files[@]} -eq 0 && ${#empty_output_files[@]} -eq 0 ]]; then
            echo -e "\e[32mAll diffs passed!\e[0m"
            elif [[ ${#empty_output_files[@]} -gt 0 ]]; then
            echo -e "\e[33mSome output files are empty. Check diff results above.\e[0m"
            elif [[ ${#fail_files[@]} -gt 0 ]]; then
            echo -e "\e[31mSome diff comparisons failed. Check diff results above.\e[0m"
            elif [[ ${#skipped_files[@]} -gt 0 ]]; then
            echo -e "\e[33mSome files were skipped due to missing dependencies. Check file status above.\e[0m"
        fi
    fi
    echo "=============================="
fi

# Exit with appropriate code
if [[ "$mode" == "make" && (${#exec_fail_files[@]} -gt 0 || ${#empty_output_files[@]} -gt 0) ]]; then
    exit 1
    elif [[ "$mode" == "test" && (${#exec_fail_files[@]} -gt 0 || ${#fail_files[@]} -gt 0 || ${#empty_output_files[@]} -gt 0) ]]; then
    exit 1
    elif [[ "$mode" == "diff" && (${#fail_files[@]} -gt 0 || ${#empty_output_files[@]} -gt 0) ]]; then
    exit 1
else
    exit 0
fi
