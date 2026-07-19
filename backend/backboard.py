"""
EcoBloom - Backboard.io chatbot integration

Handles the "why is my plant dying?" conversational feature.
Grounds answers in the real event log from ecobloom_mood.py.

SETUP:
1. Copy .env.example to .env
2. Paste your real Backboard API key into .env
3. pip install python-dotenv requests --break-system-packages
"""

import os
import json
import requests
from dotenv import load_dotenv

load_dotenv()  # reads .env file into environment variables

API_KEY = os.getenv("BACKBOARD_API_KEY")
BASE_URL = "https://app.backboard.io/api"

HEADERS = {"X-API-Key": API_KEY, "Content-Type": "application/json"}

# In-memory thread tracking. In a real app you'd key this per user/session;
# for a hackathon demo, one global thread is fine.
_thread_id = None


def ask_plant(question, event_log, mood_score=None, tier=None):
    """
    Ask the EcoBloom chatbot a question, grounded in the real event log.

    Args:
        question: user's question, e.g. "why is my plant dying?"
        event_log: list of {state, duration_seconds, timestamp} dicts
                   (this is tracker.log from ecobloom_mood.py)
        mood_score: optional current mood score for extra context
        tier: optional current tier (blooming/healthy/wilting/dying)

    Returns:
        str: the chatbot's response
    """
    global _thread_id

    # Keep the log reasonably small in the prompt - last 15 events is plenty
    recent_events = event_log[-15:] if event_log else []
    log_summary = json.dumps(recent_events, indent=2)

    context_line = ""
    if mood_score is not None:
        context_line = f"Current health: {mood_score}/100 ({tier})\n"

    prompt = f"""You are EcoBloom, a virtual houseplant that reacts to a household's
energy habits (lights left on in empty rooms make you wilt; efficient habits make you bloom).

{context_line}Recent sensor events (state = room condition, duration_seconds = how long it lasted):
{log_summary}

The user asks: "{question}"

Answer in a warm, slightly playful voice as the plant. Reference the ACTUAL events above
(don't make up numbers). Keep it under 80 words. End with one concrete, specific suggestion
based on the actual pattern you see in the events."""

    payload = {"content": prompt}
    if _thread_id:
        payload["thread_id"] = _thread_id  # continue same conversation

    try:
        response = requests.post(
            f"{BASE_URL}/threads/messages",
            json=payload,
            headers=HEADERS,
            timeout=15,
        )
        response.raise_for_status()
        result = response.json()
        _thread_id = result.get("thread_id", _thread_id)  # save for follow-ups
        return result["content"]

    except requests.exceptions.RequestException as e:
        # Never let a Backboard hiccup crash your demo - fall back gracefully
        print(f"Backboard error: {e}")
        return _fallback_response(recent_events)


def _fallback_response(recent_events):
    """If Backboard is unreachable mid-demo, give a reasonable canned answer
    instead of crashing. Better than a stack trace on stage."""
    if not recent_events:
        return "I don't have enough data yet to explain how I'm doing!"

    worst = max(recent_events, key=lambda e: e.get("duration_seconds", 0)
                if e.get("state") == "light_no_person" else 0)
    return (f"I noticed the lights were on with no one around for "
            f"{worst.get('duration_seconds', '?')} seconds. Try switching off "
            f"lights when you leave a room - that's the biggest thing hurting me right now.")


def reset_thread():
    """Call this alongside your mood tracker reset, so a new demo run
    starts a fresh conversation instead of remembering the old one."""
    global _thread_id
    _thread_id = None


if __name__ == "__main__":
    # Quick manual test - run this file directly to check your API key works
    fake_log = [
        {"state": "light_no_person", "duration_seconds": 145.0, "timestamp": "2026-07-18 20:00:00"},
        {"state": "light_person", "duration_seconds": 30.0, "timestamp": "2026-07-18 20:02:25"},
        {"state": "light_no_person", "duration_seconds": 90.0, "timestamp": "2026-07-18 20:02:55"},
    ]
    answer = ask_plant("Why are you dying?", fake_log, mood_score=32, tier="wilting")
    print("Plant says:", answer)