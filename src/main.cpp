// main.cpp - 2D Cutting Stock Problem Instance Generator
// Project: CS-2D-Data
// 支持三种模式: 兼容模式(-d), 预设模式(--preset), 手动模式(--manual)

#include "generator.h"
#include "difficulty_estimator.h"
#include <iostream>
#include <string>
#include <cstring>
#include <iomanip>
#include <chrono>
#include <thread>

void PrintUsage(const char* program) {
    std::cout << "2D Cutting Stock Problem Instance Generator\n";
    std::cout << "Version 2.0 - Decoupled Parameters\n\n";
    std::cout << "Usage: " << program << " [mode] [options]\n\n";

    std::cout << "Modes:\n";
    std::cout << "  (default)         Legacy mode: single difficulty parameter\n";
    std::cout << "  --preset <level>  Preset mode: easy/medium/hard/expert\n";
    std::cout << "  --manual          Manual mode: independent parameters\n\n";

    std::cout << "Legacy Mode Options:\n";
    std::cout << "  -d, --difficulty <0.0-1.0>  Difficulty level (default: 0.5)\n\n";

    std::cout << "Manual Mode Options:\n";
    std::cout << "  --num-types <N>             Item types (5-100, default: 20)\n";
    std::cout << "  --min-size-ratio <R>        Min item/stock area ratio (default: 0.08)\n";
    std::cout << "  --max-size-ratio <R>        Max item/stock area ratio (default: 0.35)\n";
    std::cout << "  --size-cv <V>               Size coefficient of variation (default: 0.30)\n";
    std::cout << "  --min-demand <D>            Min demand per type (default: 1)\n";
    std::cout << "  --max-demand <D>            Max demand per type (default: 15)\n";
    std::cout << "  --demand-skew <S>           Demand skewness 0-1 (default: 0.0)\n";
    std::cout << "  --prime-offset              Enable prime offset (harder)\n";
    std::cout << "  --strategy <0-3>            0=reverse, 1=random, 2=cluster, 3=residual\n\n";

    std::cout << "Common Options:\n";
    std::cout << "  -n, --count <N>             Number of instances (default: 1)\n";
    std::cout << "  -W, --width <W>             Stock width (default: 200)\n";
    std::cout << "  -L, --length <L>            Stock length (default: 400)\n";
    std::cout << "  -o, --output <dir>          Output directory (default: data)\n";
    std::cout << "  -s, --seed <seed>           Random seed (default: 0 = timestamp)\n";
    std::cout << "  -h, --help                  Show this help\n\n";

    std::cout << "Presets:\n";
    std::cout << "  easy    - 8 types, small items, high demand, known optimal\n";
    std::cout << "  medium  - 20 types, moderate items, moderate demand\n";
    std::cout << "  hard    - 35 types, large items, low demand, prime offset\n";
    std::cout << "  expert  - 50 types, very large items, minimal demand\n\n";

    std::cout << "Strategies:\n";
    std::cout << "  0 = Reverse   - Construct perfect packing, known optimal\n";
    std::cout << "  1 = Random    - Parameterized random generation\n";
    std::cout << "  2 = Cluster   - Size clustering (realistic scenarios)\n";
    std::cout << "  3 = Residual  - Hard-to-pack instances\n\n";

    std::cout << "Examples:\n";
    std::cout << "  " << program << " -d 0.5 -n 10                    # Legacy mode\n";
    std::cout << "  " << program << " --preset hard -n 5              # Preset mode\n";
    std::cout << "  " << program << " --manual --num-types 30 --prime-offset\n";
}

void PrintEstimate(const DifficultyEstimate& est) {
    std::cout << "\nDifficulty Estimate:\n";
    std::cout << "  Score: " << std::fixed << std::setprecision(2) << est.score << "\n";
    std::cout << "  Level: " << est.level_name << "\n";
    std::cout << "  Estimated Gap: " << est.estimated_gap << "\n";
    std::cout << "  Estimated Nodes: " << est.estimated_nodes << "\n";
    std::cout << "  Utilization LB: " << std::fixed << std::setprecision(1)
              << (est.utilization_lb * 100) << "%\n";

    std::cout << "\n  Factor Contributions:\n";
    std::cout << "    Size ratio:    " << std::fixed << std::setprecision(3)
              << est.size_contribution << "\n";
    std::cout << "    Types:         " << est.types_contribution << "\n";
    std::cout << "    Demand:        " << est.demand_contribution << "\n";
    std::cout << "    CV:            " << est.cv_contribution << "\n";
    std::cout << "    Width div:     " << est.width_div_contribution << "\n";
}

