import HeroDemo from "./HeroDemo";

export default function Hero() {
  return (
    <section id="top" className="mx-auto max-w-6xl px-6 pb-20 pt-14 sm:pt-20">
      <div className="grid items-center gap-14 lg:grid-cols-[1.1fr_0.9fr]">
        <div>
          <p className="font-mono text-xs uppercase tracking-[0.18em] text-pink-700">
            A lamp shaped like a flower
          </p>
          <h1 className="mt-4 font-display text-4xl font-medium leading-[1.05] text-sage-950 sm:text-5xl lg:text-6xl">
            It blooms when you
            <br />
            <span className="text-pink-700">save energy.</span>
            <br />
            It wilts when you
            <br />
            <span className="text-wilt">don&apos;t.</span>
          </h1>
          <p className="mt-6 max-w-md text-lg leading-relaxed text-sage-950/80">
            Ecobloom sits in any room like a plant, but it&apos;s a lamp. Leave
            the lights on in an empty room and it dims on its own to save the
            power — and the flower quietly wilts to show you. Build better
            habits, and it blooms.
          </p>
          <div className="mt-8 flex flex-wrap items-center gap-4">
            
              href="#waitlist"
              className="rounded-full bg-pink-700 px-6 py-3 font-mono text-sm uppercase tracking-wide text-paper transition hover:bg-sage-950"
            >
              Join the waitlist
            </a>
            
              href="#how"
              className="font-mono text-sm uppercase tracking-wide text-sage-700 underline decoration-sage-400 underline-offset-4 hover:text-sage-950"
            >
              See how it works
            </a>
          </div>
        </div>

        <div className="relative">
          <div
            aria-hidden
            className="absolute inset-0 -z-10 rounded-[3rem] bg-gradient-to-b from-pink-100 to-sage-100"
          />
          <div className="rounded-[3rem] px-6 py-12">
            <HeroDemo />
            <p className="mt-6 text-center font-mono text-[11px] text-sage-700">
              drag the slider — this is the actual flower logic
            </p>
          </div>
        </div>
      </div>
    </section>
  );
}
