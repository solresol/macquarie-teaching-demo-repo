#include <iostream>
#include <string>
#include <vector>
#include <random>
#include <sstream>
#include <iomanip>
#include <sqlite3.h>
#include "httplib.h"
#include "templates.h"

// Generate a UUID
std::string generate_uuid() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static const char* hex = "0123456789abcdef";

    std::string uuid = "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx";
    for (char& c : uuid) {
        if (c == 'x') {
            c = hex[dis(gen)];
        } else if (c == 'y') {
            c = hex[(dis(gen) & 0x3) | 0x8];
        }
    }
    return uuid;
}

// URL decode
std::string url_decode(const std::string& s) {
    std::string result;
    for (size_t i = 0; i < s.length(); i++) {
        if (s[i] == '%' && i + 2 < s.length()) {
            int val;
            std::istringstream iss(s.substr(i + 1, 2));
            if (iss >> std::hex >> val) {
                result += static_cast<char>(val);
                i += 2;
            } else {
                result += s[i];
            }
        } else if (s[i] == '+') {
            result += ' ';
        } else {
            result += s[i];
        }
    }
    return result;
}

// Parse form data
std::map<std::string, std::string> parse_form(const std::string& body) {
    std::map<std::string, std::string> params;
    std::istringstream stream(body);
    std::string pair;
    while (std::getline(stream, pair, '&')) {
        size_t pos = pair.find('=');
        if (pos != std::string::npos) {
            std::string key = url_decode(pair.substr(0, pos));
            std::string value = url_decode(pair.substr(pos + 1));
            params[key] = value;
        }
    }
    return params;
}

// Database wrapper
class Database {
public:
    sqlite3* db;

