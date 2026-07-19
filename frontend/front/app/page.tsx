import HeroDemo from "@/components/HeroDemo";
import PhoneMockup from "@/components/PhoneMockup";
import DashboardAura from "@/components/DashboardAura";

export default function Home() {
  return (
    <main className="flex flex-col">
      {/* NAV */}
      <header className="sticky top-0 z-30 backdrop-blur-md bg-bg/70 border-b border-ink/6">
        <nav className="max-w-6xl mx-auto flex items-center justify-between px-6 py-4">
          <a href="#top" className="font-display italic text-xl tracking-tight text-ink">
            🌸 ecobloom
          </a>
          <div className="hidden sm:flex items-center gap-8 text-sm text-ink-soft">
            <a href="#how" className="hover:text-ink transition-colors">
              how it works
            </a>
            <a href="#dashboard" className="hover:text-ink transition-colors">
              the dashboard
            </a>
            <a href="#app" className="hover:text-ink transition-colors">
              the app
            </a>
          </div>
          <a
            href="#app"
            className="text-sm font-medium bg-ink text-paper rounded-full px-4 py-2 hover:bg-pink-deep transition-colors"
          >
            see it bloom
          </a>
        </nav>
      </header>

      {/* HERO */}
      <section id="top" className="relative overflow-hidden">
        <div
          className="absolute inset-0 -z-10"
          style={{
            background:
              "radial-gradient(60% 50% at 80% 10%, var(--pink-soft) 0%, transparent 60%), radial-gradient(50% 45% at 10% 25%, var(--green-soft) 0%, transparent 60%)",
          }}
        />
        <div className="max-w-6xl mx-auto px-6 pt-16 pb-24 grid md:grid-cols-2 gap-12 items-center">
          <div>
            <p className="text-xs uppercase tracking-[0.2em] text-pink-deep font-semibold mb-5">
              🌸 A home sensor with a face
            </p>
            <h1 className="font-display text-5xl sm:text-6xl leading-[1.05] text-ink mb-6">
              Your home has a pulse.
              <br />
              <span className="italic text-pink-deep">Now you can see it.</span>
            </h1>
            <p className="text-lg text-ink-soft leading-relaxed max-w-md mb-8">
              Ecobloom is a flower that lives with your habits. A sensor reads
              how your household uses energy: a light left on, an empty room
              still lit. The flower blooms or wilts to match, in real time.
            </p>
            <div className="flex flex-wrap items-center gap-4">
              <a
                href="#app"
                className="text-sm font-medium bg-pink-deep text-paper rounded-full px-6 py-3 hover:bg-ink transition-colors"
              >
                meet the app
              </a>
              <a
                href="#how"
                className="text-sm font-medium text-ink-soft hover:text-ink transition-colors"
              >
                how it works ↓
              </a>
            </div>
          </div>
          <HeroDemo />
        </div>
      </section>

      {/* HOW IT WORKS: vertical timeline, not a 3-card grid */}
      <section id="how" className="max-w-6xl mx-auto px-6 py-24">
        <p className="text-xs uppercase tracking-[0.2em] text-green-deep font-semibold mb-3">
          How it works
        </p>
        <h2 className="font-display text-3xl sm:text-4xl text-ink mb-14 max-w-lg">
          From your light switch to a living flower.
        </h2>

        <div className="relative max-w-2xl">
          <div className="absolute left-[15px] top-2 bottom-2 w-px bg-gradient-to-b from-pink-soft via-green-soft to-transparent" />

          <div className="relative flex gap-6 pb-12">
            <span className="shrink-0 w-8 h-8 rounded-full bg-paper border border-pink/40 flex items-center justify-center text-xs">
              🌸
            </span>
            <div>
              <h3 className="font-display text-xl text-ink mb-1.5">
                The sensor reads the room
              </h3>
              <p className="text-ink-soft text-sm leading-relaxed max-w-md">
                A small hardware sensor tracks light, motion, and time. It
                knows how long a room has been lit with no one in it.
              </p>
            </div>
          </div>

          <div className="relative flex gap-6 pb-12">
            <span className="shrink-0 w-8 h-8 rounded-full bg-paper border border-green/40 flex items-center justify-center text-xs">
              🌸
            </span>
            <div>
              <h3 className="font-display text-xl text-ink mb-1.5">
                The flower responds, right away
              </h3>
              <p className="text-ink-soft text-sm leading-relaxed max-w-md">
                Its housing glows or dims in step with your habits. Leave the
                lights on and it visibly wilts on your desk, not in a report.
              </p>
            </div>
          </div>

          <div className="relative flex gap-6">
            <span className="shrink-0 w-8 h-8 rounded-full bg-paper border border-amber/50 flex items-center justify-center text-xs">
              🌸
            </span>
            <div>
              <h3 className="font-display text-xl text-ink mb-1.5">
                Backboard fills in the why
              </h3>
              <p className="text-ink-soft text-sm leading-relaxed max-w-md">
                Ask the built-in assistant what changed today and it points to
                the exact habit behind it, with a plain suggestion to fix it.
              </p>
            </div>
          </div>
        </div>
      </section>

      {/* DASHBOARD */}
      <section id="dashboard" className="bg-bg-deep">
        <div className="max-w-6xl mx-auto px-6 py-24">
          <p className="text-xs uppercase tracking-[0.2em] text-pink-deep font-semibold mb-3">
            The dashboard
          </p>
          <h2 className="font-display text-3xl sm:text-4xl text-ink mb-4 max-w-lg">
            Every glow and wilt is backed by a reading.
          </h2>
          <p className="text-ink-soft max-w-xl mb-12 leading-relaxed">
            The flower is the feeling. The dashboard is the receipt: a plain
            look at what your household used today, and Backboard on hand to
            explain it.
          </p>

          <DashboardAura />
        </div>
      </section>

      {/* APP */}
      <section id="app" className="max-w-6xl mx-auto px-6 py-24 grid md:grid-cols-2 gap-12 items-center">
        <div className="order-2 md:order-1">
          <p className="text-xs uppercase tracking-[0.2em] text-green-deep font-semibold mb-3">
            The app
          </p>
          <h2 className="font-display text-3xl sm:text-4xl text-ink mb-6 max-w-md">
            Your flower, in your pocket.
          </h2>
          <p className="text-ink-soft leading-relaxed max-w-md mb-6">
            You will not always be home to see it droop, so the app feels it
            for you. A notification when the flower starts wilting is written
            the way the flower would say it, not like a system log. The kWh
            behind it gets translated into something you can actually
            picture, and every bit you save gets celebrated the same way,
            shown as something real it could have powered instead.
          </p>
        </div>
        <div className="order-1 md:order-2">
          <PhoneMockup />
        </div>
      </section>

      {/* FOOTER */}
      <footer className="border-t border-ink/8">
        <div className="max-w-6xl mx-auto px-6 py-10 flex flex-col sm:flex-row items-center justify-between gap-4">
          <span className="font-display italic text-lg text-ink">🌸 ecobloom</span>
          <p className="text-xs text-ink-faint text-center sm:text-right">
            A flower that keeps your habits honest, with suggestions from Backboard.
          </p>
        </div>
      </footer>
    </main>
  );
}