void PrintInstanceInfo(const Instance& inst, const DifficultyEstimate& est) {
    std::cout << "\nInstance Summary:\n";
    std::cout << "  Stock: " << inst.stock_width << " x " << inst.stock_length
              << " (area=" << inst.StockArea() << ")\n";
    std::cout << "  Item types: " << inst.NumTypes() << "\n";
    std::cout << "  Total demand: " << inst.TotalDemand() << "\n";
    std::cout << "  Total demand area: " << inst.TotalDemandArea() << "\n";
    std::cout << "  Theoretical LB: " << std::fixed << std::setprecision(2)
              << inst.TheoreticalLowerBound() << " plates\n";
    std::cout << "  Avg size ratio: " << std::fixed << std::setprecision(2)
              << (inst.AvgSizeRatio() * 100) << "%\n";
    std::cout << "  Size CV: " << std::fixed << std::setprecision(3) << inst.SizeCV() << "\n";
    std::cout << "  Avg demand: " << std::fixed << std::setprecision(1) << inst.AvgDemand() << "\n";
    std::cout << "  Unique widths: " << inst.NumUniqueWidths()
              << " (diversity=" << std::setprecision(2) << inst.WidthDiversity() << ")\n";

    if (inst.known_optimal > 0) {
        std::cout << "  Known optimal: " << inst.known_optimal << "\n";
    }

    PrintEstimate(est);
}

Preset ParsePreset(const std::string& str) {
    if (str == "easy") return Preset::kEasy;
    if (str == "medium") return Preset::kMedium;
    if (str == "hard") return Preset::kHard;
    if (str == "expert") return Preset::kExpert;
    return Preset::kMedium;  // default
}

