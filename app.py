"""Candidate Scoring App - Flask Backend"""

import sqlite3
import uuid
from functools import wraps
from flask import Flask, render_template, request, redirect, url_for, g, flash

app = Flask(__name__)
app.secret_key = 'dev-secret-key-change-in-production'

DATABASE = 'candidate_scoring.db'


def get_db():
    """Get database connection for current request."""
    if 'db' not in g:
        g.db = sqlite3.connect(DATABASE)
        g.db.row_factory = sqlite3.Row
        g.db.execute('PRAGMA foreign_keys = ON')
    return g.db


@app.teardown_appcontext
def close_db(exception):
    """Close database connection at end of request."""
    db = g.pop('db', None)
    if db is not None:
        db.close()


def get_current_user():
    """
    Get current user from SSO headers.
    In production, this reads from SSO-provided headers.
    For development, uses mock headers or creates a test user.
    """
    # SSO would typically set these headers
    user_id = request.headers.get('X-SSO-User-ID')
    email = request.headers.get('X-SSO-Email')
    name = request.headers.get('X-SSO-Name')

    # Development fallback: use query params or defaults
    if not user_id:
        user_id = request.args.get('user_id', 'dev-user-1')
        email = request.args.get('email', 'dev@university.edu')
        name = request.args.get('name', 'Dev User')

    db = get_db()

    # Ensure user exists in database
    existing = db.execute('SELECT * FROM users WHERE id = ?', (user_id,)).fetchone()
    if not existing:
        db.execute(
            'INSERT INTO users (id, email, display_name) VALUES (?, ?, ?)',
            (user_id, email, name)
        )
        db.commit()
        existing = db.execute('SELECT * FROM users WHERE id = ?', (user_id,)).fetchone()

    return existing


def login_required(f):
    """Decorator to require authenticated user."""
    @wraps(f)
    def decorated(*args, **kwargs):
        g.user = get_current_user()
        return f(*args, **kwargs)
    return decorated


# --- Routes ---

@app.route('/')
@login_required
def index():
    """Home page - list all positions."""
    db = get_db()
    positions = db.execute('''
        SELECT p.*, u.display_name as creator_name,
               COUNT(DISTINCT c.id) as candidate_count
        FROM positions p
        JOIN users u ON p.created_by = u.id
        LEFT JOIN candidates c ON c.position_id = p.id
        GROUP BY p.id
        ORDER BY p.created_at DESC
    ''').fetchall()
    return render_template('index.html', positions=positions, user=g.user)


@app.route('/positions/new', methods=['GET', 'POST'])
@login_required
def new_position():
    """Create a new position."""
    if request.method == 'POST':
        title = request.form.get('title', '').strip()
        if not title:
            flash('Position title is required.')
            return render_template('position_form.html', user=g.user)

        db = get_db()
        position_id = str(uuid.uuid4())
        db.execute(
            'INSERT INTO positions (id, title, created_by) VALUES (?, ?, ?)',
            (position_id, title, g.user['id'])
        )
        db.commit()
        flash('Position created.')
        return redirect(url_for('position_detail', position_id=position_id))

    return render_template('position_form.html', user=g.user)


@app.route('/positions/<position_id>')
@login_required
def position_detail(position_id):
    """View position with candidate rankings."""
    db = get_db()

    position = db.execute(
        'SELECT * FROM positions WHERE id = ?', (position_id,)
    ).fetchone()

    if not position:
        flash('Position not found.')
        return redirect(url_for('index'))

    # Get candidates with rankings
    candidates = db.execute('''
        SELECT * FROM candidate_rankings
        WHERE position_id = ?
        ORDER BY avg_total DESC NULLS LAST, candidate_name
    ''', (position_id,)).fetchall()

    return render_template(
        'position_detail.html',
        position=position,
        candidates=candidates,
        user=g.user
    )


