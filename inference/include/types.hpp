/**
 * @file types.hpp
 * @brief 专家系统推理引擎的基础类型定义
 * 
 * 所有核心数据结构都在这里定义，供整个推理引擎使用。
 */

#pragma once

#include <string>
#include <vector>
#include <unordered_set>

namespace expert {

// ============================================================
// 类型别名
// ============================================================

using RuleId = std::string;      // 规则唯一标识符
using Confidence = double;       // 置信度值，范围 [0, 1]


// ============================================================
// Term（谓词中的项）
// ============================================================

struct Term {
    enum class Type {
        Constant,   ///< 常量，如 "3.7", "true"
        Variable,   ///< 变量，如 "?x", "?gpa"
        Entity      ///< 实体引用，如 "student_123"
    };

    Type type;
    std::string value;

    bool operator==(const Term& other) const {
        return type == other.type && value == other.value;
    }

    bool operator!=(const Term& other) const {
        return !(*this == other);
    }
};


// ============================================================
// Predicate（谓词）
// ============================================================

struct Predicate {
    std::string name;
    std::vector<Term> terms;

    bool operator==(const Predicate& other) const {
        return name == other.name && terms == other.terms;
    }

    bool operator!=(const Predicate& other) const {
        return !(*this == other);
    }
};


// ============================================================
// Fact（事实）
// ============================================================

struct Fact {
    Predicate predicate;
    Confidence confidence;

    Fact() : predicate(), confidence(1.0) {}
    Fact(const Predicate& pred, Confidence cf = 1.0)
        : predicate(pred), confidence(cf) {}

    bool operator==(const Fact& other) const {
        return predicate == other.predicate &&
               confidence == other.confidence;
    }

    bool operator!=(const Fact& other) const {
        return !(*this == other);
    }
};


// ============================================================
// ProductionRule（产生式规则）
// ============================================================

struct ProductionRule {
    RuleId id;
    std::vector<Predicate> antecedents;
    std::vector<Predicate> consequents;
    Confidence confidence = 1.0;
    int priority = 0;
    std::string explanation;

    bool operator==(const ProductionRule& other) const {
        return id == other.id;
    }

    bool operator!=(const ProductionRule& other) const {
        return !(*this == other);
    }
};

} // namespace expert


// ============================================================
// 哈希特化（用于 std::unordered_set）
// ============================================================

namespace std {

// 只有 Fact 需要放进 unordered_set（working_memory）
template<>
struct hash<expert::Fact> {
    size_t operator()(const expert::Fact& f) const {
        size_t h = hash<string>()(f.predicate.name);
        for (const auto& term : f.predicate.terms) {
            h ^= hash<string>()(term.value) + 0x9e3779b9 + (h << 6) + (h >> 2);
        }
        h ^= hash<double>()(f.confidence) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

} // namespace std