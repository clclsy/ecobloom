export default function Features() {
  return (
    <section id="app" className="bg-pink-100 py-24">
      <div className="mx-auto max-w-6xl px-6">
        <div className="max-w-lg">
          <h2 className="font-display text-3xl font-medium text-sage-950 sm:text-4xl">
            The app is where the flower explains itself.
          </h2>
          <p className="mt-4 text-sage-950/75">
            The lamp reacts in the room. The app tells you why — and what to
            change tomorrow.
          </p>
        </div>

        <div className="mt-14 grid gap-6 lg:grid-cols-3">
          {/* Notification mock */}
          <div className="rounded-3xl bg-paper p-6 shadow-sm">
            <p className="font-mono text-[10px] uppercase tracking-wide text-sage-700">
              Usage notification
            </p>
            <div className="mt-4 rounded-2xl border border-sage-950/10 bg-sage-100 p-4">
              <div className="flex items-center justify-between">
                <span className="font-mono text-xs font-medium text-sage-950">
                  Ecobloom
                </span>
                <span className="font-mono text-[10px] text-sage-700">now</span>
              </div>
              <p className="mt-2 text-sm text-sage-950">
                The living room light ran <strong>2h 40m</strong> with no one
                in the room today.
              </p>
            </div>
            <p className="mt-4 text-sm leading-relaxed text-sage-950/70">
              A quiet nudge, not a lecture — sent only when it&apos;s actually
              worth knowing.
            </p>
          </div>

          {/* LLM suggestion mock */}
          <div className="rounded-3xl bg-paper p-6 shadow-sm">
            <p className="font-mono text-[10px] uppercase tracking-wide text-sage-700">
              Habit coach
            </p>
            <div className="mt-4 rounded-2xl border border-sage-950/10 bg-sage-100 p-4">
              <p className="text-sm text-sage-950">
                Most of this week&apos;s waste happened between 6–8pm in the
                kitchen. Try a dusk automation so it dims itself before you
                remember to.
              </p>
            </div>
            <p className="mt-4 text-sm leading-relaxed text-sage-950/70">
              Ecobloom&apos;s assistant reads your week of habits and suggests
              one fix at a time — not a wall of statistics.
            </p>
          </div>

          {/* Weekly digest mock */}
          <div className="rounded-3xl bg-paper p-6 shadow-sm">
            <p className="font-mono text-[10px] uppercase tracking-wide text-sage-700">
              Weekly digest
            </p>
            <div className="mt-4 rounded-2xl border border-sage-950/10 bg-sage-100 p-4 font-mono text-xs text-sage-950">
              <div className="flex justify-between py-1">
                <span>Mon</span>
                <span className="text-pink-700">wilted</span>
              </div>
              <div className="flex justify-between py-1">
                <span>Tue</span>
                <span className="text-pink-700">budding</span>
              </div>
              <div className="flex justify-between py-1">
                <span>Wed</span>
                <span className="text-sage-700">in bloom</span>
              </div>
              <div className="flex justify-between py-1">
                <span>Thu</span>
                <span className="text-sage-700">in bloom</span>
              </div>
            </div>
            <p className="mt-4 text-sm leading-relaxed text-sage-950/70">
              A day-by-day read of the flower&apos;s state, so a pattern is
              easy to spot in seconds.
            </p>
          </div>
        </div>
      </div>
    </section>
  );
}
