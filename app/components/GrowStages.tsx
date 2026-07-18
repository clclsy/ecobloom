import PixelFlower from "./PixelFlower";

const STAGES = [
  {
    name: "Bloom",
    health: 92,
    desc: "Lights dim on cue, days in a row. Full color, soft glow.",
    tag: "current habit",
  },
  {
    name: "Bud",
    health: 58,
    desc: "A mixed week — some rooms caught, some left lit.",
    tag: "current habit",
  },
  {
    name: "Wilt",
    health: 22,
    desc: "Lights left on in empty rooms, several days running.",
    tag: "current habit",
  },
  {
    name: "Shrivel",
    health: 4,
    desc: "The stem itself curls inward, not just the petals.",
    tag: "coming in v2 hardware",
  },
];

export default function GrowStages() {
  return (
    <section id="stages" className="py-24">
      <div className="mx-auto max-w-6xl px-6">
        <div className="max-w-lg">
          <h2 className="font-display text-3xl font-medium text-sage-950 sm:text-4xl">
            The flower only has four moods.
          </h2>
          <p className="mt-4 text-sage-950/75">
            Each one is a real state of the hardware, not a mood we made up —
            it&apos;s just what your habits look like from across the room.
          </p>
        </div>

        <div className="mt-14 grid gap-8 sm:grid-cols-2 lg:grid-cols-4">
          {STAGES.map((stage) => (
            <div
              key={stage.name}
              className="rounded-3xl border border-sage-950/10 bg-paper p-6"
            >
              <div className="mx-auto aspect-[12/15] w-[70%]">
                <PixelFlower health={stage.health} />
              </div>
              <h3 className="mt-4 font-display text-lg font-medium text-sage-950">
                {stage.name}
              </h3>
              <p className="mt-2 text-sm leading-relaxed text-sage-950/70">
                {stage.desc}
              </p>
              <p className="mt-3 font-mono text-[10px] uppercase tracking-wide text-pink-700">
                {stage.tag}
              </p>
            </div>
          ))}
        </div>
      </div>
    </section>
  );
}
