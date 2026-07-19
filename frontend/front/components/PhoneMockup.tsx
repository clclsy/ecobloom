"use client";

import { motion } from "framer-motion";

type Notif = {
  tone: "wilt" | "stat" | "positive";
  time: string;
  title: string;
  body: string;
};

const notifs: Notif[] = [
  {
    tone: "wilt",
    time: "9:41 PM",
    title: "your flower is drooping 🥀",
    body: "the living room light's been on for 2h 14m with no one home. it's starting to feel it.",
  },
  {
    tone: "stat",
    time: "9:52 PM",
    title: "today's waste",
    body: "1.8 kWh burned on empty rooms — about what it takes to run a load of laundry.",
  },
  {
    tone: "positive",
    time: "Yesterday",
    title: "your flower is thriving 🌸",
    body: "you saved 2.3 kWh this week — enough to charge your phone every day for a month.",
  },
];

const toneStyles: Record<Notif["tone"], { bg: string; ring: string; dot: string }> = {
  wilt: { bg: "bg-[#efe2d6]", ring: "ring-[color:var(--wilt)]/25", dot: "bg-wilt" },
  stat: { bg: "bg-[#f6d3e2]/60", ring: "ring-[color:var(--pink-deep)]/20", dot: "bg-pink-deep" },
  positive: { bg: "bg-[#d9ecd6]/70", ring: "ring-[color:var(--green-deep)]/20", dot: "bg-green-deep" },
};

export default function PhoneMockup() {
  return (
    <div className="relative mx-auto w-[280px] sm:w-[300px]">
      <div className="relative rounded-[2.75rem] border-[6px] border-[#3a2e3f] bg-[#3a2e3f] shadow-2xl shadow-pink-deep/10 overflow-hidden">
        <div className="absolute top-0 left-1/2 -translate-x-1/2 w-28 h-6 bg-[#3a2e3f] rounded-b-2xl z-20" />
        <div className="relative bg-gradient-to-b from-[#fbf4f1] to-[#f6e9e3] rounded-[2.2rem] pt-10 pb-8 px-3 min-h-[560px] flex flex-col">
          <p className="text-center text-xs font-body text-ink-soft mb-5 tracking-wide">
            Today
          </p>
          <div className="flex flex-col gap-3">
            {notifs.map((n, i) => (
              <motion.div
                key={n.title}
                initial={{ opacity: 0, y: 14 }}
                whileInView={{ opacity: 1, y: 0 }}
                viewport={{ once: true, margin: "-40px" }}
                transition={{ delay: i * 0.12, duration: 0.5, ease: "easeOut" }}
                className={`rounded-2xl p-3.5 ring-1 ${toneStyles[n.tone].ring} ${toneStyles[n.tone].bg} backdrop-blur-sm`}
              >
                <div className="flex items-center justify-between mb-1">
                  <div className="flex items-center gap-1.5">
                    <span className={`w-1.5 h-1.5 rounded-full ${toneStyles[n.tone].dot}`} />
                    <span className="text-[11px] font-semibold tracking-wide text-ink-soft uppercase">
                      ecobloom
                    </span>
                  </div>
                  <span className="text-[10px] text-ink-faint">{n.time}</span>
                </div>
                <p className="font-display text-[15px] leading-snug text-ink mb-0.5">
                  {n.title}
                </p>
                <p className="text-[12.5px] leading-relaxed text-ink-soft">{n.body}</p>
              </motion.div>
            ))}
          </div>
        </div>
      </div>
    </div>
  );
}
