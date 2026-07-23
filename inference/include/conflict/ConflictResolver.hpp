/**
 * @file ConflictResolver.hpp
 * @brief 冲突消解器
 * 
 * 当多条规则同时匹配时，决定选哪条触发。
 * 
 * 决策分层：
 *   第0层：无匹配 → 封闭/开放世界假设
 *   第1层：按优先级过滤
 *   第2层：结论一致则叠加，冲突则进入裁决
 *   第3层：裁决（元规则 → 可能性）
 */

#pragma once

#include "../types.hpp"
#include <vector>
#include <algorithm>
#include <functional>
#include <unordered_set>
#include <optional>
#include <random>

namespace expert {

// 冲突消解的规则（第 0 层）
enum class WorldAssumption {
    Closed,   // 无动作，推理结束
    Open      // 询问用户
};

// ============================================================
// 冲突消解的规则表（第 0 层以上）
// ============================================================

enum class ConflictResolutionStrategy {
    Priority,      // 按照 KB 中的排序优先级
    Specificity,   // 前提越多越具体，越优先（用于备选策略）
    Recency,       // 最近使用频率越高，越优先（用于备选策略）
    MatchDegree,   // 匹配度越高，越优先（用于备选策略）
    MetaRule,      // 元规则裁决
    Possibility,   // 可能性裁决 ———— 推理方法
    Random,        // 随机（测试用）
};

// 消解结果，包含规则列表和状态信息
struct ResolutionResult {
    std::vector<ProductionRule> selected_rules;
    enum class Status {
        Success,        // 成功选出一条或多条规则
        NoMatch,        // 没有规则匹配
        NeedQuery,      // 开放世界下需要询问用户
        Conflict,       // 冲突无法解决
    } status = Status::Success;
};

// ============================================================
// vector<冲突规则> → ConflictResolver → vector<解决冲突后，保留下来的规则>
// ============================================================

class ConflictResolver {
public:
    // ---- 构造函数 ----
    ConflictResolver() = default;
    explicit ConflictResolver(ConflictResolutionStrategy strategy):m_strategy(strategy) {};

    
    // ---- 策略选择 ----
    // 选择具体策略
    void set_strategy(ConflictResolutionStrategy strategy){
        this -> m_strategy = strategy;
    };
    
    // 支持自定义策略，比如设计组合策略
    void set_override_strategy(
        std::function<std::vector<ProductionRule>(const std::vector<ProductionRule>&)> strategy
    ){
        this -> m_override_strategy = strategy;
    };
    

    // Priority, Recency 可以读取 KB、DB 来设计
    // Random 可以利用 <random> 库进行设计
    // Specificity，MatchDegree 可以利用冲突的规则自身特点，来进行设计
    // 只有 MetaRule, Possibility 要求设计ConflictResolver的 public 成员函数，支持自定义传入

    // MetaRule 相关函数
    // MetaRule example: IF condition THEN SELECT_RULE("R2")
    void set_meta_rules(const std::vector<ProductionRule>& meta_rules) {
        m_meta_rules = meta_rules;
    }
    
    // Possibility 相关函数 → 返回得分 [0, 1]
    // 支持不同推理方法的选择
    void set_possibility_function(
        std::function<double(
            const ProductionRule&,
            const std::unordered_set<Fact>&,
            const std::unordered_set<RuleId>&
        )> func
    ) {
        m_possibility_func = func;
    }
    
    // 第0层：无匹配时的行为
    void set_world_assumption(WorldAssumption assumption) {
        m_world_assumption = assumption;
    }

    // ---- 核心方法 ----
    
    // 从冲突集中选规则
    ResolutionResult resolve(
        const std::vector<ProductionRule>& conflict_set,
        const std::unordered_set<Fact>& working_memory,
        const std::unordered_set<RuleId>& fired_history
    );

    // 辅助方法：仅获取规则列表（兼容旧代码）
    std::vector<ProductionRule> resolve_rules(
        const std::vector<ProductionRule>& conflict_set,
        const std::unordered_set<Fact>& working_memory,
        const std::unordered_set<RuleId>& fired_history
    );

    // 用于调试、日志、输出等的辅助工具函数
    std::string strategy_name() const;
    bool has_meta_rules() const { return !m_meta_rules.empty(); }
    bool has_possibility_function() const { return m_possibility_func != nullptr; }


    
private:

    // ---- 数据成员 ----
    // 默认策略为按优先级排序
    ConflictResolutionStrategy m_strategy = ConflictResolutionStrategy::Priority;
    std::function<std::vector<ProductionRule>(const std::vector<ProductionRule>&)> m_override_strategy;
    
    // 一系列 metarules
    std::vector<ProductionRule> m_meta_rules;

    // 可能性函数
    std::function<double(
        const ProductionRule&,
        const std::unordered_set<Fact>&,
        const std::unordered_set<RuleId>&
    )> m_possibility_func;
    
    WorldAssumption m_world_assumption = WorldAssumption::Closed;
    std::mt19937 m_rng{std::random_device{}()};


    // ---- 内部实现 ----
    // 第1层
    std::vector<ProductionRule> filter_by_priority(const std::vector<ProductionRule>& rules);
    
    // 第2层
    // 判断冲突规则的结论是否一致，可否全部保留、用 CF 组合叠加
    bool conclusions_consistent(const std::vector<ProductionRule>& rules) const;
    
    // 当判断冲突规则的结论不一致时：
    // 第3层-A （利用MetaRules）
    std::optional<ProductionRule> resolve_by_meta_rules(
        const std::vector<ProductionRule>& conflict_set,
        const std::unordered_set<Fact>& working_memory
    );
    
    // 第3层-B （利用Possibilities）
    std::vector<ProductionRule> resolve_by_possibility(
        const std::vector<ProductionRule>& conflict_set,
        const std::unordered_set<Fact>& working_memory,
        const std::unordered_set<RuleId>& fired_history
    );
    
    // 返回被选择保留的规则ID
    std::optional<std::string> extract_selected_rule_id(const ProductionRule& meta_rule) const;


    // ---- 以下函数被set_strategy、set_override_strategy调用 ----
    // 按照 KB 中的优先级
    static std::vector<ProductionRule> by_priority(const std::vector<ProductionRule>& rules);
    // 前提越多越具体，越优先
    static std::vector<ProductionRule> by_specificity(const std::vector<ProductionRule>& rules);
    // 最近使用频率越高，越优先
    std::vector<ProductionRule> by_recency(
        const std::vector<ProductionRule>& rules,
        const std::unordered_set<RuleId>& fired_history
    );
    // 匹配度越高，越优先 
    static std::vector<ProductionRule> by_match_degree(const std::vector<ProductionRule>& rules);
    // 随机排序
    std::vector<ProductionRule> by_random(const std::vector<ProductionRule>& rules);



    // Specificity，MatchDegree 可以利用冲突的规则自身特点，来进行设计
    // 对应 ConflictResolver的 private 成员函数
    static int count_antecedents(const ProductionRule& rule);   //被 by_specificity 调用
    static double compute_match_degree(     //被 by_match_degree 调用
        const ProductionRule& rule,
        const std::unordered_set<Fact>& working_memory
    );
};

} // namespace expert