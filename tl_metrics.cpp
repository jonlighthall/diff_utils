#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <stdexcept>

struct RangeTLPair {
    double range;
    double tl;
};

class TLMetrics {
private:
    std::vector<double> ranges;
    std::vector<double> tl1;
    std::vector<double> tl2;
    double fom;
    bool use_fom;
    
    // Weighting parameters (can be modified if needed)
    const double TL_MIN = 60.0;
    const double TL_MAX = 110.0;
    
    // Calculate weight for a given TL value
    double calculateWeight(double tl) const {
        if (tl <= TL_MIN) return 1.0;
        if (tl >= TL_MAX) return 0.0;
        return (TL_MAX - tl) / (TL_MAX - TL_MIN);
    }
    
    // Convert dB to intensity
    double dbToIntensity(double db) const {
        return std::pow(10.0, -db / 10.0);
    }
    
    // Convert intensity to dB
    double intensityToDb(double intensity) const {
        return -10.0 * std::log10(intensity);
    }
    
    // Score mapping function (Figure 1 in paper)
    double scoreFromDiff(double diff) const {
        if (diff <= 3.0) {
            // High score for differences 0-3 dB (90s range)
            return 100.0 - (diff / 3.0) * 10.0;
        } else if (diff < 20.0) {
            // Linear decrease from ~90 to 0 for 3-20 dB
            return std::max(0.0, 90.0 - ((diff - 3.0) / 17.0) * 90.0);
        } else {
            return 0.0;
        }
    }
    
    // Interpolate TL curves to common range grid
    void interpolateToCommonGrid(const std::vector<RangeTLPair>& curve1,
                                  const std::vector<RangeTLPair>& curve2) {
        // Find common max range
        double maxRange1 = curve1.back().range;
        double maxRange2 = curve2.back().range;
        double maxRange = std::min(maxRange1, maxRange2);
        
        // Create common range grid
        int numPoints = std::max(curve1.size(), curve2.size());
        ranges.clear();
        tl1.clear();
        tl2.clear();
        
        for (int i = 0; i < numPoints; ++i) {
            double r = (maxRange * i) / (numPoints - 1);
            ranges.push_back(r);
        }
        
        // Interpolate both curves to intensity space, then back to dB
        for (double r : ranges) {
            // Interpolate curve 1
            double intensity1 = interpolateIntensity(curve1, r);
            tl1.push_back(intensityToDb(intensity1));
            
            // Interpolate curve 2
            double intensity2 = interpolateIntensity(curve2, r);
            tl2.push_back(intensityToDb(intensity2));
        }
    }
    
    double interpolateIntensity(const std::vector<RangeTLPair>& curve, double r) const {
        if (r <= curve.front().range) {
            return dbToIntensity(curve.front().tl);
        }
        if (r >= curve.back().range) {
            return dbToIntensity(curve.back().tl);
        }
        
        // Find bracketing points
        for (size_t i = 0; i < curve.size() - 1; ++i) {
            if (r >= curve[i].range && r <= curve[i+1].range) {
                double t = (r - curve[i].range) / (curve[i+1].range - curve[i].range);
                double int1 = dbToIntensity(curve[i].tl);
                double int2 = dbToIntensity(curve[i+1].tl);
                return int1 + t * (int2 - int1);
            }
        }
        
        return dbToIntensity(curve.back().tl);
    }
    
public:
    TLMetrics(const std::vector<RangeTLPair>& curve1,
              const std::vector<RangeTLPair>& curve2,
              double figure_of_merit = 0.0) 
        : fom(figure_of_merit), use_fom(figure_of_merit > 0.0) {
        
        if (curve1.empty() || curve2.empty()) {
            throw std::invalid_argument("Input curves cannot be empty");
        }
        
        interpolateToCommonGrid(curve1, curve2);
    }
    
