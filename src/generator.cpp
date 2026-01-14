// ============================================================================
// 工程标准 (Engineering Standards)
// - 坐标系: 左下角为原点
// - 宽度(Width): 上下方向 (Y轴)
// - 长度(Length): 左右方向 (X轴)
// - 约束: 长度 >= 宽度
// ============================================================================

// generator.cpp - 算例生成器实现
// 实现四种生成策略: 逆向/随机/聚类/残差

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
#include <cmath>

// 小质数表, 用于质数偏移生成
static const int kPrimes[] = {7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47};
static const int kNumPrimes = sizeof(kPrimes) / sizeof(kPrimes[0]);

// 从预设创建参数
GeneratorParams GeneratorParams::FromPreset(Preset preset) {
    GeneratorParams p;

    switch (preset) {
        case Preset::kEasy:
            p.num_types = 8;
            p.min_size_ratio = 0.06;
            p.max_size_ratio = 0.25;
            p.size_cv = 0.20;
            p.min_demand = 5;
            p.max_demand = 20;
            p.demand_skew = 0.0;
            p.prime_offset = false;
            p.strategy = 0;  // 逆向生成, 已知最优
            break;

        case Preset::kMedium:
            p.num_types = 20;
            p.min_size_ratio = 0.10;
            p.max_size_ratio = 0.35;
            p.size_cv = 0.30;
            p.min_demand = 3;
            p.max_demand = 12;
            p.demand_skew = 0.2;
            p.prime_offset = false;
            p.strategy = 1;  // 随机生成
            break;

        case Preset::kHard:
            p.num_types = 35;
            p.min_size_ratio = 0.15;
            p.max_size_ratio = 0.45;
            p.size_cv = 0.40;
            p.min_demand = 2;
            p.max_demand = 6;
            p.demand_skew = 0.4;
            p.prime_offset = true;
            p.strategy = 1;
            break;

        case Preset::kExpert:
            p.num_types = 50;
            p.min_size_ratio = 0.20;
            p.max_size_ratio = 0.50;
            p.size_cv = 0.50;
            p.min_demand = 1;
            p.max_demand = 3;
            p.demand_skew = 0.6;
            p.prime_offset = true;
            p.strategy = 3;  // 残差生成
            break;
    }

    return p;
}

// 参数有效性检查
bool GeneratorParams::Validate() const {
    if (num_types < 3 || num_types > 200) return false;
    if (stock_width < 50 || stock_length < 50) return false;
    if (min_size_ratio < 0.01 || min_size_ratio > 0.50) return false;
    if (max_size_ratio < min_size_ratio || max_size_ratio > 0.80) return false;
    if (min_demand < 1 || max_demand < min_demand) return false;
    if (size_cv < 0.0 || size_cv > 1.0) return false;
    if (demand_skew < 0.0 || demand_skew > 1.0) return false;
    if (strategy < 0 || strategy > 3) return false;
    return true;
}

// 打印参数摘要
std::string GeneratorParams::GetSummary() const {
    std::ostringstream ss;
    ss << "生成参数:\n"
       << "  子板类型数: " << num_types << "\n"
       << "  母板尺寸: " << stock_width << " x " << stock_length << "\n"
       << "  尺寸比例: [" << min_size_ratio << ", " << max_size_ratio << "]\n"
       << "  尺寸变异系数: " << size_cv << "\n"
       << "  需求范围: [" << min_demand << ", " << max_demand << "]\n"
       << "  需求偏斜度: " << demand_skew << "\n"
       << "  质数偏移: " << (prime_offset ? "是" : "否") << "\n"
       << "  生成策略: " << strategy << "\n";
    return ss.str();
}

// 构造函数
InstanceGenerator::InstanceGenerator() {
    SetSeed(0);
}

InstanceGenerator::InstanceGenerator(int seed) {
    SetSeed(seed);
}

void InstanceGenerator::SetSeed(int seed) {
    if (seed == 0) {
        seed = static_cast<int>(
            std::chrono::system_clock::now().time_since_epoch().count() & 0x7FFFFFFF);
    }
    rng_.seed(static_cast<unsigned int>(seed));
}

