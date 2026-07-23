"""
规则批量加载器（Loader）

职责：
    将数据库中的规则批量加载出来，格式化为 C++ 推理引擎可直接消费的 JSON 格式。
    这是 knowledge 包的“出口”——数据从 SQLite 流向 Socket，最终进入 C++ 推理引擎。

与其它模块的关系：
    - rules.py:    管理规则的增删改查（生命周期）
    - loader.py:   批量加载规则供外部使用（读取 + 格式化）

    两者职责正交，互不调用。loader.py 直接访问数据库，绕过 rules.py 的 ORM 层，
    目的是保持数据格式纯净（无 is_meta、enabled 等内部字段）。

为什么只有规则相关？
    C++ 推理引擎只需要规则（IF-THEN 逻辑）。
    事实（facts）由 Python 在推理前通过 Socket 动态发送，会话（sessions）是历史记录，
    元规则（meta_rules）已合并到 rules 表，通过 is_meta 字段区分。
"""

import json
from typing import Dict, Any, List
from loguru import logger
from .db import get_conn


def load_all_rules() -> List[Dict[str, Any]]:
    """加载所有启用的普通规则（不包含元规则）"""
    with get_conn() as conn:
        rows = conn.execute("""
            SELECT id, antecedents, consequents, confidence, priority, explanation
            FROM rules
            WHERE enabled = 1 AND is_meta = 0
            ORDER BY priority DESC
        """).fetchall()

        rules = []
        for row in rows:
            try:
                rules.append({
                    "id": row["id"],
                    "antecedents": json.loads(row["antecedents"]),
                    "consequents": json.loads(row["consequents"]),
                    "confidence": row["confidence"],
                    "priority": row["priority"],
                    "explanation": row["explanation"] or "",
                })
            except json.JSONDecodeError as e:
                logger.error(f"规则 {row['id']} JSON 解析失败: {e}")
                continue

        logger.debug(f"加载了 {len(rules)} 条普通规则")
        return rules


def load_meta_rules() -> List[Dict[str, Any]]:
    """加载所有启用的元规则（冲突消解用）"""
    with get_conn() as conn:
        rows = conn.execute("""
            SELECT id, antecedents, consequents, confidence, priority, explanation
            FROM rules
            WHERE enabled = 1 AND is_meta = 1
            ORDER BY priority DESC
        """).fetchall()

        meta_rules = []
        for row in rows:
            try:
                meta_rules.append({
                    "id": row["id"],
                    "antecedents": json.loads(row["antecedents"]),
                    "consequents": json.loads(row["consequents"]),
                    "confidence": row["confidence"],
                    "priority": row["priority"],
                    "explanation": row["explanation"] or "",
                })
            except json.JSONDecodeError as e:
                logger.error(f"元规则 {row['id']} JSON 解析失败: {e}")
                continue

        logger.debug(f"加载了 {len(meta_rules)} 条元规则")
        return meta_rules


def load_rules_by_ids(rule_ids: List[str]) -> List[Dict[str, Any]]:
    """按 ID 列表加载规则"""
    if not rule_ids:
        return []

    placeholders = ",".join("?" * len(rule_ids))
    with get_conn() as conn:
        rows = conn.execute(f"""
            SELECT id, antecedents, consequents, confidence, priority, explanation
            FROM rules
            WHERE id IN ({placeholders}) AND enabled = 1
        """, rule_ids).fetchall()

        rules = []
        for row in rows:
            try:
                rules.append({
                    "id": row["id"],
                    "antecedents": json.loads(row["antecedents"]),
                    "consequents": json.loads(row["consequents"]),
                    "confidence": row["confidence"],
                    "priority": row["priority"],
                    "explanation": row["explanation"] or "",
                })
            except json.JSONDecodeError as e:
                logger.error(f"规则 {row['id']} JSON 解析失败: {e}")
                continue

        return rules