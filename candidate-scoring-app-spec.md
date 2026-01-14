# Candidate Scoring App Specification

## Overview

A minimal web application for academic hiring committees to score interview candidates on two criteria and view aggregated rankings.

---

## Users & Authentication

- **User type**: Interviewers (hiring committee members)
- **Authentication**: University SSO integration
- **Permissions**: All authenticated users can:
  - Create positions
  - Add candidates to positions
  - Score candidates
  - View rankings

---

## Data Model

### Position
| Field | Type | Description |
|-------|------|-------------|
| id | UUID | Primary key |
| title | String | Position title (e.g., "Assistant Professor - Chemistry") |
| created_at | Timestamp | When the position was created |
| created_by | User ID | Who created the position |

### Candidate
| Field | Type | Description |
|-------|------|-------------|
| id | UUID | Primary key |
| position_id | UUID | Foreign key to Position |
| name | String | Candidate's full name |
| student_feedback | Text | Plain text field for historical student feedback reports (optional) |
| created_at | Timestamp | When the candidate was added |

### Score
| Field | Type | Description |
|-------|------|-------------|
| id | UUID | Primary key |
| candidate_id | UUID | Foreign key to Candidate |
| interviewer_id | User ID | Who submitted the score |
| hand_gestures | Integer | 1-5 rating for quality of hand gestures |
| stayed_awake | Integer | 1-5 rating for whether interviewer stayed awake |
| created_at | Timestamp | When the score was submitted |

**Constraint**: One score per interviewer per candidate (upsert on resubmission).

---

## Scoring Criteria

Two fixed criteria, each rated on a 1-5 scale:

1. **Quality of Hand Gestures** (1-5)
   - 1 = No gestures / hands in pockets
   - 5 = Emphatic, expressive, TED-talk caliber

2. **Stayed Awake** (1-5)
   - 1 = Lost consciousness entirely
   - 5 = Riveted throughout

---

## Score Aggregation

- **Method**: Arithmetic mean across all interviewers
- **Display**: Average score per criterion + total average
- **Ranking**: Candidates ranked by total average score (descending)

---

## User Interface

### Pages

1. **Home / Position List**
   - List of all positions
   - Button to create new position

2. **Position Detail**
   - Position title
   - List of candidates with their average scores
   - Candidates sorted by total average score (ranking)
   - Button to add new candidate

3. **Candidate Detail**
   - Candidate name
   - Student feedback field (editable text area)
   - Scoring form (two 1-5 inputs)
   - Display of current user's submitted score (if any)
   - Summary of all scores (averaged)

4. **Add/Edit Forms**
   - Create position: title field
   - Add candidate: name field

### Design Principles
- Minimal, functional UI
- No unnecessary styling or animations
- Mobile-friendly (committee members may score from phones)

---

## Technical Requirements

### Stack (Suggested)
- **Frontend**: Simple HTML/CSS/JS or lightweight framework (e.g., React, Vue, Svelte)
- **Backend**: Any (Node.js, Python/Flask, etc.)
- **Database**: PostgreSQL or SQLite
- **Auth**: SAML/OAuth integration with university SSO

### Scale
- Expected: 5-10 candidates per position
- Low traffic; no special performance optimization needed

### Persistence
- All data persisted to database
- No session-only storage

---

## Non-Requirements

The following are explicitly **out of scope**:
- Export to CSV/PDF
- Email notifications
- Comments/notes beyond the student feedback field
- Configurable scoring criteria
- Admin vs. regular user roles
- Audit logging

---

## Future Considerations (Not in v1)

- Additional scoring criteria
- Weighted scoring
- Export functionality
- Interview scheduling integration
