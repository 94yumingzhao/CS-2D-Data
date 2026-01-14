// ============================================================================
// 工程标准 (Engineering Standards)
// - 坐标系: 左下角为原点
// - 宽度(Width): 上下方向 (Y轴)
// - 长度(Length): 左右方向 (X轴)
// - 约束: 长度 >= 宽度
// ============================================================================

// difficulty_estimator.cpp - 求解难度预估与校准实现

#include "difficulty_estimator.h"
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>

DifficultyEstimator::DifficultyEstimator()
    : w_size_ratio_(0.35),    // 尺寸比是最关键因素
      w_num_types_(0.25),     // 种类数影响组合复杂度
      w_demand_(0.20),        // 低需求增加整数化难度
      w_cv_(0.15),            // 异质性影响装填效率
      w_width_div_(0.05) {    // 宽度多样性影响条带类型数
}

DifficultyEstimate DifficultyEstimator::Estimate(const Instance& inst) const {
    DifficultyEstimate result;

    // 提取算例特征
    double size_ratio = inst.AvgSizeRatio();
    int num_types = inst.NumTypes();
    double avg_demand = inst.AvgDemand();
    double size_cv = inst.SizeCV();
    double width_div = inst.WidthDiversity();

    // 计算各因素的归一化贡献
    // 尺寸比: 以20%为基准, 越大越难
    result.size_contribution = size_ratio / 0.20;

    // 种类数: 以30种为基准
    result.types_contribution = static_cast<double>(num_types) / 30.0;

    // 需求量: 需求越低越难, 以5为基准
    result.demand_contribution = (avg_demand > 0) ? 5.0 / avg_demand : 2.0;

    // 变异系数: 以0.30为基准
    result.cv_contribution = size_cv / 0.30;

    // 宽度多样性: 直接使用
    result.width_div_contribution = width_div;

    // 加权求和得到综合评分
    result.score = w_size_ratio_ * result.size_contribution
                 + w_num_types_ * result.types_contribution
                 + w_demand_ * result.demand_contribution
                 + w_cv_ * result.cv_contribution
                 + w_width_div_ * result.width_div_contribution;

    // 映射到难度等级
    result.level = ScoreToLevel(result.score);
    result.level_name = LevelToString(result.level);

    // 预估Gap和节点数
    result.estimated_gap = EstimateGapString(result.score);
    result.estimated_nodes = EstimateNodes(result.score);

    // 利用率下界 = 总需求面积 / (理论最少板数 * 板面积)
    double lb = inst.TheoreticalLowerBound();
    int lb_plates = static_cast<int>(std::ceil(lb));
    if (lb_plates > 0 && inst.StockArea() > 0) {
        result.utilization_lb = static_cast<double>(inst.TotalDemandArea())
                              / (lb_plates * inst.StockArea());
    } else {
        result.utilization_lb = 0.0;
    }

    return result;
}

double DifficultyEstimator::ComputeScore(double size_ratio, int num_types,
                                          double avg_demand, double size_cv,
                                          double width_diversity) const {
    double f_size = size_ratio / 0.20;
    double f_types = static_cast<double>(num_types) / 30.0;
    double f_demand = (avg_demand > 0) ? 5.0 / avg_demand : 2.0;
    double f_cv = size_cv / 0.30;
    double f_width = width_diversity;

    return w_size_ratio_ * f_size
         + w_num_types_ * f_types
         + w_demand_ * f_demand
         + w_cv_ * f_cv
         + w_width_div_ * f_width;
}

DifficultyLevel DifficultyEstimator::ScoreToLevel(double score) const {
    if (score < 0.5) return DifficultyLevel::kTrivial;
    if (score < 0.8) return DifficultyLevel::kEasy;
    if (score < 1.2) return DifficultyLevel::kMedium;
    if (score < 1.6) return DifficultyLevel::kHard;
    if (score < 2.0) return DifficultyLevel::kVeryHard;
    return DifficultyLevel::kExpert;
}

std::string DifficultyEstimator::LevelToString(DifficultyLevel level) const {
    switch (level) {
        case DifficultyLevel::kTrivial:  return "极易";
        case DifficultyLevel::kEasy:     return "简单";
        case DifficultyLevel::kMedium:   return "中等";
        case DifficultyLevel::kHard:     return "困难";
        case DifficultyLevel::kVeryHard: return "很难";
        case DifficultyLevel::kExpert:   return "极难";
        default: return "未知";
    }
}

std::string DifficultyEstimator::EstimateGapString(double score) const {
    // 根据难度评分预估Gap范围
    if (score < 0.5) return "<1%";
    if (score < 0.8) return "1-3%";
    if (score < 1.2) return "3-8%";
    if (score < 1.6) return "8-15%";
    if (score < 2.0) return "15-25%";
    return ">25%";
}

