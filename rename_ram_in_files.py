#!/usr/bin/env python3
"""
Script to rename .in files based on frequency and depth parameters from line 2.

For each .in file, reads the first two numbers from line 2 (frequency and depth),
then renames the file to: ram_fname_d0000_f00000.in

Where:
- d0000: depth (second number), rounded down, 4 digits with leading zeros
- f00000: frequency (first number), rounded down, 5 digits with leading zeros

Only renames files that don't already follow this naming pattern.
Uses git mv if the file is tracked by git, otherwise uses regular mv.
"""

import os
import glob
import shutil
import subprocess


def is_file_tracked_by_git(filepath):
    """Check if a file is tracked by git."""
    try:
        # Use git ls-files to check if file is tracked
        result = subprocess.run(
            ["git", "ls-files", "--error-unmatch", filepath],
            capture_output=True,
            text=True,
            cwd=os.path.dirname(filepath),
        )
        return result.returncode == 0
    except Exception:
        return False


def extract_parameters_from_file(filepath):
    """Extract first two numbers from line 2 of the file."""
    try:
        with open(filepath, "r") as f:
            lines = f.readlines()
            if len(lines) < 2:
                print(f"Warning: {filepath} has fewer than 2 lines")
                return None, None

            # Get line 2 (index 1)
            line2 = lines[1].strip()
            if not line2:
                print(f"Warning: Line 2 is empty in {filepath}")
                return None, None

            # Split by whitespace and get first two numbers
            parts = line2.split()
            if len(parts) < 2:
                print(f"Warning: Line 2 has fewer than 2 values in {filepath}")
                return None, None

            try:
                freq = float(parts[0])
                depth = float(parts[1])
                return freq, depth
            except ValueError as e:
                print(
                    f"Warning: Could not parse numbers from line 2 in {filepath}: {e}"
                )
                return None, None

    except Exception as e:
        print(f"Error reading {filepath}: {e}")
        return None, None


def generate_target_filename(original_name, freq, depth):
    """Generate the target filename based on frequency and depth."""
    # Remove .in extension to get base name
    base_name = original_name[:-3] if original_name.endswith(".in") else original_name

    # Round down to nearest integers
    freq_int = int(freq)  # floor() equivalent for positive numbers
    depth_int = int(depth)

    # Format with leading zeros
    freq_str = f"{freq_int:05d}"
    depth_str = f"{depth_int:04d}"

    # Create target filename
    target_name = f"ram_{base_name}_d{depth_str}_f{freq_str}.in"
    return target_name


def rename_file(source_path, target_path):
    """Rename file using git mv if tracked by git, otherwise use regular mv."""
    source_dir = os.path.dirname(source_path)
    source_name = os.path.basename(source_path)
    target_name = os.path.basename(target_path)

    # Check if file is tracked by git
    if is_file_tracked_by_git(source_path):
        print(f"  Using git mv (file is tracked by git)")
        try:
            result = subprocess.run(
                ["git", "mv", source_name, target_name],
                cwd=source_dir,
                capture_output=True,
                text=True,
            )
            if result.returncode == 0:
                print(f"  Successfully renamed with git mv")
                return True
            else:
                print(f"  git mv failed: {result.stderr}")
                return False
        except Exception as e:
            print(f"  Error with git mv: {e}")
            return False
    else:
        print(f"  Using regular mv (file not tracked by git)")
        try:
            os.rename(source_path, target_path)
            print(f"  Successfully renamed with regular mv")
            return True
        except Exception as e:
            print(f"  Error with regular mv: {e}")
            return False


def main():
    """Main function to process all .in files in current directory."""
    # Get current directory
    current_dir = os.getcwd()
    print(f"Processing .in files in: {current_dir}")

    # Find all .in files
    in_files = glob.glob("*.in")
    print(f"Found {len(in_files)} .in files")

    renamed_count = 0
    skipped_count = 0

    for filename in sorted(in_files):
        print(f"\nProcessing: {filename}")

        # Check if file already follows the target naming pattern
        if (
            filename.startswith("ram_")
            and "_d" in filename
            and "_f" in filename
            and filename.endswith(".in")
        ):
            print(f"  Skipping: {filename} (already follows target naming pattern)")
            skipped_count += 1
            continue

        # Extract parameters from file
        freq, depth = extract_parameters_from_file(filename)
        if freq is None or depth is None:
            print(f"  Skipping: {filename} (could not extract parameters)")
            continue

        print(f"  Frequency: {freq}, Depth: {depth}")

        # Generate target filename
        target_name = generate_target_filename(filename, freq, depth)
        print(f"  Target name: {target_name}")

        # Check if target file already exists
        if os.path.exists(target_name):
            if os.path.samefile(filename, target_name):
                print(f"  Skipping: {filename} is already the target file")
                skipped_count += 1
                continue
            else:
                print(
                    f"  Warning: Target file {target_name} already exists and is different from source"
                )
                print("  Consider manual intervention to avoid conflicts")
                skipped_count += 1
                continue

        # Rename the file
        source_path = os.path.join(current_dir, filename)
        target_path = os.path.join(current_dir, target_name)

        if rename_file(source_path, target_path):
            renamed_count += 1
        else:
            print(f"  Failed to rename {filename}")

    print("\nSummary:")
    print(f"  Files renamed: {renamed_count}")
    print(f"  Files skipped: {skipped_count}")
    print(f"  Total processed: {len(in_files)}")


if __name__ == "__main__":
    main()
