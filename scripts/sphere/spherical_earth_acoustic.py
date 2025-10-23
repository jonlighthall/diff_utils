#!/usr/bin/env python3
"""
Spherical Earth Acoustic Ray Intersection Calculator

Program to calculate where an acoustic beam intersects with Earth's surface
accounting for spherical Earth curvature, with visualization.

Purpose: Test spherical Earth corrections for underwater acoustic propagation.
A beam shot horizontally (0 degrees) from a source at depth will eventually
intersect the surface due to Earth's curvature.

Author: Created for acoustic propagation testing
Date: October 2025
"""

import numpy as np
import matplotlib

matplotlib.use("TkAgg")  # Use TkAgg backend for better window persistence
import matplotlib.pyplot as plt
import sys
import time
import argparse


# Earth parameters (defaults)
DEFAULT_EARTH_RADIUS = 6371008.8  # Earth radius in meters
DEG_TO_RAD = np.pi / 180.0
RAD_TO_DEG = 180.0 / np.pi


def calculate_surface_intersection(r_source, angle_rad, r_earth):
    """
    Calculate where a straight-line ray from source intersects Earth's surface.

    Uses law of sines in the triangle formed by:
    - Earth center
    - Source point
    - Surface intersection point

    Parameters
    ----------
    r_source : float
        Distance from Earth center to source [m]
    angle_rad : float
        Beam angle from surface tangent [radians] (positive = downward into Earth)
        0 = parallel to surface (tangent to circle at source depth)
    r_earth : float
        Earth radius [m]

    Returns
    -------
    range_m : float
        Horizontal distance along surface [m], -1 if no intersection
    central_angle : float
        Angle subtended at Earth center [radians]
    """

    # For a horizontal ray (angle = 0), the ray is tangent to the circle at source depth
    # The ray travels in a straight line in 3D space (isovelocity medium)
    #
    # Geometry: The ray starts tangent to radius r_source and intersects surface at r_earth
    # Central angle is: θ = arccos((r_source)/r_earth)
    #
    # For non-zero angles:
    # - Positive angle = ray angled downward (into Earth)
    # - Negative angle = ray angled upward (toward surface)

    # Effective source radius considering beam angle
    # For horizontal (0°): use r_source
    # For downward (+): ray starts at larger effective radius
    # For upward (-): ray starts at smaller effective radius
    r_effective = r_source * np.cos(angle_rad)

    # Check if ray can reach surface
    if r_effective <= 0 or r_effective > r_earth:
        return -1.0, 0.0

    # Calculate central angle using the tangent geometry formula
    # θ = arccos(r_effective / r_earth)
    cos_central = r_effective / r_earth

    if cos_central < -1.0 or cos_central > 1.0:
        return -1.0, 0.0

    central_angle = np.arccos(cos_central)

    # Calculate horizontal range along Earth's surface
    range_m = r_earth * central_angle

    return range_m, central_angle


def calculate_ray_depth_profile(
    source_depth, beam_angle, r_source, range_m, central_angle, r_earth, num_points=100
):
    """
    Calculate the depth of the acoustic ray path below the surface.

    Parameters
    ----------
    source_depth : float
        Source depth below surface [m]
    beam_angle : float
        Beam angle from horizontal [degrees]
    r_source : float
        Distance from Earth center to source [m]
    range_m : float
        Horizontal range to intersection [m]
    central_angle : float
        Central angle [radians]
    r_earth : float
        Earth radius [m]
    num_points : int
        Number of points to calculate along the path

    Returns
    -------
    horizontal_ranges : array
        Horizontal distances along surface [m]
    depths_below_surface : array
        Depths below surface [m]
    max_depth : float
        Maximum depth along the path [m]
    max_depth_range : float
        Horizontal range where maximum depth occurs [m]
    """

    # Source position (at angle = 0)
    source_angle = 0.0
    source_x = r_source * np.cos(source_angle)
    source_y = r_source * np.sin(source_angle)

    # Intersection position
    intersect_angle = central_angle
    intersect_x = r_earth * np.cos(intersect_angle)
    intersect_y = r_earth * np.sin(intersect_angle)

    # Parameterize the straight-line ray path
    t = np.linspace(0, 1, num_points)
    ray_x = source_x + t * (intersect_x - source_x)
    ray_y = source_y + t * (intersect_y - source_y)

    # Calculate distance from Earth center for each point on ray
    ray_r = np.sqrt(ray_x**2 + ray_y**2)

    # Depth below surface at each point
    depths_below_surface = r_earth - ray_r

    # Calculate horizontal range along surface for each point
    # Use angle from Earth center
    ray_angles = np.arctan2(ray_y, ray_x)
    horizontal_ranges = r_earth * ray_angles

    # Find maximum depth
    max_depth_idx = np.argmax(depths_below_surface)
    max_depth = depths_below_surface[max_depth_idx]
    max_depth_range = horizontal_ranges[max_depth_idx]

    return horizontal_ranges, depths_below_surface, max_depth, max_depth_range