// 主生成函数
GenerationResult InstanceGenerator::Generate(const GeneratorParams& params) {
    GenerationResult result;
    result.success = false;

    // 参数检查
    if (!params.Validate()) {
        result.error_message = "Invalid parameters";
        return result;
    }

    // 设置随机种子
    if (params.seed != 0) {
        SetSeed(params.seed);
    }

    // 根据策略选择生成方法
    Instance inst;
    switch (params.strategy) {
        case 0:
            inst = GenerateReverse(params);
            break;
        case 1:
            inst = GenerateRandom(params);
            break;
        case 2:
            inst = GenerateCluster(params);
            break;
        case 3:
            inst = GenerateResidual(params);
            break;
        default:
            inst = GenerateRandom(params);
    }

    // 验证并修正
    if (!ValidateAndFix(inst, params)) {
        result.error_message = "Failed to generate valid instance";
        return result;
    }

    // 难度预估
    result.instance = inst;
    result.estimate = estimator_.Estimate(inst);
    result.success = true;
    return result;
}

// 快捷生成 (使用预设)
GenerationResult InstanceGenerator::Generate(Preset preset) {
    return Generate(GeneratorParams::FromPreset(preset));
}

// 兼容旧接口
GenerationResult InstanceGenerator::GenerateLegacy(double difficulty,
    int stock_width, int stock_length) {
    // 将0-1难度值映射到新参数
    difficulty = std::clamp(difficulty, 0.0, 1.0);

    GeneratorParams params;
    params.stock_width = stock_width;
    params.stock_length = stock_length;
    params.num_types = 5 + static_cast<int>(difficulty * 35);
    params.min_size_ratio = 0.08 + difficulty * 0.07;
    params.max_size_ratio = 0.35 + difficulty * 0.15;
    params.min_demand = std::max(1, 6 - static_cast<int>(difficulty * 5));
    params.max_demand = std::max(3, 30 - static_cast<int>(difficulty * 27));
    params.size_cv = 0.15 + difficulty * 0.35;
    params.demand_skew = difficulty * 0.5;
    params.prime_offset = (difficulty > 0.5);

    if (difficulty < 0.3) params.strategy = 0;
    else if (difficulty < 0.8) params.strategy = 1;
    else params.strategy = 3;

    return Generate(params);
}

// 策略0: 逆向生成 (构造完美填充, 已知最优解)
Instance InstanceGenerator::GenerateReverse(const GeneratorParams& params) {
    Instance inst;
    inst.stock_width = params.stock_width;
    inst.stock_length = params.stock_length;
    inst.known_optimal = -1;

    int W = params.stock_width;
    int L = params.stock_length;

    // 决定母板数量 (即最优解)
    std::uniform_int_distribution<int> num_stocks_dist(3, 8);
    int num_stocks = num_stocks_dist(rng_);
    inst.known_optimal = num_stocks;

    // 生成基础子板尺寸
    std::vector<std::pair<int, int>> base_sizes;
    for (int i = 0; i < params.num_types; i++) {
        auto size = GenerateItemSize(params);
        base_sizes.push_back(size);
    }

    // 统计每种子板需求量
    std::map<std::pair<int, int>, int> demand_map;

    // 对每张母板进行贪心填充
    for (int s = 0; s < num_stocks; s++) {
        int remaining_width = W;

        // Stage1: 沿宽度方向切条带
        while (remaining_width > 0) {
            // 随机选择子板宽度作为条带宽度
            std::uniform_int_distribution<int> type_dist(0, params.num_types - 1);
            int type_idx = type_dist(rng_);
            int strip_width = base_sizes[type_idx].first;

            // 如果放不下, 找一个能放的
            if (strip_width > remaining_width) {
                bool found = false;
                for (int i = 0; i < params.num_types; i++) {
                    if (base_sizes[i].first <= remaining_width) {
                        strip_width = base_sizes[i].first;
                        type_idx = i;
                        found = true;
                        break;
                    }
                }
                if (!found) break;
            }

            // Stage2: 在条带内沿长度方向切子板
            int remaining_length = L;
            while (remaining_length > 0) {
                // 找能放入的子板 (宽度匹配)
                std::vector<int> valid_types;
                for (int i = 0; i < params.num_types; i++) {
                    if (base_sizes[i].first == strip_width &&
                        base_sizes[i].second <= remaining_length) {
                        valid_types.push_back(i);
                    }
                }
                if (valid_types.empty()) break;

                std::uniform_int_distribution<int> pick_dist(0,
                    static_cast<int>(valid_types.size()) - 1);
                int picked = valid_types[pick_dist(rng_)];

                demand_map[base_sizes[picked]]++;
                remaining_length -= base_sizes[picked].second;
            }

            remaining_width -= strip_width;
        }
    }

    // 转换为子板列表
    int id = 0;
    for (const auto& [size, demand] : demand_map) {
        if (demand > 0) {
            Item item;
            item.id = id++;
            item.width = size.first;
            item.length = size.second;
            item.demand = demand;
            inst.items.push_back(item);
        }
    }

    // 保证最少3种子板
    while (static_cast<int>(inst.items.size()) < 3) {
        auto size = GenerateItemSize(params);
        Item item;
        item.id = id++;
        item.width = size.first;
        item.length = size.second;
        item.demand = 1;
        inst.items.push_back(item);
        inst.known_optimal = -1;  // 不再确定最优解
    }

    return inst;
}

