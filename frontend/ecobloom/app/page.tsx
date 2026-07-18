import Nav from "./components/Nav";
import Hero from "./components/Hero";
import HowItWorks from "./components/HowItWorks";
import GrowStages from "./components/GrowStages";
import Features from "./components/Features";
import Roadmap from "./components/Roadmap";
import Waitlist from "./components/Waitlist";
import Footer from "./components/Footer";

export default function Home() {
  return (
    <>
      <Nav />
      <main>
        <Hero />
        <HowItWorks />
        <GrowStages />
        <Features />
        <Roadmap />
        <Waitlist />
      </main>
      <Footer />
    </>
  );
}
