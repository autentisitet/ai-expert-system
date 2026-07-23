/**
 * @file ConflictResolver.cpp
 * @brief 冲突消解器实现
 * 
 * 实现四层冲突消解策略：
 *   第0层：无匹配 → 封闭/开放世界假设
 *   第1层：按优先级过滤
 *   第2层：结论一致则叠加，冲突则进入裁决
 *   第3层：裁决（元规则 → 可能性 → 兜底）
 */

#include "conflict/ConflictResolver.hpp"
#include <algorithm>
#include <random>
#include <cmath>

namespace expert {

// ============================================================
// 核心方法
// ============================================================

ResolutionResult ConflictResolver::resolve(
    const std::vector<ProductionRule>& conflict_set,
    const std::unordered_set<Fact>& working_memory,
    const std::unordered_set<RuleId>& fired_history
) {
    ResolutionResult result;

    // ---- 第0层：无规则匹配 ----
    if (conflict_set.empty()) {
        if (m_world_assumption == WorldAssumption::Closed) {
            result.status = ResolutionResult::Status::NoMatch;
            return result;  // 封闭世界：无动作，推理结束
        } else {
            result.status = ResolutionResult::Status::NeedQuery;
            return result;  // 开放世界：返回特殊状态，上层触发询问
        }
    }

    // ---- 自定义策略优先 ----
    if (m_override_strategy) {
        result.selected_rules = m_override_strategy(conflict_set);
        result.status = ResolutionResult::Status::Success;
        return result;
    }

    // ---- 根据策略分发 ----
    switch (m_strategy) {
        case ConflictResolutionStrategy::Priority:
            result.selected_rules = by_priority(conflict_set);
            break;

        case ConflictResolutionStrategy::Specificity:
            result.selected_rules = by_specificity(conflict_set);
            break;

        case ConflictResolutionStrategy::Recency:
            result.selected_rules = by_recency(conflict_set, fired_history);
            break;

        case ConflictResolutionStrategy::MatchDegree:
            result.selected_rules = by_match_degree(conflict_set);
            break;

        case ConflictResolutionStrategy::Random:
            result.selected_rules = by_random(conflict_set);
            break;

        case ConflictResolutionStrategy::MetaRule: {
            auto selected_rule = resolve_by_meta_rules(conflict_set, working_memory);
            if (selected_rule.has_value()) {
                result.selected_rules = std::vector<ProductionRule>{selected_rule.value()};
            } else {
                // 降级到优先级
                result.selected_rules = by_priority(conflict_set);
            }
            break;
        }

        case ConflictResolutionStrategy::Possibility: {
            auto selected_rules = resolve_by_possibility(conflict_set, working_memory, fired_history);
            if (!selected_rules.empty()) {
                result.selected_rules = selected_rules;
            } else {
                // 降级到优先级
                result.selected_rules = by_priority(conflict_set);
            }
            break;
        }

        default:
            result.selected_rules = by_priority(conflict_set);
            break;
    }

    // 检查是否成功选出了规则，如果返回空但不算失败（比如不冲突时返回空）
    if (!result.selected_rules.empty()) {
        result.status = ResolutionResult::Status::Success;
    } else {
        // 如果没有选择任何规则，检查是否没有匹配
        // 注意：某些策略可能返回空，这里需要区分
        result.status = ResolutionResult::Status::NoMatch;
    }

    return result;
}

std::vector<ProductionRule> ConflictResolver::resolve_rules(
    const std::vector<ProductionRule>& conflict_set,
    const std::unordered_set<Fact>& working_memory,
    const std::unordered_set<RuleId>& fired_history
) {
    auto result = resolve(conflict_set, working_memory, fired_history);
    return result.selected_rules;
}

// ============================================================
// 第1层：按优先级过滤
// ============================================================

std::vector<ProductionRule> ConflictResolver::filter_by_priority(
    const std::vector<ProductionRule>& rules
) {
    if (rules.empty()) {
        return {};
    }

    // 找最高优先级
    int max_priority = rules[0].priority;
    for (const auto& rule : rules) {
        if (rule.priority > max_priority) {
            max_priority = rule.priority;
        }
    }

    // 收集所有最高优先级的规则
    std::vector<ProductionRule> result;
    result.reserve(rules.size());
    for (const auto& rule : rules) {
        if (rule.priority == max_priority) {
            result.push_back(rule);
        }
    }
    return result;
}

// ============================================================
// 第2层：结论一致性检查
// ============================================================

bool ConflictResolver::conclusions_consistent(
    const std::vector<ProductionRule>& rules
) const {
    if (rules.empty()) {
        return true;
    }

    // 用结论的签名（谓词名+参数）来判断是否一致
    // 如果所有规则的结论签名都相同，则认为一致
    std::unordered_set<std::string> signatures;

    for (const auto& rule : rules) {
        for (const auto& pred : rule.consequents) {
            std::string sig = pred.name + "(";
            for (const auto& term : pred.terms) {
                sig += term.value + ",";
            }
            sig += ")";
            signatures.insert(sig);
        }
    }

    // 只有一个唯一签名 → 一致
    // 多个不同签名 → 冲突
    return signatures.size() <= 1;
}

// ============================================================
// 第3层-A：元规则裁决
// ============================================================

std::optional<ProductionRule> ConflictResolver::resolve_by_meta_rules(
    const std::vector<ProductionRule>& conflict_set,
    const std::unordered_set<Fact>& working_memory
) {
    if (conflict_set.empty()) {
        return std::nullopt;
    }

    // 检查每条元规则
    for (const auto& meta_rule : m_meta_rules) {
        // 检查元规则的所有前提是否都在工作内存中
        bool all_match = true;
        for (const auto& ant : meta_rule.antecedents) {
            bool found = false;
            for (const auto& fact : working_memory) {
                if (ant.name == fact.predicate.name) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                all_match = false;
                break;
            }
        }

        if (all_match) {
            // 元规则触发，提取 SELECT_RULE 结论
            auto selected_id = extract_selected_rule_id(meta_rule);
            if (selected_id.has_value()) {
                // 在冲突集中查找匹配的规则
                for (const auto& rule : conflict_set) {
                    if (rule.id == selected_id.value()) {
                        return rule;
                    }
                }
            }
        }
    }

    return std::nullopt;
}

// ============================================================
// 第3层-B：可能性裁决
// ============================================================

std::vector<ProductionRule> ConflictResolver::resolve_by_possibility(
    const std::vector<ProductionRule>& conflict_set,
    const std::unordered_set<Fact>& working_memory,
    const std::unordered_set<RuleId>& fired_history
) {
    if (conflict_set.empty()) {
        return {};
    }

    // 如果没有可能性函数，无法裁决
    if (!m_possibility_func) {
        return {};
    }

    // 计算每条规则的得分
    struct ScoredRule {
        ProductionRule rule;
        double score;
    };

    std::vector<ScoredRule> scored;
    scored.reserve(conflict_set.size());

    for (const auto& rule : conflict_set) {
        double score = m_possibility_func(rule, working_memory, fired_history);
        scored.push_back({rule, score});
    }

    // 按得分降序排序
    std::sort(scored.begin(), scored.end(),
        [](const ScoredRule& a, const ScoredRule& b) {
            return a.score > b.score;
        });

    // 返回得分最高的规则（如果得分接近，可以返回多条）
    double top_score = scored.front().score;
    const double threshold = top_score * 0.9;  // 90% 阈值

    std::vector<ProductionRule> result;
    for (const auto& sr : scored) {
        if (sr.score >= threshold) {
            result.push_back(sr.rule);
        } else {
            break;
        }
    }

    return result;
}

// ============================================================
// 辅助：从元规则提取 SELECT_RULE 的规则 ID
// ============================================================

std::optional<std::string> ConflictResolver::extract_selected_rule_id(
    const ProductionRule& meta_rule
) const {
    for (const auto& pred : meta_rule.consequents) {
        if (pred.name == "SELECT_RULE" && !pred.terms.empty()) {
            return pred.terms[0].value;
        }
        if (pred.name == "select_rule" && !pred.terms.empty()) {
            return pred.terms[0].value;
        }
    }
    return std::nullopt;
}

// ============================================================
// 内置策略函数
// ============================================================

std::vector<ProductionRule> ConflictResolver::by_priority(
    const std::vector<ProductionRule>& rules
) {
    if (rules.empty()) {
        return {};
    }

    auto sorted = rules;
    std::sort(sorted.begin(), sorted.end(),
        [](const ProductionRule& a, const ProductionRule& b) {
            return a.priority > b.priority;
        });

    // 返回最高优先级的规则（可能有多条）
    int max_priority = sorted.front().priority;
    std::vector<ProductionRule> result;
    for (const auto& rule : sorted) {
        if (rule.priority == max_priority) {
            result.push_back(rule);
        } else {
            break;
        }
    }
    return result;
}

std::vector<ProductionRule> ConflictResolver::by_specificity(
    const std::vector<ProductionRule>& rules
) {
    if (rules.empty()) {
        return {};
    }

    auto sorted = rules;
    std::sort(sorted.begin(), sorted.end(),
        [](const ProductionRule& a, const ProductionRule& b) {
            // 前提越多越具体
            return count_antecedents(a) > count_antecedents(b);
        });

    // 返回最具体的规则
    int max_count = count_antecedents(sorted.front());
    std::vector<ProductionRule> result;
    for (const auto& rule : sorted) {
        if (count_antecedents(rule) == max_count) {
            result.push_back(rule);
        } else {
            break;
        }
    }
    return result;
}

std::vector<ProductionRule> ConflictResolver::by_recency(
    const std::vector<ProductionRule>& rules,
    const std::unordered_set<RuleId>& fired_history
) {
    if (rules.empty()) {
        return {};
    }

    auto sorted = rules;
    std::sort(sorted.begin(), sorted.end(),
        [&fired_history](const ProductionRule& a, const ProductionRule& b) {
            bool a_in_history = fired_history.find(a.id) != fired_history.end();
            bool b_in_history = fired_history.find(b.id) != fired_history.end();
            // 不在历史中的优先（防止规则饥饿）
            if (a_in_history != b_in_history) {
                return !a_in_history;
            }
            return a.id < b.id;
        });

    // 返回最高分（即最近最少使用的）
    bool first_in_history = fired_history.find(sorted.front().id) != fired_history.end();
    std::vector<ProductionRule> result;
    for (const auto& rule : sorted) {
        bool in_history = fired_history.find(rule.id) != fired_history.end();
        if (in_history == first_in_history) {
            result.push_back(rule);
        } else {
            break;
        }
    }
    return result;
}

std::vector<ProductionRule> ConflictResolver::by_match_degree(
    const std::vector<ProductionRule>& rules
) {
    if (rules.empty()) {
        return {};
    }

    // 注意：compute_match_degree 需要 working_memory
    // 但这里没有传入，所以用默认实现
    // 实际应该在 resolve() 里调用时传入 working_memory
    auto sorted = rules;
    std::sort(sorted.begin(), sorted.end(),
        [](const ProductionRule& a, const ProductionRule& b) {
            // 这里只能用规则自身的属性计算匹配度
            // 真正的匹配度需要 working_memory，所以在 resolve() 里单独处理
            // 这个函数仅作为备选
            return a.antecedents.size() > b.antecedents.size();
        });

    // 返回匹配度最高的
    return {sorted.front()};
}

std::vector<ProductionRule> ConflictResolver::by_random(
    const std::vector<ProductionRule>& rules
) {
    if (rules.empty()) {
        return {};
    }

    std::uniform_int_distribution<size_t> dist(0, rules.size() - 1);
    return {rules[dist(m_rng)]};
}

// ============================================================
// 辅助工具函数
// ============================================================

int ConflictResolver::count_antecedents(const ProductionRule& rule) {
    return static_cast<int>(rule.antecedents.size());
}

double ConflictResolver::compute_match_degree(
    const ProductionRule& rule,
    const std::unordered_set<Fact>& working_memory
) {
    if (rule.antecedents.empty()) {
        return 1.0;  // 无前提的规则永远匹配
    }
    if (working_memory.empty()) {
        return 0.0;
    }

    int matched = 0;
    for (const auto& ant : rule.antecedents) {
        for (const auto& fact : working_memory) {
            if (ant.name == fact.predicate.name) {
                matched++;
                break;
            }
        }
    }
    return static_cast<double>(matched) / rule.antecedents.size();
}

std::string ConflictResolver::strategy_name() const {
    switch (m_strategy) {
        case ConflictResolutionStrategy::Priority:   return "Priority";
        case ConflictResolutionStrategy::Specificity: return "Specificity";
        case ConflictResolutionStrategy::Recency:    return "Recency";
        case ConflictResolutionStrategy::MatchDegree: return "MatchDegree";
        case ConflictResolutionStrategy::MetaRule:   return "MetaRule";
        case ConflictResolutionStrategy::Possibility: return "Possibility";
        case ConflictResolutionStrategy::Random:     return "Random";
        default: return "Unknown";
    }
}

} // namespace expert