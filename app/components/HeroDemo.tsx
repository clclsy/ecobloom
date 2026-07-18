"use client";

import { useState } from "react";
import PixelFlower from "./PixelFlower";

function stageLabel(health: number) {
  if (health < 20) return { label: "Shriveled", hint: "lights left on, empty room" };
  if (health < 45) return { label: "Wilted", hint: "energy habits slipping" };
  if (health < 75) return { label: "Budding", hint: "getting more mindful" };
  return { label: "In bloom", hint: "lamp dims itself on cue" };
}

export default function HeroDemo() {
  const [health, setHealth] = useState(72);
  const stage = stageLabel(health);

  return (
    <div className="flex flex-col items-center">
      <div className="w-[min(78vw,320px)] aspect-[12/15]">
        <PixelFlower health={health} />
      </div>

      <div className="mt-6 w-full max-w-[320px]">
        <div className="flex items-baseline justify-between font-mono text-xs text-sage-700">
          <span>WASTEFUL</span>
          <span>MINDFUL</span>
        </div>
        <input
          type="range"
          min={0}
          max={100}
          value={health}
          onChange={(e) => setHealth(Number(e.target.value))}
          aria-label="Simulate a week of light-usage habits"
          className="mt-2 w-full accent-pink-700"
        />
        <div className="mt-3 flex items-center justify-between font-mono text-sm">
          <span className="rounded-full bg-sage-950 px-3 py-1 text-paper">
            {stage.label}
          </span>
          <span className="text-sage-700">{stage.hint}</span>
        </div>
      </div>
    </div>
  );
}