    Database(const std::string& path) {
        if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
            throw std::runtime_error("Failed to open database");
        }
        sqlite3_exec(db, "PRAGMA foreign_keys = ON", nullptr, nullptr, nullptr);
        init_schema();
    }

    void init_schema() {
        const char* schema = R"(
            CREATE TABLE IF NOT EXISTS users (
                id TEXT PRIMARY KEY,
                email TEXT NOT NULL UNIQUE,
                display_name TEXT NOT NULL,
                created_at TEXT NOT NULL DEFAULT (datetime('now'))
            );

            CREATE TABLE IF NOT EXISTS positions (
                id TEXT PRIMARY KEY,
                title TEXT NOT NULL,
                created_by TEXT NOT NULL REFERENCES users(id),
                created_at TEXT NOT NULL DEFAULT (datetime('now'))
            );

            CREATE TABLE IF NOT EXISTS candidates (
                id TEXT PRIMARY KEY,
                position_id TEXT NOT NULL REFERENCES positions(id) ON DELETE CASCADE,
                name TEXT NOT NULL,
                student_feedback TEXT,
                created_at TEXT NOT NULL DEFAULT (datetime('now'))
            );

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

            CREATE INDEX IF NOT EXISTS idx_candidates_position ON candidates(position_id);
            CREATE INDEX IF NOT EXISTS idx_scores_candidate ON scores(candidate_id);
            CREATE INDEX IF NOT EXISTS idx_scores_interviewer ON scores(interviewer_id);
            CREATE INDEX IF NOT EXISTS idx_positions_created_by ON positions(created_by);
        )";

        char* err_msg = nullptr;
        if (sqlite3_exec(db, schema, nullptr, nullptr, &err_msg) != SQLITE_OK) {
            std::string error = err_msg ? err_msg : "Unknown error";
            sqlite3_free(err_msg);
            throw std::runtime_error("Failed to initialize schema: " + error);
        }
    }

    ~Database() {
        sqlite3_close(db);
    }

    void ensure_user(const std::string& id, const std::string& email, const std::string& name) {
        const char* sql = "INSERT OR IGNORE INTO users (id, email, display_name) VALUES (?, ?, ?)";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, email.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    std::vector<Position> get_positions() {
        std::vector<Position> positions;
        const char* sql = R"(
            SELECT p.id, p.title, u.display_name, COUNT(DISTINCT c.id) as cnt
            FROM positions p
            JOIN users u ON p.created_by = u.id
            LEFT JOIN candidates c ON c.position_id = p.id
            GROUP BY p.id
            ORDER BY p.created_at DESC
        )";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Position p;
            p.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            p.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            p.creator_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            p.candidate_count = sqlite3_column_int(stmt, 3);
            positions.push_back(p);
        }
        sqlite3_finalize(stmt);
        return positions;
    }

    std::string create_position(const std::string& title, const std::string& user_id) {
        std::string id = generate_uuid();
        const char* sql = "INSERT INTO positions (id, title, created_by) VALUES (?, ?, ?)";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, title.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, user_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return id;
    }

    bool get_position(const std::string& id, std::string& title) {
        const char* sql = "SELECT title FROM positions WHERE id = ?";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
        bool found = sqlite3_step(stmt) == SQLITE_ROW;
        if (found) {
            title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        }
        sqlite3_finalize(stmt);
        return found;
    }

    std::vector<CandidateRanking> get_candidates_for_position(const std::string& position_id) {
        std::vector<CandidateRanking> candidates;
        const char* sql = R"(
            SELECT c.id, c.name, COUNT(s.id), AVG(s.hand_gestures), AVG(s.stayed_awake),
                   (AVG(s.hand_gestures) + AVG(s.stayed_awake)) / 2
            FROM candidates c
            LEFT JOIN scores s ON c.id = s.candidate_id
            WHERE c.position_id = ?
            GROUP BY c.id
            ORDER BY (AVG(s.hand_gestures) + AVG(s.stayed_awake)) / 2 DESC NULLS LAST, c.name
        )";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, position_id.c_str(), -1, SQLITE_TRANSIENT);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            CandidateRanking c;
            c.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            c.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            c.num_scores = sqlite3_column_int(stmt, 2);
            c.avg_hand_gestures = sqlite3_column_type(stmt, 3) != SQLITE_NULL ? sqlite3_column_double(stmt, 3) : 0;
            c.avg_stayed_awake = sqlite3_column_type(stmt, 4) != SQLITE_NULL ? sqlite3_column_double(stmt, 4) : 0;
            c.avg_total = sqlite3_column_type(stmt, 5) != SQLITE_NULL ? sqlite3_column_double(stmt, 5) : 0;
            candidates.push_back(c);
        }
        sqlite3_finalize(stmt);
        return candidates;
    }

    std::string create_candidate(const std::string& position_id, const std::string& name) {
        std::string id = generate_uuid();
        const char* sql = "INSERT INTO candidates (id, position_id, name) VALUES (?, ?, ?)";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, position_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return id;
    }

    bool get_candidate(const std::string& id, CandidateDetail& candidate) {
        const char* sql = R"(
            SELECT c.id, c.name, c.position_id, p.title, COALESCE(c.student_feedback, '')
            FROM candidates c
            JOIN positions p ON c.position_id = p.id
            WHERE c.id = ?
        )";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
        bool found = sqlite3_step(stmt) == SQLITE_ROW;
        if (found) {
            candidate.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            candidate.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            candidate.position_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            candidate.position_title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            candidate.student_feedback = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        }
        sqlite3_finalize(stmt);
        return found;
    }

    ScoreStats get_score_stats(const std::string& candidate_id) {
        ScoreStats stats = {0, 0, 0, 0};
        const char* sql = R"(
            SELECT COUNT(*), AVG(hand_gestures), AVG(stayed_awake),
                   (AVG(hand_gestures) + AVG(stayed_awake)) / 2
            FROM scores WHERE candidate_id = ?
        )";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, candidate_id.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            stats.num_scores = sqlite3_column_int(stmt, 0);
            if (stats.num_scores > 0) {
                stats.avg_hand_gestures = sqlite3_column_double(stmt, 1);
                stats.avg_stayed_awake = sqlite3_column_double(stmt, 2);
                stats.avg_total = sqlite3_column_double(stmt, 3);
            }
        }
        sqlite3_finalize(stmt);
        return stats;
    }

    MyScore get_my_score(const std::string& candidate_id, const std::string& user_id) {
        MyScore score = {false, 0, 0};
        const char* sql = "SELECT hand_gestures, stayed_awake FROM scores WHERE candidate_id = ? AND interviewer_id = ?";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, candidate_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, user_id.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            score.exists = true;
            score.hand_gestures = sqlite3_column_int(stmt, 0);
            score.stayed_awake = sqlite3_column_int(stmt, 1);
        }
        sqlite3_finalize(stmt);
        return score;
    }

    void upsert_score(const std::string& candidate_id, const std::string& user_id, int hand_gestures, int stayed_awake) {
        // Check if exists
        MyScore existing = get_my_score(candidate_id, user_id);
        if (existing.exists) {
            const char* sql = "UPDATE scores SET hand_gestures = ?, stayed_awake = ?, updated_at = datetime('now') WHERE candidate_id = ? AND interviewer_id = ?";
            sqlite3_stmt* stmt;
            sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
            sqlite3_bind_int(stmt, 1, hand_gestures);
            sqlite3_bind_int(stmt, 2, stayed_awake);
            sqlite3_bind_text(stmt, 3, candidate_id.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 4, user_id.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        } else {
            std::string id = generate_uuid();
            const char* sql = "INSERT INTO scores (id, candidate_id, interviewer_id, hand_gestures, stayed_awake) VALUES (?, ?, ?, ?, ?)";
            sqlite3_stmt* stmt;
            sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
            sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, candidate_id.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, user_id.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 4, hand_gestures);
            sqlite3_bind_int(stmt, 5, stayed_awake);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    void update_feedback(const std::string& candidate_id, const std::string& feedback) {
        const char* sql = "UPDATE candidates SET student_feedback = ? WHERE id = ?";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, feedback.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, candidate_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
};

// Get current user from headers (SSO) or defaults
struct User {
    std::string id;
    std::string email;
    std::string name;
};

User get_current_user(const httplib::Request& req, Database& db) {
    User user;
    user.id = req.get_header_value("X-SSO-User-ID");
    user.email = req.get_header_value("X-SSO-Email");
    user.name = req.get_header_value("X-SSO-Name");

    // Development fallback
    if (user.id.empty()) {
        user.id = "dev-user-1";
        user.email = "dev@university.edu";
        user.name = "Dev User";
    }

    db.ensure_user(user.id, user.email, user.name);
    return user;
}

int main() {
    Database db("candidate_scoring.db");
    httplib::Server svr;

    // Home page - list positions
    svr.Get("/", [&db](const httplib::Request& req, httplib::Response& res) {
        User user = get_current_user(req, db);
        auto positions = db.get_positions();
        res.set_content(index_page(user.name, "", positions), "text/html");
    });

    // New position form
    svr.Get("/positions/new", [&db](const httplib::Request& req, httplib::Response& res) {
        User user = get_current_user(req, db);
        res.set_content(position_form_page(user.name, ""), "text/html");
    });

    // Create position
    svr.Post("/positions/new", [&db](const httplib::Request& req, httplib::Response& res) {
        User user = get_current_user(req, db);
        auto params = parse_form(req.body);
        std::string title = params["title"];

        if (title.empty()) {
            res.set_content(position_form_page(user.name, "Position title is required."), "text/html");
            return;
        }

        std::string id = db.create_position(title, user.id);
        res.set_redirect("/positions/" + id);
    });

    // Position detail
    svr.Get(R"(/positions/([a-f0-9-]+))", [&db](const httplib::Request& req, httplib::Response& res) {
        User user = get_current_user(req, db);
        std::string position_id = req.matches[1];
        std::string title;

        if (!db.get_position(position_id, title)) {
            res.set_redirect("/");
            return;
        }

        auto candidates = db.get_candidates_for_position(position_id);
        res.set_content(position_detail_page(user.name, "", position_id, title, candidates), "text/html");
    });

    // New candidate form
    svr.Get(R"(/positions/([a-f0-9-]+)/candidates/new)", [&db](const httplib::Request& req, httplib::Response& res) {
        User user = get_current_user(req, db);
        std::string position_id = req.matches[1];
        std::string title;

        if (!db.get_position(position_id, title)) {
            res.set_redirect("/");
            return;
        }

        res.set_content(candidate_form_page(user.name, "", position_id, title), "text/html");
    });

    // Create candidate
    svr.Post(R"(/positions/([a-f0-9-]+)/candidates/new)", [&db](const httplib::Request& req, httplib::Response& res) {
        User user = get_current_user(req, db);
        std::string position_id = req.matches[1];
        std::string title;

        if (!db.get_position(position_id, title)) {
            res.set_redirect("/");
            return;
        }

        auto params = parse_form(req.body);
        std::string name = params["name"];

        if (name.empty()) {
            res.set_content(candidate_form_page(user.name, "Candidate name is required.", position_id, title), "text/html");
            return;
        }

        std::string id = db.create_candidate(position_id, name);
        res.set_redirect("/candidates/" + id);
    });

    // Candidate detail
    svr.Get(R"(/candidates/([a-f0-9-]+))", [&db](const httplib::Request& req, httplib::Response& res) {
        User user = get_current_user(req, db);
        std::string candidate_id = req.matches[1];
        CandidateDetail candidate;

        if (!db.get_candidate(candidate_id, candidate)) {
            res.set_redirect("/");
            return;
        }

        ScoreStats stats = db.get_score_stats(candidate_id);
        MyScore my_score = db.get_my_score(candidate_id, user.id);

        res.set_content(candidate_detail_page(user.name, "", candidate, stats, my_score), "text/html");
    });

    // Score/feedback submission
    svr.Post(R"(/candidates/([a-f0-9-]+))", [&db](const httplib::Request& req, httplib::Response& res) {
        User user = get_current_user(req, db);
        std::string candidate_id = req.matches[1];
        CandidateDetail candidate;

        if (!db.get_candidate(candidate_id, candidate)) {
            res.set_redirect("/");
            return;
        }

        auto params = parse_form(req.body);
        std::string action = params["action"];
        std::string flash;

        if (action == "score") {
            int hand_gestures = std::stoi(params["hand_gestures"]);
            int stayed_awake = std::stoi(params["stayed_awake"]);

            if (hand_gestures >= 1 && hand_gestures <= 5 && stayed_awake >= 1 && stayed_awake <= 5) {
                db.upsert_score(candidate_id, user.id, hand_gestures, stayed_awake);
                flash = "Score saved.";
            } else {
                flash = "Scores must be between 1 and 5.";
            }
        } else if (action == "feedback") {
            std::string feedback = params["student_feedback"];
            db.update_feedback(candidate_id, feedback);
            flash = "Feedback saved.";
        }

        // Refresh data
        db.get_candidate(candidate_id, candidate);
        ScoreStats stats = db.get_score_stats(candidate_id);
        MyScore my_score = db.get_my_score(candidate_id, user.id);

        res.set_content(candidate_detail_page(user.name, flash, candidate, stats, my_score), "text/html");
    });

    std::cout << "Server running at http://localhost:5000" << std::endl;
    svr.listen("0.0.0.0", 5000);

    return 0;
}
