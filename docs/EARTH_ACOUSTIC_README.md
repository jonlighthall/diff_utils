# Spherical Earth Acoustic Ray Calculator

## Overview

This program calculates where an acoustic beam intersects with Earth's surface, accounting for spherical Earth curvature. It's designed to test spherical Earth corrections for underwater acoustic propagation models.

## Purpose

In flat-Earth acoustic propagation models, a beam shot horizontally (0 degrees) from a source at depth continues parallel to the surface forever. However, in reality, due to Earth's curvature, such a beam will eventually intersect the surface. This program provides ballpark estimates of where this occurs for given source depths and beam angles.

## Usage

```bash
./earth_acoustic [depth_meters] [angle_degrees]
```

### Parameters

- `depth_meters`: Source depth below surface (positive, in meters)
- `angle_degrees`: Beam angle from horizontal (0 = horizontal, positive = upward, -90 to +90 degrees)

If no parameters are provided, the program will prompt for input.

### Examples

```bash
# 100m deep source, horizontal beam
./earth_acoustic 100 0

# 1000m deep source, 10 degree upward beam  
./earth_acoustic 1000 10

# Interactive mode
./earth_acoustic
```

## Theory

The program uses geometric ray theory with the following assumptions:
- Straight-line ray propagation in a spherically stratified medium
- Constant sound speed (range- and depth-independent)
- Earth modeled as a perfect sphere with radius 6,371 km

The calculation uses the law of sines to solve the triangle formed by:
- Earth center
- Source point
- Surface intersection point

## Output

The program provides:
- Input parameters (Earth radius, source depth/radius, beam angle)
- Horizontal range to surface intersection (km and m)
- Central angle subtended by the ray path
- Context classification (local/coastal/regional/basin/global scale)
- Flat-Earth comparison showing curvature effects

## Key Findings

1. **Horizontal beams** can travel thousands of kilometers before surface intersection
2. **Small upward angles** dramatically reduce range
3. **Earth curvature effects** are significant for long-range propagation
4. **Flat-Earth models** severely underestimate propagation ranges

## Example Results

| Depth | Angle | Range (km) | Flat Earth (km) | Curvature Effect |
|-------|-------|------------|-----------------|------------------|
| 100m  | 0°    | 9,972      | ∞               | Infinite difference |
| 1000m | 1°    | 9,849      | 57.3            | +9,792 km |
| 100m  | 45°   | 5,004      | 0.1             | +5,004 km |
| 100m  | 89°   | 111        | 0.002           | +111 km |

## Applications

This tool is useful for:
- Validating spherical Earth corrections in acoustic propagation codes
- Understanding the scale of curvature effects in ocean acoustics
- Educational demonstrations of geometric acoustics
- Ballpark estimates for acoustic propagation planning

## Limitations

- Assumes straight-line propagation (no ray bending due to sound speed gradients)
- Constant sound speed profile
- Perfect spherical Earth (no topography)
- No absorption or scattering effects
- Geometric acoustics approximation (high frequency limit)

## Files

- `earth_acoustic.f90` - Main program source code
- `earth_acoustic` - Compiled executable  
- `test_earth_acoustic.sh` - Test script with example scenarios
- `bin/earth_acoustic` - Executable in bin directory