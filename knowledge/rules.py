import json
from typing import List, Dict, Any, Optional
from loguru import logger
from .db import get_conn
from enum import Enum


class RuleStatus(Enum):
    """规则状态枚举"""
    ENABLED = 1
    DISABLED = 0


class RuleType(Enum):
    """规则类型枚举"""
    NORMAL = 0   # 普通规则
    META = 1     # 元规则


def _safe_json_loads(data: str) -> Any:
    """安全解析 JSON，失败时返回空列表"""
    if not data:
        return []
    try:
        return json.loads(data)
    except json.JSONDecodeError as e:
        logger.error(f"JSON 解析失败: {e}, 数据: {data[:100]}...")
        return []


# ============================================================
# 规则 CRUD
# ============================================================

def _build_where_clause(
    status: Optional[RuleStatus] = None,
    rule_type: Optional[RuleType] = None
) -> str:
    """
    构建 WHERE 子句
    status: None=不限制, ENABLED=启用, DISABLED=禁用
    rule_type: None=不限制, NORMAL=普通规则, META=元规则
    """
    conditions = []
    
    if status == RuleStatus.ENABLED:
        conditions.append("enabled = 1")
    elif status == RuleStatus.DISABLED:
        conditions.append("enabled = 0")
    
    if rule_type == RuleType.NORMAL:
        conditions.append("is_meta = 0")
    elif rule_type == RuleType.META:
        conditions.append("is_meta = 1")
    
    return "WHERE " + " AND ".join(conditions) if conditions else ""


def get_all_rules(
    status: Optional[RuleStatus] = None,
    rule_type: Optional[RuleType] = None
) -> List[Dict[str, Any]]:
    """
    获取所有规则

    Args:
        status: None=全部, ENABLED=仅启用, DISABLED=仅禁用
        rule_type: None=全部, NORMAL=仅普通规则, META=仅元规则
    """
    with get_conn() as conn:
        where_clause = _build_where_clause(status, rule_type)
        query = f"""
            SELECT id, antecedents, consequents, confidence, priority, explanation, is_meta
            FROM rules
            {where_clause}
            ORDER BY priority DESC
        """
        rows = conn.execute(query).fetchall()

        return [
            {
                "id": row["id"],
                "antecedents": _safe_json_loads(row["antecedents"]),
                "consequents": _safe_json_loads(row["consequents"]),
                "confidence": row["confidence"],
                "priority": row["priority"],
                "explanation": row["explanation"] or "",
                "is_meta": row["is_meta"],
            }
            for row in rows
        ]


def get_rule(rule_id: str) -> Optional[Dict[str, Any]]:
    """按 ID 获取单条规则"""
    with get_conn() as conn:
        row = conn.execute(
            """
            SELECT id, antecedents, consequents, confidence, priority, explanation, is_meta
            FROM rules
            WHERE id = ?
            """,
            (rule_id,)
        ).fetchone()

        if not row:
            return None

        return {
            "id": row["id"],
            "antecedents": _safe_json_loads(row["antecedents"]),
            "consequents": _safe_json_loads(row["consequents"]),
            "confidence": row["confidence"],
            "priority": row["priority"],
            "explanation": row["explanation"] or "",
            "is_meta": row["is_meta"],
        }


def add_rule(rule: Dict[str, Any]) -> bool:
    """添加或更新规则"""
    with get_conn() as conn:
        conn.execute(
            """
            INSERT OR REPLACE INTO rules
            (id, antecedents, consequents, confidence, priority, explanation, enabled, is_meta, updated_at)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, CURRENT_TIMESTAMP)
            """,
            (
                rule["id"],
                json.dumps(rule.get("antecedents", [])),
                json.dumps(rule.get("consequents", [])),
                rule.get("confidence", 1.0),
                rule.get("priority", 0),
                rule.get("explanation", ""),
                rule.get("enabled", 1),
                rule.get("is_meta", 0),
            )
        )
        rule_type_str = "元规则" if rule.get("is_meta", 0) else "规则"
        logger.debug(f"{rule_type_str} {rule['id']} 已保存")
        return True


def soft_delete_rule(rule_id: str) -> bool:
    """软删除规则（设置 enabled=0）"""
    with get_conn() as conn:
        conn.execute(
            "UPDATE rules SET enabled = 0 WHERE id = ?",
            (rule_id,)
        )
        logger.debug(f"规则 {rule_id} 已软删除")
        return True


def restore_rule(rule_id: str) -> bool:
    """恢复软删除的规则（设置 enabled=1）"""
    with get_conn() as conn:
        conn.execute(
            "UPDATE rules SET enabled = 1 WHERE id = ?",
            (rule_id,)
        )
        logger.debug(f"规则 {rule_id} 已恢复")
        return True


def hard_delete_rule(rule_id: str) -> bool:
    """物理删除规则（不可恢复）"""
    with get_conn() as conn:
        conn.execute("DELETE FROM rules WHERE id = ?", (rule_id,))
        logger.debug(f"规则 {rule_id} 已物理删除")
        return True


def count_rules(
    status: Optional[RuleStatus] = None,
    rule_type: Optional[RuleType] = None
) -> int:
    """统计规则数量"""
    with get_conn() as conn:
        where_clause = _build_where_clause(status, rule_type)
        if where_clause:
            query = f"SELECT COUNT(*) FROM rules {where_clause}"
            result = conn.execute(query).fetchone()
        else:
            result = conn.execute("SELECT COUNT(*) FROM rules").fetchone()
        return result[0] if result else 0