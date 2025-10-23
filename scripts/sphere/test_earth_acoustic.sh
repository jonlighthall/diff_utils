#!/bin/bash
# Test script for spherical Earth acoustic ray calculator
# Shows the effect of Earth's curvature on acoustic propagation

echo "Spherical Earth Acoustic Ray Test Suite"
echo "========================================"
echo ""

echo "Test 1: Shallow source, horizontal beam (typical for long-range sonar)"
./earth_acoustic 100 0
echo ""

echo "Test 2: Deep source, horizontal beam"
./earth_acoustic 1000 0
echo ""

echo "Test 3: Deep source, slight upward angle"
./earth_acoustic 1000 1
echo ""

echo "Test 4: Shallow source, moderate upward angle"
./earth_acoustic 100 10
echo ""

echo "Test 5: Shallow source, steep upward angle"
./earth_acoustic 100 60
echo ""

echo "Test 6: Very shallow source, nearly vertical beam"
./earth_acoustic 50 89
echo ""

echo "Summary:"
echo "========"
echo "This demonstrates that for acoustic propagation modeling:"
echo "1. Horizontal beams can travel thousands of km before hitting surface"
echo "2. Even small upward angles dramatically reduce range"
echo "3. Earth curvature effects are significant for long-range propagation"
echo "4. Flat-earth models severely underestimate ranges"