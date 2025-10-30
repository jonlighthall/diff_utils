#!/usr/bin/env python3
"""
Plot PE (Propagation Environment) data comparison.

Compares reference and test files with multiple TL (Transmission Loss) curves.
Each curve is plotted in its own subplot for clarity.
"""

import numpy as np
import matplotlib.pyplot as plt
import sys
from pathlib import Path

def read_pe_data(filename):
    """
    Read PE data file.
    
    Returns:
        range_values: 1D array of range values (column 1)
        tl_curves: 2D array of TL values (columns 2+), shape (n_points, n_curves)
    """
    data = np.loadtxt(filename)
    range_values = data[:, 0]
    tl_curves = data[:, 1:]  # All columns except first
    return range_values, tl_curves

def plot_pe_comparison(ref_file, test_file, output_file=None):
    """
    Plot comparison of reference and test PE data.
    
    Creates a multi-panel figure with each TL curve in its own subplot.
    
    Args:
        ref_file: Path to reference file
        test_file: Path to test file
        output_file: Optional path to save figure (default: show interactive)
    """
    # Read data
    range_ref, tl_ref = read_pe_data(ref_file)
    range_test, tl_test = read_pe_data(test_file)
    
    n_curves = tl_ref.shape[1]
    print(f"Loaded data:")
    print(f"  Reference: {ref_file}")
    print(f"  Test:      {test_file}")
    print(f"  Range points: {len(range_ref)}")
    print(f"  TL curves:    {n_curves}")
    
    # Determine grid layout (aim for roughly square, prefer more columns)
    n_cols = int(np.ceil(np.sqrt(n_curves)))
    n_rows = int(np.ceil(n_curves / n_cols))
    
    print(f"  Grid layout:  {n_rows} rows × {n_cols} columns")
    print()
    
    # Create figure with subplots
    fig, axes = plt.subplots(n_rows, n_cols, figsize=(4*n_cols, 3*n_rows))
    fig.suptitle(f'PE Comparison: {Path(ref_file).stem} vs {Path(test_file).stem}', 
                 fontsize=14, fontweight='bold')
    
    # Flatten axes array for easier iteration
    axes = axes.flatten() if n_curves > 1 else [axes]
    
    # Plot each curve in its own subplot
    for i in range(n_curves):
        ax = axes[i]
        
        # Plot reference (solid line)
        ax.plot(range_ref, tl_ref[:, i], 'b-', linewidth=1.5, 
                label='Reference', alpha=0.8)
        
        # Plot test (dotted line) - easier to see when curves overlap
        ax.plot(range_test, tl_test[:, i], 'r:', linewidth=2.0, 
                label='Test', alpha=0.8)
        
        # Formatting
        ax.set_xlabel('Range (km)', fontsize=9)
        ax.set_ylabel('TL (dB)', fontsize=9)
        ax.set_title(f'Curve {i+1}', fontsize=10, fontweight='bold')
        ax.grid(True, alpha=0.3, linestyle='--', linewidth=0.5)
        ax.legend(loc='best', fontsize=8)
        
        # Invert y-axis (higher TL = more loss = plot downward)
        ax.invert_yaxis()
        
        # Tick label size
        ax.tick_params(labelsize=8)
    
    # Hide unused subplots
    for i in range(n_curves, len(axes)):
        axes[i].set_visible(False)
    
    # Adjust layout to prevent overlap
    plt.tight_layout(rect=[0, 0.03, 1, 0.97])
    
    # Save or show
    if output_file:
        plt.savefig(output_file, dpi=300, bbox_inches='tight')
        print(f"Figure saved to: {output_file}")
    else:
        print("Displaying interactive plot...")
        plt.show()

def main():
    """Main entry point with command-line argument handling."""
    if len(sys.argv) < 3:
        print("Usage: plot_pe_comparison.py <ref_file> <test_file> [output_file]")
        print()
        print("Arguments:")
        print("  ref_file    : Reference data file")
        print("  test_file   : Test data file")
        print("  output_file : (Optional) Output figure filename (e.g., comparison.png)")
        print()
        print("Examples:")
        print("  # Interactive display:")
        print("  ./plot_pe_comparison.py pe.std1.pe01.ref.txt pe.std1.pe01.test.txt")
        print()
        print("  # Save to file:")
        print("  ./plot_pe_comparison.py pe.std1.pe01.ref.txt pe.std1.pe01.test.txt pe_comparison.png")
        sys.exit(1)
    
    ref_file = sys.argv[1]
    test_file = sys.argv[2]
    output_file = sys.argv[3] if len(sys.argv) > 3 else None
    
    # Check files exist
    if not Path(ref_file).exists():
        print(f"Error: Reference file not found: {ref_file}")
        sys.exit(1)
    if not Path(test_file).exists():
        print(f"Error: Test file not found: {test_file}")
        sys.exit(1)
    
    plot_pe_comparison(ref_file, test_file, output_file)

if __name__ == '__main__':
    main()