int DifficultyEstimator::EstimateNodes(double score) const {
    // 根据难度评分预估分支节点数
    if (score < 0.5) return 10;
    if (score < 0.8) return 50;
    if (score < 1.2) return 300;
    if (score < 1.6) return 1000;
    if (score < 2.0) return 5000;
    return 10000;
}

void DifficultyEstimator::AddCalibrationPoint(const CalibrationPoint& point) {
    calibration_data_.push_back(point);
}

double DifficultyEstimator::Calibrate() {
    if (calibration_data_.size() < 5) {
        return 0.0;  // 数据点太少无法校准
    }

    // 计算校准前的RMSE
    double rmse_before = GetPredictionRMSE();

    // 简单的网格搜索优化权重
    // 约束: 所有权重之和为1, 每个权重在0.05-0.50之间
    double best_rmse = rmse_before;
    double best_w[5] = {w_size_ratio_, w_num_types_, w_demand_, w_cv_, w_width_div_};

    // 保存原始权重
    double orig_w[5] = {w_size_ratio_, w_num_types_, w_demand_, w_cv_, w_width_div_};

    // 网格搜索 (步长0.05)
    for (double w1 = 0.20; w1 <= 0.50; w1 += 0.05) {      // size_ratio
        for (double w2 = 0.15; w2 <= 0.40; w2 += 0.05) {  // num_types
            for (double w3 = 0.10; w3 <= 0.30; w3 += 0.05) {  // demand
                double w4 = 1.0 - w1 - w2 - w3 - 0.05;  // cv (留0.05给width_div)
                if (w4 < 0.05 || w4 > 0.30) continue;
                double w5 = 1.0 - w1 - w2 - w3 - w4;    // width_div

                // 临时设置权重
                w_size_ratio_ = w1;
                w_num_types_ = w2;
                w_demand_ = w3;
                w_cv_ = w4;
                w_width_div_ = w5;

                double rmse = GetPredictionRMSE();
                if (rmse < best_rmse) {
                    best_rmse = rmse;
                    best_w[0] = w1;
                    best_w[1] = w2;
                    best_w[2] = w3;
                    best_w[3] = w4;
                    best_w[4] = w5;
                }
            }
        }
    }

    // 应用最优权重
    w_size_ratio_ = best_w[0];
    w_num_types_ = best_w[1];
    w_demand_ = best_w[2];
    w_cv_ = best_w[3];
    w_width_div_ = best_w[4];

    return rmse_before - best_rmse;  // 返回RMSE改进量
}

double DifficultyEstimator::GetPredictionRMSE() const {
    if (calibration_data_.empty()) return 0.0;

    double sum_sq_error = 0.0;
    for (const auto& point : calibration_data_) {
        // 计算预测的difficulty score
        double predicted_score = ComputeScore(
            point.avg_size_ratio, point.num_types,
            point.avg_demand, point.size_cv, point.width_diversity);

        // 将实际gap映射到score (简化: gap*10 约等于 score)
        double actual_score = point.actual_gap * 10.0;

        double error = predicted_score - actual_score;
        sum_sq_error += error * error;
    }

    return std::sqrt(sum_sq_error / calibration_data_.size());
}

int DifficultyEstimator::GetCalibrationPointCount() const {
    return static_cast<int>(calibration_data_.size());
}

void DifficultyEstimator::GetWeights(double& w_size, double& w_types,
                                      double& w_demand, double& w_cv,
                                      double& w_width_div) const {
    w_size = w_size_ratio_;
    w_types = w_num_types_;
    w_demand = w_demand_;
    w_cv = w_cv_;
    w_width_div = w_width_div_;
}

void DifficultyEstimator::SetWeights(double w_size, double w_types,
                                      double w_demand, double w_cv,
                                      double w_width_div) {
    w_size_ratio_ = w_size;
    w_num_types_ = w_types;
    w_demand_ = w_demand;
    w_cv_ = w_cv;
    w_width_div_ = w_width_div;
}

bool DifficultyEstimator::SaveCalibration(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) return false;

    // 保存权重
    file << "# CS-2D-Data Difficulty Estimator Calibration\n";
    file << "w_size_ratio=" << w_size_ratio_ << "\n";
    file << "w_num_types=" << w_num_types_ << "\n";
    file << "w_demand=" << w_demand_ << "\n";
    file << "w_cv=" << w_cv_ << "\n";
    file << "w_width_div=" << w_width_div_ << "\n";

    return true;
}

bool DifficultyEstimator::LoadCalibration(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        auto pos = line.find('=');
        if (pos == std::string::npos) continue;

        std::string key = line.substr(0, pos);
        double value = std::stod(line.substr(pos + 1));

        if (key == "w_size_ratio") w_size_ratio_ = value;
        else if (key == "w_num_types") w_num_types_ = value;
        else if (key == "w_demand") w_demand_ = value;
        else if (key == "w_cv") w_cv_ = value;
        else if (key == "w_width_div") w_width_div_ = value;
    }

    return true;
}
