// generator.cpp - 算例生成器实现
// 实现三种生成策略:
// 1. 逆向生成: 已知最优解, 适合验证算法正确性
// 2. 参数化随机: 灵活控制难度, 适合性能测试
// 3. 残差算例: 高难度, 适合极限测试

#include "generator.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <thread>
#include <set>
#include <map>
#include <iostream>
#include <filesystem>

using namespace std;

// 小质数表, 用于生成"不友好"的尺寸偏移
static const int kPrimes[] = {7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47};
static const int kNumPrimes = sizeof(kPrimes) / sizeof(kPrimes[0]);

InstanceGenerator::InstanceGenerator(unsigned int seed) {
    if (seed == 0) {
        seed = static_cast<unsigned int>(
            chrono::system_clock::now().time_since_epoch().count());
    }
    rng_.seed(seed);
}

Instance InstanceGenerator::Generate(double difficulty, int stock_width, int stock_height) {
    DifficultyParams params(difficulty);

    Instance inst;
    inst.stock_width = stock_width;
    inst.stock_height = stock_height;
    inst.difficulty = difficulty;
    inst.known_optimal = -1;

    // 根据难度选择生成策略
    int strategy = params.Strategy();

    switch (strategy) {
        case 0:
            inst = GenerateReverse(params, stock_width, stock_height);
            break;
        case 1:
            inst = GenerateRandom(params, stock_width, stock_height);
            break;
        case 2:
            inst = GenerateResidual(params, stock_width, stock_height);
            break;
        default:
            inst = GenerateRandom(params, stock_width, stock_height);
    }

    return inst;
}

// 策略0: 逆向生成
// 思路: 先用贪心方法填满若干张母板, 统计各子板类型的产出作为需求
Instance InstanceGenerator::GenerateReverse(const DifficultyParams& params, int W, int H) {
    Instance inst;
    inst.stock_width = W;
    inst.stock_height = H;
    inst.difficulty = params.difficulty;

    // 决定使用的母板数量 (即最优解)
    uniform_int_distribution<int> num_stocks_dist(3, 8);
    int num_stocks = num_stocks_dist(rng_);
    inst.known_optimal = num_stocks;

    // 生成若干种基础子板尺寸
    int num_base_types = params.NumItemTypes();
    vector<pair<int, int>> base_sizes;

    for (int i = 0; i < num_base_types; i++) {
        auto [w, h] = GenerateItemSize(W, H, params);
        base_sizes.push_back({w, h});
    }

    // 统计每种子板的需求量
    map<pair<int, int>, int> demand_map;

    // 对每张母板进行贪心填充
    for (int s = 0; s < num_stocks; s++) {
        int remaining_height = H;

        // 沿高度方向逐行填充
        while (remaining_height > 0) {
            // 随机选择一种子板作为当前行高度
            uniform_int_distribution<int> type_dist(0, num_base_types - 1);
            int type_idx = type_dist(rng_);
            int row_height = base_sizes[type_idx].second;

            if (row_height > remaining_height) {
                // 尝试找一个能放下的
                bool found = false;
                for (int i = 0; i < num_base_types; i++) {
                    if (base_sizes[i].second <= remaining_height) {
                        row_height = base_sizes[i].second;
                        type_idx = i;
                        found = true;
                        break;
                    }
                }
                if (!found) break;
            }

            // 在当前行内沿宽度方向填充
            int remaining_width = W;
            while (remaining_width > 0) {
                // 随机选择能放入的子板
                vector<int> valid_types;
                for (int i = 0; i < num_base_types; i++) {
                    if (base_sizes[i].first <= remaining_width &&
                        base_sizes[i].second == row_height) {
                        valid_types.push_back(i);
                    }
                }

                if (valid_types.empty()) break;

                uniform_int_distribution<int> pick_dist(0, (int)valid_types.size() - 1);
                int picked = valid_types[pick_dist(rng_)];

                demand_map[base_sizes[picked]]++;
                remaining_width -= base_sizes[picked].first;
            }

            remaining_height -= row_height;
        }
    }

    // 将统计结果转换为子板列表
    int id = 0;
    for (const auto& [size, demand] : demand_map) {
        if (demand > 0) {
            ItemType item;
            item.id = id++;
            item.width = size.first;
            item.height = size.second;
            item.demand = demand;
            inst.items.push_back(item);
        }
    }

    // 如果生成的子板类型太少, 补充一些
    while ((int)inst.items.size() < 3) {
        auto [w, h] = GenerateItemSize(W, H, params);
        ItemType item;
        item.id = id++;
        item.width = w;
        item.height = h;
        item.demand = 1;
        inst.items.push_back(item);
        inst.known_optimal = -1;  // 添加额外子板后最优解不再确定
    }

    return inst;
}

