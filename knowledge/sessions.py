import json
import uuid
from typing import Dict, Any, Optional, List
from datetime import datetime
from loguru import logger
from .db import get_conn


def _safe_json_loads(data: str) -> Any:
    """安全解析 JSON，失败时返回空对象"""
    if not data:
        return {} if data == "" or data is None else []
    try:
        return json.loads(data)
    except json.JSONDecodeError as e:
        logger.error(f"JSON 解析失败: {e}, 数据: {data[:100]}...")
        return {} if data.startswith("{") else []


def _normalize_date_range(
    start_date: Optional[str],
    end_date: Optional[str]
) -> tuple[Optional[str], Optional[str]]:
    """校验并规范化日期范围，如果 end < start 则交换"""
    if start_date and end_date and end_date < start_date:
        logger.warning(f"end_date '{end_date}' 早于 start_date '{start_date}'，已自动交换")
        return end_date, start_date
    return start_date, end_date


def _build_session_where_clause(
    start_date: Optional[str] = None,
    end_date: Optional[str] = None
) -> tuple[str, list]:
    """
    构建会话查询的 WHERE 子句和参数列表
    返回 (where_clause, params)
    """
    start_date, end_date = _normalize_date_range(start_date, end_date)
    conditions = []
    params = []

    if start_date:
        conditions.append("created_at >= ?")
        params.append(start_date)
    if end_date:
        conditions.append("created_at <= ?")
        params.append(end_date)

    if conditions:
        return "WHERE " + " AND ".join(conditions), params
    return "", params


def save_session(
    query: str,
    context: Dict[str, Any],
    result: Dict[str, Any]
) -> str:
    """保存会话，返回会话 ID"""
    session_id = str(uuid.uuid4())[:8]

    with get_conn() as conn:
        conn.execute(
            """
            INSERT INTO sessions
            (id, query, context, result, fired_rules, overall_confidence, execution_time_ms, requested_at)
            VALUES (?, ?, ?, ?, ?, ?, ?, CURRENT_TIMESTAMP)
            """,
            (
                session_id,
                query,
                json.dumps(context, ensure_ascii=False),
                json.dumps(result, ensure_ascii=False),
                json.dumps(result.get("fired_rules", []), ensure_ascii=False),
                result.get("overall_confidence", 0.0),
                result.get("execution_time_ms", 0.0),
            )
        )
        logger.debug(f"会话 {session_id} 已保存: {query[:30]}...")
        return session_id


def get_session(session_id: str) -> Optional[Dict[str, Any]]:
    """获取单条会话"""
    with get_conn() as conn:
        row = conn.execute(
            """
            SELECT id, query, context, result, fired_rules, overall_confidence,
                   execution_time_ms, requested_at
            FROM sessions WHERE id = ?
            """,
            (session_id,)
        ).fetchone()

        if not row:
            return None

        return {
            "id": row["id"],
            "query": row["query"],
            "context": _safe_json_loads(row["context"]),
            "result": _safe_json_loads(row["result"]),
            "fired_rules": _safe_json_loads(row["fired_rules"]),
            "overall_confidence": row["overall_confidence"],
            "execution_time_ms": row["execution_time_ms"],
            "requested_at": row["requested_at"],
        }


def list_sessions(
    limit: int = 20,
    offset: int = 0,
    start_date: Optional[str] = None,
    end_date: Optional[str] = None
) -> List[Dict[str, Any]]:
    """分页查询会话列表（支持日期过滤）"""
    with get_conn() as conn:
        where_clause, params = _build_session_where_clause(start_date, end_date)
        sql_command = f"""
            SELECT id, query, overall_confidence, execution_time_ms, requested_at
            FROM sessions
            {where_clause}
            ORDER BY requested_at DESC
            LIMIT ? OFFSET ?
        """
        params.extend([limit, offset])
        rows = conn.execute(sql_command, params).fetchall()

        return [
            {
                "id": row["id"],
                "query": row["query"],
                "overall_confidence": row["overall_confidence"],
                "execution_time_ms": row["execution_time_ms"],
                "requested_at": row["requested_at"],
            }
            for row in rows
        ]


def delete_session(session_id: str) -> bool:
    """删除单条会话"""
    with get_conn() as conn:
        conn.execute("DELETE FROM sessions WHERE id = ?", (session_id,))
        logger.debug(f"会话 {session_id} 已删除")
        return True


def clear_sessions(confirm: bool = False, older_than_days: Optional[int] = None) -> None:
    """
    清空会话历史

    Args:
        confirm: 必须为 True 才会执行
        older_than_days: 只删除 N 天前的会话，None 表示删除全部
    """
    if not confirm:
        logger.warning("清空会话操作需要 confirm=True")
        return

    with get_conn() as conn:
        if older_than_days:
            conn.execute(
                "DELETE FROM sessions WHERE requested_at < datetime('now', ?)",
                (f"-{older_than_days} days",)
            )
            logger.info(f"已删除 {older_than_days} 天前的会话")
        else:
            conn.execute("DELETE FROM sessions")
            logger.warning("已清空所有会话")


def count_sessions(start_date: Optional[str] = None, end_date: Optional[str] = None) -> int:
    """统计会话数量（支持日期过滤）"""
    with get_conn() as conn:
        where_clause, params = _build_session_where_clause(start_date, end_date)
        if where_clause:
            sql_command = f"SELECT COUNT(*) FROM sessions {where_clause}"
            result = conn.execute(sql_command, params).fetchone()
        else:
            result = conn.execute("SELECT COUNT(*) FROM sessions").fetchone()
        return result[0] if result else 0


def get_session_stats() -> Dict[str, Any]:
    """获取会话统计信息"""
    with get_conn() as conn:
        total = conn.execute("SELECT COUNT(*) FROM sessions").fetchone()[0]

        avg_cf = conn.execute(
            "SELECT AVG(overall_confidence) FROM sessions WHERE overall_confidence > 0"
        ).fetchone()[0] or 0.0

        avg_time = conn.execute(
            "SELECT AVG(execution_time_ms) FROM sessions WHERE execution_time_ms > 0"
        ).fetchone()[0] or 0.0

        last = conn.execute(
            "SELECT query, requested_at FROM sessions ORDER BY requested_at DESC LIMIT 1"
        ).fetchone()

        return {
            "total_sessions": total,
            "avg_confidence": round(avg_cf, 3),
            "avg_execution_ms": round(avg_time, 2),
            "last_query": last["query"] if last else None,
            "last_time": last["requested_at"] if last else None,
        }