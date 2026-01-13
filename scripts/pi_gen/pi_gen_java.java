
/**
 * Pi Precision Test - Java Version
 *
 * Tests different rounding behaviors across languages.
 * Java uses HALF_UP rounding mode by default in String.format,
 * which is "round half away from zero" (traditional rounding).
 */

import java.io.FileWriter;
import java.io.IOException;
import java.io.PrintWriter;

public class pi_gen_java {

    /**
     * Write value with varying decimal precision to file
     * Includes an index column to mimic range/time structure
     *
     * @param filename output file name
     * @param value    the number to write (e.g., pi, sqrt(2), etc.)
     * @param startDp  starting decimal places
     * @param endDp    ending decimal places
     * @param step     increment (+1 for ascending, -1 for descending)
     */
    public static void writePrecisionFile(String filename, double value,
            int startDp, int endDp, int step) {
        try (PrintWriter writer = new PrintWriter(new FileWriter(filename))) {
            // Loop over decimal places and include index column (mimics range/time)
            int lineNo = 1;
            if (step > 0) {
                for (int i = startDp; i <= endDp; i += step) {
                    writer.print(lineNo + "  ");
                    if (i == 0) {
                        writer.println((int) value);
                    } else {
                        writer.println(String.format("%." + i + "f", value));
                    }
                    lineNo++;
                }
            } else {
                for (int i = startDp; i >= endDp; i += step) {
                    writer.print(lineNo + "  ");
                    if (i == 0) {
                        writer.println((int) value);
                    } else {
                        writer.println(String.format("%." + i + "f", value));
                    }
                    lineNo++;
                }
            }
        } catch (IOException e) {
            System.err.println("Error: Could not write to file " + filename);
            e.printStackTrace();
        }
    }

    public static void main(String[] args) {
        // Calculate pi
        double pi = 4.0 * Math.atan(1.0);

        // Get machine epsilon (approximate for double)
        double epsilon = Math.ulp(1.0);

        // Calculate max decimal places (conservative)
        int maxDecimalPlaces = (int) (-Math.log10(epsilon)) - 1;
        if (maxDecimalPlaces > 15) {
            maxDecimalPlaces = 15;
        }

        // Program identifier (standardized)
        String prog = "pi_gen_java";
        
        // Use standardized filenames so scripts can find them reliably
        // Format: pi_<language>_asc.txt / pi_<language>_desc.txt
        String ascName = "pi_java_asc.txt";
        String descName = "pi_java_desc.txt";

        // Generate ascending precision file (0 to max decimal places)
        writePrecisionFile(ascName, pi, 0, maxDecimalPlaces, 1);

        // Generate descending precision file (max to 0 decimal places)
        writePrecisionFile(descName, pi, maxDecimalPlaces, 0, -1);

        // Print summary to console
        System.out.println("Using program id: " + prog);
        System.out.println("Pi Precision Test Program (Java)");
        System.out.println("=================================");
        System.out.printf("Calculated pi:           %.15f%n", pi);
        System.out.printf("Machine epsilon:         %.5e%n", epsilon);
        System.out.printf("Max valid decimal places: %d%n", maxDecimalPlaces);
        System.out.println();
        System.out.println("Ascending file:  " + ascName);
        System.out.println("Descending file: " + descName);
        System.out.println("Each contains " + (maxDecimalPlaces + 1) + " lines");
        System.out.println();
        System.out.println("Rounding mode: HALF_UP (round half away from zero)");
    }
}