def plot_geometry(source_depth, beam_angle, r_source, range_m, central_angle, r_earth):
    """
    Plot the geometry of the acoustic ray path on a spherical Earth.

    Parameters
    ----------
    source_depth : float
        Source depth below surface [m]
    beam_angle : float
        Beam angle from horizontal [degrees]
    r_source : float
        Distance from Earth center to source [m]
    range_m : float
        Horizontal range to intersection [m]
    central_angle : float
        Central angle [radians]
    r_earth : float
        Earth radius [m]
    """

    # Create figure with 3 subplots
    fig = plt.figure(figsize=(18, 6))
    gs = fig.add_gridspec(2, 3, height_ratios=[1, 1])
    ax1 = fig.add_subplot(gs[:, 0])  # Left: full geometry (spans both rows)
    ax2 = fig.add_subplot(gs[0, 1:])  # Top right: zoomed view
    ax3 = fig.add_subplot(gs[1, 1:])  # Bottom right: depth profile

    # Left plot: Full geometry
    ax = ax1

    # Draw Earth circle
    theta_earth = np.linspace(0, 2 * np.pi, 1000)
    x_earth = r_earth * np.cos(theta_earth)
    y_earth = r_earth * np.sin(theta_earth)
    ax.plot(x_earth / 1000, y_earth / 1000, "b-", linewidth=2, label="Earth surface")

    # Mark Earth center
    ax.plot(0, 0, "ko", markersize=8, label="Earth center")

    # Source position - rotated by π/2 so θ=0 is at top (12 o'clock)
    # This makes a "horizontal" ray actually look horizontal in the figure
    source_angle = np.pi / 2  # Start at top of circle
    source_x = r_source * np.cos(source_angle)
    source_y = r_source * np.sin(source_angle)
    ax.plot(
        source_x / 1000,
        source_y / 1000,
        "go",
        markersize=10,
        label=f"Source ({source_depth:.0f} m depth)",
    )

    if range_m > 0:
        # Intersection position - going clockwise (to the right)
        intersect_angle = np.pi / 2 - central_angle
        intersect_x = r_earth * np.cos(intersect_angle)
        intersect_y = r_earth * np.sin(intersect_angle)
        ax.plot(
            intersect_x / 1000,
            intersect_y / 1000,
            "ro",
            markersize=10,
            label=f"Intersection ({range_m/1000:.1f} km range)",
        )

        # Draw the ray path
        ax.plot(
            [source_x / 1000, intersect_x / 1000],
            [source_y / 1000, intersect_y / 1000],
            "r--",
            linewidth=2,
            label="Acoustic ray",
        )

        # Draw radii to source and intersection
        ax.plot(
            [0, source_x / 1000], [0, source_y / 1000], "k--", alpha=0.3, linewidth=1
        )
        ax.plot(
            [0, intersect_x / 1000],
            [0, intersect_y / 1000],
            "k--",
            alpha=0.3,
            linewidth=1,
        )

        # Draw arc to show central angle (going clockwise from top)
        arc_angles = np.linspace(np.pi / 2, np.pi / 2 - central_angle, 50)
        arc_radius = r_earth * 0.3
        arc_x = arc_radius * np.cos(arc_angles)
        arc_y = arc_radius * np.sin(arc_angles)
        ax.plot(arc_x / 1000, arc_y / 1000, "g-", linewidth=2, alpha=0.5)

        # Add angle annotation
        mid_angle = np.pi / 2 - central_angle / 2
        text_x = arc_radius * 1.3 * np.cos(mid_angle)
        text_y = arc_radius * 1.3 * np.sin(mid_angle)
        ax.text(
            text_x / 1000,
            text_y / 1000,
            f"{central_angle*RAD_TO_DEG:.2f}°",
            fontsize=12,
            ha="center",
            color="green",
            weight="bold",
        )

    ax.set_aspect("equal")
    ax.grid(True, alpha=0.3)
    ax.set_xlabel("Distance (km)", fontsize=12)
    ax.set_ylabel("Distance (km)", fontsize=12)
    ax.set_title(
        "Spherical Earth Acoustic Ray Geometry\n(Full View)", fontsize=14, weight="bold"
    )
    ax.legend(loc="best", fontsize=10)

    # Right plot: Zoomed view - Circular geometry showing arc vs chord
    # This shows a cross-section with the Earth's circular surface and straight ray
    ax = ax2

    if range_m > 0:
        # Create the circular arc of the Earth's surface
        # Start at π/2 (top) and go clockwise (decreasing angle) to the right
        theta_arc = np.linspace(np.pi / 2, np.pi / 2 - central_angle, 200)

        # Surface arc in Cartesian coordinates (centered at Earth's center)
        x_surface_cart = r_earth * np.cos(theta_arc)
        y_surface_cart = r_earth * np.sin(theta_arc)

        # Source position (at top of circle)
        x_source_cart = r_source * np.cos(np.pi / 2)
        y_source_cart = r_source * np.sin(np.pi / 2)

        # Intersection position (to the right, so subtract angle)
        x_intersect_cart = r_earth * np.cos(np.pi / 2 - central_angle)
        y_intersect_cart = r_earth * np.sin(np.pi / 2 - central_angle)

        # Transform to depth coordinates with horizontal distance
        # Use the x-coordinate as horizontal distance, transform y to depth below surface
        # Surface point at source location has x=0, y=r_earth

        # Translate so source is at origin horizontally
        x_surface = x_surface_cart - x_source_cart
        x_source = 0
        x_intersect = x_intersect_cart - x_source_cart

        # Transform y-coordinates to depth (surface at top is depth 0)
        # The maximum y is at the source location (y_source_cart = r_source)
        # Surface at source is at y = r_earth
        depth_surface = (
            r_earth - y_surface_cart
        )  # Depth of surface points below the top
        depth_source = r_earth - y_source_cart  # Source depth
        depth_intersect = r_earth - y_intersect_cart  # Intersection depth (should be 0)

        # For the acoustic ray (straight chord), it's a straight line in x-y Cartesian space
        num_chord_points = 200
        t = np.linspace(0, 1, num_chord_points)
        chord_x_cart = x_source_cart + t * (x_intersect_cart - x_source_cart)
        chord_y_cart = y_source_cart + t * (y_intersect_cart - y_source_cart)

        # Transform chord to depth coordinates
        chord_x = chord_x_cart - x_source_cart
        chord_depth = r_earth - chord_y_cart

        # Fill the water column between surface and ray
        ax.fill_between(
            x_surface / 1000,
            depth_surface,
            np.interp(x_surface, chord_x, chord_depth),
            where=(depth_surface <= np.interp(x_surface, chord_x, chord_depth)),
            color="cyan",
            alpha=0.2,
            label="Water column",
            zorder=1,
        )

        # Plot the curved Earth surface (blue solid)
        ax.plot(
            x_surface / 1000,
            depth_surface,
            "b-",
            linewidth=2,
            label="Ocean surface (curved)",
            zorder=2,
        )

        # Plot the straight acoustic ray path (red dashed - horizontal line)
        ax.plot(
            chord_x / 1000,
            chord_depth,
            "r--",
            linewidth=2.5,
            label="Acoustic ray (straight)",
            zorder=3,
        )

        # Mark source (green) and intersection (red)
        ax.plot(
            x_source / 1000,
            depth_source,
            "go",
            markersize=10,
            label=f"Source ({source_depth:.0f} m depth)",
            zorder=5,
        )
        ax.plot(
            x_intersect / 1000,
            depth_intersect,
            "ro",
            markersize=10,
            label="Surface intersection",
            zorder=5,
        )

        # Add annotations
        ax.text(
            x_intersect / 2000,
            depth_surface.max() * 0.2,
            f"Surface curves down\n{range_m/1000:.2f} km arc",
            fontsize=9,
            ha="center",
            bbox=dict(boxstyle="round,pad=0.3", facecolor="lightblue", alpha=0.8),
        )

        # Calculate chord length
        chord_length = np.sqrt(
            (x_intersect_cart - x_source_cart) ** 2
            + (y_intersect_cart - y_source_cart) ** 2
        )
        ax.text(
            x_intersect / 2000,
            depth_source * 0.5,
            f"Ray stays straight\n{chord_length/1000:.2f} km chord",
            fontsize=9,
            ha="center",
            bbox=dict(boxstyle="round,pad=0.3", facecolor="lightcoral", alpha=0.8),
        )

        # Set axis limits
        ax.set_xlim(x_surface.min() / 1000 * 1.1, x_surface.max() / 1000 * 1.1)
        ax.set_ylim(-source_depth * 0.2, depth_surface.max() * 1.1)

    # Invert y-axis so depth increases downward
    ax.invert_yaxis()
    ax.grid(True, alpha=0.3)
    ax.set_xlabel("Horizontal Distance (km)", fontsize=12)
    ax.set_ylabel("Depth Below Surface (m)", fontsize=12)
    ax.set_title(
        "Physical Geometry: Straight Ray vs Curved Earth", fontsize=14, weight="bold"
    )
    ax.legend(loc="best", fontsize=9)

    # Third plot: Depth profile along ray path
    ax = ax3

    # Calculate depth profile
    horiz_ranges, depths, max_depth, max_depth_range = calculate_ray_depth_profile(
        source_depth, beam_angle, r_source, range_m, central_angle, r_earth
    )

    # Plot the FLAT ocean surface (blue solid line at depth=0)
    ax.axhline(
        y=0, color="b", linewidth=2, label="Ocean surface (flat reference)", zorder=2
    )

    # Plot the CURVED acoustic ray path (red dashed)
    ax.plot(
        horiz_ranges / 1000,
        depths,
        "r--",
        linewidth=2.5,
        label="Acoustic ray path",
        zorder=3,
    )

    # Mark max depth point
    ax.plot(
        max_depth_range / 1000,
        max_depth,
        "mo",
        markersize=8,
        label=f"Max depth: {max_depth:.1f} m at {max_depth_range/1000:.1f} km",
        zorder=4,
    )

    # Mark source and intersection points (swapped colors: green=source, red=intersection)
    ax.plot(
        0,
        source_depth,
        "go",
        markersize=10,
        label=f"Source: {source_depth:.0f} m",
        zorder=5,
    )
    ax.plot(
        range_m / 1000, 0, "ro", markersize=10, label="Surface intersection", zorder=5
    )

    # Fill the area between surface and ray (water column)
    ax.fill_between(horiz_ranges / 1000, 0, depths, alpha=0.2, color="cyan", zorder=1)

    # Invert y-axis so depth increases downward
    ax.invert_yaxis()
    ax.grid(True, alpha=0.3)
    ax.set_xlabel("Horizontal Range (km)", fontsize=12)
    ax.set_ylabel("Depth Below Surface (m)", fontsize=12)
    ax.set_title(
        "Depth Profile: Ray Curves Up to Meet Flat Surface", fontsize=14, weight="bold"
    )
    ax.legend(loc="best", fontsize=9)

    # Add statistics text box
    stats_text = f"Source depth: {source_depth:.1f} m\n"
    stats_text += f"Max depth: {max_depth:.1f} m\n"
    stats_text += f"Depth increase: {max_depth - source_depth:.1f} m"
    ax.text(
        0.98,
        0.98,
        stats_text,
        transform=ax.transAxes,
        verticalalignment="top",
        horizontalalignment="right",
        bbox=dict(boxstyle="round", facecolor="wheat", alpha=0.5),
        fontsize=10,
        family="monospace",
    )

    plt.tight_layout()
    plt.savefig("spherical_earth_acoustic.png", dpi=150, bbox_inches="tight")
    print(f"\nPlot saved as: spherical_earth_acoustic.png")

    # Show the plot
    plt.show(block=False)
    plt.pause(0.1)  # Give the window time to render
    print("Plot window opened")

    return max_depth, max_depth_range


