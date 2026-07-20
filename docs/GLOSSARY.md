# Glossary

A reference for shorthand, abbreviations, and project-specific terms used throughout Red Plasma documentation and code. When in doubt, check here first.

**Contribution rule:** if you introduce a new abbreviation, acronym, prefix, or project-specific term anywhere in Red Plasma — in code, comments, or documentation — add it here before submitting. This is a requirement, not a suggestion. Help your fellow developers: do not assume readers will already know what something means.

**If you are unsure whether something needs a glossary entry — add it.** The cost of an unnecessary entry is one line. The cost of a missing one is every developer who hits that term and has to go hunting. When in doubt, always add it.

---

## Naming prefixes

All prefixes used in Red Plasma must have an entry here. No undocumented prefixes, ever.

| Prefix | Stands for | Used in |
|---|---|---|
| `irp_` | Interface Red Plasma | Prefix for Red Plasma interface header files. The `i` stands for interface — declarations only, no implementation. Example: `irp_os.h`. |
| `rp_` | Red Plasma | Prefix for all Red Plasma IOS functions. If you see `rp_`, it is a Red Plasma IOS call. Declared in `os/irp_os.h`, implemented in `os/<target>/`. |
| `rpdx_` | Red Plasma DirectX | DirectX renderer module (future). |
| `rpgl_` | Red Plasma OpenGL | OpenGL renderer module (future). |
| `rpvk_` | Red Plasma Vulkan | Vulkan renderer module — Red Plasma's wrapper around Vulkan APIs. Distinguishes Red Plasma Vulkan calls from raw `vk_` Vulkan calls, which are not used directly outside the Vulkan renderer module. |
| `rpwin_` | Red Plasma Windows | Windows windowing module (future). |

---

## Abbreviations and acronyms

## Abbreviations and acronyms

| Short | Full | Meaning in Red Plasma |
|---|---|---|
| **ABI** | Application Binary Interface | The low-level contract that defines how compiled code calls between different binaries. Red Plasma uses the C ABI at all plugin and module boundaries so any language can talk to any other. |
| **ECS** | Entity Component System | A common game engine architecture pattern. Not a Red Plasma concept — mentioned only for contrast. |
| **EMA** | Exponential Moving Average | The smoothing algorithm the optimizer module uses to learn the average loop execution time across sessions. Implemented as a bit-shift divide-by-two: `(old + new) >> 1`. |
| **FFI** | Foreign Function Interface | The mechanism that lets Rust code call into C code (and vice versa). Used at every plugin and module boundary via `extern "C"`. |
| **HAL** | Hardware Abstraction Layer | Similar concept to PAL/IOS, common in embedded and PLC systems. The IOS serves this role for Red Plasma on bare metal targets. |
| **Hz** | Hertz | Cycles per second. Used to describe the engine loop's target tick rate. |
| **IOS** | Interface Operating System | Red Plasma's own abstraction layer between the engine and the OS. All OS calls go through IOS functions (`rp_*`). The only place in the codebase where OS-specific code lives. |
| **MPL** | Mozilla Public License | The open source license Red Plasma uses (version 2.0). File-level copyleft — changes to Red Plasma's own files must be shared; games and modules built against it are free to use any license. |
| **OS** | Operating System | The underlying platform the engine runs on — Linux, Windows, macOS, or bare metal hardware. |
| **PAL** | Platform Abstraction Layer | A general industry term for what the IOS does — abstracts OS differences behind a common interface. Red Plasma calls this the IOS. |
| **PLC** | Programmable Logic Controller | Industrial control hardware. The design philosophy behind Red Plasma draws heavily on PLC thinking: deterministic timing, explicit ownership, first-class interrupts, integer-first math. |
| **Python** | Python (programming language) | Used for Red Plasma development tooling only — glossary sorting, pre-commit hooks, dev scripts. Not used in the engine itself. Python 3 required. |
| **RHI** | Rendering Hardware Interface | An industry term for a renderer abstraction layer. Red Plasma's renderer contract serves this purpose but uses Red Plasma's own vocabulary rather than exposing GPU API concepts. |

---

## Project-specific terms

