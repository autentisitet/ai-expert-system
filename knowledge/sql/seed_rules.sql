-- ============================================================
-- 初始规则（考研决策场景）
-- ============================================================

-- 普通规则（is_meta = 0）
INSERT OR REPLACE INTO rules (id, antecedents, consequents, confidence, priority, explanation, is_meta) VALUES
('R1', '[{"name":"GPA_high","terms":[]}]', '[{"name":"recommend_postgraduate","terms":[]}]', 0.7, 10, 'GPA高说明学术能力强，适合继续深造', 0),
('R2', '[{"name":"interest_research","terms":[]}]', '[{"name":"recommend_postgraduate","terms":[]}]', 0.8, 10, '对科研有兴趣，这是读研的核心动力', 0),
('R3', '[{"name":"career_ambition","terms":[]}]', '[{"name":"recommend_job","terms":[]}]', 0.75, 10, '有明确的职业目标，直接工作更有针对性', 0),
('R4', '[{"name":"family_needs_income","terms":[]}]', '[{"name":"recommend_job","terms":[]}]', 0.6, 10, '家庭经济压力使尽快工作成为更现实的选择', 0),
('R5', '[{"name":"undergraduate_school","terms":[{"type":"constant","value":"top"}]}]', '[{"name":"recommend_postgraduate","terms":[]}]', 0.5, 5, '本科学校较好，保研或考研有竞争力', 0),
('R6', '[{"name":"undergraduate_school","terms":[{"type":"constant","value":"ordinary"}]}]', '[{"name":"recommend_job","terms":[]}]', 0.4, 5, '本科学校一般，通过工作积累经验也是好选择', 0);

-- 元规则（is_meta = 1）
INSERT OR REPLACE INTO rules (id, antecedents, consequents, confidence, priority, explanation, is_meta) VALUES
('META1', '[{"name":"family_needs_income","terms":[]}]', '[{"name":"SELECT_RULE","terms":[{"type":"constant","value":"R3"}]}]', 0.9, 100, '有经济压力时，优先考虑推荐就业的规则', 1),
('META2', '[{"name":"interest_research","terms":[]}]', '[{"name":"SELECT_RULE","terms":[{"type":"constant","value":"R2"}]}]', 0.85, 90, '有科研兴趣时，优先考虑推荐考研的规则', 1);