"""
EcoBloom - Mood/AI Logic Server (Person 2's component)

Owns:
- Converting camera states into a mood score
- Logging every state transition with duration
- Exposing endpoints for Person 1 (input) and Person 3 (output/app)

Run with: python ecobloom_mood.py
Then test with: curl http://localhost:5000/state/light_no_person
"""

import time
import json
import threading
from flask import Flask, jsonify, request
from flask_cors import CORS
from backboard import ask_plant, reset_thread

app = Flask(__name__)
CORS(app)  # allows Person 3's frontend (likely a different port) to call this

STATE_LOG_FILE = "event_log.json"

# ---- DEMO MODE ----
# Flip to False for "realistic" timing. True = fast thresholds for live demos.
DEMO_MODE = True

if DEMO_MODE:
    NO_PERSON_LIT_WARNING = 5      # seconds
    NO_PERSON_LIT_CRITICAL = 15    # seconds
    RECOVERY_RATE = 2              # mood recovery per tick
    DECAY_WARNING = 1.5
    DECAY_CRITICAL = 4
else:
    NO_PERSON_LIT_WARNING = 30
    NO_PERSON_LIT_CRITICAL = 120
    RECOVERY_RATE = 0.5
    DECAY_WARNING = 0.5
    DECAY_CRITICAL = 2

VALID_STATES = {"dark", "light_person", "light_no_person"}


class MoodTracker:
    def __init__(self):
        self.current_state = None
        self.state_start_time = None
        self.mood_score = 100.0  # 0-100, 100 = healthy bloom
        self.log = []
        self.lock = threading.Lock()

    def update(self, new_state):
        if new_state not in VALID_STATES:
            raise ValueError(f"Unknown state: {new_state}")

        with self.lock:
            now = time.time()

            if new_state != self.current_state:
                if self.current_state is not None:
                    duration = now - self.state_start_time
                    self._log_event(self.current_state, duration)
                self.current_state = new_state
                self.state_start_time = now

            elapsed = now - self.state_start_time

            if new_state == "light_no_person":
                if elapsed > NO_PERSON_LIT_CRITICAL:
                    self.mood_score = max(0, self.mood_score - DECAY_CRITICAL)
                elif elapsed > NO_PERSON_LIT_WARNING:
                    self.mood_score = max(0, self.mood_score - DECAY_WARNING)
            else:
                self.mood_score = min(100, self.mood_score + RECOVERY_RATE)

            return round(self.mood_score, 1)

    def get_tier(self):
        """Maps mood score to a plant health tier for the app's plant graphic / HP bar."""
        if self.mood_score >= 75:
            return "blooming"
        elif self.mood_score >= 50:
            return "healthy"
        elif self.mood_score >= 25:
            return "wilting"
        else:
            return "dying"

    def _log_event(self, state, duration):
        entry = {
            "state": state,
            "duration_seconds": round(duration, 1),
            "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
        }
        self.log.append(entry)
        self._save_log()

    def _save_log(self):
        with open(STATE_LOG_FILE, "w") as f:
            json.dump(self.log, f, indent=2)


tracker = MoodTracker()


# ---- ENDPOINTS ----

@app.route("/state/<state>")
def update_state(state):
    """Person 1 calls this every ~1 second with the detected camera state."""
    try:
        mood = tracker.update(state)
        return jsonify({"mood": mood, "tier": tracker.get_tier(), "state": state})
    except ValueError as e:
        return jsonify({"error": str(e)}), 400


@app.route("/mood")
def get_mood():
    """Person 3's app polls this for the live HP bar / plant graphic."""
    return jsonify({
        "mood": round(tracker.mood_score, 1),
        "tier": tracker.get_tier(),
        "current_state": tracker.current_state,
    })


@app.route("/log")
def get_log():
    """Person 3's app + Backboard use this for 'why is my plant dying' reasoning."""
    return jsonify(tracker.log)


@app.route("/chat", methods=["POST"])
def chat():
    """Person 3's chatbot UI posts {"question": "..."} here.
    Answer is grounded in the real event log + current mood."""
    data = request.get_json(silent=True) or {}
    question = data.get("question", "").strip()
    if not question:
        return jsonify({"error": "missing 'question' field"}), 400

    answer = ask_plant(
        question=question,
        event_log=tracker.log,
        mood_score=round(tracker.mood_score, 1),
        tier=tracker.get_tier(),
    )
    return jsonify({"answer": answer})


@app.route("/reset")
def reset():
    """Handy for demo re-runs without restarting the server."""
    global tracker
    tracker = MoodTracker()
    reset_thread()  # start a fresh Backboard conversation too
    return jsonify({"status": "reset"})


if __name__ == "__main__":
    app.run(debug=True, port=5000)