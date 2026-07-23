-- ============================================================
-- 专家系统数据库表结构
-- ============================================================
-- 设计说明：
--   1. 使用 SQLite 作为存储后端，单文件部署，零运维
--   2. 规则/事实等结构化数据使用 JSON 序列化存储，保持 Schema 灵活性
--   3. Python 层负责读写，C++ 推理引擎通过 Socket 接收 JSON 数据，不直接访问 DB
-- ============================================================




-- ============================================================
-- 1. 规则表（rules）
--   存储产生式规则，是专家系统的核心知识载体
-- ============================================================
CREATE TABLE IF NOT EXISTS rules (
    id TEXT PRIMARY KEY,
    antecedents TEXT NOT NULL,      -- 规则前提（IF 部分），JSON 数组格式
    consequents TEXT NOT NULL,      -- 规则结论（THEN 部分），JSON 数组格式
    confidence REAL DEFAULT 1.0,      -- 规则置信度，范围 [0, 1]
    priority INTEGER DEFAULT 0,     -- 规则优先级，数值越大越优先触发
    explanation TEXT DEFAULT '',         -- 规则解释文本，用于生成可解释的推理链
    enabled INTEGER DEFAULT 1,           -- 启用状态：1=启用，0=软删除（保留历史，不参与推理）
    is_meta INTEGER DEFAULT 0,              -- 规则类型：0=普通规则，1=元规则
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP   -- 规则最后修改时间（导入/更新时触发）
);


-- ============================================================
-- 2. 事实表（facts）
--   存储工作内存中的事实，用于规则匹配和推理
-- ============================================================
CREATE TABLE IF NOT EXISTS facts (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    predicate TEXT NOT NULL,            -- 谓词名称，如 "GPA_high", "interest_research"
    terms TEXT,                         -- 谓词参数，JSON 数组格式，如 [{"type":"constant","value":"top"}] 
    confidence REAL DEFAULT 1.0,              -- 事实置信度，范围 [0, 1]
    session_id TEXT,                       -- 所属会话 ID，用于隔离不同推理会话的事实
    asserted_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP         -- 事实被断言的时间（业务动作）
);




-- ============================================================
-- 3. 会话表（sessions）
--   记录每次推理会话的完整信息，用于审计和复现
-- ============================================================
CREATE TABLE IF NOT EXISTS sessions (
    id TEXT PRIMARY KEY,
    query TEXT,                         -- 用户原始查询文本
    context TEXT,                       -- 用户上下文，JSON 对象格式
    result TEXT,                        -- 推理结果，JSON 对象格式，包含 conclusions、fired_rules、explanation 等
    fired_rules TEXT,                   -- 本次推理触发的规则列表，JSON 数组格式， 如["R1","R3"]
    overall_confidence REAL,                -- 整体推理置信度，范围 [0, 1]
    execution_time_ms REAL,                 -- 推理执行耗时（毫秒）
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP          -- 用户发起请求的时间（业务动作）
);




-- ============================================================
-- 4. 索引
--   加速常见查询
-- ============================================================

-- 按优先级降序查询规则（冲突消解第一层）
CREATE INDEX IF NOT EXISTS idx_rules_priority ON rules(priority DESC);

-- 按类型过滤规则
CREATE INDEX IF NOT EXISTS idx_rules_is_meta ON rules(is_meta);

-- 按时间降序查询会话（最近会话列表）
CREATE INDEX IF NOT EXISTS idx_sessions_requested ON sessions(requested_at DESC);

-- 按会话 ID 查询事实（恢复工作内存）
CREATE INDEX IF NOT EXISTS idx_facts_session ON facts(session_id);