    // Component 1: Weighted difference over all ranges (Equation 1)
    double calculateTLDiff1() const {
        double sumWeightedDiff = 0.0;
        double sumWeights = 0.0;
        
        for (size_t i = 0; i < tl1.size(); ++i) {
            double diff = std::abs(tl1[i] - tl2[i]);
            double weight = calculateWeight(tl1[i]);
            sumWeightedDiff += diff * weight;
            sumWeights += weight;
        }
        
        if (sumWeights < 1e-10) {
            return 0.0;
        }
        
        return sumWeightedDiff / sumWeights;
    }
    
    // Component 2: Mean difference in last 4% of range (Equation 2)
    double calculateTLDiff2() const {
        double maxRange = ranges.back();
        double rangeThreshold = maxRange * 0.96;  // Last 4%
        
        double sumDiff = 0.0;
        int count = 0;
        
        for (size_t i = 0; i < ranges.size(); ++i) {
            if (ranges[i] >= rangeThreshold) {
                sumDiff += std::abs(tl1[i] - tl2[i]);
                count++;
            }
        }
        
        if (count == 0) return 0.0;
        return sumDiff / count;
    }
    
    // Component 3: Correlation coefficient
    double calculateCorrelation() const {
        if (tl1.size() < 2) return 0.0;
        
        double mean1 = std::accumulate(tl1.begin(), tl1.end(), 0.0) / tl1.size();
        double mean2 = std::accumulate(tl2.begin(), tl2.end(), 0.0) / tl2.size();
        
        double numerator = 0.0;
        double denom1 = 0.0;
        double denom2 = 0.0;
        
        for (size_t i = 0; i < tl1.size(); ++i) {
            double diff1 = tl1[i] - mean1;
            double diff2 = tl2[i] - mean2;
            numerator += diff1 * diff2;
            denom1 += diff1 * diff1;
            denom2 += diff2 * diff2;
        }
        
        if (denom1 < 1e-10 || denom2 < 1e-10) return 0.0;
        
        return numerator / std::sqrt(denom1 * denom2);
    }
    
    // Component 4: Range coverage energy difference (requires FOM)
    double calculateRangeCoverage() const {
        if (!use_fom) return 0.0;
        
        double maxRange = ranges.back();
        double coverage1 = 0.0;
        double coverage2 = 0.0;
        
        for (size_t i = 0; i < ranges.size(); ++i) {
            double se1 = fom - tl1[i];  // Signal excess
            double se2 = fom - tl2[i];
            
            if (se1 > 0) coverage1 += se1;
            if (se2 > 0) coverage2 += se2;
        }
        
        // Convert to percentage by normalizing by max range
        double pctCoverage1 = (coverage1 / maxRange) * 100.0;
        double pctCoverage2 = (coverage2 / maxRange) * 100.0;
        
        return std::abs(pctCoverage1 - pctCoverage2);
    }
    
    // Component 5: Near-continuous detection range (requires FOM)
    double calculateDetectionRange() const {
        if (!use_fom) return 0.0;
        
        auto findDetectionRange = [this](const std::vector<double>& tl_curve) -> double {
            double detRange = 0.0;
            int consecutiveDips = 0;
            const int maxDips = 1;  // Allow 1 range step dip
            
            for (size_t i = 0; i < ranges.size(); ++i) {
                double se = fom - tl_curve[i];
                
                if (se > 0) {
                    detRange = ranges[i];
                    consecutiveDips = 0;
                } else {
                    consecutiveDips++;
                    if (consecutiveDips > maxDips) {
                        break;
                    }
                }
            }
            return detRange;
        };
        
        double detRange1 = findDetectionRange(tl1);
        double detRange2 = findDetectionRange(tl2);
        double maxRange = ranges.back();
        
        double pctRange1 = (detRange1 / maxRange) * 100.0;
        double pctRange2 = (detRange2 / maxRange) * 100.0;
        
        return std::abs(pctRange1 - pctRange2);
    }
    
    // Calculate metric values (scored 0-100) for each component
    double getMetric1() const {
        return scoreFromDiff(calculateTLDiff1());
    }
    
    double getMetric2() const {
        return scoreFromDiff(calculateTLDiff2());
    }
    
    double getMetric3() const {
        double corr = calculateCorrelation();
        // Normalize: negative correlations score 0, positive scale 0-100
        return std::max(0.0, corr * 100.0);
    }
    
