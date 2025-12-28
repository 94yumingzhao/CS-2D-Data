// generator.h - 2D切割问题算例生成器
// 项目: CS-2D-Data
// 功能: 根据难度参数生成不同复杂度的二维下料问题算例

#ifndef CS_2D_DATA_GENERATOR_H_
#define CS_2D_DATA_GENERATOR_H_

#include <vector>
#include <string>
#include <random>
#include <cmath>

// 子板类型
struct ItemType {
    int id;
    int width;      // 水平方向尺寸
    int height;     // 垂直方向尺寸
    int demand;     // 需求量
};

// 算例数据
struct Instance {
    int stock_width;                // 母板宽度
    int stock_height;               // 母板高度
    std::vector<ItemType> items;    // 子板列表
    int known_optimal;              // 已知最优解 (-1表示未知)
    double difficulty;              // 生成时使用的难度参数
};

// 难度参数派生器
// 根据单一难度值 (0-1) 自动计算各项生成参数
struct DifficultyParams {
    double difficulty;  // 主难度系数 0.0(易) - 1.0(难)

    // 构造函数: 输入难度值, 自动计算派生参数
    explicit DifficultyParams(double diff) : difficulty(std::clamp(diff, 0.0, 1.0)) {}

    // 子板类型数量: 难度越高, 类型越多
    // 范围: 5 (难度0) 到 40 (难度1)
    int NumItemTypes() const {
        return 5 + static_cast<int>(difficulty * 35);
    }

    // 尺寸相似度: 难度越高, 尺寸越相似
    // 范围: 0.0 (完全随机) 到 0.9 (非常相似)
    double SizeSimilarity() const {
        return difficulty * 0.9;
    }

    // 最大需求量: 难度越高, 需求量越小
    // 范围: 30 (难度0) 到 3 (难度1)
    int MaxDemand() const {
        return std::max(3, 30 - static_cast<int>(difficulty * 27));
    }

    // 最小需求量
    int MinDemand() const {
        return std::max(1, MaxDemand() / 5);
    }

    // 子板尺寸下限 (相对于母板的比例)
    // 难度越高, 最小尺寸越大 (减少简单填充)
    double MinSizeRatio() const {
        return 0.08 + difficulty * 0.07;  // 0.08 - 0.15
    }

    // 子板尺寸上限 (相对于母板的比例)
    double MaxSizeRatio() const {
        return 0.35 + difficulty * 0.15;  // 0.35 - 0.50
    }

    // 是否使用质数偏移 (增加不可整除性)
    bool UsePrimeOffset() const {
        return difficulty > 0.5;
    }

    // 生成策略选择
    // 0: 逆向生成 (已知最优解)
    // 1: 参数化随机
    // 2: 残差算例
    int Strategy() const {
        if (difficulty < 0.3) return 0;
        if (difficulty < 0.8) return 1;
        return 2;
    }
};

// 算例生成器类
class InstanceGenerator {
public:
    // 构造函数: 可指定随机种子, 默认使用时间戳
    explicit InstanceGenerator(unsigned int seed = 0);

    // 主生成函数: 根据难度生成算例
    // difficulty: 难度系数 0.0-1.0
    // stock_width, stock_height: 母板尺寸
    Instance Generate(double difficulty, int stock_width = 1000, int stock_height = 500);

    // 导出为 2DPackLib CSV 格式
    bool ExportCSV(const Instance& inst, const std::string& filepath);

    // 生成带时间戳的文件名
    // 格式: inst_{YYYYMMDD}_{HHMMSS}_d{difficulty}.csv
    // 时间戳在前确保按文件名排序即为时间顺序
    std::string GenerateFilename(double difficulty, const std::string& output_dir);

    // 批量生成算例
    // count: 生成数量
    // difficulty: 难度系数
    // output_dir: 输出目录
    void GenerateBatch(int count, double difficulty, const std::string& output_dir);

private:
    std::mt19937 rng_;  // 随机数生成器

    // 策略0: 逆向生成 (已知最优解)
    // 先构造完美填充的切割方案, 再统计子板需求
    Instance GenerateReverse(const DifficultyParams& params, int W, int H);

    // 策略1: 参数化随机生成
    // 根据难度参数控制尺寸分布和需求量
    Instance GenerateRandom(const DifficultyParams& params, int W, int H);

    // 策略2: 残差算例生成
    // 生成难以完美填充的子板组合
    Instance GenerateResidual(const DifficultyParams& params, int W, int H);

    // 生成单个子板尺寸
    // base_w, base_h: 基准尺寸 (用于相似度控制)
    // similarity: 相似度 0-1
    std::pair<int, int> GenerateItemSize(int W, int H,
        const DifficultyParams& params,
        int base_w = 0, int base_h = 0);

    // 生成"不友好"的尺寸 (难以整除母板)
    int GenerateDifficultSize(int stock_size, double difficulty);

    // 验证算例有效性 (所有子板都能放入母板)
    bool ValidateInstance(const Instance& inst);
};

#endif  // CS_2D_DATA_GENERATOR_H_
