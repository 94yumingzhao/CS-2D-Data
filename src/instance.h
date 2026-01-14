// ============================================================================
// 工程标准 (Engineering Standards)
// - 坐标系: 左下角为原点
// - 宽度(Width): 上下方向 (Y轴)
// - 长度(Length): 左右方向 (X轴)
// - 约束: 长度 >= 宽度
// ============================================================================

// instance.h - 2D Cutting Stock Problem Instance Data Structures
// Project: CS-2D-Data
// Purpose: Define Instance and Item structures with statistics methods

#ifndef CS_2D_DATA_INSTANCE_H_
#define CS_2D_DATA_INSTANCE_H_

#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <set>

// Item type definition
struct Item {
    int id;         // Item type ID (0-indexed)
    int width;      // Width (Stage 1 cutting direction)
    int length;     // Length (Stage 2 cutting direction)
    int demand;     // Demand quantity

    // Calculate item area
    int Area() const { return width * length; }

    // Calculate aspect ratio (length / width)
    double AspectRatio() const {
        return static_cast<double>(length) / width;
    }
};

// Instance definition with statistics methods
struct Instance {
    int stock_width;              // Stock width W
    int stock_length;             // Stock length L
    std::vector<Item> items;      // List of item types
    int known_optimal;            // Known optimal solution (-1 if unknown)
    double difficulty;            // Difficulty parameter used for generation

    // Constructor
    Instance() : stock_width(0), stock_length(0), known_optimal(-1), difficulty(0.0) {}

    // Stock area
    int StockArea() const { return stock_width * stock_length; }

    // Number of item types
    int NumTypes() const { return static_cast<int>(items.size()); }

    // Total demand (sum of all d_i)
    int TotalDemand() const {
        int total = 0;
        for (const auto& item : items) {
            total += item.demand;
        }
        return total;
    }

    // Total demand area (sum of w_i * l_i * d_i)
    long long TotalDemandArea() const {
        long long total = 0;
        for (const auto& item : items) {
            total += static_cast<long long>(item.Area()) * item.demand;
        }
        return total;
    }

    // Theoretical lower bound on plates needed
    double TheoreticalLowerBound() const {
        if (StockArea() == 0) return 0.0;
        return static_cast<double>(TotalDemandArea()) / StockArea();
    }

    // Average item area
    double AvgItemArea() const {
        if (items.empty()) return 0.0;
        double sum = 0.0;
        for (const auto& item : items) {
            sum += item.Area();
        }
        return sum / items.size();
    }

    // Average size ratio (item_area / stock_area)
    double AvgSizeRatio() const {
        if (StockArea() == 0) return 0.0;
        return AvgItemArea() / StockArea();
    }

    // Average width ratio
    double AvgWidthRatio() const {
        if (items.empty() || stock_width == 0) return 0.0;
        double sum = 0.0;
        for (const auto& item : items) {
            sum += static_cast<double>(item.width) / stock_width;
        }
        return sum / items.size();
    }

    // Average length ratio
    double AvgLengthRatio() const {
        if (items.empty() || stock_length == 0) return 0.0;
        double sum = 0.0;
        for (const auto& item : items) {
            sum += static_cast<double>(item.length) / stock_length;
        }
        return sum / items.size();
    }

    // Average demand per item type
    double AvgDemand() const {
        if (items.empty()) return 0.0;
        return static_cast<double>(TotalDemand()) / items.size();
    }

    // Demand variance
    double DemandVariance() const {
        if (items.size() < 2) return 0.0;
        double avg = AvgDemand();
        double sum_sq = 0.0;
        for (const auto& item : items) {
            double diff = item.demand - avg;
            sum_sq += diff * diff;
        }
        return sum_sq / (items.size() - 1);
    }

    // Demand coefficient of variation (std / mean)
    double DemandCV() const {
        double avg = AvgDemand();
        if (avg < 1e-9) return 0.0;
        return std::sqrt(DemandVariance()) / avg;
    }

    // Size coefficient of variation (combined width and length)
    double SizeCV() const {
        if (items.size() < 2) return 0.0;

        // Calculate mean width and length
        double mean_w = 0.0, mean_l = 0.0;
        for (const auto& item : items) {
            mean_w += item.width;
            mean_l += item.length;
        }
        mean_w /= items.size();
        mean_l /= items.size();

        // Calculate variance
        double var_w = 0.0, var_l = 0.0;
        for (const auto& item : items) {
            var_w += (item.width - mean_w) * (item.width - mean_w);
            var_l += (item.length - mean_l) * (item.length - mean_l);
        }
        var_w /= (items.size() - 1);
        var_l /= (items.size() - 1);

        // Combined CV
        double cv_w = (mean_w > 0) ? std::sqrt(var_w) / mean_w : 0.0;
        double cv_l = (mean_l > 0) ? std::sqrt(var_l) / mean_l : 0.0;

        return std::sqrt((cv_w * cv_w + cv_l * cv_l) / 2.0);
    }

    // Number of unique widths (equals number of strip types)
    int NumUniqueWidths() const {
        std::set<int> widths;
        for (const auto& item : items) {
            widths.insert(item.width);
        }
        return static_cast<int>(widths.size());
    }

    // Width diversity ratio (unique_widths / num_types)
    double WidthDiversity() const {
        if (items.empty()) return 0.0;
        return static_cast<double>(NumUniqueWidths()) / items.size();
    }

    // Min/Max size ratios
    double MinSizeRatio() const {
        if (items.empty() || StockArea() == 0) return 0.0;
        int min_area = items[0].Area();
        for (const auto& item : items) {
            min_area = std::min(min_area, item.Area());
        }
        return static_cast<double>(min_area) / StockArea();
    }

    double MaxSizeRatio() const {
        if (items.empty() || StockArea() == 0) return 0.0;
        int max_area = items[0].Area();
        for (const auto& item : items) {
            max_area = std::max(max_area, item.Area());
        }
        return static_cast<double>(max_area) / StockArea();
    }

    // Validate instance (all items fit in stock)
    bool IsValid() const {
        for (const auto& item : items) {
            if (item.width > stock_width || item.length > stock_length) {
                return false;
            }
            if (item.width <= 0 || item.length <= 0 || item.demand <= 0) {
                return false;
            }
        }
        return stock_width > 0 && stock_length > 0 && !items.empty();
    }

    // Print statistics summary
    std::string GetStatsSummary() const {
        char buf[1024];
        snprintf(buf, sizeof(buf),
            "Instance Statistics:\n"
            "  Stock: %d x %d (area=%d)\n"
            "  Item types: %d\n"
            "  Total demand: %d items\n"
            "  Total demand area: %lld\n"
            "  Theoretical LB: %.2f plates\n"
            "  Avg size ratio: %.2f%%\n"
            "  Size CV: %.3f\n"
            "  Avg demand: %.1f\n"
            "  Demand CV: %.3f\n"
            "  Unique widths: %d (diversity=%.2f)\n"
            "  Known optimal: %d\n",
            stock_width, stock_length, StockArea(),
            NumTypes(),
            TotalDemand(),
            TotalDemandArea(),
            TheoreticalLowerBound(),
            AvgSizeRatio() * 100,
            SizeCV(),
            AvgDemand(),
            DemandCV(),
            NumUniqueWidths(), WidthDiversity(),
            known_optimal);
        return std::string(buf);
    }
};

#endif  // CS_2D_DATA_INSTANCE_H_