    double getMetric4() const {
        if (!use_fom) return 0.0;
        double covDiff = calculateRangeCoverage();
        // Score coverage difference (smaller is better)
        return std::max(0.0, 100.0 - covDiff);
    }
    
    double getMetric5() const {
        if (!use_fom) return 0.0;
        double drDiff = calculateDetectionRange();
        // Score detection range difference (smaller is better)
        return std::max(0.0, 100.0 - drDiff);
    }
    
    // Calculate M_curve (Equation 3) - average of first 3 metrics
    double getMCurve() const {
        return (getMetric1() + getMetric2() + getMetric3()) / 3.0;
    }
    
    // Calculate M_total (Equation 4) - average of all 5 metrics
    double getMTotal() const {
        if (!use_fom) {
            return getMCurve();  // Fall back to M_curve if no FOM
        }
        return (getMetric1() + getMetric2() + getMetric3() + 
                getMetric4() + getMetric5()) / 5.0;
    }
    
    // Print detailed results
    void printResults() const {
        std::cout << "\n===== TL Curve Comparison Metrics =====\n";
        std::cout << "Number of range points: " << ranges.size() << "\n";
        std::cout << "Range: " << ranges.front() << " to " << ranges.back() << "\n";
        if (use_fom) {
            std::cout << "Figure of Merit (FOM): " << fom << " dB\n";
        }
        std::cout << "\n----- Component Values -----\n";
        std::cout << "TL_diff_1 (weighted diff):    " << calculateTLDiff1() << " dB\n";
        std::cout << "TL_diff_2 (last 4% diff):     " << calculateTLDiff2() << " dB\n";
        std::cout << "Correlation coefficient:      " << calculateCorrelation() << "\n";
        if (use_fom) {
            std::cout << "Range coverage diff:          " << calculateRangeCoverage() << " %\n";
            std::cout << "Detection range diff:         " << calculateDetectionRange() << " %\n";
        }
        std::cout << "\n----- Metric Scores (0-100) -----\n";
        std::cout << "M1 (weighted diff):           " << getMetric1() << "\n";
        std::cout << "M2 (last 4% diff):            " << getMetric2() << "\n";
        std::cout << "M3 (correlation):             " << getMetric3() << "\n";
        if (use_fom) {
            std::cout << "M4 (range coverage):          " << getMetric4() << "\n";
            std::cout << "M5 (detection range):         " << getMetric5() << "\n";
        }
        std::cout << "\n----- Final Metrics -----\n";
        std::cout << "M_curve (avg of M1-M3):       " << getMCurve() << "\n";
        if (use_fom) {
            std::cout << "M_total (avg of M1-M5):       " << getMTotal() << "\n";
        }
        std::cout << "======================================\n\n";
    }
};

// Example usage
int main() {
    // Example data - replace with your actual TL curves
    std::vector<RangeTLPair> curve1 = {
        {0.0, 50.0}, {1.0, 55.0}, {2.0, 60.0}, {3.0, 65.0}, 
        {4.0, 70.0}, {5.0, 75.0}, {6.0, 80.0}, {7.0, 85.0},
        {8.0, 90.0}, {9.0, 95.0}, {10.0, 100.0}
    };
    
    std::vector<RangeTLPair> curve2 = {
        {0.0, 52.0}, {1.0, 57.0}, {2.0, 62.0}, {3.0, 67.0},
        {4.0, 71.0}, {5.0, 76.0}, {6.0, 82.0}, {7.0, 87.0},
        {8.0, 92.0}, {9.0, 97.0}, {10.0, 102.0}
    };
    
    // Without FOM (M_curve only)
    std::cout << "===== Example 1: Without FOM =====\n";
    TLMetrics metrics1(curve1, curve2);
    metrics1.printResults();
    
    // With FOM (M_total)
    std::cout << "\n===== Example 2: With FOM = 75 dB =====\n";
    TLMetrics metrics2(curve1, curve2, 75.0);
    metrics2.printResults();
    
    return 0;
}
