# server.py
#
# Backend for the carbon emission dashboard.
#   - Receives camera readings from the QNX/Raspberry Pi device (camera_test.cpp)
#   - Accepts manual entries (gas, electricity) from the website's UI
#   - Serves aggregated data to the React dashboard
#   - Generates a monthly report using Backboard.io (routes to whatever
#     model you configure -- Claude, GPT, etc. -- through one API)
#
# Setup:
#   pip install flask flask-cors requests
#   export BACKBOARD_API_KEY=your_key_here   (from app.backboard.io -> Settings -> API Keys)
#   python server.py
#
# Storage is a flat JSON file (data.json) so the demo survives a restart
# without needing a real database.

import json
import os
from datetime import datetime, timezone
from pathlib import Path

import requests
from flask import Flask, jsonify, request
from flask_cors import CORS

from ecobloom_mood import MoodTracker
from backboard import ask_plant, reset_thread as reset_plant_thread

app = Flask(__name__)
CORS(app)  # allow the React frontend (different origin/port) to call this API

DATA_FILE = Path(__file__).parent / "data.json"

BACKBOARD_API_KEY = os.environ.get("BACKBOARD_API_KEY", "")
BACKBOARD_BASE_URL = "https://app.backboard.io/api"

# One tracker for the demo device. If you ever support multiple devices,
# key this dict by device_id instead of using a single global tracker.
mood_tracker = MoodTracker()

# ---------------------------------------------------------------------
# Storage helpers
# ---------------------------------------------------------------------
def load_data():
    if not DATA_FILE.exists():
        return {"cameraReadings": [], "manualEntries": []}
    with open(DATA_FILE, "r") as f:
        return json.load(f)


def save_data(data):
    with open(DATA_FILE, "w") as f:
        json.dump(data, f, indent=2)


def _parse_timestamp_to_epoch(timestamp_str):
    """Turn the device's ISO8601 timestamp (e.g. '2026-07-19T14:03:22Z')
    into epoch seconds for the mood tracker. Falls back to now() if it
    can't be parsed."""
    if not timestamp_str:
        return datetime.now(timezone.utc).timestamp()
    try:
        # fromisoformat doesn't accept a trailing 'Z' before Python 3.11
        return datetime.fromisoformat(timestamp_str.replace("Z", "+00:00")).timestamp()
    except ValueError:
        return datetime.now(timezone.utc).timestamp()


# ---------------------------------------------------------------------
# POST /api/readings
# Called by camera_test.cpp on the device every ReportIntervalSeconds.
# Body: {
#   "device_id": "rpi5-demo-01",
#   "carbon_score": 12.3456,
#   "timestamp": "2026-07-19T14:03:22Z",
#   "light_on": true          <- optional, drives the EcoBloom mood tracker
# }
#
# carbon_score from the device is the device's *cumulative* gCO2e since it
# booted. We store the device's own timestamp if provided (see the
# camera_test.cpp update that adds this field), else fall back to arrival time.
#
# If light_on is present, we also feed it into mood_tracker so the plant's
# health reflects how long the light has been left on -- this is the
# simplified, no-person-detection feasibility test.
# ---------------------------------------------------------------------
@app.route("/api/readings", methods=["POST"])
def post_reading():
    body = request.get_json(silent=True) or {}
    device_id = body.get("device_id")
    carbon_score = body.get("carbon_score")
    timestamp = body.get("timestamp")
    light_on = body.get("light_on")

    if not isinstance(device_id, str) or not isinstance(carbon_score, (int, float)):
        return jsonify({"error": "device_id (string) and carbon_score (number) are required"}), 400

    data = load_data()
    data["cameraReadings"].append({
        "device_id": device_id,
        "carbon_score": carbon_score,
        "timestamp": timestamp or datetime.now(timezone.utc).isoformat(),
    })
    save_data(data)

    if isinstance(light_on, bool):
        mood_tracker.process_reading(light_on=light_on, timestamp=_parse_timestamp_to_epoch(timestamp))

    print(f"[readings] {device_id} -> {carbon_score} g CO2e (cumulative), light_on={light_on}")
    return jsonify({"ok": True}), 201


@app.route("/api/readings", methods=["GET"])
def get_readings():
    device_id = request.args.get("device_id")
    data = load_data()
    readings = data["cameraReadings"]
    if device_id:
        readings = [r for r in readings if r["device_id"] == device_id]
    return jsonify(readings)


# ---------------------------------------------------------------------
# POST /api/manual-entry
# Called from the website when the user logs gas/electricity usage.
# Body: { "type": "electricity" | "gas", "amount": 45.2, "unit": "kWh" | "m3" | "L", "date": "2026-07-19" }
# ---------------------------------------------------------------------
EMISSION_FACTORS = {
    # grams CO2e per unit -- rough defaults, tune for your region/prize criteria
    "electricity_kWh": 30,  # Ontario/IESO-ish grid intensity
    "gas_m3": 1900,         # natural gas, per cubic meter
    "gas_L": 2300,          # e.g. gasoline/propane, per liter -- adjust to your use case
}


