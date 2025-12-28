// main.cpp - 2D切割问题算例生成器入口
// 项目: CS-2D-Data
// 用法: CS-2D-Data [选项]
//   -d, --difficulty <0.0-1.0>  难度系数 (默认0.5)
//   -n, --count <数量>          生成算例数量 (默认1)
//   -W, --width <宽度>          母板宽度 W (切割方向, 默认200)
//   -L, --length <长度>         母板长度 L (条带延伸方向, 默认400)
//   -o, --output <目录>         输出目录 (默认data)
//   -s, --seed <种子>           随机种子 (默认0=时间戳)
//   -h, --help                  显示帮助

#include "generator.h"
#include <iostream>
#include <string>
#include <cstring>

using namespace std;

void PrintUsage(const char* program) {
    cout << "2D Cutting Stock Problem Instance Generator (OR Standard Format)\n";
    cout << "Usage: " << program << " [options]\n\n";
    cout << "Options:\n";
    cout << "  -d, --difficulty <0.0-1.0>  Difficulty level (default: 0.5)\n";
    cout << "                              0.0 = Easy, 1.0 = Hard\n";
    cout << "  -n, --count <num>           Number of instances (default: 1)\n";
    cout << "  -W, --width <size>          Stock width W (Stage1 cutting, default: 200)\n";
    cout << "  -L, --length <size>         Stock length L (Stage2 cutting, default: 400)\n";
    cout << "  -o, --output <dir>          Output directory (default: data)\n";
    cout << "  -s, --seed <value>          Random seed (default: 0 = timestamp)\n";
    cout << "  -h, --help                  Show this help\n\n";
    cout << "OR Standard:\n";
    cout << "  Stage1: Cut stock (W x L) into strips along W, strip width = w_j\n";
    cout << "  Stage2: Cut strips along L into items, item length = l_i\n\n";
    cout << "Difficulty Levels:\n";
    cout << "  0.0-0.3  Easy   - Reverse generation, known optimal\n";
    cout << "  0.3-0.8  Medium - Parameterized random generation\n";
    cout << "  0.8-1.0  Hard   - Residual instances\n\n";
    cout << "Examples:\n";
    cout << "  " << program << " -d 0.5 -n 10\n";
    cout << "  " << program << " -d 0.8 -W 300 -L 600 -o hard_instances\n";
}

void PrintInstanceInfo(const Instance& inst) {
    cout << "Instance Summary:\n";
    cout << "  Stock size: W=" << inst.stock_width << " x L=" << inst.stock_length << "\n";
    cout << "  Item types: " << inst.items.size() << "\n";
    cout << "  Difficulty: " << inst.difficulty << "\n";

    if (inst.known_optimal > 0) {
        cout << "  Known optimal: " << inst.known_optimal << "\n";
    }

    // 统计总需求
    int total_demand = 0;
    for (const auto& item : inst.items) {
        total_demand += item.demand;
    }
    cout << "  Total demand: " << total_demand << "\n";

    // 计算理论下界
    long long total_area = 0;
    for (const auto& item : inst.items) {
        total_area += (long long)item.width * item.length * item.demand;
    }
    long long stock_area = (long long)inst.stock_width * inst.stock_length;
    double lb = (double)total_area / stock_area;
    cout << "  Area lower bound: " << lb << "\n";
}

int main(int argc, char* argv[]) {
    // 默认参数 (OR标准: W=宽度, L=长度)
    double difficulty = 0.5;
    int count = 1;
    int stock_width = 200;   // W: 切割方向
    int stock_length = 400;  // L: 条带延伸方向
    string output_dir = "data";
    unsigned int seed = 0;

    // 解析命令行参数
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            PrintUsage(argv[0]);
            return 0;
        }
        else if ((arg == "-d" || arg == "--difficulty") && i + 1 < argc) {
            difficulty = stod(argv[++i]);
        }
        else if ((arg == "-n" || arg == "--count") && i + 1 < argc) {
            count = stoi(argv[++i]);
        }
        else if ((arg == "-W" || arg == "--width") && i + 1 < argc) {
            stock_width = stoi(argv[++i]);
        }
        else if ((arg == "-L" || arg == "--length") && i + 1 < argc) {
            stock_length = stoi(argv[++i]);
        }
        else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            output_dir = argv[++i];
        }
        else if ((arg == "-s" || arg == "--seed") && i + 1 < argc) {
            seed = stoul(argv[++i]);
        }
        else {
            cerr << "Unknown option: " << arg << "\n";
            PrintUsage(argv[0]);
            return 1;
        }
    }

    // 参数验证
    if (difficulty < 0.0 || difficulty > 1.0) {
        cerr << "Error: Difficulty must be between 0.0 and 1.0\n";
        return 1;
    }
    if (count < 1) {
        cerr << "Error: Count must be at least 1\n";
        return 1;
    }
    if (stock_width < 50 || stock_length < 50) {
        cerr << "Error: Stock dimensions must be at least 50\n";
        return 1;
    }

    cout << "2D Cutting Stock Instance Generator (OR Standard)\n";
    cout << "Configuration:\n";
    cout << "  Difficulty: " << difficulty << "\n";
    cout << "  Count: " << count << "\n";
    cout << "  Stock size: W=" << stock_width << " x L=" << stock_length << "\n";
    cout << "  Output: " << output_dir << "\n";
    cout << "  Seed: " << (seed == 0 ? "timestamp" : to_string(seed)) << "\n";
    cout << "\n";

    // 创建生成器
    InstanceGenerator generator(seed);

    // 生成算例
    if (count == 1) {
        // 单个算例: 显示详细信息
        Instance inst = generator.Generate(difficulty, stock_width, stock_length);
        PrintInstanceInfo(inst);

        // 使用时间戳格式文件名
        string filepath = generator.GenerateFilename(difficulty, output_dir);
        if (generator.ExportCSV(inst, filepath)) {
            cout << "\nExported to: " << filepath << "\n";
        }
    } else {
        // 批量生成
        generator.GenerateBatch(count, difficulty, output_dir);
        cout << "\nGenerated " << count << " instances in " << output_dir << "\n";
    }

    return 0;
}
