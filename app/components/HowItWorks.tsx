const STEPS = [
  {
    tag: "Sense",
    title: "It notices you've left.",
    body:
      "A light and motion sensor in the base knows when a room is lit but empty — the single most common way homes waste electricity.",
  },
  {
    tag: "Dim",
    title: "The lamp dims itself.",
    body:
      "No app required for this part. Ecobloom quietly lowers its own glow the moment it senses the room doesn't need it lit.",
  },
  {
    tag: "Grow",
    title: "The habit shows on the flower.",
    body:
      "Keep leaving rooms lit and the petals wilt over the week. Catch yourself and dim early, and it blooms back — same flower, different habit.",
  },
];

export default function HowItWorks() {
  return (
    <section id="how" className="bg-sage-100 py-24">
      <div className="mx-auto max-w-6xl px-6">
        <h2 className="max-w-lg font-display text-3xl font-medium text-sage-950 sm:text-4xl">
          Three things happen, in order, every time.
        </h2>
        <div className="mt-14 grid gap-10 sm:grid-cols-3">
          {STEPS.map((step, i) => (
            <div key={step.tag} className="relative">
              <div className="flex items-center gap-3 font-mono text-xs uppercase tracking-wide text-pink-700">
                <span>{step.tag}</span>
                {i < STEPS.length - 1 && (
                  <span aria-hidden className="hidden text-sage-400 sm:inline">
                    →
                  </span>
                )}
              </div>
              <h3 className="mt-3 font-display text-xl font-medium text-sage-950">
                {step.title}
              </h3>
              <p className="mt-3 text-sm leading-relaxed text-sage-950/75">
                {step.body}
              </p>
            </div>
          ))}
        </div>
      </div>
    </section>
  );
}
