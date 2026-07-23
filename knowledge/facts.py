import json
from typing import List, Dict, Any, Optional
from loguru import logger
from .db import get_conn


def _safe_json_loads(data: str) -> Any:
    """安全解析 JSON，失败时返回空列表"""
    if not data:
        return []
    try:
        return json.loads(data)
    except json.JSONDecodeError as e:
        logger.error(f"JSON 解析失败: {e}, 数据: {data[:100]}...")
        return []


def get_all_facts(session_id: Optional[str] = None) -> List[Dict[str, Any]]:
    """获取所有事实（可过滤会话）"""
    with get_conn() as conn:
        if session_id:
            rows = conn.execute(
                "SELECT predicate, terms, confidence FROM facts WHERE session_id = ?",
                (session_id,)
            ).fetchall()
        else:
            rows = conn.execute(
                "SELECT predicate, terms, confidence FROM facts"
            ).fetchall()

        return [
            {
                "predicate": row["predicate"],
                "terms": _safe_json_loads(row["terms"]),
                "confidence": row["confidence"],
            }
            for row in rows
        ]


def get_facts_by_predicate(predicate: str) -> List[Dict[str, Any]]:
    """按谓词名称获取事实"""
    with get_conn() as conn:
        rows = conn.execute(
            "SELECT predicate, terms, confidence FROM facts WHERE predicate = ?",
            (predicate,)
        ).fetchall()
        return [
            {
                "predicate": row["predicate"],
                "terms": _safe_json_loads(row["terms"]),
                "confidence": row["confidence"],
            }
            for row in rows
        ]


def add_fact(
    predicate: str,
    terms: Optional[List[Dict[str, Any]]] = None,
    confidence: float = 1.0,
    session_id: Optional[str] = None
) -> bool:
    """添加事实"""
    with get_conn() as conn:
        conn.execute(
            """
            INSERT INTO facts (predicate, terms, confidence, session_id)
            VALUES (?, ?, ?, ?)
            """,
            (predicate, json.dumps(terms or []), confidence, session_id)
        )
        logger.debug(f"添加事实: {predicate} (cf={confidence})")
        return True


def delete_fact(fact_id: int) -> bool:
    """按 ID 删除单条事实"""
    with get_conn() as conn:
        conn.execute("DELETE FROM facts WHERE id = ?", (fact_id,))
        logger.debug(f"删除事实 ID: {fact_id}")
        return True


def clear_facts(session_id: Optional[str] = None) -> None:
    """
    清空事实（可指定会话）

    警告：此操作不可恢复！
    清空所有事实前会打印警告日志，如需指定会话则只清空该会话的事实。
    """
    with get_conn() as conn:
        if session_id:
            conn.execute("DELETE FROM facts WHERE session_id = ?", (session_id,))
            logger.info(f"清空会话 {session_id} 的所有事实")
        else:
            conn.execute("DELETE FROM facts")
            logger.warning("清空所有事实 ———— 此操作不可恢复！")


def count_facts(session_id: Optional[str] = None) -> int:
    """统计事实数量"""
    with get_conn() as conn:
        if session_id:
            result = conn.execute(
                "SELECT COUNT(*) FROM facts WHERE session_id = ?",
                (session_id,)
            ).fetchone()
        else:
            result = conn.execute("SELECT COUNT(*) FROM facts").fetchone()
        return result[0] if result else 0