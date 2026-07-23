import sqlite3
from pathlib import Path
from contextlib import contextmanager
from typing import Generator, Any
from loguru import logger

# 项目根目录：knowledge/db.py → knowledge/ → ai-expert-system/
PROJECT_ROOT = Path(__file__).parent.parent
DB_PATH = PROJECT_ROOT / "data" / "expert.db"
SQL_DIR = Path(__file__).parent / "sql"


def get_db_path() -> Path:
    """获取数据库路径，确保目录存在"""
    DB_PATH.parent.mkdir(parents=True, exist_ok=True)
    return DB_PATH


def _read_sql(filename: str) -> str:
    """读取 SQL 脚本文件"""
    with open(SQL_DIR / filename, "r", encoding="utf-8") as f:
        return f.read()


@contextmanager
def get_conn() -> Generator[sqlite3.Connection, Any, None]:
    """获取数据库连接（上下文管理器）"""
    conn = sqlite3.connect(get_db_path())
    conn.row_factory = sqlite3.Row
    try:
        yield conn
        conn.commit()
    except Exception:
        conn.rollback()
        raise
    finally:
        conn.close()


def init_db() -> None:
    """从 init.sql 建表"""
    with get_conn() as conn:
        conn.executescript(_read_sql("init.sql"))
        logger.success("数据库表结构初始化完成")


def reset_db() -> None:
    """从 reset.sql 删表"""
    with get_conn() as conn:
        conn.executescript(_read_sql("reset.sql"))
        logger.warning("数据库已重置")


def seed_db() -> None:
    """从 seed_rules.sql 导入初始数据"""
    with get_conn() as conn:
        conn.executescript(_read_sql("seed_rules.sql"))
        logger.success("初始规则导入完成")


def get_table_count(table_name: str) -> int:
    """获取表中记录数"""
    allowed = {"rules", "facts", "sessions", "meta_rules"}
    if table_name not in allowed:
        raise ValueError(f"非法表名: {table_name}")
    with get_conn() as conn:
        result = conn.execute(f"SELECT COUNT(*) FROM {table_name}").fetchone()
        return result[0] if result else 0
        