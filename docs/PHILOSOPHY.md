# Philosophy

## North star

> "You have the hardware, my engine can handle that."

Red Plasma is designed so that the engine adapts to whatever it's running on, rather than assuming a fixed baseline of CPU, memory, or OS features. This doesn't mean every target is supported on day one — it means no early decision should make it *harder* to support weaker hardware later. The door stays open even when nothing is built behind it yet.

## Why this engine exists

Red Plasma was born from real frustration developing a game in existing engines (Unreal, Unity) on Linux. The specific, recurring problems that drove the design:

**Editor crashes taking engine state with them.** A plugin that loaded cleanly one session would hard-crash the editor on the next open, for no apparent reason. On reopening, meshes had detached from objects, references had vanished from blueprints, assets had disappeared entirely. Work was lost with no explanation. This produced the hard architectural rule that the editor is a completely separate project from the engine — a boundary enforced by design, not convention.

**Silent failures that only manifest as a crash.** Forgetting to check for null caused both the editor and IDE to crash on open — meaning the tools needed to fix the problem were made unavailable by the problem itself. This produced the choice of Rust (where the type system prevents null from being silently ignored at compile time) and the whole-project stance that failures must surface loudly, immediately, and with a specific explanation, never silently.

**Non-deterministic plugin behaviour.** The same plugin, opened twice, behaved differently for no recoverable reason. This produced the manifest-first validation approach: every module is fully validated (magic number, contract version, dependency graph, cycle check) before any of its code is called, with a hard reject and a named error on any mismatch.

**"Just don't make these mistakes."** The prevailing advice online for both Unreal and Unity on Linux is a community-maintained list of things to avoid — actions that are technically possible but will destroy your project if done. That is not engineering. It is asking developers to carry, in their heads, a map of the engine's failure modes. Red Plasma's answer is that the wrong path should be hard to take, failures should be loud and named, and a developer should never need to already know what went wrong in order to find out.

## Background

This design leans heavily on a PLC / industrial-automation mindset rather than a typical high-level software background: deterministic cycle timing, distrust of hidden cost, a preference for precomputed/lookup-table answers over runtime calculation, and treating interrupts as a first-class, serious concept rather than a software nicety. That perspective shapes nearly every decision below.

## Core principles

### 1. Modules own their own complexity completely
The engine core never knows what's inside a module — only the fixed, thin contract it exposes. A Vulkan renderer module and a Wayland window module can be wildly different and arbitrarily complex internally, as long as they expose the same shape to everything else. This is what makes swapping Vulkan/DirectX/OpenGL, or Win32/Xorg/Wayland, possible without forking the engine.

### 2. Prefer the cheapest correct operation
Integer math over floating point where possible. Addition/subtraction/comparison over multiplication. Multiplication over division. Lookup tables over repeated runtime calculation. Bit shifts over division by powers of two. None of this is premature optimization for its own sake — it's a default stance, applied consistently, so the engine never assumes more hardware than it needs.

### 3. Push expensive operations to setup time, not the hot path
Division, dynamic allocation, and dynamic symbol resolution are not banned — they're costs to justify. Where they're unavoidable, they belong at initialization/calibration time, never inside a per-frame or per-tick loop.

### 4. Make expensive subsystems swappable per target, not universally heavy
Rather than building one allocator, one handle manager, or one renderer that tries to serve every target at once, each of these is a module that can have multiple implementations — a generous version for modern desktops, a lean fixed-size version for constrained/old hardware — all behind the same contract.

### 5. Errors are information, not just pass/fail
Every function returns a real error/status code, not a boolean. Codes are tiered by source (OS, engine, module) so failures are traceable to where they actually happened, and modules are free to define their own error space without the engine needing to understand it. Where a failure involves multiple modules or a relationship between them (such as a circular dependency), the engine reports the *specific* modules and the *specific* path involved, in plain language — not just a code. A code tells you something failed; a named, explained failure tells you what to actually go fix. This is treated as a real requirement, not a nice-to-have, since indirect or effortful debugging is a cost this project deliberately designs away wherever possible.

