"""
ecobloom_mood.py

Converts a stream of camera light readings into a simple "plant health"
mood score, based on nothing more than how long the light is left on.

This is the feasibility-test version: no person/face detection, just
light on vs. light off. The rule is deliberately simple:
  - The light being on for a long continuous stretch drains the plant's
    health (mood_score).
  - Turning the light off lets the plant recover.
  - A short grace period means quick trips into a room don't hurt you.

Feed it readings via process_reading(light_on=...) every time a camera
report arrives. It maintains:
  - mood_score: 0-100
  - tier: blooming / healthy / wilting / dying
  - log: a list of {state, duration_seconds, timestamp} dicts, in the
    exact shape backboard.py's ask_plant() expects as event_log.
"""

import time
from dataclasses import dataclass, field
from typing import List, Optional

# Tune these during your demo -- they're the whole "model" for now.
DRAIN_PER_SECOND = 0.05        # mood lost per second the light is continuously on (past the grace period)
RECOVER_PER_SECOND = 0.02      # mood regained per second the light is off
DRAIN_GRACE_SECONDS = 30       # lights can be on this long before mood starts dropping

TIERS = [
    (75, "blooming"),
    (50, "healthy"),
    (25, "wilting"),
    (0, "dying"),
]


def tier_for_score(score: float) -> str:
    for threshold, name in TIERS:
        if score >= threshold:
            return name
    return "dying"


@dataclass
class MoodTracker:
    mood_score: float = 100.0
    log: List[dict] = field(default_factory=list)

    _current_state: Optional[str] = None        # "light_on" | "light_off"
    _state_started_at: Optional[float] = None    # epoch seconds
    _last_update_at: Optional[float] = None

    def process_reading(self, light_on: bool, timestamp: Optional[float] = None):
        """
        Feed one reading in. Call this every time a camera report arrives.

        Args:
            light_on: whether the camera currently sees the light as on.
            timestamp: epoch seconds; defaults to now. Pass the camera's
                       own reading time if you have it, so mood updates
                       stay accurate even if reports arrive in a burst.
        """
        now = timestamp if timestamp is not None else time.time()
        new_state = "light_on" if light_on else "light_off"

        if self._current_state is None:
            self._current_state = new_state
            self._state_started_at = now
            self._last_update_at = now
            return

        elapsed_since_update = max(0.0, now - self._last_update_at)

        if new_state == self._current_state:
            # Still in the same state -- apply drain/recovery for the elapsed time.
            self._apply_score_delta(self._current_state, elapsed_since_update, now)
        else:
            # State changed -- close out the previous run as a log entry.
            duration = now - self._state_started_at
            self.log.append({
                "state": self._current_state,
                "duration_seconds": round(duration, 1),
                "timestamp": now,
            })
            self._current_state = new_state
            self._state_started_at = now

        self._last_update_at = now

    def _apply_score_delta(self, state: str, elapsed_seconds: float, now: float):
        if state == "light_on":
            run_duration = now - self._state_started_at if self._state_started_at else 0
            if run_duration > DRAIN_GRACE_SECONDS:
                self.mood_score -= DRAIN_PER_SECOND * elapsed_seconds
        else:
            self.mood_score += RECOVER_PER_SECOND * elapsed_seconds

        self.mood_score = max(0.0, min(100.0, self.mood_score))

    @property
    def tier(self) -> str:
        return tier_for_score(self.mood_score)

    def reset(self):
        self.mood_score = 100.0
        self.log = []
        self._current_state = None
        self._state_started_at = None
        self._last_update_at = None