// 策略1: 参数化随机生成
Instance InstanceGenerator::GenerateRandom(const GeneratorParams& params) {
    Instance inst;
    inst.stock_width = params.stock_width;
    inst.stock_length = params.stock_length;
    inst.known_optimal = -1;

    std::set<std::pair<int, int>> used_sizes;

    // 确定热门子板数量
    int num_peak = static_cast<int>(params.num_types * params.peak_ratio);

    for (int i = 0; i < params.num_types; i++) {
        int w, l;
        int attempts = 0;
        const int max_attempts = 50;

        // 尝试生成不重复的尺寸
        do {
            auto size = GenerateItemSize(params);
            w = size.first;
            l = size.second;
            attempts++;
        } while (used_sizes.count({w, l}) && attempts < max_attempts);

        if (attempts >= max_attempts) continue;
        used_sizes.insert({w, l});

        Item item;
        item.id = i;
        item.width = w;
        item.length = l;
        item.demand = GenerateDemand(params, i < num_peak);
        inst.items.push_back(item);
    }

    // 重新编号
    for (int i = 0; i < static_cast<int>(inst.items.size()); i++) {
        inst.items[i].id = i;
    }

    return inst;
}

// 策略2: 聚类生成 (尺寸分群)
Instance InstanceGenerator::GenerateCluster(const GeneratorParams& params) {
    Instance inst;
    inst.stock_width = params.stock_width;
    inst.stock_length = params.stock_length;
    inst.known_optimal = -1;

    int W = params.stock_width;
    int L = params.stock_length;

    // 确定聚类数 (默认3-5个)
    int num_clusters = params.num_clusters;
    if (num_clusters <= 0) {
        std::uniform_int_distribution<int> cluster_dist(3, 5);
        num_clusters = cluster_dist(rng_);
    }

    // 生成聚类中心
    std::vector<std::pair<int, int>> centers;
    for (int c = 0; c < num_clusters; c++) {
        auto center = GenerateItemSize(params);
        centers.push_back(center);
    }

    // 每个聚类分配的子板数量
    int per_cluster = params.num_types / num_clusters;
    int remainder = params.num_types % num_clusters;

    std::set<std::pair<int, int>> used_sizes;
    int id = 0;

    for (int c = 0; c < num_clusters; c++) {
        int cluster_size = per_cluster + (c < remainder ? 1 : 0);
        auto [center_w, center_l] = centers[c];

        // 聚类内变异范围 (比正常变异小)
        int var_w = static_cast<int>(W * params.size_cv * 0.3);
        int var_l = static_cast<int>(L * params.size_cv * 0.3);
        var_w = std::max(5, var_w);
        var_l = std::max(5, var_l);

        for (int i = 0; i < cluster_size; i++) {
            int w, l;
            int attempts = 0;

            do {
                // 围绕聚类中心生成
                std::uniform_int_distribution<int> w_dist(
                    std::max(1, center_w - var_w), center_w + var_w);
                std::uniform_int_distribution<int> l_dist(
                    std::max(1, center_l - var_l), center_l + var_l);
                w = std::min(w_dist(rng_), W);
                l = std::min(l_dist(rng_), L);
                attempts++;
            } while (used_sizes.count({w, l}) && attempts < 30);

            if (attempts >= 30) continue;
            used_sizes.insert({w, l});

            Item item;
            item.id = id++;
            item.width = w;
            item.length = l;
            item.demand = GenerateDemand(params, false);
            inst.items.push_back(item);
        }
    }

    return inst;
}