@app.route('/positions/<position_id>/candidates/new', methods=['GET', 'POST'])
@login_required
def new_candidate(position_id):
    """Add a new candidate to a position."""
    db = get_db()

    position = db.execute(
        'SELECT * FROM positions WHERE id = ?', (position_id,)
    ).fetchone()

    if not position:
        flash('Position not found.')
        return redirect(url_for('index'))

    if request.method == 'POST':
        name = request.form.get('name', '').strip()
        if not name:
            flash('Candidate name is required.')
            return render_template('candidate_form.html', position=position, user=g.user)

        candidate_id = str(uuid.uuid4())
        db.execute(
            'INSERT INTO candidates (id, position_id, name) VALUES (?, ?, ?)',
            (candidate_id, position_id, name)
        )
        db.commit()
        flash('Candidate added.')
        return redirect(url_for('candidate_detail', candidate_id=candidate_id))

    return render_template('candidate_form.html', position=position, user=g.user)


@app.route('/candidates/<candidate_id>', methods=['GET', 'POST'])
@login_required
def candidate_detail(candidate_id):
    """View and score a candidate."""
    db = get_db()

    candidate = db.execute('''
        SELECT c.*, p.title as position_title, p.id as position_id
        FROM candidates c
        JOIN positions p ON c.position_id = p.id
        WHERE c.id = ?
    ''', (candidate_id,)).fetchone()

    if not candidate:
        flash('Candidate not found.')
        return redirect(url_for('index'))

    if request.method == 'POST':
        action = request.form.get('action')

        if action == 'score':
            hand_gestures = request.form.get('hand_gestures', type=int)
            stayed_awake = request.form.get('stayed_awake', type=int)

            if not (1 <= hand_gestures <= 5) or not (1 <= stayed_awake <= 5):
                flash('Scores must be between 1 and 5.')
            else:
                # Upsert score
                existing = db.execute(
                    'SELECT id FROM scores WHERE candidate_id = ? AND interviewer_id = ?',
                    (candidate_id, g.user['id'])
                ).fetchone()

                if existing:
                    db.execute('''
                        UPDATE scores
                        SET hand_gestures = ?, stayed_awake = ?, updated_at = datetime('now')
                        WHERE id = ?
                    ''', (hand_gestures, stayed_awake, existing['id']))
                else:
                    db.execute('''
                        INSERT INTO scores (id, candidate_id, interviewer_id, hand_gestures, stayed_awake)
                        VALUES (?, ?, ?, ?, ?)
                    ''', (str(uuid.uuid4()), candidate_id, g.user['id'], hand_gestures, stayed_awake))

                db.commit()
                flash('Score saved.')

        elif action == 'feedback':
            feedback = request.form.get('student_feedback', '').strip()
            db.execute(
                'UPDATE candidates SET student_feedback = ? WHERE id = ?',
                (feedback, candidate_id)
            )
            db.commit()
            flash('Feedback saved.')

        return redirect(url_for('candidate_detail', candidate_id=candidate_id))

    # Get current user's score
    my_score = db.execute(
        'SELECT * FROM scores WHERE candidate_id = ? AND interviewer_id = ?',
        (candidate_id, g.user['id'])
    ).fetchone()

    # Get aggregated scores
    stats = db.execute('''
        SELECT
            COUNT(*) as num_scores,
            ROUND(AVG(hand_gestures), 2) as avg_hand_gestures,
            ROUND(AVG(stayed_awake), 2) as avg_stayed_awake,
            ROUND((AVG(hand_gestures) + AVG(stayed_awake)) / 2, 2) as avg_total
        FROM scores WHERE candidate_id = ?
    ''', (candidate_id,)).fetchone()

    # Refresh candidate data after potential update
    candidate = db.execute('''
        SELECT c.*, p.title as position_title, p.id as position_id
        FROM candidates c
        JOIN positions p ON c.position_id = p.id
        WHERE c.id = ?
    ''', (candidate_id,)).fetchone()

    return render_template(
        'candidate_detail.html',
        candidate=candidate,
        my_score=my_score,
        stats=stats,
        user=g.user
    )


if __name__ == '__main__':
    app.run(debug=True, port=5000)