// 策略1: 参数化随机生成
// 根据难度参数控制尺寸分布、相似度、需求量
Instance InstanceGenerator::GenerateRandom(const DifficultyParams& params, int W, int H) {
    Instance inst;
    inst.stock_width = W;
    inst.stock_height = H;
    inst.difficulty = params.difficulty;
    inst.known_optimal = -1;

    int num_types = params.NumItemTypes();
    double similarity = params.SizeSimilarity();

    // 生成第一个子板作为基准
    auto [base_w, base_h] = GenerateItemSize(W, H, params);

    // 用于去重的集合
    set<pair<int, int>> used_sizes;
    used_sizes.insert({base_w, base_h});

    // 添加第一个子板
    ItemType first_item;
    first_item.id = 0;
    first_item.width = base_w;
    first_item.height = base_h;
    uniform_int_distribution<int> demand_dist(params.MinDemand(), params.MaxDemand());
    first_item.demand = demand_dist(rng_);
    inst.items.push_back(first_item);

    // 生成剩余子板
    for (int i = 1; i < num_types; i++) {
        int w, h;
        int attempts = 0;
        const int max_attempts = 50;

        do {
            // 根据相似度决定是基于基准尺寸还是完全随机
            uniform_real_distribution<double> prob_dist(0.0, 1.0);
            if (prob_dist(rng_) < similarity) {
                // 基于基准尺寸生成相似尺寸
                tie(w, h) = GenerateItemSize(W, H, params, base_w, base_h);
            } else {
                // 完全随机生成
                tie(w, h) = GenerateItemSize(W, H, params);
            }
            attempts++;
        } while (used_sizes.count({w, h}) && attempts < max_attempts);

        if (attempts >= max_attempts) continue;

        used_sizes.insert({w, h});

        ItemType item;
        item.id = i;
        item.width = w;
        item.height = h;
        item.demand = demand_dist(rng_);
        inst.items.push_back(item);
    }

    // 重新编号
    for (int i = 0; i < (int)inst.items.size(); i++) {
        inst.items[i].id = i;
    }

    return inst;
}

// 策略2: 残差算例生成
// 生成一组"几乎能填满"但有少量浪费的子板组合
Instance InstanceGenerator::GenerateResidual(const DifficultyParams& params, int W, int H) {
    Instance inst;
    inst.stock_width = W;
    inst.stock_height = H;
    inst.difficulty = params.difficulty;
    inst.known_optimal = -1;

    // 生成基础尺寸, 使用质数偏移确保不能完美填充
    vector<pair<int, int>> sizes;
    int num_types = params.NumItemTypes();

    // 使用母板尺寸的"不友好"分割
    for (int i = 0; i < num_types; i++) {
        int w = GenerateDifficultSize(W, params.difficulty);
        int h = GenerateDifficultSize(H, params.difficulty);

        // 确保尺寸在合理范围内
        int min_w = (int)(W * params.MinSizeRatio());
        int max_w = (int)(W * params.MaxSizeRatio());
        int min_h = (int)(H * params.MinSizeRatio());
        int max_h = (int)(H * params.MaxSizeRatio());

        w = clamp(w, min_w, max_w);
        h = clamp(h, min_h, max_h);

        sizes.push_back({w, h});
    }

    // 去重并创建子板
    set<pair<int, int>> unique_sizes(sizes.begin(), sizes.end());

    int id = 0;
    uniform_int_distribution<int> demand_dist(1, params.MaxDemand() / 2);  // 残差算例需求量更小

    for (const auto& [w, h] : unique_sizes) {
        ItemType item;
        item.id = id++;
        item.width = w;
        item.height = h;
        item.demand = demand_dist(rng_);
        inst.items.push_back(item);
    }

    return inst;
}