// 策略3: 残差生成 (难以完美填充)
Instance InstanceGenerator::GenerateResidual(const GeneratorParams& params) {
    Instance inst;
    inst.stock_width = params.stock_width;
    inst.stock_length = params.stock_length;
    inst.known_optimal = -1;

    int W = params.stock_width;
    int L = params.stock_length;

    std::set<std::pair<int, int>> used_sizes;
    int id = 0;

    for (int i = 0; i < params.num_types; i++) {
        // 使用质数偏移生成"不友好"尺寸
        int w = GeneratePrimeOffsetSize(W, params.min_size_ratio, params.max_size_ratio);
        int l = GeneratePrimeOffsetSize(L, params.min_size_ratio, params.max_size_ratio);

        // 如果尺寸重复, 略微调整
        int attempts = 0;
        while (used_sizes.count({w, l}) && attempts < 20) {
            w = std::min(w + 1, W);
            l = std::min(l + 1, L);
            attempts++;
        }
        if (used_sizes.count({w, l})) continue;
        used_sizes.insert({w, l});

        Item item;
        item.id = id++;
        item.width = w;
        item.length = l;
        // 残差算例需求量通常较小
        item.demand = GenerateDemand(params, false);
        inst.items.push_back(item);
    }

    return inst;
}

// 生成单个子板尺寸
std::pair<int, int> InstanceGenerator::GenerateItemSize(
    const GeneratorParams& params, int base_w, int base_l) {

    int W = params.stock_width;
    int L = params.stock_length;

    // 计算尺寸范围 (基于面积比)
    double stock_area = static_cast<double>(W * L);
    double min_area = stock_area * params.min_size_ratio;
    double max_area = stock_area * params.max_size_ratio;

    // 宽度范围
    int min_w = static_cast<int>(std::sqrt(min_area * 0.5));
    int max_w = static_cast<int>(std::sqrt(max_area * 2.0));
    min_w = std::max(5, std::min(min_w, W - 1));
    max_w = std::min(max_w, W);

    int w, l;

    if (base_w > 0 && base_l > 0) {
        // 基于基准尺寸生成相似尺寸
        int var_w = static_cast<int>(W * params.size_cv * 0.5);
        int var_l = static_cast<int>(L * params.size_cv * 0.5);
        var_w = std::max(3, var_w);
        var_l = std::max(3, var_l);

        std::uniform_int_distribution<int> w_dist(
            std::max(min_w, base_w - var_w), std::min(max_w, base_w + var_w));
        std::uniform_int_distribution<int> l_dist(
            std::max(5, base_l - var_l), std::min(L, base_l + var_l));
        w = w_dist(rng_);
        l = l_dist(rng_);
    } else {
        // 随机生成
        std::uniform_int_distribution<int> w_dist(min_w, max_w);
        w = w_dist(rng_);

        // 根据面积约束计算长度范围
        int target_area_min = static_cast<int>(min_area);
        int target_area_max = static_cast<int>(max_area);
        int l_min = std::max(5, target_area_min / w);
        int l_max = std::min(L, target_area_max / w);
        l_max = std::max(l_min, l_max);

        std::uniform_int_distribution<int> l_dist(l_min, l_max);
        l = l_dist(rng_);
    }

    // 应用质数偏移
    if (params.prime_offset) {
        std::uniform_int_distribution<int> prime_idx(0, kNumPrimes - 1);
        std::uniform_int_distribution<int> sign_dist(0, 1);
        int offset = kPrimes[prime_idx(rng_)] * (sign_dist(rng_) ? 1 : -1);
        w = std::clamp(w + offset / 2, min_w, max_w);
        l = std::clamp(l + offset, 5, L);
    }

    // 确保 length >= width (工程规范要求)
    if (l < w) {
        std::swap(w, l);
    }

    return {w, l};
}

// 生成质数偏移尺寸
int InstanceGenerator::GeneratePrimeOffsetSize(int stock_size,
    double min_ratio, double max_ratio) {

    // 选择除数 (3-7)
    std::uniform_int_distribution<int> divisor_dist(3, 7);
    int divisor = divisor_dist(rng_);
    int base = stock_size / divisor;

    // 添加质数偏移
    std::uniform_int_distribution<int> prime_idx(0, kNumPrimes - 1);
    int prime = kPrimes[prime_idx(rng_)];
    std::uniform_int_distribution<int> sign_dist(0, 1);
    int sign = sign_dist(rng_) ? 1 : -1;

    int result = base + sign * prime;

    // 确保在范围内
    int min_size = static_cast<int>(stock_size * min_ratio);
    int max_size = static_cast<int>(stock_size * max_ratio);
    return std::clamp(result, min_size, max_size);
}

