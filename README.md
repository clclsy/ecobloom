# EcoBloom

### *Making energy visible. Making waste emotional.*

---

## ✧ Inspiration

> *How can AI be resource-efficient while solving environmental problems?*

That question led us beyond software… into something tangible.

What if energy waste wasn’t just a number on a dashboard,
but something you could *feel*?

**EcoBloom** is our answer:
a living, reactive system that transforms invisible energy usage into an **emotional experience**.

People don’t change behavior from data.
They change from *feeling*.

A dashboard saying:

> “Lights on for 23 minutes”

gets ignored.

But a plant slowly *wilting* because of that same action?
That’s impossible to ignore.

---

## ✧ What It Does

EcoBloom observes, interprets, and responds in real time.

### The Plant (Hardware)

A physical plant powered by **green → yellow LED gradients**
that reflect your energy behavior:

* **100 (Healthy):** Fully green
* **0 (Dying):** Fully yellow
* Smooth crossfade between states

It doesn’t just display data —
it *feels alive*.

---

### LLM Chatbot (AI)

Ask:

> *“Why are you dying?”*

EcoBloom responds with **real explanations**:

> “Lights were on for 145 seconds with no one present.”

Powered by **Backboard**, it uses actual sensor logs,
not generic advice.

---

### The Dashboard (Web)

A responsive interface showing:

* Live mood / health score
* Event history
* Visual plant synced with hardware
* Integrated chatbot

A digital mirror of the physical system.

### ⚙️ The Backend (Real-Time System)

Running on **QNX (Raspberry Pi)**:

* Camera-based room state detection
* Event logging with precise timing
* Stateful mood computation
* API powering all components

Everything updates in **real time**.

---

## ✧ How We Built It

### Tech Stack

* **Backend:** Python Flask + QNX RTOS (GPIO control)
* **AI Layer:** Backboard.io (LLM orchestration + grounding)
* **Frontend:** React + HTML/CSS
* **Hardware:** Raspberry Pi + LED system

---

### Design Decisions

**✦ Stateful Mood System**
Tracks *duration*, not just events
→ avoids false alarms, captures real waste

**✦ Grounded AI Responses**
Backboard ensures outputs come from *actual logs*
→ no hallucinations

**✦ Real-Time Guarantees (QNX)**
Sub-100ms LED response
→ makes the plant feel alive

**✦ Mock-First Development**
Simulated camera inputs early
→ unblocked full system development

---

## ✧ Challenges

**🔌 Hardware Reality**
Wiring LEDs + GPIO took longer than expected
→ physical systems ≠ instant feedback

**🔄 System Coordination**
Multiple services required strict API contracts
→ solved with early mocking

**🧠 Backboard Learning Curve**
Understanding routing + threads took time
→ but enabled powerful grounding

**📸 Sensor Noise**
Camera flickering in low light
→ fixed with temporal smoothing

---

## ✧ Accomplishments

✔ Full **end-to-end pipeline** working
✔ AI responses grounded in *real data*
✔ Smooth, continuous LED transitions
✔ Deterministic real-time system (QNX)
✔ Demo-ready in **36 hours**

---

## ✧ What We Learned

**Real-time changes everything**
Latency isn’t just technical — it’s *perceptual*.

**Hardware humbles you**
Every wire matters.

**AI is more than prompts**
Orchestration + grounding = real systems.

**Emotion > Information**
People don’t respond to numbers.
They respond to *life*.

---

## ✧ What’s Next

### 🚀 Immediate

* Finalize LED hardware wiring
* Launch full physical demo
* Deploy backend (Render / Railway)

### 🌍 Future Vision

* Multi-room energy tracking
* Smart home integrations
* Personalized behavioral insights
* Scalable sustainability platform

---

## ✧ Final Thought

EcoBloom isn’t just a project.

It’s a shift in perspective:

> *What if technology didn’t just inform us —*
> *but made us care?*

---
