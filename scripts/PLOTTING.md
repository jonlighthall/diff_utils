# Plotting and Visualization

This repository (`diff_utils`) provides **numerical comparison and text-based analysis only**.

For **plotting and visualization** of NSPE data, see:

**Repository:** `~/utils/nspe_python_plot_utils`

## Available Plotting Tools

- `plot_nspe_tlr_ascii.py` - ASCII TL vs range (nspe01/02/03.asc)
- `plot_nspe_tlr_binary.py` - Binary TL vs range (for041/042.dat)
- `plot_nspe_field2d.py` - 2D contour plots (for003/043.dat)
- `plot_ssfpe_unravg.py` - SSFPE unaveraged TL (unravg.tl)
- `plot_tl_comparison.py` - Compare two TL files with difference highlighting

## Quick Access

If the plotting utilities are installed in `~/utils/nspe_python_plot_utils`, you can call them directly:

```bash
# Example: Plot TL comparison
~/utils/nspe_python_plot_utils/plot_tl_comparison.py reference.asc test.asc

# Example: 2D field plot
~/utils/nspe_python_plot_utils/plot_nspe_field2d.py -f for003.dat -i nspe.in
```

See the plotting repository's README for full documentation.
