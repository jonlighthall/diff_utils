#!/usr/bin/env python3
"""
Pi Precision Test - Python Version

Tests different rounding behaviors across languages.
Python 3 uses banker's rounding (round half to even) by default.
"""

import math
import sys


def write_precision_file(filename, value, start_dp, end_dp, step):
    """
    Write value with varying decimal precision to file.

    Args:
        filename: output file name
        value: the number to write (e.g., pi, sqrt(2), etc.)
        start_dp: starting decimal places
        end_dp: ending decimal places
        step: increment (+1 for ascending, -1 for descending)
    """
    with open(filename, 'w') as f:
        # Loop over decimal places
        current = start_dp
        while (step > 0 and current <= end_dp) or (step < 0 and current >= end_dp):
            if current == 0:
                f.write(f"{int(value)}\n")
            else:
                # Python format uses banker's rounding (round half to even)
                f.write(f"{value:.{current}f}\n")
            current += step


def main():
    # Calculate pi
    pi = 4.0 * math.atan(1.0)

    # Get machine epsilon
    epsilon = sys.float_info.epsilon

    # Calculate max decimal places (conservative)
    max_decimal_places = int(-math.log10(epsilon)) - 1
    if max_decimal_places > 15:
        max_decimal_places = 15

    # Get base filename from command line or use default
    base_filename = sys.argv[1] if len(sys.argv) > 1 else "pi_output.txt"

    # Generate ascending precision file
    write_precision_file(f"{base_filename}.asc", pi,
                        0, max_decimal_places, 1)

    # Generate descending precision file
    write_precision_file(f"{base_filename}.desc", pi,
                        max_decimal_places, 0, -1)

    # Print summary to console
    print("Pi Precision Test Program (Python)")
    print("===================================")
    print(f"Calculated pi:           {pi:.15f}")
    print(f"Machine epsilon:         {epsilon:.5e}")
    print(f"Max valid decimal places: {max_decimal_places}")
    print()
    print(f"Ascending file:  {base_filename}.asc")
    print(f"Descending file: {base_filename}.desc")
    print(f"Each contains {max_decimal_places + 1} lines")
    print()
    print("Rounding mode: Banker's rounding (round half to even)")


if __name__ == "__main__":
    main()
