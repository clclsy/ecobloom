# ecobloom

A flower that lives with your habits.

A hardware sensor reads how a household uses energy (lights left on in
empty rooms) and a flower glows or wilts to match. This is the marketing
and demo site: a live interactive hero, a "how it works" walkthrough, an
aura-style live dashboard with a Backboard AI assistant, and a phone
notification preview.

## Run it locally

```bash
npm install
npm run dev
```

Then open http://localhost:3000

## Deploy to Vercel

1. Push this folder to a GitHub repo.
2. Go to vercel.com, New Project, import the repo.
3. Framework preset: Next.js (auto-detected). No env vars needed.
4. Deploy.

Or from the CLI:

```bash
npm i -g vercel
vercel
```

## Stack

- Next.js 16 (App Router) + TypeScript
- Tailwind CSS v4
- Framer Motion (flower + notification animations)
- Self-hosted fonts via `@fontsource` (Fraunces + Sora), no external font
  fetch at build time, so it will not break on a flaky network.

## Where things live

- `app/page.tsx`, the whole page, section by section
- `components/HeroDemo.tsx`, the light-switch toggle driving the flower
- `components/FlowerOrb.tsx`, the animated SVG flower (bloom to wilt)
- `components/DashboardAura.tsx`, the live-reading dashboard card with the
  flower, logo, vitality bar, and the "Ask Backboard" button
- `components/PhoneMockup.tsx`, the notification preview
- `app/globals.css`, color and font tokens (pastel pink and green theme)

## Easy edits

- Colors: `app/globals.css`, the `:root` block at the top.
- Copy: mostly in `app/page.tsx`.
- Flower vitality shown on the dashboard: `components/DashboardAura.tsx`,
  the `VITALITY` constant near the top.
- Notification text: `components/PhoneMockup.tsx`, the `notifs` array.
- Backboard button link: `components/DashboardAura.tsx`, the `<a href="#">`
  near the bottom, point it at your Backboard integration.
