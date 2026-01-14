-- Candidate Scoring App Database Schema
-- SQLite implementation

PRAGMA foreign_keys = ON;

-- Users table (populated from SSO)
CREATE TABLE IF NOT EXISTS users (
    id TEXT PRIMARY KEY,
    email TEXT NOT NULL UNIQUE,
    display_name TEXT NOT NULL,
    created_at TEXT NOT NULL DEFAULT (datetime('now'))
);

-- Positions (job openings)
CREATE TABLE IF NOT EXISTS positions (
    id TEXT PRIMARY KEY,
    title TEXT NOT NULL,
    created_by TEXT NOT NULL REFERENCES users(id),
    created_at TEXT NOT NULL DEFAULT (datetime('now'))
);

-- Candidates
CREATE TABLE IF NOT EXISTS candidates (
    id TEXT PRIMARY KEY,
    position_id TEXT NOT NULL REFERENCES positions(id) ON DELETE CASCADE,
    name TEXT NOT NULL,
    student_feedback TEXT,
    created_at TEXT NOT NULL DEFAULT (datetime('now'))
);

-- Scores (one per interviewer per candidate)
CREATE TABLE IF NOT EXISTS scores (
    id TEXT PRIMARY KEY,
    candidate_id TEXT NOT NULL REFERENCES candidates(id) ON DELETE CASCADE,
    interviewer_id TEXT NOT NULL REFERENCES users(id),
    hand_gestures INTEGER NOT NULL CHECK (hand_gestures BETWEEN 1 AND 5),
    stayed_awake INTEGER NOT NULL CHECK (stayed_awake BETWEEN 1 AND 5),
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at TEXT NOT NULL DEFAULT (datetime('now')),
    UNIQUE (candidate_id, interviewer_id)
);

-- Indexes for common queries
CREATE INDEX IF NOT EXISTS idx_candidates_position ON candidates(position_id);
CREATE INDEX IF NOT EXISTS idx_scores_candidate ON scores(candidate_id);
CREATE INDEX IF NOT EXISTS idx_scores_interviewer ON scores(interviewer_id);
CREATE INDEX IF NOT EXISTS idx_positions_created_by ON positions(created_by);

-- View: Candidate rankings with averaged scores
CREATE VIEW IF NOT EXISTS candidate_rankings AS
SELECT
    c.id AS candidate_id,
    c.name AS candidate_name,
    c.position_id,
    p.title AS position_title,
    COUNT(s.id) AS num_scores,
    ROUND(AVG(s.hand_gestures), 2) AS avg_hand_gestures,
    ROUND(AVG(s.stayed_awake), 2) AS avg_stayed_awake,
    ROUND((AVG(s.hand_gestures) + AVG(s.stayed_awake)) / 2, 2) AS avg_total
FROM candidates c
JOIN positions p ON c.position_id = p.id
LEFT JOIN scores s ON c.id = s.candidate_id
GROUP BY c.id, c.name, c.position_id, p.title;
