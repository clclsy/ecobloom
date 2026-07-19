"use client";

import { useEffect, useRef, useState } from "react";
import FlowerOrb from "./FlowerOrb";

export default function HeroDemo() {
  const [lightsOn, setLightsOn] = useState(false);
  const [minutesLeft, setMinutesLeft] = useState(0);
  const intervalRef = useRef<ReturnType<typeof setInterval> | null>(null);

  useEffect(() => {
    const tick = () => {
      setMinutesLeft((m) =>
        lightsOn ? Math.min(m + 1, 45) : Math.max(m - 3, 0)
      );
    };
    intervalRef.current = setInterval(tick, lightsOn ? 450 : 200);
    return () => {
      if (intervalRef.current) clearInterval(intervalRef.current);
    };
  }, [lightsOn]);

  const vitality = Math.max(6, 100 - minutesLeft * 2.1);

  return (
    <div className="flex flex-col items-center">
      <FlowerOrb vitality={vitality} />

      <div className="mt-6 flex flex-col items-center gap-3">
        <button
          onClick={() => setLightsOn((v) => !v)}
          aria-pressed={lightsOn}
          className="group flex items-center gap-3 rounded-full bg-paper/80 border border-ink/10 pl-4 pr-1.5 py-1.5 shadow-sm hover:shadow-md transition-shadow"
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
        </button>
        <p className="text-xs text-ink-faint tracking-wide">
          try it, this is the sensor talking to the flower, live
        </p>
      </div>
    </div>
  );
}
