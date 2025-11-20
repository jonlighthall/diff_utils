// Dedicated header for output control structures to avoid circular dependencies.
#ifndef PRINT_LEVEL_H
#define PRINT_LEVEL_H

// Controls user-facing output verbosity (what results/statistics to show)
struct VerbosityControl {
  int level = 0;              // Verbosity level: <0=quiet, 0=normal, 1+=verbose
  bool quiet = false;         // Suppress all non-essential output (level < 0)
  bool show_statistics = false;   // Show basic statistics (level >= 1)
  bool show_detailed = false;     // Show detailed analysis (level >= 2)
};

// Controls developer debug diagnostics (troubleshooting/edge case investigation)
struct DebugControl {
  int level = 0;         // Debug level: 0=off, 1=basic, 2=detailed, 3=verbose
  bool enabled = false;  // Basic debug output (level >= 1)
  bool detailed = false; // Detailed debug output (level >= 2)
  bool verbose = false;  // Ultra-verbose debug output (level >= 3)
};

// Controls difference table output filtering
struct TableControl {
  double threshold = 1.0;        // Minimum difference to print in table
  size_t max_rows = 32;          // Maximum rows before truncation notice
  bool print_all_nonzero = false; // When threshold==0, print like diff
};

// Legacy compatibility - kept for backward compatibility during transition
// TODO: Remove after full migration
struct PrintLevel {
  int level = 0;           // Debug level for printing
  bool diff_only = false;  // Print only differences
  bool debug = false;      // Print debug messages
  bool debug2 = false;     // Print additional debug messages
  bool debug3 = false;     // Print even more debug messages
};

#endif  // PRINT_LEVEL_H
