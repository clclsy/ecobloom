"use client";

import { useState } from "react";

// NOTE: This is a static export (see next.config.ts), so there's no server
// to receive this form. Wire the onSubmit below to a form backend such as
// Formspree, Resend, or a Cloudflare Pages Function before launch — see
// README.md for the two easiest options.
export default function Waitlist() {
  const [email, setEmail] = useState("");
  const [submitted, setSubmitted] = useState(false);

  function handleSubmit(e: React.FormEvent) {
    e.preventDefault();
    if (!email) return;
    setSubmitted(true);
  }

  return (
    <section id="waitlist" className="bg-sage-950 py-24 text-paper">
      <div className="mx-auto max-w-2xl px-6 text-center">
        <h2 className="font-display text-3xl font-medium sm:text-4xl">
          Be one of the first gardens.
        </h2>
        <p className="mt-4 text-paper/70">
          First batch ships to a small waitlist before the public launch.
          We&apos;ll email you when it&apos;s your turn.
        </p>

        {submitted ? (
          <p className="mt-8 font-mono text-sm text-amber-glow">
            You&apos;re on the list — we&apos;ll be in touch.
          </p>
        ) : (
          <form
            onSubmit={handleSubmit}
            className="mx-auto mt-8 flex max-w-md flex-col gap-3 sm:flex-row"
          >
            <label htmlFor="email" className="sr-only">
              Email address
            </label>
            <input
              id="email"
              type="email"
              required
              value={email}
              onChange={(e) => setEmail(e.target.value)}
              placeholder="you@example.com"
              className="w-full rounded-full border border-paper/20 bg-paper/5 px-5 py-3 text-paper placeholder:text-paper/40 focus:border-pink-400 focus:outline-none"
            />
            <button
              type="submit"
              className="rounded-full bg-pink-400 px-6 py-3 font-mono text-sm uppercase tracking-wide text-sage-950 transition hover:bg-amber-glow"
            >
              Join
            </button>
          </form>
        )}
      </div>
    </section>
  );
}