// 生成需求量
int InstanceGenerator::GenerateDemand(const GeneratorParams& params, bool is_peak) {
    if (is_peak) {
        // 热门子板: 需求量放大2-4倍
        std::uniform_int_distribution<int> mult_dist(2, 4);
        int mult = mult_dist(rng_);
        std::uniform_int_distribution<int> base_dist(params.min_demand, params.max_demand);
        return std::min(base_dist(rng_) * mult, 50);  // 上限50
    }

    if (params.demand_skew < 0.01) {
        // 均匀分布
        std::uniform_int_distribution<int> dist(params.min_demand, params.max_demand);
        return dist(rng_);
    }

    // 偏斜分布: 更多低需求, 少量高需求
    std::uniform_real_distribution<double> u(0.0, 1.0);
    double r = u(rng_);

    // 指数偏斜
    double skewed = std::pow(r, 1.0 + params.demand_skew * 2.0);
    int range = params.max_demand - params.min_demand;
    return params.min_demand + static_cast<int>(skewed * range);
}

// 验证并修正算例
bool InstanceGenerator::ValidateAndFix(Instance& inst, const GeneratorParams& params) {
    // 移除无效子板
    auto it = std::remove_if(inst.items.begin(), inst.items.end(),
        [&inst](const Item& item) {
            return item.width <= 0 || item.length <= 0 ||
                   item.demand <= 0 ||
                   item.width > inst.stock_width ||
                   item.length > inst.stock_length;
        });
    inst.items.erase(it, inst.items.end());

    // 确保至少3种子板
    while (static_cast<int>(inst.items.size()) < 3) {
        auto size = GenerateItemSize(params);
        Item item;
        item.id = static_cast<int>(inst.items.size());
        item.width = std::min(size.first, inst.stock_width);
        item.length = std::min(size.second, inst.stock_length);
        item.demand = GenerateDemand(params, false);
        inst.items.push_back(item);
    }

    // 重新编号
    for (int i = 0; i < static_cast<int>(inst.items.size()); i++) {
        inst.items[i].id = i;
    }

    return inst.IsValid();
}

// 导出CSV格式
bool InstanceGenerator::ExportCSV(const Instance& inst, const std::string& filepath) {
    std::filesystem::path path(filepath);
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }

    std::ofstream fout(filepath);
    if (!fout) {
        std::cerr << "Error: Cannot open file " << filepath << std::endl;
        return false;
    }

    // 文件头
    fout << "# 2D Cutting Stock Problem Instance\n";
    fout << "# Generated by CS-2D-Data\n";
    if (inst.known_optimal > 0) {
        fout << "# Known Optimal: " << inst.known_optimal << "\n";
    }
    fout << "#\n";

    // 母板尺寸
    fout << "stock_width,stock_length\n";
    fout << inst.stock_width << "," << inst.stock_length << "\n";
    fout << "#\n";

    // 子板数据
    fout << "id,width,length,demand\n";
    for (const auto& item : inst.items) {
        fout << item.id << ","
             << item.width << ","
             << item.length << ","
             << item.demand << "\n";
    }

    return true;
}

// 生成文件名
std::string InstanceGenerator::GenerateFilename(const GeneratorParams& params,
    const std::string& output_dir, double difficulty_score) {

    (void)params;  // 参数保留供将来扩展

    auto now = std::chrono::system_clock::now();
    auto time_t_val = std::chrono::system_clock::to_time_t(now);

    std::tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &time_t_val);
#else
    localtime_r(&time_t_val, &tm_buf);
#endif

    // 格式: inst_d{difficulty}_{YYYYMMDD}_{HHMMSS}.csv
    std::ostringstream filename;
    filename << output_dir << "/inst_d"
             << std::fixed << std::setprecision(2) << difficulty_score
             << "_" << std::put_time(&tm_buf, "%Y%m%d_%H%M%S")
             << ".csv";

    return filename.str();
}

// 批量生成
void InstanceGenerator::GenerateBatch(const GeneratorParams& params,
    int count, const std::string& output_dir) {

    std::filesystem::create_directories(output_dir);

    for (int i = 0; i < count; i++) {
        auto result = Generate(params);
        if (!result.success) {
            std::cerr << "警告: 生成第 " << i << " 个算例失败" << std::endl;
            continue;
        }

        std::string filepath = GenerateFilename(params, output_dir, result.estimate.score);
        ExportCSV(result.instance, filepath);

        std::cout << "已生成: " << filepath
                  << " (难度=" << std::fixed << std::setprecision(2)
                  << result.estimate.score << ", " << result.estimate.level_name << ")"
                  << std::endl;

        // 等待1秒确保时间戳唯一
        if (i < count - 1) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}