@app.route("/api/manual-entry", methods=["POST"])
def post_manual_entry():
    body = request.get_json(silent=True) or {}
    entry_type = body.get("type")
    amount = body.get("amount")
    unit = body.get("unit")
    date = body.get("date")

    if not isinstance(entry_type, str) or not isinstance(amount, (int, float)) or not isinstance(unit, str):
        return jsonify({"error": "type (string), amount (number), unit (string) are required"}), 400

    factor = EMISSION_FACTORS.get(f"{entry_type}_{unit}")
    estimated_grams_co2e = factor * amount if factor else None

    data = load_data()
    data["manualEntries"].append({
        "type": entry_type,
        "amount": amount,
        "unit": unit,
        "date": date or datetime.now(timezone.utc).date().isoformat(),
        "estimatedGramsCO2e": estimated_grams_co2e,
    })
    save_data(data)

    return jsonify({"ok": True, "estimatedGramsCO2e": estimated_grams_co2e}), 201


@app.route("/api/manual-entries", methods=["GET"])
def get_manual_entries():
    return jsonify(load_data()["manualEntries"])


# ---------------------------------------------------------------------
# GET /api/summary
# Aggregates everything into a shape that's easy to chart and easy to
# feed into the LLM report prompt.
# ---------------------------------------------------------------------
def compute_summary():
    data = load_data()

    camera_total_g = max((r["carbon_score"] for r in data["cameraReadings"]), default=0)
    manual_total_g = sum(e["estimatedGramsCO2e"] or 0 for e in data["manualEntries"])

    return {
        "cameraTotalGramsCO2e": camera_total_g,
        "manualTotalGramsCO2e": manual_total_g,
        "combinedTotalGramsCO2e": camera_total_g + manual_total_g,
        "readingCount": len(data["cameraReadings"]),
        "manualEntryCount": len(data["manualEntries"]),
    }


@app.route("/api/summary", methods=["GET"])
def get_summary():
    return jsonify(compute_summary())


# ---------------------------------------------------------------------
# POST /api/report
# Generates a natural-language monthly report via Backboard.io, based on
# the stored camera + manual data.
# ---------------------------------------------------------------------
@app.route("/api/report", methods=["POST"])
def post_report():
    if not BACKBOARD_API_KEY:
        return jsonify({"error": "BACKBOARD_API_KEY is not set on the server"}), 500

    data = load_data()
    summary = compute_summary()

    prompt = f"""You are a sustainability coach. Here is a household's carbon emissions data for the month:

Camera-estimated lighting/electricity-related emissions (cumulative): {summary['cameraTotalGramsCO2e']:.2f} g CO2e
Manually logged gas/electricity emissions: {summary['manualTotalGramsCO2e']:.2f} g CO2e
Combined total: {summary['combinedTotalGramsCO2e']:.2f} g CO2e
Number of manual entries logged: {summary['manualEntryCount']}

Raw manual entries: {json.dumps(data['manualEntries'])}

Write a short, encouraging monthly report (under 250 words) that:
1. States how they did this month in plain terms.
2. Highlights the single biggest contributor to their footprint.
3. Gives 2-3 concrete, specific suggestions to reduce it next month.
Keep it warm and non-judgmental, not preachy."""

    try:
        resp = requests.post(
            f"{BACKBOARD_BASE_URL}/threads/messages",
            json={"content": prompt},
            # Swap in a specific model/provider if you want, e.g.:
            # json={"content": prompt, "llm_provider": "anthropic", "model_name": "claude-sonnet-4-6"},
            headers={"X-API-Key": BACKBOARD_API_KEY, "Content-Type": "application/json"},
            timeout=30,
        )
        resp.raise_for_status()
        result = resp.json()
        return jsonify({"report": result.get("content", "")})
    except requests.RequestException as err:
        print(f"Backboard request failed: {err}")
        return jsonify({"error": "Failed to generate report"}), 500


# ---------------------------------------------------------------------
# GET /api/mood
# Current EcoBloom plant health, for the dashboard's plant widget.
# ---------------------------------------------------------------------
@app.route("/api/mood", methods=["GET"])
def get_mood():
    return jsonify({
        "mood_score": round(mood_tracker.mood_score, 1),
        "tier": mood_tracker.tier,
        "log": mood_tracker.log[-15:],  # recent events, same window ask_plant uses
    })


# ---------------------------------------------------------------------
# POST /api/ask-plant
# The "why is my plant dying?" conversational feature. Grounds the answer
# in the real light on/off event log via backboard.py's ask_plant().
# Body: { "question": "why are you dying?" }
# ---------------------------------------------------------------------
@app.route("/api/ask-plant", methods=["POST"])
def post_ask_plant():
    body = request.get_json(silent=True) or {}
    question = body.get("question", "How are you doing?")

    answer = ask_plant(
        question,
        mood_tracker.log,
        mood_score=round(mood_tracker.mood_score, 1),
        tier=mood_tracker.tier,
    )
    return jsonify({
        "answer": answer,
        "mood_score": round(mood_tracker.mood_score, 1),
        "tier": mood_tracker.tier,
    })


# ---------------------------------------------------------------------
# POST /api/reset
# Resets the mood tracker and the plant's conversation thread together,
# so a fresh demo run doesn't carry over stale mood/context.
# ---------------------------------------------------------------------
@app.route("/api/reset", methods=["POST"])
def post_reset():
    mood_tracker.reset()
    reset_plant_thread()
    return jsonify({"ok": True})


if __name__ == "__main__":
    port = int(os.environ.get("PORT", 8000))
    print(f"Carbon backend listening on http://0.0.0.0:{port}")
    print(f"Point camera_test.cpp's ReportHost/ReportPort at this machine's LAN IP and {port}.")
    app.run(host="0.0.0.0", port=port, debug=True)