### 6. Ownership is explicit and never silently shared
Whoever creates a resource owns it and is responsible for destroying it. Cross-module access to something you don't own goes through a managed indirection (the handle manager), not a raw shared pointer — turning a class of crashes (use-after-free) into a normal, handleable error code instead.

### 7. Treat interrupts as real, even in software
Hardware-facing modules may one day need genuine interrupt handling (fault lines, emergency stop equivalents). Rather than adding that capability only when hardware support arrives, every module's contract includes an interrupt path from the start — usually a no-op, always present, never something that can be silently undeclared and forgotten. The full module contract is six slots: `init`, `run`, `recall`, `update`, `delete`, `interrupt` — always all six, no capability flags, unused slots return 0.

### 8. Open source the engine, keep games and modules free
MPL-2.0 was chosen specifically so that improvements to Red Plasma itself flow back, while developers building modules or games against it keep full freedom over their own licensing.

### 9. The core is memory-safe by construction; modules are trusted by review
The engine core is written in **Rust**, specifically because the core owns the project's most ownership-sensitive code — the handle manager and the custom allocators — where Rust's compile-time borrow checking directly prevents the exact bug classes (use-after-free, double-free, dangling pointers) the architecture is already designed around avoiding at a higher level. This mirrors the precedent set by the Linux kernel's adoption of Rust: the safe language doesn't replace the C ABI boundary, it sits behind it.

Every module, regardless of implementation language, exposes a plain **C ABI** at its boundary — this is a hard requirement, not a preference, because it's the only ABI every mainstream systems language (C, C++, Rust, and others) can reliably agree on. Inside that boundary, a module author is free to use any language they want.

Rust's safety guarantees apply to the engine core's own code. They do **not** and **cannot** extend across the FFI boundary into a module written in C, C++, or any other language — raw pointers crossing that boundary are inherently unverifiable from the core's side. Module authors are responsible for the safety and correctness of the code they submit. Modules are expected to be audited before being trusted, the same way any native plugin in any ecosystem would be — Red Plasma does not claim or imply a safety guarantee it cannot enforce.

### 10. All OS contact is owned by the IOS — nothing else touches the OS directly
Red Plasma defines its own OS-agnostic vocabulary of system calls (`rp_file_open`, `rp_load_library`, `rp_get_time`, etc.) — the **IOS (Interface Operating System)**. The IOS sits between Red Plasma and the actual OS, mapping Red Plasma's calls to whatever the underlying platform needs. It is the only place in the entire codebase where OS-specific code lives.

This is a hard rule, not a guideline: **nothing outside `os/` may make a direct OS call**. No `fopen`, no `dlopen`, no `clock_gettime`, no `CreateFile` anywhere in engine core, plugins, or modules. Every OS operation goes through IOS. A grep for raw OS calls outside `os/` should return zero hits — any hit is a bug.

The IOS is a **never-ending update file** — whenever Red Plasma needs to interact with something new at the OS level, the IOS grows a new function. This is accepted and expected. Every serious cross-platform system has this file; Red Plasma just names it honestly and keeps it in one place. The discipline is keeping the IOS thin: pure mapping, no logic, no state, no decisions — just `rp_*` call in, OS call out.

Porting Red Plasma to a new OS means implementing the IOS for that target. Nothing else changes.

### 11. Help your fellow developers — maintain the glossary
Red Plasma's documentation uses a lot of abbreviations, acronyms, and project-specific terms. If you introduce a new one — in code, comments, or documentation — add it to `docs/GLOSSARY.md` before submitting. This applies to contributors at every level, from engine core changes to new modules.

**If you are unsure whether something needs a glossary entry — add it.** The cost of an unnecessary entry is one line. The cost of a missing one is every developer who has to go hunting. When in doubt, always add it.

**Naming prefixes follow the same rule.** Every prefix used in Red Plasma must have a glossary entry — no undocumented prefixes, ever. Prefixes declare ownership and origin (`rp_` for IOS calls, `rpvk_` for the Vulkan renderer module, and so on). A prefix is meaningless without its glossary entry; with it, it becomes a navigational tool that tells a reader exactly where something comes from. Function and variable names do the descriptive work; prefixes do the ownership work — both matter, both must be documented.

Clear language is part of the architecture too.