| Term | Meaning |
|---|---|
| **`all_modules`** | The flat pointer array of all loaded modules, sorted by `run_priority` at load time. Walked once per tick for the `run` pass. |
| **`always_wait`** | Manifest boolean. If `true`, the engine blocks after calling `run`/`recall` on this module until it returns. The optimizer sets this to `true`. |
| **Bootstrap sequence** | The fixed hardcoded order in which engine plugins are loaded at startup: allocator → logger → sort → handle manager → module loader. Managed by the IOS implementation. |
| **Circular dependency** | When module A depends on B and B depends on A (directly or through a chain). Detected by topological sort before any `init` is called. Entire batch rejected, exact cycle path named in the error. |
| **Destruction notification** | When a module destroys a resource, it announces this via the subscriber system and waits (up to a timeout) for dependents to acknowledge. On timeout, the caller chooses: leave it or terminate. Always logged loudly. |
| **Engine plugin** | A 3-slot (`init`, `call`, `delete`) dynamic library that provides engine infrastructure — allocator, logger, sort, handle manager, module loader. Lives in `plugins/`. Never participates in the game loop. |
| **Error code tiers** | 0–99: OS errors. 100–999: engine errors. 1000+: module-defined errors. Every function returns one of these; data comes back via out-parameter. |
| **First Night Out** | The vampire RPG game that motivated building Red Plasma. Inspired by Vampire: The Masquerade. The engine exists because existing engines on Linux were too opaque and fragile to build it in. |
| **Handle manager** | An engine plugin that maintains an ID → pointer table. Modules never share raw pointers across boundaries — they register a pointer, get back an opaque ID, and resolve it only when needed. Turns use-after-free crashes into clean "invalid handle" error codes. |
| **Hard reject** | Red Plasma's standard failure policy: if a validation check fails (wrong magic number, wrong contract version, circular dependency, etc.), the binary is refused entirely — no partial loading, no fallback. |
| **Hot path** | The code that runs every single tick. Red Plasma's design rule: the hot path uses only add, subtract, compare, and bit-shift — no division, no floating point, no dynamic allocation. |
| **IOS** | See abbreviations above. Red Plasma's OS abstraction layer, compiled in, lives in `os/irp_os.h`. |
| **Kind** | A manifest field — either `plugin` or `module`. Tells the loader which contract (3-slot or 6-slot) to apply. |
| **Magic number** | A fixed constant in the manifest that identifies a binary as a genuine Red Plasma plugin or module. Checked first, before anything else. |
| **Manifest** | The in-binary metadata every plugin and module must expose via `get_manifest()`. Contains magic number, kind, contract version, dependencies, and other fields. Validated before any code in the binary is called. |
| **Module** | A 6-slot (`init`, `run`, `recall`, `update`, `delete`, `interrupt`) dynamic library that provides game-engine functionality — optimizer, renderer, windowing, physics, audio, input. Lives in `modules/`. |
| **Optimizer module** | The module responsible for all loop timing — internal timer, EMA self-tuning, Hz persistence, and yielding between ticks. The engine core knows nothing about timing; the optimizer module owns it entirely. |
| **Out-parameter** | A pointer passed into a function that the function writes its result into. Used throughout Red Plasma so every function can return an error code as its actual return value. |
| **Pre-commit hook** | A git hook that runs automatically before every commit. Red Plasma's pre-commit hook (`tools/pre-commit`) sorts GLOSSARY.md tables alphabetically and checks that no raw OS calls exist outside `os/`. Install via `setup.sh` — see below. |
| **`recall_modules`** | The flat pointer array of modules that declared `uses_recall: true`, sorted by `recall_priority` at load time. Walked once per tick for the `recall` pass. |
| **`recall_priority`** | Manifest integer field. Higher number = called earlier in the `recall` pass. Controls position in `recall_modules`. Separate from `run_priority` — the optimizer needs high `run_priority` and low `recall_priority`. |
| **`rp_*`** | The naming prefix for all IOS functions (`rp_file_open`, `rp_get_time`, etc.). If you see an `rp_` prefix, it's an IOS call — OS-specific implementation lives in `os/<target>/`, declaration lives in `os/irp_os.h`. |
| **`run_priority`** | Manifest integer field. Higher number = called earlier in the `run` pass. Controls position in `all_modules`. |
| **`setup.sh`** | One-command development environment setup script at the repo root. Run once after cloning: `chmod +x setup.sh && ./setup.sh`. Checks for Python 3 and Rust, then installs the pre-commit hook automatically. |
| **Sidecar file** | A separate file placed alongside a binary (e.g. `mymodule.manifest`). Red Plasma explicitly rejects this pattern for manifests — in-binary only. |
| **Subscriber system** | A generic publish/subscribe event mechanism in the engine core. Used for window resize events, handle lifecycle notifications, and similar cross-module communication. |
| **`uses_recall`** | Manifest boolean. If `true`, the module is added to `recall_modules`. A performance routing hint, not a capability flag — a stale `false` is a missed optimization, not a safety hazard. |
| **Wait flag** | A per-module boolean. If set, the engine loop blocks after calling `run` or `recall` on that module until the call returns. Seeded by `always_wait` in the manifest, overridable per tick at runtime. |