int main(int argc, char* argv[]) {
    // 运行模式
    enum class Mode { kLegacy, kPreset, kManual };
    Mode mode = Mode::kLegacy;

    // 公共参数
    int count = 1;
    std::string output_dir = "data";
    int seed = 0;

    // Legacy模式参数
    double difficulty = 0.5;

    // Preset模式参数
    Preset preset = Preset::kMedium;

    // Manual模式参数
    GeneratorParams params;

    // 解析命令行
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            PrintUsage(argv[0]);
            return 0;
        }
        // 模式选择
        else if (arg == "--preset" && i + 1 < argc) {
            mode = Mode::kPreset;
            preset = ParsePreset(argv[++i]);
        }
        else if (arg == "--manual") {
            mode = Mode::kManual;
        }
        // Legacy模式参数
        else if ((arg == "-d" || arg == "--difficulty") && i + 1 < argc) {
            difficulty = std::stod(argv[++i]);
        }
        // Manual模式参数
        else if (arg == "--num-types" && i + 1 < argc) {
            params.num_types = std::stoi(argv[++i]);
        }
        else if (arg == "--min-size-ratio" && i + 1 < argc) {
            params.min_size_ratio = std::stod(argv[++i]);
        }
        else if (arg == "--max-size-ratio" && i + 1 < argc) {
            params.max_size_ratio = std::stod(argv[++i]);
        }
        else if (arg == "--size-cv" && i + 1 < argc) {
            params.size_cv = std::stod(argv[++i]);
        }
        else if (arg == "--min-demand" && i + 1 < argc) {
            params.min_demand = std::stoi(argv[++i]);
        }
        else if (arg == "--max-demand" && i + 1 < argc) {
            params.max_demand = std::stoi(argv[++i]);
        }
        else if (arg == "--demand-skew" && i + 1 < argc) {
            params.demand_skew = std::stod(argv[++i]);
        }
        else if (arg == "--prime-offset") {
            params.prime_offset = true;
        }
        else if (arg == "--strategy" && i + 1 < argc) {
            params.strategy = std::stoi(argv[++i]);
        }
        // 公共参数
        else if ((arg == "-n" || arg == "--count") && i + 1 < argc) {
            count = std::stoi(argv[++i]);
        }
        else if ((arg == "-W" || arg == "--width") && i + 1 < argc) {
            int w = std::stoi(argv[++i]);
            params.stock_width = w;
        }
        else if ((arg == "-L" || arg == "--length") && i + 1 < argc) {
            int l = std::stoi(argv[++i]);
            params.stock_length = l;
        }
        else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            output_dir = argv[++i];
        }
        else if ((arg == "-s" || arg == "--seed") && i + 1 < argc) {
            seed = std::stoi(argv[++i]);
        }
        else {
            std::cerr << "Unknown option: " << arg << "\n";
            PrintUsage(argv[0]);
            return 1;
        }
    }

    // 参数验证
    if (count < 1) {
        std::cerr << "Error: Count must be at least 1\n";
        return 1;
    }

    std::cout << "2D Cutting Stock Instance Generator v2.0\n";
    std::cout << "=========================================\n";

    // 创建生成器
    InstanceGenerator generator(seed);

    // 根据模式生成
    GenerationResult result;

    switch (mode) {
        case Mode::kLegacy: {
            std::cout << "Mode: Legacy (difficulty=" << difficulty << ")\n";
            if (count == 1) {
                result = generator.GenerateLegacy(difficulty,
                    params.stock_width, params.stock_length);
                if (result.success) {
                    PrintInstanceInfo(result.instance, result.estimate);
                    std::string filepath = InstanceGenerator::GenerateFilename(
                        params, output_dir);
                    if (InstanceGenerator::ExportCSV(result.instance, filepath)) {
                        std::cout << "\nExported to: " << filepath << "\n";
                    }
                } else {
                    std::cerr << "Error: " << result.error_message << "\n";
                    return 1;
                }
            } else {
                // 批量: 使用旧参数映射
                for (int i = 0; i < count; i++) {
                    result = generator.GenerateLegacy(difficulty,
                        params.stock_width, params.stock_length);
                    if (result.success) {
                        std::string filepath = InstanceGenerator::GenerateFilename(
                            params, output_dir);
                        InstanceGenerator::ExportCSV(result.instance, filepath);
                        std::cout << "Generated: " << filepath
                                  << " (" << result.estimate.level_name
                                  << ", score=" << std::fixed << std::setprecision(2)
                                  << result.estimate.score << ")\n";
                    }
                    if (i < count - 1) {
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                    }
                }
                std::cout << "\nGenerated " << count << " instances in " << output_dir << "\n";
            }
            break;
        }

        case Mode::kPreset: {
            std::string preset_name;
            switch (preset) {
                case Preset::kEasy:   preset_name = "Easy"; break;
                case Preset::kMedium: preset_name = "Medium"; break;
                case Preset::kHard:   preset_name = "Hard"; break;
                case Preset::kExpert: preset_name = "Expert"; break;
            }
            std::cout << "Mode: Preset (" << preset_name << ")\n";

            GeneratorParams preset_params = GeneratorParams::FromPreset(preset);
            preset_params.stock_width = params.stock_width;
            preset_params.stock_length = params.stock_length;
            preset_params.seed = seed;

            if (count == 1) {
                result = generator.Generate(preset_params);
                if (result.success) {
                    PrintInstanceInfo(result.instance, result.estimate);
                    std::string filepath = InstanceGenerator::GenerateFilename(
                        preset_params, output_dir);
                    if (InstanceGenerator::ExportCSV(result.instance, filepath)) {
                        std::cout << "\nExported to: " << filepath << "\n";
                    }
                } else {
                    std::cerr << "Error: " << result.error_message << "\n";
                    return 1;
                }
            } else {
                generator.GenerateBatch(preset_params, count, output_dir);
            }
            break;
        }

        case Mode::kManual: {
            std::cout << "Mode: Manual\n";
            params.seed = seed;

            std::cout << params.GetSummary();

            if (!params.Validate()) {
                std::cerr << "Error: Invalid parameters\n";
                return 1;
            }

            if (count == 1) {
                result = generator.Generate(params);
                if (result.success) {
                    PrintInstanceInfo(result.instance, result.estimate);
                    std::string filepath = InstanceGenerator::GenerateFilename(
                        params, output_dir);
                    if (InstanceGenerator::ExportCSV(result.instance, filepath)) {
                        std::cout << "\nExported to: " << filepath << "\n";
                    }
                } else {
                    std::cerr << "Error: " << result.error_message << "\n";
                    return 1;
                }
            } else {
                generator.GenerateBatch(params, count, output_dir);
            }
            break;
        }
    }

    return 0;
}
