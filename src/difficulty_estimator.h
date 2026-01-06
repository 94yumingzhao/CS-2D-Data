// difficulty_estimator.h - 求解难度预估与校准
// 根据算例特征预估Branch-and-Price求解难度

#ifndef CS_2D_DATA_DIFFICULTY_ESTIMATOR_H_
#define CS_2D_DATA_DIFFICULTY_ESTIMATOR_H_

#include "instance.h"
#include <string>
#include <vector>

// 难度等级枚举
enum class DifficultyLevel {
    kTrivial,   // 极易: score < 0.5
    kEasy,      // 简单: score 0.5-0.8
    kMedium,    // 中等: score 0.8-1.2
    kHard,      // 困难: score 1.2-1.6
    kVeryHard,  // 很难: score 1.6-2.0
    kExpert     // 极难: score > 2.0
};

// 难度预估结果
struct DifficultyEstimate {
    double score;                // 综合难度评分 (0.0 - 2.0+)
    DifficultyLevel level;       // 难度等级
    std::string level_name;      // 等级名称 (中文)
    std::string estimated_gap;   // 预估Gap范围 (如 "5-10%")
    int estimated_nodes;         // 预估分支节点数
    double utilization_lb;       // 利用率下界

    // 各因素贡献值 (用于分析)
    double size_contribution;
    double types_contribution;
    double demand_contribution;
    double cv_contribution;
    double width_div_contribution;
};

// 校准数据点 (来自实际求解结果)
struct CalibrationPoint {
    // 算例特征
    int num_types;
    double avg_size_ratio;
    double avg_demand;
    double size_cv;
    double width_diversity;

    // 求解结果
    double actual_gap;      // 实际Gap
    int actual_nodes;       // 实际节点数
    double solve_time;      // 求解时间(秒)
    bool timed_out;         // 是否超时
};

// 难度预估器
class DifficultyEstimator {
public:
    DifficultyEstimator();

    // 预估算例难度
    DifficultyEstimate Estimate(const Instance& inst) const;

    // 添加校准数据点
    void AddCalibrationPoint(const CalibrationPoint& point);

    // 执行校准优化权重, 返回RMSE改进量
    double Calibrate();

    // 保存/加载校准参数
    bool SaveCalibration(const std::string& filepath) const;
    bool LoadCalibration(const std::string& filepath);

    // 获取/设置权重
    void GetWeights(double& w_size, double& w_types, double& w_demand,
                    double& w_cv, double& w_width_div) const;
    void SetWeights(double w_size, double w_types, double w_demand,
                    double w_cv, double w_width_div);

    // 校准统计
    int GetCalibrationPointCount() const;
    double GetPredictionRMSE() const;

private:
    // 难度因素权重 (基于分析报告)
    double w_size_ratio_;   // 尺寸比权重 (默认0.35)
    double w_num_types_;    // 种类数权重 (默认0.25)
    double w_demand_;       // 需求量权重 (默认0.20)
    double w_cv_;           // 变异系数权重 (默认0.15)
    double w_width_div_;    // 宽度多样性权重 (默认0.05)

    // 校准数据
    std::vector<CalibrationPoint> calibration_data_;

    // 内部方法
    double ComputeScore(double size_ratio, int num_types, double avg_demand,
                        double size_cv, double width_diversity) const;
    DifficultyLevel ScoreToLevel(double score) const;
    std::string LevelToString(DifficultyLevel level) const;
    std::string EstimateGapString(double score) const;
    int EstimateNodes(double score) const;
};

#endif  // CS_2D_DATA_DIFFICULTY_ESTIMATOR_H_
