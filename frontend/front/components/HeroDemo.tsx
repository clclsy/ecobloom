"use client";

import { useEffect, useRef, useState } from "react";
import FlowerOrb from "./FlowerOrb";

// Replace with real sensor readings, e.g. fetched from an API or CSV.
// Each entry represents "lights on" state at a simulated tick.
const TEST_DATA: boolean[] = [
  false, false, false, false, true, true, true, true, true, true,
  true, true, false, false, true, true, true, false, false, false,
];

export default function HeroDemo() {
  const [step, setStep] = useState(0);
  const [minutesLeft, setMinutesLeft] = useState(0);
  const intervalRef = useRef<ReturnType<typeof setInterval> | null>(null);

  // Fixed-tempo loop, driven purely by data — no dependency on click state,
  // so it never gets torn down/rebuilt mid-playback.
  useEffect(() => {
    intervalRef.current = setInterval(() => {
      setStep((s) => {
        const next = s + 1;
        const lightsOnNow = TEST_DATA[next % TEST_DATA.length];
        setMinutesLeft((m) =>
          lightsOnNow ? Math.min(m + 1, 45) : Math.max(m - 3, 0)
        );
        return next;
      });
    }, 300);

    return () => {
      if (intervalRef.current) clearInterval(intervalRef.current);
    };
  }, []);

  const lightsOn = TEST_DATA[step % TEST_DATA.length];
  const vitality = Math.max(6, 100 - minutesLeft * 2.1);

  return (
    <div className="flex flex-col items-center">
      <FlowerOrb vitality={vitality} />

      <div className="mt-6 flex flex-col items-center gap-3">
        <div
          aria-live="polite"
          className="flex items-center gap-3 rounded-full bg-paper/80 border border-ink/10 pl-4 pr-1.5 py-1.5 shadow-sm"
        >
          <span className="text-sm font-medium text-ink-soft">
            {lightsOn ? "an empty room, light left on" : "lights off, all clear"}
          </span>
          <span
            className={`relative w-11 h-6 rounded-full transition-colors duration-300 ${
              lightsOn ? "bg-wilt" : "bg-green"
            }`}
          >
            <span
              className={`absolute top-0.5 left-0.5 w-5 h-5 rounded-full bg-paper shadow transition-transform duration-300 ${
                lightsOn ? "translate-x-5" : "translate-x-0"
              }`}
            />
          </span>
        </div>
        <p className="text-xs text-ink-faint tracking-wide">
          this is real sensor data, playing back live
        </p>
      </div>
    </div>
  );
}