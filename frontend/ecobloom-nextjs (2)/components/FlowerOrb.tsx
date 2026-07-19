"use client";

import { motion, useSpring, useTransform } from "framer-motion";
import { useEffect, useState } from "react";

/**
 * vitality: 0 (fully wilted), 100 (fully bloomed)
 */
export default function FlowerOrb({
  vitality,
  compact = false,
}: {
  vitality: number;
  compact?: boolean;
}) {
  const spring = useSpring(vitality, { stiffness: 90, damping: 18 });

  useEffect(() => {
    spring.set(vitality);
  }, [vitality, spring]);

  const petalScale = useTransform(spring, [0, 100], [0.72, 1]);
  const droop = useTransform(spring, [0, 100], [26, 0]);
  const glowOpacity = useTransform(spring, [0, 100], [0.12, 0.55]);
  const glowScale = useTransform(spring, [0, 100], [0.85, 1.25]);

  const [petalColor, setPetalColor] = useState("#ea93b8");
  const [centerColor, setCenterColor] = useState("#f3c579");
  const [leafColor, setLeafColor] = useState("#82b98c");

  useEffect(() => {
    const unsub = spring.on("change", (v) => {
      setPetalColor(mix("#a98164", "#ea93b8", v / 100));
      setCenterColor(mix("#7c5c46", "#f3c579", v / 100));
      setLeafColor(mix("#8b8064", "#82b98c", v / 100));
    });
    return () => unsub();
  }, [spring]);

  const petals = Array.from({ length: 6 });

  return (
    <div
      className={`relative flex items-center justify-center w-full aspect-square mx-auto ${
        compact ? "max-w-[180px]" : "max-w-md"
      }`}
    >
      {/* aura glow */}
      <motion.div
        style={{ opacity: glowOpacity, scale: glowScale }}
        className="absolute inset-0 rounded-full blur-3xl"
        aria-hidden
      >
        <div
          className="w-full h-full rounded-full"
          style={{
            background:
              "radial-gradient(circle, var(--pink-soft) 0%, var(--green-soft) 55%, transparent 75%)",
          }}
        />
      </motion.div>

      <svg
        viewBox="0 0 320 320"
        className="relative w-4/5 h-4/5"
        role="img"
        aria-label="Flower vitality visualization"
      >
        {/* stem */}
        <path
          d="M160 300 C 160 260, 150 230, 160 200"
          stroke={leafColor}
          strokeWidth="5"
          strokeLinecap="round"
          fill="none"
        />
        {/* leaves */}
        <motion.path
          style={{ rotate: droop }}
          d="M160 260 C 130 255, 110 240, 108 218 C 135 218, 155 232, 160 260 Z"
          fill={leafColor}
          opacity={0.85}
        />
        <motion.path
          style={{ rotate: useTransform(droop, (d) => -d) }}
          d="M160 245 C 190 240, 208 224, 210 202 C 183 204, 165 218, 160 245 Z"
          fill={leafColor}
          opacity={0.85}
        />

        {/* petals + center, droops as vitality falls */}
        <motion.g style={{ rotate: droop, originX: "160px", originY: "190px" }}>
          <g transform="translate(160,150)">
            {petals.map((_, i) => {
              const angle = (360 / petals.length) * i;
              return (
                // rotation lives on this static wrapper so framer-motion's
                // CSS transform (used for scale below) never overwrites it
                <g key={i} transform={`rotate(${angle})`}>
                  <motion.ellipse
                    cx="0"
                    cy="-46"
                    rx="26"
                    ry="42"
                    fill={petalColor}
                    style={{ scale: petalScale }}
                    opacity={0.94}
                  />
                </g>
              );
            })}
            <motion.circle r="20" fill={centerColor} style={{ scale: petalScale }} />
          </g>
        </motion.g>
      </svg>
    </div>
  );
}

function mix(hexA: string, hexB: string, t: number) {
  const a = hexToRgb(hexA);
  const b = hexToRgb(hexB);
  const r = Math.round(a.r + (b.r - a.r) * t);
  const g = Math.round(a.g + (b.g - a.g) * t);
  const bl = Math.round(a.b + (b.b - a.b) * t);
  return `rgb(${r}, ${g}, ${bl})`;
}

function hexToRgb(hex: string) {
  const clean = hex.replace("#", "");
  const bigint = parseInt(clean, 16);
  return {
    r: (bigint >> 16) & 255,
    g: (bigint >> 8) & 255,
    b: bigint & 255,
  };
}
