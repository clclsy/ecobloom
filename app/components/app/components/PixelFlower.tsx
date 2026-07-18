"use client";

import { useMemo } from "react";

/**
 * Hand-drawn pixel map of the Ecobloom hardware.
 * P = petal, C = center (the LED / lamp core), S = stem, L = leaf
 */
const GRID = [
  "....PPP....",
  "...PPPPP...",
  "..PPPPPPP..",
  "..PPCCCPP..",
  "..PPPPPPP..",
  "...PPPPP...",
  "....PPP....",
  ".....S.....",
  ".....S.....",
  "...L.S.L...",
  "..LL.S.LL..",
  ".....S.....",
  ".....S.....",
  ".....S.....",
  "....SSS....",
];

const COLS = GRID[0].length;
const ROWS = GRID.length;
const HEAD_ROWS = 7; // rows 0-6 are the flower head, rotates as one group

function mix(hexA: string, hexB: string, t: number) {
  const a = hexA.match(/\w\w/g)!.map((h) => parseInt(h, 16));
  const b = hexB.match(/\w\w/g)!.map((h) => parseInt(h, 16));
  const c = a.map((v, i) => Math.round(v + (b[i] - v) * t));
  return `rgb(${c[0]}, ${c[1]}, ${c[2]})`;
}

/**
 * health: 0 (neglected / lights left on) -> 100 (mindful / lamp dims itself)
 */
export default function PixelFlower({ health }: { health: number }) {
  const t = health / 100;

  const petalColor = mix("#a99483", "#e88aa6", t);
  const centerColor = mix("#c9c2b4", "#f3c064", t);
  const stemColor = mix("#a99483", "#4f7a5e", t);
  const leafColor = mix("#a99483", "#8fbf9f", t);

  const droopDeg = -34 * (1 - t); // 0 upright at full health, -34deg wilted
  const droopY = 10 * (1 - t);

  const glow = 2 + t * 16;

  const cells = useMemo(() => {
    const out: { x: number; y: number; part: string }[] = [];
    GRID.forEach((row, y) => {
      row.split("").forEach((part, x) => {
        if (part !== ".") out.push({ x, y, part });
      });
    });
    return out;
  }, []);

  const size = 22; // px per pixel

  return (
    <svg
      viewBox={`0 0 ${COLS * size} ${ROWS * size}`}
      width="100%"
      height="100%"
      role="img"
      aria-label={
        t > 0.66
          ? "Pixel flower in full bloom, glowing warmly"
          : t > 0.33
          ? "Pixel flower budding, a little dim"
          : "Pixel flower wilted and dim"
      }
    >
      {/* stem + leaves: fixed in place */}
      <g>
        {cells
          .filter((c) => c.y >= HEAD_ROWS)
          .map((c) => (
            <rect
              key={`${c.x}-${c.y}`}
              x={c.x * size}
              y={c.y * size}
              width={size - 2}
              height={size - 2}
              rx={3}
              fill={c.part === "S" ? stemColor : leafColor}
              style={{ transition: "fill 700ms ease" }}
            />
          ))}
      </g>

      {/* flower head: rotates/droops as a group from the stem's top */}
      <g
        style={{
          transform: `translate(${(COLS * size) / 2}px, ${HEAD_ROWS * size}px) rotate(${droopDeg}deg) translate(${-(COLS * size) / 2}px, ${-HEAD_ROWS * size}px) translateY(${droopY}px)`,
          transition: "transform 900ms cubic-bezier(0.34, 1.2, 0.4, 1)",
        }}
      >
        {cells
          .filter((c) => c.y < HEAD_ROWS)
          .map((c) => (
            <rect
              key={`${c.x}-${c.y}`}
              x={c.x * size}
              y={c.y * size}
              width={size - 2}
              height={size - 2}
              rx={3}
              fill={c.part === "C" ? centerColor : petalColor}
              style={{
                transition: "fill 700ms ease",
                filter:
                  c.part === "C"
                    ? `drop-shadow(0 0 ${glow}px ${centerColor})`
                    : undefined,
              }}
            />
          ))}
      </g>
    </svg>
  );
}
