#ifndef TEMPLATES_H
#define TEMPLATES_H

#include <string>
#include <vector>
#include <sstream>

// Simple HTML escaping
inline std::string html_escape(const std::string& s) {
    std::string result;
    for (char c : s) {
        switch (c) {
            case '&': result += "&amp;"; break;
            case '<': result += "&lt;"; break;
            case '>': result += "&gt;"; break;
            case '"': result += "&quot;"; break;
            case '\'': result += "&#39;"; break;
            default: result += c;
        }
    }
    return result;
}

inline std::string base_template(const std::string& title, const std::string& user_name,
                                  const std::string& flash, const std::string& content) {
    std::ostringstream html;
    html << R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>)" << html_escape(title) << R"( - Candidate Scoring</title>
    <style>
        * { box-sizing: border-box; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            line-height: 1.5;
            max-width: 800px;
            margin: 0 auto;
            padding: 1rem;
            background: #f5f5f5;
        }
        header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 0.5rem 0;
            border-bottom: 1px solid #ddd;
            margin-bottom: 1rem;
        }
        header a { text-decoration: none; color: #333; }
        header h1 { margin: 0; font-size: 1.25rem; }
        .user-info { font-size: 0.875rem; color: #666; }
        .flash {
            padding: 0.75rem;
            margin-bottom: 1rem;
            background: #d4edda;
            border: 1px solid #c3e6cb;
            border-radius: 4px;
        }
        .card {
            background: white;
            border: 1px solid #ddd;
            border-radius: 4px;
            padding: 1rem;
            margin-bottom: 0.5rem;
        }
        .card h2 { margin: 0 0 0.5rem 0; font-size: 1.1rem; }
        .card-meta { font-size: 0.875rem; color: #666; }
        a { color: #0066cc; }
        form { margin: 0; }
        label { display: block; margin-bottom: 0.25rem; font-weight: 500; }
        input[type="text"], textarea, select {
            width: 100%;
            padding: 0.5rem;
            margin-bottom: 1rem;
            border: 1px solid #ccc;
            border-radius: 4px;
            font-size: 1rem;
        }
        textarea { min-height: 100px; resize: vertical; }
        button, .btn {
            display: inline-block;
            padding: 0.5rem 1rem;
            background: #0066cc;
            color: white;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            text-decoration: none;
            font-size: 1rem;
        }
        button:hover, .btn:hover { background: #0055aa; }
        .btn-secondary { background: #666; }
        .btn-secondary:hover { background: #555; }
        table { width: 100%; border-collapse: collapse; }
        th, td { text-align: left; padding: 0.5rem; border-bottom: 1px solid #ddd; }
        th { background: #f9f9f9; }
        .score-input { display: flex; gap: 1rem; margin-bottom: 1rem; }
        .score-input > div { flex: 1; }
        .stats { display: flex; gap: 2rem; margin: 1rem 0; }
        .stat { text-align: center; }
        .stat-label { font-size: 0.75rem; color: #666; text-transform: uppercase; }
        .stat-value { font-size: 1.5rem; font-weight: bold; }
        .breadcrumb { font-size: 0.875rem; margin-bottom: 1rem; }
        .breadcrumb a { color: #666; }
        .header-row { display: flex; justify-content: space-between; align-items: center; margin-bottom: 1rem; }
        .header-row h2 { margin: 0; }
    </style>
</head>
<body>
    <header>
        <a href="/"><h1>Candidate Scoring</h1></a>
        <span class="user-info">)" << html_escape(user_name) << R"(</span>
    </header>
)";

    if (!flash.empty()) {
        html << "    <div class=\"flash\">" << html_escape(flash) << "</div>\n";
    }

    html << content;
    html << R"(
</body>
</html>)";
    return html.str();
}

struct Position {
    std::string id;
    std::string title;
    std::string creator_name;
    int candidate_count;
};

inline std::string index_page(const std::string& user_name, const std::string& flash,
                               const std::vector<Position>& positions) {
    std::ostringstream content;
    content << R"(
<div class="header-row">
    <h2>Positions</h2>
    <a href="/positions/new" class="btn">New Position</a>
</div>
)";

    if (positions.empty()) {
        content << "<p>No positions yet. <a href=\"/positions/new\">Create one</a> to get started.</p>";
    } else {
        for (const auto& p : positions) {
            content << "<div class=\"card\">\n";
            content << "    <h2><a href=\"/positions/" << html_escape(p.id) << "\">"
                    << html_escape(p.title) << "</a></h2>\n";
            content << "    <div class=\"card-meta\">" << p.candidate_count
                    << " candidate" << (p.candidate_count != 1 ? "s" : "")
                    << " &middot; Created by " << html_escape(p.creator_name) << "</div>\n";
            content << "</div>\n";
        }
    }

    return base_template("Positions", user_name, flash, content.str());
}

inline std::string position_form_page(const std::string& user_name, const std::string& flash) {
    std::string content = R"(
<div class="breadcrumb">
    <a href="/">Positions</a> &raquo; New
</div>

<div class="card">
    <h2>Create Position</h2>
    <form method="POST">
        <label for="title">Position Title</label>
        <input type="text" id="title" name="title" placeholder="e.g., Assistant Professor - Chemistry" required>
        <button type="submit">Create Position</button>
        <a href="/" class="btn btn-secondary">Cancel</a>
    </form>
</div>
)";
    return base_template("New Position", user_name, flash, content);
}

struct CandidateRanking {
    std::string id;
    std::string name;
    int num_scores;
    double avg_hand_gestures;
    double avg_stayed_awake;
    double avg_total;
};

inline std::string position_detail_page(const std::string& user_name, const std::string& flash,
                                         const std::string& position_id, const std::string& position_title,
                                         const std::vector<CandidateRanking>& candidates) {
    std::ostringstream content;
    content << R"(
<div class="breadcrumb">
    <a href="/">Positions</a> &raquo; )" << html_escape(position_title) << R"(
</div>

<div class="header-row">
    <h2>)" << html_escape(position_title) << R"(</h2>
    <a href="/positions/)" << html_escape(position_id) << R"(/candidates/new" class="btn">Add Candidate</a>
</div>
)";

    if (candidates.empty()) {
        content << "<p>No candidates yet. <a href=\"/positions/" << html_escape(position_id)
                << "/candidates/new\">Add one</a> to get started.</p>";
    } else {
        content << R"(<div class="card">
    <table>
        <thead>
            <tr>
                <th>Rank</th>
                <th>Candidate</th>
                <th>Hand Gestures</th>
                <th>Stayed Awake</th>
                <th>Average</th>
                <th>Scores</th>
            </tr>
        </thead>
        <tbody>
)";
        int rank = 1;
        for (const auto& c : candidates) {
            content << "            <tr>\n";
            content << "                <td>" << rank++ << "</td>\n";
            content << "                <td><a href=\"/candidates/" << html_escape(c.id) << "\">"
                    << html_escape(c.name) << "</a></td>\n";
            if (c.num_scores > 0) {
                content << "                <td>" << std::fixed << std::setprecision(2) << c.avg_hand_gestures << "</td>\n";
                content << "                <td>" << std::fixed << std::setprecision(2) << c.avg_stayed_awake << "</td>\n";
                content << "                <td><strong>" << std::fixed << std::setprecision(2) << c.avg_total << "</strong></td>\n";
            } else {
                content << "                <td>-</td>\n";
                content << "                <td>-</td>\n";
                content << "                <td><strong>-</strong></td>\n";
            }
            content << "                <td>" << c.num_scores << "</td>\n";
            content << "            </tr>\n";
        }
        content << R"(        </tbody>
    </table>
</div>
)";
    }

    return base_template(position_title, user_name, flash, content.str());
}

inline std::string candidate_form_page(const std::string& user_name, const std::string& flash,
                                        const std::string& position_id, const std::string& position_title) {
    std::ostringstream content;
    content << R"(
<div class="breadcrumb">
    <a href="/">Positions</a> &raquo;
    <a href="/positions/)" << html_escape(position_id) << "\">" << html_escape(position_title) << R"(</a> &raquo;
    Add Candidate
</div>

<div class="card">
    <h2>Add Candidate</h2>
    <form method="POST">
        <label for="name">Candidate Name</label>
        <input type="text" id="name" name="name" placeholder="e.g., Dr. Jane Smith" required>
        <button type="submit">Add Candidate</button>
        <a href="/positions/)" << html_escape(position_id) << R"(" class="btn btn-secondary">Cancel</a>
    </form>
</div>
)";
    return base_template("Add Candidate", user_name, flash, content.str());
}

struct CandidateDetail {
    std::string id;
    std::string name;
    std::string position_id;
    std::string position_title;
    std::string student_feedback;
};

struct ScoreStats {
    int num_scores;
    double avg_hand_gestures;
    double avg_stayed_awake;
    double avg_total;
};

struct MyScore {
    bool exists;
    int hand_gestures;
    int stayed_awake;
};

inline std::string score_option(int value, int selected, const std::string& label) {
    std::ostringstream opt;
    opt << "<option value=\"" << value << "\"";
    if (selected == value) opt << " selected";
    opt << ">" << value << " - " << label << "</option>\n";
    return opt.str();
}

inline std::string candidate_detail_page(const std::string& user_name, const std::string& flash,
                                          const CandidateDetail& candidate, const ScoreStats& stats,
                                          const MyScore& my_score) {
    std::ostringstream content;
    content << R"(
<div class="breadcrumb">
    <a href="/">Positions</a> &raquo;
    <a href="/positions/)" << html_escape(candidate.position_id) << "\">"
            << html_escape(candidate.position_title) << R"(</a> &raquo;
    )" << html_escape(candidate.name) << R"(
</div>

<h2>)" << html_escape(candidate.name) << R"(</h2>

<!-- Score Summary -->
<div class="card">
    <h3 style="margin-top: 0;">Score Summary</h3>
)";

    if (stats.num_scores > 0) {
        content << R"(    <div class="stats">
        <div class="stat">
            <div class="stat-label">Hand Gestures</div>
            <div class="stat-value">)" << std::fixed << std::setprecision(2) << stats.avg_hand_gestures << R"(</div>
        </div>
        <div class="stat">
            <div class="stat-label">Stayed Awake</div>
            <div class="stat-value">)" << std::fixed << std::setprecision(2) << stats.avg_stayed_awake << R"(</div>
        </div>
        <div class="stat">
            <div class="stat-label">Average</div>
            <div class="stat-value">)" << std::fixed << std::setprecision(2) << stats.avg_total << R"(</div>
        </div>
        <div class="stat">
            <div class="stat-label">Reviewers</div>
            <div class="stat-value">)" << stats.num_scores << R"(</div>
        </div>
    </div>
)";
    } else {
        content << "    <p>No scores yet. Be the first to score this candidate.</p>\n";
    }

    content << R"(</div>

<!-- Your Score -->
<div class="card">
    <h3 style="margin-top: 0;">Your Score</h3>
    <form method="POST">
        <input type="hidden" name="action" value="score">
        <div class="score-input">
            <div>
                <label for="hand_gestures">Hand Gestures (1-5)</label>
                <select id="hand_gestures" name="hand_gestures" required>
                    <option value="">Select...</option>
)";
    content << score_option(1, my_score.exists ? my_score.hand_gestures : 0, "No gestures");
    content << score_option(2, my_score.exists ? my_score.hand_gestures : 0, "Minimal");
    content << score_option(3, my_score.exists ? my_score.hand_gestures : 0, "Adequate");
    content << score_option(4, my_score.exists ? my_score.hand_gestures : 0, "Expressive");
    content << score_option(5, my_score.exists ? my_score.hand_gestures : 0, "TED-talk caliber");

    content << R"(                </select>
            </div>
            <div>
                <label for="stayed_awake">Stayed Awake (1-5)</label>
                <select id="stayed_awake" name="stayed_awake" required>
                    <option value="">Select...</option>
)";
    content << score_option(1, my_score.exists ? my_score.stayed_awake : 0, "Lost consciousness");
    content << score_option(2, my_score.exists ? my_score.stayed_awake : 0, "Struggled");
    content << score_option(3, my_score.exists ? my_score.stayed_awake : 0, "Stayed awake");
    content << score_option(4, my_score.exists ? my_score.stayed_awake : 0, "Engaged");
    content << score_option(5, my_score.exists ? my_score.stayed_awake : 0, "Riveted");

    content << R"(                </select>
            </div>
        </div>
        <button type="submit">)" << (my_score.exists ? "Update Score" : "Submit Score") << R"(</button>
    </form>
</div>

<!-- Student Feedback -->
<div class="card">
    <h3 style="margin-top: 0;">Student Feedback Reports</h3>
    <form method="POST">
        <input type="hidden" name="action" value="feedback">
        <label for="student_feedback">Historical feedback from students (optional)</label>
        <textarea id="student_feedback" name="student_feedback" placeholder="Paste student feedback or evaluations here...">)"
            << html_escape(candidate.student_feedback) << R"(</textarea>
        <button type="submit">Save Feedback</button>
    </form>
</div>
)";

    return base_template(candidate.name, user_name, flash, content.str());
}

#endif // TEMPLATES_H
