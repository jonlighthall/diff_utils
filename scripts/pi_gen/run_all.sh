#!/bin/bash
# Generate pi precision test data in all languages
# Output files are written to ../../data/

set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DATA_DIR="${SCRIPT_DIR}/../../data"
BUILD_DIR="${SCRIPT_DIR}/../../build"

echo "=== Pi Precision Test Data Generator ==="
echo "Output directory: ${DATA_DIR}"
echo

# Fortran
echo "Building Fortran generator..."
gfortran -o "${BUILD_DIR}/bin/pi_gen_fortran" "${SCRIPT_DIR}/pi_gen_fortran.f90"
echo "Running Fortran (ascending)..."
"${BUILD_DIR}/bin/pi_gen_fortran" > "${DATA_DIR}/pi_fortran_asc.txt"
echo "Running Fortran (descending)..."
"${BUILD_DIR}/bin/pi_gen_fortran" desc > "${DATA_DIR}/pi_fortran_desc.txt"

# C++
echo "Building C++ generator..."
g++ -std=c++17 -o "${BUILD_DIR}/bin/pi_gen_cpp" "${SCRIPT_DIR}/pi_gen_cpp.cpp"
echo "Running C++ (ascending)..."
"${BUILD_DIR}/bin/pi_gen_cpp" > "${DATA_DIR}/pi_cpp_asc.txt"
echo "Running C++ (descending)..."
"${BUILD_DIR}/bin/pi_gen_cpp" desc > "${DATA_DIR}/pi_cpp_desc.txt"

# Java
echo "Building Java generator..."
javac -d "${BUILD_DIR}/classes" "${SCRIPT_DIR}/pi_gen_java.java"
echo "Running Java (ascending)..."
java -cp "${BUILD_DIR}/classes" pi_gen_java > "${DATA_DIR}/pi_java_asc.txt"
echo "Running Java (descending)..."
java -cp "${BUILD_DIR}/classes" pi_gen_java desc > "${DATA_DIR}/pi_java_desc.txt"

# Python (no build needed)
echo "Running Python (ascending)..."
python3 "${SCRIPT_DIR}/pi_gen_python.py" > "${DATA_DIR}/pi_python_asc.txt"
echo "Running Python (descending)..."
python3 "${SCRIPT_DIR}/pi_gen_python.py" desc > "${DATA_DIR}/pi_python_desc.txt"

echo
echo "=== All generators complete ==="
echo "Generated files:"
ls -la "${DATA_DIR}"/pi_*.txt
