
"use client";
 
import { useEffect, useRef, useState } from "react";
import { AnimatePresence, motion } from "framer-motion";
import FlowerOrb from "./FlowerOrb";
 
const API = process.env.NEXT_PUBLIC_API_URL ?? "http://localhost:8000";
 
interface ChatMessage {
  role: "user" | "plant";
  text: string;
}
 
export default function DashboardAura() {
  // ── live data ──────────────────────────────────────────────────────────────
  const [vitality, setVitality] = useState(78);
  const [tier, setTier] = useState("healthy");
 
  useEffect(() => {
    async function fetchMood() {
      try {
        const res = await fetch(`${API}/mood`);
        const data = await res.json();
        setVitality(data.mood);
        setTier(data.tier);
      } catch {}
    }
    fetchMood();
    const interval = setInterval(fetchMood, 2000);
    return () => clearInterval(interval);
  }, []);
 
  // ── chat popup ─────────────────────────────────────────────────────────────
  const [open, setOpen] = useState(false);
  const [chat, setChat] = useState<ChatMessage[]>([]);
  const [input, setInput] = useState("");
  const [loading, setLoading] = useState(false);
  const chatEndRef = useRef<HTMLDivElement>(null);
 
  useEffect(() => {
    chatEndRef.current?.scrollIntoView({ behavior: "smooth" });
  }, [chat]);
 
  async function sendMessage() {
    const q = input.trim();
    if (!q || loading) return;
    setInput("");
    setChat((prev) => [...prev, { role: "user", text: q }]);
    setLoading(true);
    try {
      const res = await fetch(`${API}/chat`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ question: q }),
      });
      const data = await res.json();
      setChat((prev) => [...prev, { role: "plant", text: data.answer }]);
    } catch {
      setChat((prev) => [
        ...prev,
        { role: "plant", text: "Sorry, I can't reach the server right now." },
      ]);
    } finally {
      setLoading(false);
    }
  }
 
  const now = new Date().toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
 
  return (
    <>
      <div className="relative rounded-[28px] overflow-hidden border border-ink/8 bg-paper px-6 py-8 sm:px-10 sm:py-12">
        {/* aura wash */}
        <div
          className="absolute inset-0 -z-10"
          style={{
            background:
              "radial-gradient(48% 55% at 50% 42%, var(--pink-soft) 0%, var(--green-soft) 45%, transparent 78%)",
          }}
        />
 
        <div className="flex items-start justify-between text-xs">
          <span className="uppercase tracking-[0.18em] text-ink-soft font-semibold">
            ecobloom · live reading
          </span>
          <span className="uppercase tracking-[0.18em] text-ink-faint">
            Today, {now}
          </span>
        </div>
 
        <div className="flex flex-col items-center py-6 sm:py-10">
          <FlowerOrb vitality={vitality} compact />
          <p className="font-display italic text-lg text-ink -mt-2">🌸 ecobloom</p>
 
          <div className="w-full max-w-xs mt-6">
            <div className="flex items-baseline justify-between mb-2">
              <span className="text-xs uppercase tracking-wider text-ink-soft">
                Flower vitality
              </span>
              <span className="font-display text-xl text-ink">
                {Math.round(vitality)}%
              </span>
            </div>
            <div className="h-2 rounded-full bg-ink/8 overflow-hidden">
              <motion.div
                className="h-full rounded-full"
                animate={{ width: `${vitality}%` }}
                transition={{ duration: 0.8, ease: "easeInOut" }}
                style={{
                  background:
                    "linear-gradient(90deg, var(--wilt), var(--amber), var(--green))",
                }}
              />
            </div>
            <p className="text-xs text-ink-faint mt-2 capitalize">
              {tier === "blooming" && "Thriving. No room has sat lit and empty for long."}
              {tier === "healthy" && "Doing well. Keep up the good habits!"}
              {tier === "wilting" && "Wilting a little. Lights left on in an empty room."}
              {tier === "dying" && "Struggling. Lights have been on with no one around."}
            </p>
          </div>
        </div>
 
        <div className="flex flex-col sm:flex-row sm:items-end sm:justify-between gap-4 pt-2 border-t border-ink/8">
          <p className="text-sm text-ink-soft max-w-xs">
            Ask the plant why it&apos;s wilting, what habits are hurting it, or how to help it bloom.
          </p>
 
          <button
            onClick={() => setOpen(true)}
            className="self-start sm:self-auto inline-flex items-center gap-2 rounded-full bg-ink text-paper text-sm font-medium px-4 py-2.5 hover:bg-pink-deep transition-colors"
          >
            🌸 Ask Backboard
          </button>
        </div>
      </div>
 
      {/* ── chat popup ── */}
      <AnimatePresence>
        {open && (
          <>
            {/* backdrop */}
            <motion.div
              initial={{ opacity: 0 }}
              animate={{ opacity: 1 }}
              exit={{ opacity: 0 }}
              onClick={() => setOpen(false)}
              className="fixed inset-0 z-40 bg-ink/30 backdrop-blur-sm"
            />
 
            {/* popup */}
            <motion.div
              initial={{ opacity: 0, y: 40, scale: 0.96 }}
              animate={{ opacity: 1, y: 0, scale: 1 }}
              exit={{ opacity: 0, y: 40, scale: 0.96 }}
              transition={{ type: "spring", stiffness: 260, damping: 24 }}
              className="fixed bottom-6 right-6 z-50 w-[min(420px,calc(100vw-3rem))] rounded-[24px] border border-ink/10 bg-paper shadow-2xl flex flex-col overflow-hidden"
              style={{ maxHeight: "70vh" }}
            >
              {/* header */}
              <div className="flex items-center justify-between px-5 py-4 border-b border-ink/8">
                <div>
                  <p className="text-sm font-semibold text-ink">🌸 Ask your plant</p>
                  <p className="text-xs text-ink-faint mt-0.5">
                    Powered by Backboard
                  </p>
                </div>
                <button
                  onClick={() => setOpen(false)}
                  className="text-ink-faint hover:text-ink transition-colors text-lg leading-none"
                >
                  ✕
                </button>
              </div>
 
              {/* messages */}
              <div className="flex-1 overflow-y-auto px-5 py-4 space-y-3">
                {chat.length === 0 && (
                  <div className="flex flex-col items-center justify-center h-full gap-3 text-center py-8">
                    <span className="text-4xl">🌿</span>
                    <p className="text-sm text-ink-faint">
                      Try asking{" "}
                      <button
                        onClick={() => setInput("Why are you dying?")}
                        className="underline decoration-ink/30 underline-offset-2 hover:text-ink"
                      >
                        &ldquo;Why are you dying?&rdquo;
                      </button>
                    </p>
                  </div>
                )}
 
                {chat.map((msg, i) => (
                  <motion.div
                    key={i}
                    initial={{ opacity: 0, y: 6 }}
                    animate={{ opacity: 1, y: 0 }}
                    transition={{ duration: 0.2 }}
                    className={`flex ${msg.role === "user" ? "justify-end" : "justify-start"}`}
                  >
                    <div
                      className={`max-w-[85%] rounded-2xl px-4 py-3 text-sm leading-relaxed whitespace-pre-wrap ${
                        msg.role === "user"
                          ? "bg-ink text-paper"
                          : "bg-ink/6 text-ink"
                      }`}
                    >
                      {msg.text}
                    </div>
                  </motion.div>
                ))}
 
                {loading && (
                  <motion.div
                    initial={{ opacity: 0 }}
                    animate={{ opacity: 1 }}
                    className="flex justify-start"
                  >
                    <div className="rounded-2xl bg-ink/6 px-4 py-3 text-xs text-ink-faint">
                      thinking…
                    </div>
                  </motion.div>
                )}
                <div ref={chatEndRef} />
              </div>
 
              {/* input */}
              <div className="border-t border-ink/8 p-4">
                <div className="flex gap-2">
                  <input
                    type="text"
                    value={input}
                    onChange={(e) => setInput(e.target.value)}
                    onKeyDown={(e) => e.key === "Enter" && sendMessage()}
                    placeholder="Ask the plant something…"
                    className="flex-1 rounded-full border border-ink/12 bg-ink/4 px-4 py-2 text-sm text-ink placeholder:text-ink-faint focus:outline-none focus:ring-2 focus:ring-pink-deep/40"
                  />
                  <button
                    onClick={sendMessage}
                    disabled={loading || !input.trim()}
                    className="rounded-full bg-ink text-paper px-4 py-2 text-sm font-medium hover:bg-pink-deep transition-colors disabled:opacity-40"
                  >
                    Send
                  </button>
                </div>
              </div>
            </motion.div>
          </>
        )}
      </AnimatePresence>
    </>
  );
}