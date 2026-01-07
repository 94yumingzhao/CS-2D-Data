// generator.h - 2D Cutting Stock Problem Instance Generator
// Project: CS-2D-Data
// 功能: 基于解耦参数生成不同特征的二维下料问题算例

#ifndef CS_2D_DATA_GENERATOR_H_
#define CS_2D_DATA_GENERATOR_H_

#include "instance.h"
#include "difficulty_estimator.h"
#include <string>
#include <random>

// 预设难度档位
enum class Preset {
    kEasy,      // 简单: 少种类, 小尺寸比, 高需求
    kMedium,    // 中等: 平衡配置
    kHard,      // 困难: 多种类, 大尺寸比, 低需求
    kExpert     // 专家: 极端配置, 质数偏移
};

// 生成参数结构体 (解耦设计, 各参数独立控制)
struct GeneratorParams {
    // 规模参数
    int num_types = 20;         // 子板种类数 (5-100)
    int stock_width = 200;      // 母板宽度 W
    int stock_length = 400;     // 母板长度 L

    // 尺寸参数 (相对于母板面积的比例)
    double min_size_ratio = 0.08;   // 子板最小面积比 (0.03-0.25)
    double max_size_ratio = 0.35;   // 子板最大面积比 (0.15-0.60)
    double size_cv = 0.30;          // 尺寸变异系数 (0.0-0.8)

    // 需求参数
    int min_demand = 1;         // 最小需求量 (1-10)
    int max_demand = 15;        // 最大需求量 (2-50)
    double demand_skew = 0.0;   // 需求偏斜度 (0=均匀, 1=高度偏斜)

    // 高级选项
    bool prime_offset = false;  // 质数偏移 (增加不可整除性)
    int num_clusters = 0;       // 尺寸聚类数 (0=不使用, 2-5=聚类生成)
    double peak_ratio = 0.0;    // 热门子板比例 (0=均匀, 0.1-0.3=部分高需求)

    // 生成策略
    int strategy = 1;           // 0=逆向(已知最优), 1=随机, 2=聚类, 3=残差

    // 随机种子
    int seed = 0;               // 0=使用时间戳

    // 从预设创建参数
    static GeneratorParams FromPreset(Preset preset);

    // 参数有效性检查
    bool Validate() const;

    // 打印参数摘要
    std::string GetSummary() const;
};

// 生成结果 (包含算例和预估难度)
struct GenerationResult {
    Instance instance;              // 生成的算例
    DifficultyEstimate estimate;    // 难度预估
    bool success;                   // 是否成功
    std::string error_message;      // 错误信息
};

// 算例生成器类
class InstanceGenerator {
public:
    InstanceGenerator();
    explicit InstanceGenerator(int seed);

    // 主生成函数 (使用参数结构体)
    GenerationResult Generate(const GeneratorParams& params);

    // 快捷生成 (使用预设)
    GenerationResult Generate(Preset preset);

    // 兼容旧接口: 单难度参数生成 (自动映射到参数)
    GenerationResult GenerateLegacy(double difficulty, int stock_width = 200,
                                    int stock_length = 400);

    // 导出为CSV格式 (2DPackLib兼容)
    static bool ExportCSV(const Instance& inst, const std::string& filepath);

    // 生成文件名 (带时间戳和参数标识)
    static std::string GenerateFilename(const GeneratorParams& params,
                                        const std::string& output_dir,
                                        double difficulty_score = 0.0);

    // 批量生成
    void GenerateBatch(const GeneratorParams& params, int count,
                       const std::string& output_dir);

    // 获取难度预估器 (用于校准)
    DifficultyEstimator& GetEstimator() { return estimator_; }
    const DifficultyEstimator& GetEstimator() const { return estimator_; }

private:
    std::mt19937 rng_;              // 随机数生成器
    DifficultyEstimator estimator_; // 难度预估器

    // 设置随机种子
    void SetSeed(int seed);

    // 策略0: 逆向生成 (构造完美填充, 已知最优解)
    Instance GenerateReverse(const GeneratorParams& params);

    // 策略1: 参数化随机生成
    Instance GenerateRandom(const GeneratorParams& params);

    // 策略2: 聚类生成 (尺寸分群)
    Instance GenerateCluster(const GeneratorParams& params);

    // 策略3: 残差生成 (难以完美填充)
    Instance GenerateResidual(const GeneratorParams& params);

    // 生成单个子板尺寸
    std::pair<int, int> GenerateItemSize(const GeneratorParams& params,
                                         int base_w = 0, int base_l = 0);

    // 生成"不友好"的尺寸 (质数偏移)
    int GeneratePrimeOffsetSize(int stock_size, double min_ratio, double max_ratio);

    // 生成需求量 (支持偏斜分布)
    int GenerateDemand(const GeneratorParams& params, bool is_peak = false);

    // 验证并修正算例
    bool ValidateAndFix(Instance& inst, const GeneratorParams& params);
};

#endif  // CS_2D_DATA_GENERATOR_H_