pair<int, int> InstanceGenerator::GenerateItemSize(int W, int H,
    const DifficultyParams& params, int base_w, int base_h) {

    int min_w = (int)(W * params.MinSizeRatio());
    int max_w = (int)(W * params.MaxSizeRatio());
    int min_h = (int)(H * params.MinSizeRatio());
    int max_h = (int)(H * params.MaxSizeRatio());

    int w, h;

    if (base_w > 0 && base_h > 0) {
        // 基于基准尺寸生成相似尺寸
        double similarity = params.SizeSimilarity();
        int variation_w = (int)((1.0 - similarity) * (max_w - min_w) / 2);
        int variation_h = (int)((1.0 - similarity) * (max_h - min_h) / 2);

        variation_w = max(1, variation_w);
        variation_h = max(1, variation_h);

        uniform_int_distribution<int> var_w_dist(-variation_w, variation_w);
        uniform_int_distribution<int> var_h_dist(-variation_h, variation_h);

        w = base_w + var_w_dist(rng_);
        h = base_h + var_h_dist(rng_);
    } else {
        // 完全随机生成
        uniform_int_distribution<int> w_dist(min_w, max_w);
        uniform_int_distribution<int> h_dist(min_h, max_h);

        w = w_dist(rng_);
        h = h_dist(rng_);
    }

    // 应用质数偏移增加难度
    if (params.UsePrimeOffset()) {
        uniform_int_distribution<int> prime_idx(0, kNumPrimes - 1);
        uniform_int_distribution<int> sign_dist(0, 1);

        int offset_w = kPrimes[prime_idx(rng_)] * (sign_dist(rng_) ? 1 : -1);
        int offset_h = kPrimes[prime_idx(rng_)] * (sign_dist(rng_) ? 1 : -1);

        w += offset_w;
        h += offset_h;
    }

    // 确保在有效范围内
    w = clamp(w, min_w, max_w);
    h = clamp(h, min_h, max_h);

    // 确保能放入母板
    w = min(w, W);
    h = min(h, H);

    return {w, h};
}

int InstanceGenerator::GenerateDifficultSize(int stock_size, double difficulty) {
    // 基础策略: 选择不能整除母板的尺寸
    uniform_int_distribution<int> divisor_dist(3, 7);
    int divisor = divisor_dist(rng_);
    int base = stock_size / divisor;

    // 添加质数偏移, 难度越高偏移越大
    uniform_int_distribution<int> prime_idx(0, kNumPrimes - 1);
    int prime = kPrimes[prime_idx(rng_)];

    // 偏移方向随机
    uniform_int_distribution<int> sign_dist(0, 1);
    int sign = sign_dist(rng_) ? 1 : -1;

    // 偏移量与难度相关
    int offset = (int)(prime * difficulty);

    return base + sign * offset;
}

bool InstanceGenerator::ValidateInstance(const Instance& inst) {
    for (const auto& item : inst.items) {
        if (item.width > inst.stock_width || item.height > inst.stock_height) {
            return false;
        }
        if (item.width <= 0 || item.height <= 0 || item.demand <= 0) {
            return false;
        }
    }
    return !inst.items.empty();
}

bool InstanceGenerator::ExportCSV(const Instance& inst, const string& filepath) {
    // 确保目录存在
    filesystem::path path(filepath);
    if (path.has_parent_path()) {
        filesystem::create_directories(path.parent_path());
    }

    ofstream fout(filepath);
    if (!fout) {
        cerr << "Error: Cannot open file " << filepath << endl;
        return false;
    }

    // 写入文件头注释
    fout << "# 2DPackLib CSV Format - 2D Cutting Stock Problem\n";
    fout << "# Generated by CS-2D-Data\n";
    fout << "# Difficulty: " << fixed << setprecision(2) << inst.difficulty << "\n";
    if (inst.known_optimal > 0) {
        fout << "# Known Optimal: " << inst.known_optimal << "\n";
    }
    fout << "#\n";

    // 写入母板尺寸
    fout << "stock_width,stock_height\n";
    fout << inst.stock_width << "," << inst.stock_height << "\n";
    fout << "#\n";

    // 写入子板数据
    fout << "id,width,height,demand\n";
    for (const auto& item : inst.items) {
        fout << item.id << ","
             << item.width << ","
             << item.height << ","
             << item.demand << "\n";
    }

    fout.close();
    return true;
}

string InstanceGenerator::GenerateFilename(double difficulty, const string& output_dir) {
    // 获取当前时间
    auto now = chrono::system_clock::now();
    auto time_t_val = chrono::system_clock::to_time_t(now);

    tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &time_t_val);
#else
    localtime_r(&time_t_val, &tm_buf);
#endif

    // 生成文件名: inst_{YYYYMMDD}_{HHMMSS}_d{difficulty}.csv
    // 时间戳在前, 确保按文件名排序即为时间顺序
    ostringstream filename;
    filename << output_dir << "/inst_"
             << put_time(&tm_buf, "%Y%m%d_%H%M%S")
             << "_d" << fixed << setprecision(2) << difficulty
             << ".csv";

    return filename.str();
}

void InstanceGenerator::GenerateBatch(int count, double difficulty,
    const string& output_dir) {

    filesystem::create_directories(output_dir);

    for (int i = 0; i < count; i++) {
        Instance inst = Generate(difficulty);

        // 生成带时间戳的文件名
        string filepath = GenerateFilename(difficulty, output_dir);

        ExportCSV(inst, filepath);
        cout << "Generated: " << filepath << endl;

        // 批量生成时等待1秒确保时间戳唯一
        if (i < count - 1) {
            this_thread::sleep_for(chrono::seconds(1));
        }
    }
}
