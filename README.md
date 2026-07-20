# Red Plasma

Red Plasma is a modular game engine built around one core idea: **everything is a module/plugin, and modules own their own complexity completely.** The engine core only knows thin, fixed contracts — never the internals of what's behind them.

This isn't just a plugin *system* bolted onto a normal engine. The renderer is a module. The window/OS layer is a module. Memory allocation is a module. Even the handle/ownership manager is a module. Swap any one of them out, and the rest of the engine doesn't need to know or care.

## Origin

Red Plasma started with a game idea — a vampire RPG inspired by Vampire: The Masquerade, with updated graphics, called *First Night Out*. Development started in Unreal Engine. It kept crashing. Not just crashing — crashing in ways that were silent, opaque, and destructive: the editor would go down and take engine state with it, leaving no clear trail of what had actually broken. Recovery could take days.

At some point the thought was: *what if the editor crashing couldn't touch what's already in the engine? And what if the engine crashing couldn't corrupt the editor's work either?* That complete separation — not as a feature, but as a hard architectural boundary — became the seed of Red Plasma.

The name followed the theme. Vampires drink blood. Blood is sometimes called plasma. And royalty is said to have blue blood — but blood is red, whatever the mythology says. Red Plasma: stating the obvious.

The original game is still the long-term goal. The engine came first because the right tool didn't exist.

## The real problems Red Plasma is built to solve

These aren't hypothetical concerns — they're a direct list of painful, real experiences with existing engines on Linux:

**The editor crashes and takes your work with it.** Opening a plugin works fine one session and hard-crashes the editor the next, for no apparent reason. When it reopens, meshes have detached from objects, references have disappeared from blueprints, or assets are simply gone. You've lost work and you don't know why. Red Plasma's answer: the editor is a completely separate project. Engine state lives in the engine, not the editor. A crashed editor cannot corrupt engine state because the two never share internal state — the boundary is architectural, not just a good intention.

**Silent failures that only surface as a crash.** Forget to check for null and both the editor and the IDE refuse to open — they just crash. You can't open the tools you need to fix the problem because the problem prevents the tools from opening. Red Plasma's answer: Rust's type system has no traditional null — `Option<T>` forces the "nothing here" case to be handled at compile time. The compiler refuses to build code that ignores a potentially absent value, rather than letting it crash at runtime and take everything else down with it.

**Non-deterministic plugin loading.** The same plugin loads cleanly sometimes and crashes the editor other times, with no explanation. Red Plasma's answer: every module is validated before a single line of its code runs — magic number, contract version, dependency graph, cycle check — and any failure is a hard reject with a named, specific error. There is no "sometimes it works."

**"Just don't make these mistakes."** The standard advice online for both Unreal and Unity on Linux amounts to a list of cursed actions to avoid, maintained by folklore rather than the engine itself. That is not engineering — it's asking developers to memorise failure modes that the tools should prevent or explain. Red Plasma's answer: make the wrong path hard, make failures loud and named, and never ask a developer to already know what went wrong in order to find out what went wrong.

## Status

Early design phase. Core architecture and contracts are defined (see `PHILOSOPHY.md`, `CONTRACTS.md`, `DESIGN_DECISIONS.md`). No code yet — next steps are version control setup, then the game loop, then module loading, then a window, then a triangle.

## Why modular all the way down

Most engines pick a hardware/performance ceiling and design down from there, then bolt on portability later. Red Plasma is designed the other way: every expensive operation (division, dynamic allocation, dynamic symbol lookups) is either avoided in hot paths, pushed to setup time, or made swappable per target hardware from day one — so the engine can scale from constrained/old hardware up to modern desktops without the core design having to change.

The guiding principle: **you have the hardware, my engine can handle that.**

See `PHILOSOPHY.md` for the full reasoning.

## License

Mozilla Public License 2.0 (MPL-2.0). See `LICENSE`. In short: modify Red Plasma's own source files and distribute them, and those changes must be shared under MPL. Build your own modules/plugins or games against Red Plasma's interfaces, and you're free to license that work however you like, including fully closed/proprietary.

## Getting started

**1. Clone the repo**
```bash
git clone https://github.com/yourusername/red_plasma.git
cd red_plasma
```

**2. Run the setup script**
```bash
chmod +x setup.sh
./setup.sh
```

This installs the pre-commit hook. After this, every commit automatically sorts `docs/GLOSSARY.md` and checks for raw OS calls outside `os/`.

**3. Install Rust** (if not already installed)
```bash
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
source $HOME/.cargo/env
rustup component add clippy
```

**4. Build**
```bash
cargo build
```

## Dependencies

**To build Red Plasma:**
- Rust (install via `rustup` — see rustup.rs)

**For development tooling:**
- Python 3 (pre-commit hooks, glossary sorting, dev scripts)

## Documentation

- `docs/PHILOSOPHY.md` — the design philosophy and guiding principles behind every decision.
- `docs/CONTRACTS.md` — the concrete technical contracts every plugin and module must follow.
- `docs/DESIGN_DECISIONS.md` — the reasoning and current status of each major architectural choice.
- `docs/ROADMAP.md` — what's being built, in what order.
- `docs/GLOSSARY.md` — shorthand, abbreviations, and project-specific terms explained plainly.
- `LICENSE` — full MPL-2.0 text.