def main():
    """Main program execution."""

    # Set up argument parser
    parser = argparse.ArgumentParser(
        description="Calculate acoustic ray intersection with spherical Earth surface",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )

    parser.add_argument(
        "depth", type=float, nargs="?", help="Source depth below surface (meters)"
    )
    parser.add_argument(
        "angle",
        type=float,
        nargs="?",
        help="Beam angle from horizontal (degrees, 0=horizontal, positive=downward)",
    )
    parser.add_argument(
        "--earth-radius",
        type=float,
        default=DEFAULT_EARTH_RADIUS,
        help=f"Earth radius in meters (default: {DEFAULT_EARTH_RADIUS:.1f} m = 6371 km)",
    )

    args = parser.parse_args()

    print()
    print("Spherical Earth Acoustic Ray Intersection Calculator")
    print("====================================================")
    print()
    print("This program calculates where an acoustic beam intersects")
    print("the Earth surface accounting for spherical curvature.")
    print()

    # Get Earth radius from command line
    earth_radius = args.earth_radius

    # Get source depth from command line or prompt
    if args.depth is not None:
        source_depth = args.depth
    else:
        source_depth = float(input("Enter source depth below surface (meters): "))

    # Get beam angle from command line or prompt
    if args.angle is not None:
        beam_angle = args.angle
    else:
        beam_angle = float(
            input(
                "Enter beam angle from horizontal in degrees\n"
                "(0 = horizontal, positive = downward): "
            )
        )

    # Validate inputs
    if source_depth < 0.0:
        print("Error: Source depth must be positive (below surface)")
        sys.exit(1)

    if source_depth >= earth_radius:
        print("Error: Source depth exceeds Earth radius")
        sys.exit(1)

    if abs(beam_angle) > 90.0:
        print("Error: Beam angle must be between -90 and +90 degrees")
        sys.exit(1)

    # Convert angle to radians
    beam_angle_rad = beam_angle * DEG_TO_RAD

    # Calculate source position (distance from Earth center)
    r_source = earth_radius - source_depth

    print()
    print("Input Parameters:")
    print("=================")
    print(f"Earth radius:        {earth_radius/1000:12.1f} km")
    print(f"Source depth:        {source_depth:12.3f} m")
    print(f"Source radius:       {r_source/1000:12.3f} km")
    print(f"Beam angle:          {beam_angle:8.3f} degrees")
    print()

    # Calculate intersection
    range_m, central_angle = calculate_surface_intersection(
        r_source, beam_angle_rad, earth_radius
    )

    # Display results
    print("Results:")
    print("========")

    if range_m < 0:
        print("No surface intersection - beam directed downward")
        print("or insufficient upward angle to reach surface")
    else:
        range_km = range_m / 1000.0
        print(f"Horizontal range:     {range_km:12.3f} km")
        print(f"Horizontal range:     {range_m:12.1f} m")
        print(f"Central angle:        {central_angle*RAD_TO_DEG:8.3f} degrees")
        print(f"Central angle:        {central_angle:8.5f} radians")

        # Context
        print()
        print("Context:")
        print("========")
        if range_km < 1.0:
            print("Very short range - local/near-field effects")
        elif range_km < 10.0:
            print("Short range - harbor/coastal scale")
        elif range_km < 100.0:
            print("Medium range - regional oceanographic scale")
        elif range_km < 1000.0:
            print("Long range - basin scale")
        else:
            print("Very long range - global/trans-oceanic scale")

        # Flat-earth comparison
        if abs(beam_angle) > 1e-6:
            flat_earth_range = source_depth / np.tan(beam_angle_rad)
            if flat_earth_range > 0.0:
                print()
                print(f"Flat Earth range:     {flat_earth_range/1000:12.3f} km")
                print(
                    f"Curvature effect:     {(range_km - flat_earth_range/1000):8.2f} km difference"
                )

    print()

    # Plot geometry
    if range_m > 0:
        try:
            max_depth, max_depth_range = plot_geometry(
                source_depth, beam_angle, r_source, range_m, central_angle, earth_radius
            )

            # Display depth profile information
            print()
            print("Ray Depth Profile:")
            print("==================")
            print(f"Source depth:         {source_depth:12.1f} m")
            print(f"Maximum depth:        {max_depth:12.1f} m")
            print(f"Depth increase:       {max_depth - source_depth:12.1f} m")
            print(f"Max depth location:   {max_depth_range/1000:12.3f} km from source")
            print()

            # Keep the plot window open until user presses Enter
            try:
                input(
                    "Press Enter to close the plot and exit (or close the plot window manually)..."
                )
            except (KeyboardInterrupt, EOFError):
                print("\nExiting...")
            finally:
                plt.close("all")

        except Exception as e:
            print(f"Warning: Could not create plot: {e}")
            print("(matplotlib may not be available)")


if __name__ == "__main__":
    main()
