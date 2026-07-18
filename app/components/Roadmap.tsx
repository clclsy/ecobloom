export default function Roadmap() {
  return (
    <section id="roadmap" className="py-24">
      <div className="mx-auto grid max-w-6xl gap-10 px-6 lg:grid-cols-[0.9fr_1.1fr] lg:items-center">
        <div>
          <p className="font-mono text-xs uppercase tracking-wide text-pink-700">
            What's next
          </p>
          <h2 className="mt-3 font-display text-3xl font-medium text-sage-950 sm:text-4xl">
            Next, the stem curls too.
          </h2>
          <p className="mt-4 max-w-md text-sage-950/75">
            Petal color and glow are the first release. The next hardware
            revision adds a motorized stem, so a truly neglected Ecobloom
            doesn&apos;t just dim — it curls inward like a real wilting
            flower, and straightens back up as habits improve.
          </p>
        </div>
        <div className="rounded-3xl border border-dashed border-sage-400 bg-sage-100/60 p-8">
          <p className="font-mono text-xs uppercase tracking-wide text-sage-700">
            v2 hardware — in development
          </p>
          <ul className="mt-4 space-y-3 text-sm text-sage-950/80">
            <li className="flex gap-3">
              <span aria-hidden className="text-pink-700">
                ·
              </span>
              Motorized stem that curls when neglected, straightens when
              cared for
            </li>
            <li className="flex gap-3">
              <span aria-hidden className="text-pink-700">
                ·
              </span>
              Multi-room sync, so every Ecobloom in the house shares one
              habit score
            </li>
            <li className="flex gap-3">
              <span aria-hidden className="text-pink-700">
                ·
              </span>
              Matte ceramic finishes, for a lamp that looks at home on a
              shelf
            </li>
          </ul>
        </div>
      </div>
    </section>
  );
}
