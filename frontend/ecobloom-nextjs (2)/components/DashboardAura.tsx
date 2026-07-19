import FlowerOrb from "./FlowerOrb";

const VITALITY = 78;

export default function DashboardAura() {
  return (
    <div className="relative rounded-[28px] overflow-hidden border border-ink/8 bg-paper px-6 py-8 sm:px-10 sm:py-12">
      {/* aura wash behind everything */}
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
        <span className="uppercase tracking-[0.18em] text-ink-faint">Today, 9:52 PM</span>
      </div>

      <div className="flex flex-col items-center py-6 sm:py-10">
        <FlowerOrb vitality={VITALITY} compact />
        <p className="font-display italic text-lg text-ink -mt-2">🌸 ecobloom</p>

        <div className="w-full max-w-xs mt-6">
          <div className="flex items-baseline justify-between mb-2">
            <span className="text-xs uppercase tracking-wider text-ink-soft">
              Flower vitality
            </span>
            <span className="font-display text-xl text-ink">{VITALITY}%</span>
          </div>
          <div className="h-2 rounded-full bg-ink/8 overflow-hidden">
            <div
              className="h-full rounded-full"
              style={{
                width: `${VITALITY}%`,
                background: "linear-gradient(90deg, var(--wilt), var(--amber), var(--green))",
              }}
            />
          </div>
          <p className="text-xs text-ink-faint mt-2">
            Thriving. No room has sat lit and empty for more than a few minutes today.
          </p>
        </div>
      </div>

      <div className="flex flex-col sm:flex-row sm:items-end sm:justify-between gap-4 pt-2 border-t border-ink/8">
        <p className="text-sm text-ink-soft max-w-xs">
          Left on today: 2h 14m in the living room. Wasted: 1.8 kWh, about one
          load of laundry.
        </p>

        <a
          href="#"
          className="self-start sm:self-auto inline-flex items-center gap-2 rounded-full bg-ink text-paper text-sm font-medium px-4 py-2.5 hover:bg-pink-deep transition-colors"
        >
          🌸 Ask Backboard
        </a>
      </div>
    </div>
  );
}
