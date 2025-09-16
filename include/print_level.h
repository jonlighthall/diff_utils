// Dedicated header for PrintLevel to avoid circular dependencies.
#ifndef PRINT_LEVEL_H
#define PRINT_LEVEL_H
struct PrintLevel {
  int level = 0;           // Debug level for printing
  bool diff_only = false;  // Print only differences
  bool debug = false;      // Print debug messages
  bool debug2 = false;     // Print additional debug messages
  bool debug3 = false;     // Print even more debug messages
};
#endif  // PRINT_LEVEL_H
