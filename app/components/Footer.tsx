export default function Footer() {
  return (
    <footer className="border-t border-sage-950/10 py-10">
      <div className="mx-auto flex max-w-6xl flex-col items-center justify-between gap-4 px-6 font-mono text-xs text-sage-700 sm:flex-row">
        <span>© {new Date().getFullYear()} Ecobloom</span>
        <span>A lamp that grows with your habits</span>
      </div>
    </footer>
  );
}
