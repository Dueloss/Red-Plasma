# Contracts

These are the concrete, binding technical rules every plugin and module in Red Plasma must follow. Where `PHILOSOPHY.md` explains *why*, this document defines *what*, precisely enough to implement against.

---

## 0. Two-tier system: engine plugins and modules

Red Plasma has two distinct tiers of extensible binary, each with its own contract:

| | Engine plugins | Modules |
|---|---|---|
| **Purpose** | Pure engine infrastructure — no gameplay, no game-developer-facing capability | Game engine functionality — what game developers write and use |
| **Contract** | 3-slot: `init`, `call`, `delete` | 6-slot: `init`, `run`, `recall`, `update`, `delete`, `interrupt` |
| **Loaded by** | Bootstrap loader (compiled in — the only hardcoded piece) | Module loader (itself an engine plugin) |
| **Load order** | Fixed, hardcoded sequence before anything else | Priority-sorted, manifest-driven |
| **Game loop** | Never participates | Two-pass hot loop (`run` then `recall`) |
| **Examples** | Allocator, sort, handle manager, logger, module loader | Optimizer, renderer, windowing, physics, audio, input |

Both tiers use the **same manifest shape** — magic number, contract version, contract-shape hash, dependencies, author metadata, and a `kind` field (`plugin` or `module`) so the bootstrap loader knows which contract to apply. Same validation path, same hard-reject policy.

---

## 0a. Engine plugin lifecycle (3-slot table)

Every engine plugin exposes exactly three functions, always, in a fixed order. Unused functions return `0` (success) immediately and do nothing.

| Slot | Name | Meaning |
|---|---|---|
| 0 | `init` | One-time setup. Called by the bootstrap loader in the fixed boot sequence. |
| 1 | `call` | Explicit invocation by whatever needs the plugin's service. Not driven by any loop — called on demand. The sort plugin's `call` is invoked once to sort the module arrays; an allocator's `call` is invoked whenever something needs memory. |
| 2 | `delete` | Teardown. Called during engine shutdown in reverse boot order. |

Engine plugins **never participate in the game loop** — they have no `run`, `recall`, `update`, or `interrupt`. They are infrastructure the engine calls explicitly, not participants in the tick.

**Bootstrap sequence:** the IOS implementation (compiled in — the only piece that touches OS APIs directly) loads a fixed, known list of plugins in a hardcoded order before anything else starts. Plugin loading itself uses `rp_load_library`/`rp_get_symbol` — IOS calls, never raw OS calls:

1. Allocator plugin(s)
2. Logger plugin
3. Sort plugin
4. Handle manager plugin
5. Module loader plugin ← from this point, full module loading is available

After step 5, the module loader takes over and loads all modules via the normal manifest/priority/two-array system.

---

## 0b. IOS — Interface Operating System

The IOS is the only place in the entire Red Plasma codebase where OS-specific code lives. It sits between Red Plasma and the actual OS, mapping Red Plasma's own vocabulary of system calls to whatever the underlying platform requires.

**Folder structure:**
```
os/
├── irp_os.h   — OS interface declarations (rp_* functions), no implementation
├── linux/     — Linux IOS implementation
├── windows/   — Windows IOS implementation (future)
├── macos/     — macOS IOS implementation (future)
└── 16bit/     — 16-bit bare metal IOS implementation (future)
```

`os/irp_os.h` contains only **declarations** — the `rp_*` function signatures that everything else in Red Plasma calls. No OS knowledge, no implementation. Each OS subfolder contains one complete implementation of those declarations, compiled in at build time for the target platform.

**The hard rule:** nothing outside `os/` may make a direct OS call. No `fopen`, `dlopen`, `clock_gettime`, `CreateFile`, `pthread_create`, or any other OS API anywhere in engine core, plugins, or modules. Every OS operation goes through an `rp_*` IOS function. A grep for raw OS calls outside `os/` should return zero hits — any hit is a bug, not a design choice.

**IOS function vocabulary (initial — grows as needed):**

| Function | Purpose |
|---|---|
| `rp_load_library(path)` | Load a dynamic library (`dlopen` / `LoadLibrary`) |
| `rp_get_symbol(lib, name)` | Resolve a symbol from a loaded library (`dlsym` / `GetProcAddress`) |
| `rp_unload_library(lib)` | Unload a dynamic library |
| `rp_get_time()` | Current time as an integer (nanoseconds/ticks, platform-dependent unit) |
| `rp_file_open(path, mode)` | Open a file |
| `rp_file_read(file, buf, len)` | Read from a file |
| `rp_file_write(file, buf, len)` | Write to a file |
| `rp_file_close(file)` | Close a file |
| `rp_console_write(msg)` | Write to console/terminal output |
| `rp_thread_create(fn, arg)` | Create a thread |
| `rp_thread_join(thread)` | Wait for a thread to finish |
| `rp_mutex_create()` | Create a mutex |
| `rp_mutex_lock(mutex)` | Lock a mutex |
| `rp_mutex_unlock(mutex)` | Unlock a mutex |
| `rp_signal_handle(sig, fn)` | Register a signal/interrupt handler |

This list is explicitly **not exhaustive** — the IOS is a never-ending update file. When Red Plasma needs to interact with something new at the OS level, a new `rp_*` function is added. This is expected and accepted; the discipline is keeping each function a thin mapping with no logic, no state, and no decisions.

**The capability implication:** the IOS vocabulary for a given target defines what Red Plasma can do on that target. If `rp_thread_create` isn't implemented in the 16-bit IOS, threading isn't available there. If `rp_file_write` isn't implemented, persistence isn't available. The IOS list is implicitly a capability document for each port.

**Porting Red Plasma to a new OS** means implementing the IOS for that target in a new `os/<target>/` folder and compiling it in. Nothing else in the codebase changes.

---

Every module — regardless of what it does — exposes exactly six functions, always, in a fixed order. No module may omit any of them. Unused functions return `0` (success) immediately and do nothing.

| Slot | Name        | Meaning |
|------|-------------|---------|
| 0    | `init`      | One-time setup. Allocate via the appropriate allocator module, register with the handle manager if needed, resolve dependencies on other modules. |
| 1    | `run`       | Called once per tick, first pass — before any other module's `recall`. The optimizer module uses this to record its start timestamp. Most modules do their primary per-tick work here. |
| 2    | `recall`    | Called once per tick, second pass — after every module's `run` has completed. The optimizer module uses this to record its end timestamp, calculate elapsed time, update the EMA, and yield until the next tick begins. Most modules leave this as a no-op returning 0. |
| 3    | `update`    | Reactive execution — called when external state has changed and the module needs to resync its internals. Not on a fixed schedule. |
| 4    | `delete`    | Teardown counterpart to `init`. Frees what it allocated, releases handles it owns. |
| 5    | `interrupt` | Emergency/priority override — stop normal flow and react immediately. Hardware fault lines, OS-level signals (e.g. window close), or software-level forced events (e.g. a multiplayer server forcing a client resync). |

**Rule:** the engine always calls all six slots on every module without checking capability flags. The cost of calling a no-op is negligible compared to the cost (and risk) of checking a table to decide whether to call it.

**The main loop shape:**
```
while running:
    for ptr in all_modules:    ptr.run()    // wait flag checked after each
    for ptr in recall_modules: ptr.recall() // wait flag checked after each
```

**Two-array optimization:** the engine maintains two flat pointer arrays, built once at load time:

| Array | Contents | Sorted by |
|---|---|---|
| `all_modules` | Pointer to every loaded module | `run_priority` descending |
| `recall_modules` | Pointer to modules declaring `uses_recall: true` | `recall_priority` descending |

The hot loop walks each array in order — pure pointer iteration, no branching, no priority checks at runtime. All ordering decisions happen once at load time and are never revisited unless a module is loaded or unloaded.

**Arrays hold pointers only** — they do not own the modules. Actual module data lives wherever the module loader placed it. Removing a module means removing its pointer from the array(s), nothing else.

**Sorting is itself a module.** The sort module is responsible for ordering both arrays before the first tick. The default implementation uses **heapsort** — in-place, no extra memory required, guaranteed O(n log n) worst case (no pathological inputs unlike quicksort), predictable cost on any hardware. A constrained target can swap in a simpler sort (e.g. insertion sort, which is smaller in code size and faster on very small arrays). The sort module is a **core dependency** — it must run before any other module's `init`, since the arrays must be ordered before the first tick begins.

**Priority is declared per-pass in the manifest** — two separate integer fields, higher number = higher priority (runs earlier in that pass). Separating them matters: the optimizer must be first in `run` (to stamp the tick start time) but last in `recall` (to stamp the end time after all other recall work is done, then yield — being first in recall would leave untimed work after it).



## 1a. Implementation language and ABI

- The **engine core and bootstrap loader** are implemented in **Rust**.
- Every **engine plugin** must expose its 3-slot table as a plain **C ABI**.
- Every **module** must expose its 6-slot table as a plain **C ABI**.
- C ABI applies to both tiers — `extern "C"` linkage, no C++ name mangling, no language-specific calling convention. This is the only common ground every systems language can reliably agree on.
- **Authors of both plugins and modules may use any language internally** (C, C++, Rust, or otherwise), provided the exported boundary is C ABI compliant.
- The engine core's memory-safety guarantees apply only to the core's own code. They do not, and cannot, extend across the FFI boundary. **Plugin and module authors are responsible for the safety and correctness of their own code.** All are expected to be reviewed/audited before being trusted in a build.

## 1b. Coding style

The engine core follows standard idiomatic **Rust naming conventions** (the same ones `rustc`/`clippy` enforce by default), rather than a custom scheme — chosen specifically to avoid permanently fighting the compiler's own lints over a stylistic preference that wasn't strongly held.

| Element | Convention | Example |
|---|---|---|
| Functions | `snake_case` | `create_window`, `resolve_handle` |
| Variables | `snake_case` | `frame_count`, `window_handle` |
| Constants | `UPPER_SNAKE_CASE` | `MAX_HANDLE_COUNT`, `FILE_NOT_FOUND` |
| Types (structs, enums, traits) | `PascalCase` | `WindowHandle`, `ModuleManifest` |
| Enum variants | `PascalCase` | `ErrorCode::FileNotFound` |
| Modules/files | `snake_case` | `handle_manager.rs`, `window_linux.rs` |
| Booleans | `snake_case`, prefixed for clarity | `is_loaded`, `has_dependency`, `should_retry` |
| Acronyms in names | Treated as a normal word — only the first letter capitalized | `VulkanId`, not `VulkanID` |

- **Error code constants** (the 0–99 / 100–999 / 1000+ tiers from §2) follow the constants rule: `UPPER_SNAKE_CASE`, e.g. `FILE_NOT_FOUND = 22`.
- **C-ABI exported function names** (every module's lifecycle table and manifest entry point) use `snake_case` regardless of the module's internal implementation language, matching standard C convention for exported symbols — e.g. `get_module_manifest`, `module_init`.
- **Visibility** (public vs. private) is expressed through Rust's `pub` keyword and module structure, not through a naming prefix or suffix.
- This convention applies to the **engine core**. Modules written in other languages are free to follow that language's own idioms internally, as long as their exported C-ABI symbols still use `snake_case` at the boundary.
- **Naming prefixes declare ownership and origin.** Every prefix used in Red Plasma must have a glossary entry — no undocumented prefixes, ever. See `docs/GLOSSARY.md` for the current prefix table. Function and variable names do the descriptive work; prefixes do the ownership work.
- **Abbreviations, acronyms, prefixes, and project-specific terms must be added to `docs/GLOSSARY.md`.** If you introduce a new one anywhere in Red Plasma — code, comments, or documentation — add it before submitting. **If you are unsure whether something needs a glossary entry — add it.** The cost of an unnecessary entry is one line; the cost of a missing one is every developer who has to go hunting.

## 2. Function return contract

Every function in the engine, in every module, follows the same shape:

```c
int function_name(/* parameters */, OutputType** out_result);
```

- The **return value is always an integer status/error code.**
- Any actual data the function produces is returned through an **out-parameter** (pointer, or pointer-to-pointer for handles/opaque types) — never packed into the return value itself.
- Callers check the return code first. Out-parameter data is only valid/meaningful when the code indicates success.

### Error code tiers

| Range        | Source |
|--------------|--------|
| 0–99         | OS-level errors |
| 100–999      | Engine-level errors |
| 1000+        | Module-defined errors — each module owns and defines its own codes in this space. The engine does not need to know what a module's codes mean. |

Modules should provide a way to translate their own codes to a human-readable string for logging/debugging purposes (exact mechanism TBD — see open questions).

## 3. Ownership and handles

- **The creator owns it.** Whatever module/code calls a `create_*` function is responsible for calling the matching `destroy_*` function. No implicit transfer of ownership.
- **No raw long-lived pointers shared across module boundaries.** If module B needs to use something module A owns (e.g. renderer using a window's surface), it does so through the **handle manager module** rather than holding a raw pointer directly.
- **Handle manager** maintains an ID → pointer table. Other modules register pointers and get back an opaque ID; they resolve that ID to a real pointer only when needed. When the owning module destroys the underlying resource, the ID becomes invalid — any subsequent resolve attempt returns a clean "invalid handle" error code rather than a dangling pointer.
- **Handle manager is itself a swappable module.** A desktop implementation can use a dynamically growing table; a constrained-hardware implementation can use a fixed-size table tuned to the platform's memory budget. Same contract, different implementation.
- **Double-free / use-after-free mitigation:** `destroy_*` functions should null out the caller's pointer (via the pointer-to-pointer pattern) after freeing, and no-op safely if called again on an already-null/invalid handle.
- **Destruction notification: blocking, with a timeout.** When a module destroys a resource that other modules may be depending on, it announces this through the subscriber system and **waits, up to a fixed maximum timeout**, for dependents to acknowledge they're done with it. This gives dependents a real chance to release their reference safely before the underlying memory is freed, while the timeout prevents one unresponsive or buggy module from hanging the entire engine.
- **On timeout, the caller decides what happens — but the failure is always reported loudly.** The engine does not silently pick a resolution. The caller (whatever code initiated the destroy) chooses the outcome when the timeout fires:
  - **Leave it** — do not free the resource; treat it as a deliberate, known, temporary leak rather than risk a dangling pointer. The caller can retry, investigate, or force it later.
  - **Terminate** — treat the unresponsive dependent as serious enough to crash/halt the program rather than continue in a state that can't be guaranteed safe.
  
  Regardless of which the caller chooses, the engine **always logs/reports the event clearly** — naming the resource and the specific dependent module that failed to acknowledge in time — consistent with the project's broader stance that failures must be diagnosable, never silent.

## 4. Memory allocation

- No reliance on system `malloc`/`free` inside engine or module code. Allocation goes through dedicated **allocator modules**.
- Allocator strategy is chosen **per use case**, not globally:
  - **Pool allocator** — fixed-size blocks, O(1) alloc/free via free-list. For frequently created/destroyed same-sized items (handles, entities, particles).
  - **Arena/linear allocator** — bump-pointer allocation, freed all at once (e.g. per frame, per level load). No per-item free.
  - **Stack allocator** — like an arena but supports strict LIFO freeing for nested/temporary scopes.
  - **General-purpose allocator** — fallback for variable-size, variable-lifetime allocations where the above don't fit.
- Allocator modules, like the handle manager, are swappable per hardware target (e.g. a constrained target may only ship pool/arena allocators and have no general-purpose fallback at all).
- The handle manager's internal table is expected to be built on top of an allocator module, not manage raw memory itself.

## 5. Timing and the game loop

The engine core loop is intentionally minimal — it knows nothing about timing, Hz, or optimization. Its entire job is:

1. Call `run` on every loaded module in order
2. After each `run`, check the module's **wait flag** — if set, block until `run` returns before moving to the next module
3. Repeat

The engine does not measure time, does not know what Hz it is running at, and does not know what "waiting" means underneath. All of that complexity lives in the **optimizer module**.

### 5a. The optimizer module

The optimizer module owns everything related to loop timing:

- An internal timer (OS clock, hardware register, or whatever the target provides — entirely invisible to the engine core)
- The EMA self-tuning calculation
- Persisted Hz across sessions (written on `delete`, read on `init`)
- The decision of whether and how long to yield each tick

From the engine core's perspective, the optimizer is just another module whose `run` happens to take some time before returning. The engine has no awareness that any sleeping or timing is occurring.

**Timing rules (internal to the optimizer module):**
- All timing values stored as **integers** — nanoseconds/microseconds in a `u64` on modern hardware, scaled to smaller types on constrained targets. Never floating point.
- Hz is **self-tuning via EMA**: `new_average = (old_average + measured) >> 1` (bit-shift divide-by-two, no true division).
- The learned average is **persisted across sessions** and **held fixed within a session** — physics/simulation stays deterministic per run, while the engine adapts to hardware over time.
- **First launch** uses a developer-defined fixed Hz. Session-mined auto-calibration is a planned future enhancement.
- Division is restricted to setup time only (e.g. Hz → interval conversion once at `init`). The per-tick hot path uses only add/subtract/compare/bit-shift.
- Swappable per hardware target — a 16-bit implementation uses a hardware timer register, possibly millisecond resolution, no file-based persistence. Same contract, completely different internals.

### 5b. The wait flag

Every module has a **wait flag** — a boolean the engine core checks after calling `run`. If set, the loop waits for `run` to return before proceeding to the next module. If not set, the loop moves on immediately.

| Source | Behaviour |
|---|---|
| **Manifest declaration** (`always_wait`) | Static default — set once at load time, seeds the runtime flag. Auditable at load time before any code runs. |
| **Runtime override** | The module can change its own flag per tick at runtime. One cheap boolean check per module per tick; negligible cost even if never changed. |

The hybrid approach is deliberate: the manifest declaration makes the default auditable and predictable; the runtime override allows dynamic behaviour (e.g. "block this tick but not the next") without requiring a separate mechanism.

**The optimizer module sets `always_wait: true` in its manifest** — it always blocks, because its entire purpose is to control how long a tick takes. This is the mechanism that makes "the engine runs as fast as it can and the optimizer decides when to continue" work without the engine needing any special knowledge of timing.

**Other modules that may legitimately use the wait flag:**
- An audio module syncing to a buffer boundary
- Any hardware-facing module that must complete before the next tick can safely proceed
- Any module using the threading module internally that needs its async work to finish before returning

**Threading note:** if a module does async work via the threading module, it still simply does not return from `run` until that work is done. How it waits internally (spinning, sleeping, thread signal) is the module's own business — the loop sees the same thing either way.



## 6. Events / subscriber system

- A generic publish/subscribe mechanism lives in the engine core. Any module can subscribe to events raised by another module (e.g. a window's `onResize`/`onConfigure` event) rather than each module inventing its own ad-hoc callback registration style.
- Used for, at minimum: window resize/configure notifications, and (planned) handle lifecycle notifications.

## 7. Loading — plugins and modules

- Both engine plugins and modules are compiled as **dynamic libraries** — the OS-specific format (`.so` on Linux, `.dylib` on macOS, `.dll` on Windows) is an implementation detail owned entirely by the bootstrap loader. Everything above the bootstrap loader refers only to "load this plugin/module," never to a specific file format or OS API.
- The **bootstrap loader** (the only compiled-in piece) owns all OS-specific dynamic library loading — `dlopen`/`dlsym` on Linux, `LoadLibrary`/`GetProcAddress` on Windows, or whatever the target platform requires. Swapping targets means swapping the bootstrap loader's loading implementation, nothing else.
- On constrained targets with no dynamic loading support, plugins and modules may be statically linked at compile time — the same contract applies, only the loading mechanism changes.
- **Engine plugins** live in the `plugins/` folder, loaded by the bootstrap loader in a fixed hardcoded sequence.
- **Modules** live in the `modules/` folder, loaded by the module loader plugin after bootstrap completes, using the manifest/priority/two-array system.
- Dependency declaration lives in the manifest (§7a), validated before `init` is called on either tier.

## 7a. Manifest

Both engine plugins and modules expose a manifest through a **known, fixed exported function** (`get_manifest()`) — never as a separate sidecar file. The same manifest shape applies to both tiers.

| Field | Purpose |
|---|---|
| **Magic number** | Fixed constant identifying a genuine Red Plasma binary. Checked first, before anything else. |
| **`kind`** | `plugin` or `module` — tells the loader which contract (3-slot or 6-slot) to apply. Hard reject if kind doesn't match what the loader expects at that stage. |
| **Contract version** | Single incrementing integer. Engine expects one version; binary declares what it was built against. Hard reject on mismatch. |
| **Contract-shape hash** *(optional)* | Hash of the expected function table shape — catches accidental drift even when version matches. |
| **Dependencies** | List of other plugins/modules required to be loaded first. |
| **`always_wait`** | *(modules only)* Boolean — engine blocks on `run`/`recall` until they return. Seeds runtime wait flag. Default: `false`. |
| **`uses_recall`** | *(modules only)* Boolean — adds module to `recall_modules` array. Performance routing hint, not capability flag. Default: `false`. |
| **`run_priority`** | *(modules only)* Integer, higher = earlier in `run` pass. Anchors: 1000 optimizer, 800 platform, 600 systems, 400 gameplay, 200 rendering, 1–199 debug/tooling. |
| **`recall_priority`** | *(modules only)* Integer, higher = earlier in `recall` pass. Separate from `run_priority`. Only meaningful if `uses_recall: true`. |
| **Author/writer metadata** | Descriptive only. Supports audit trail — does not gate loading. |



**Dependency resolution policy:** if a declared dependency is not already loaded, the engine treats this the same as any other manifest validation failure — **hard reject**, the module does not load, a clear error code is returned identifying the missing dependency. The engine does not attempt to auto-load dependencies on a module's behalf; load order is the responsibility of whatever is orchestrating module loading (the engine's startup sequence, or a game/application's own init code), not something modules silently trigger in each other.

**Circular dependency detection:** before `init` is called on *any* module in a load batch, the engine loads each module (without initializing it), reads every manifest, and builds the full dependency graph. A cycle check (topological sort) runs against that graph before initialization begins. If a cycle is found:

- The **entire batch is rejected** — no module in the batch is initialized, even ones not directly involved in the cycle. A load batch either fully resolves or it doesn't; there is no partial/best-effort loading.
- The error report **names every module in the cycle, in order** (e.g. `module_a → module_b → module_c → module_a`), not just a generic failure code. This is a deliberate accessibility and debuggability requirement, not an afterthought: a bare error code forces the developer to manually reconstruct what went wrong, which is exactly the kind of indirect, effortful diagnosis this project is designed to avoid. The engine should report the problem the way a clear, direct person would explain it — naming the exact modules on the exact path, with a plain-language suggestion (e.g. "check for an updated version, or remove one of the modules in this cycle").

**Validation policy: hard reject.** If the magic number is missing/wrong, or the contract version does not exactly match, the module is refused — it is not loaded, not partially trusted, and no attempt is made at backward/forward compatibility. The engine's own contract is expected to change rarely and deliberately, so a single exact-match check is sufficient; there is no need for semver-style major/minor reasoning.

**Why in-binary, never a sidecar file:** a sidecar manifest file invites a reviewer to inspect the manifest and skip actually opening the module's real code — the convenient path and the safe path diverge. An in-binary manifest can only be read by actually loading/inspecting the real binary, so there is no shortcut that bypasses the audit it's meant to support. This is a deliberate "make it hard to do it wrong" choice, not just a robustness one.

## 7b. Logging and error string generation

**Error string generation is automated, not hand-maintained.** Each error is defined once in a single source list (code, short name, message template — e.g. `22, FileNotFound, "could not read file: {path}"`). The numeric constant and the human-readable lookup are both generated from that one definition, so adding or changing an error never requires hand-syncing a separate table. In the Rust core this is implemented via an enum plus codegen (derive macro or build-script); a manually maintained switch-style lookup is explicitly avoided.

**Error templates are static; context is filled in at the point of logging.** A template like `"could not read file: {path}"` carries no dynamic data itself — the actual path, and the timestamp, are filled in only when the error is actually logged, not baked into the generated table.

**Logging is separate from console output.** Every log entry is timestamped and written to a persistent log, regardless of whether anything is printed to the console. Console output is a fatal-only filtered view by default; non-fatal entries are still recorded for later inspection, they just don't interrupt the console.

**Verbosity is a compile-time setting, not a runtime flag.** A runtime `-v`-style flag was considered and rejected: even a single cheap branch per function call is a cost constrained/16-bit targets may not be able to afford if paid on every call, every tick. Compile-time selection allows trace-level logging to be physically stripped from the build entirely when not needed — true zero cost, not just low cost.

| Level | Meaning |
|---|---|
| 0 — silent | Nothing logged |
| 1 — fatal | Unrecoverable errors only (default console output) |
| 2 — error | + recoverable errors |
| 3 — warn | + warnings |
| 4 — info | + general informational events (e.g. module loaded) |
| 5 — debug/trace | + every function call, success or failure |

**Scope: engine core only.** This logging system is implemented and owned by the engine core. It is not mandated as a shared implementation across modules — modules may be written in any language, and a single logging implementation cannot be forced across all of them.

**Modules seeking "official" status must still follow the convention.** A module is not required to use the engine's logging system internally, but to be considered an official/trusted Red Plasma module it must:
- Use the **same error-message style/format convention** as the engine core (tier/code/message-template shape).
- Write its own log file, named after the module (`<module_name>.log`).

This keeps the actual logging mechanism flexible per module language while keeping the *output* consistent enough that a developer auditing or debugging a multi-module setup isn't reading several incompatible log formats side by side.

## 8. Windowing

- Each OS/windowing backend (Win32, Xorg, Wayland, ...) is its own module, but all expose the same public contract.
- The window module is **naive by design**: a bare drawable surface only — no widgets, no toolkit event loop, no UI chrome. Full toolkits (Qt, GTK/libadwaita) are not window backends; they're reserved for a possible future editor (see `DESIGN_DECISIONS.md`).
- Implemented via **GLFW** internally for now; a hand-rolled native replacement (raw Xlib/XCB, raw Wayland protocol, raw Win32) is planned later, once the contract is proven.
- `createWindow(x, y, w, h, ...)` returns the **requested** values immediately as a placeholder (via out-parameter), since not all backends (notably Wayland) can guarantee position/size synchronously.
- The **actual** resulting position/size is delivered via an `onResize`/`onConfigure` event through the subscriber system. Win32/Xorg are expected to fire this near-instantly; Wayland fires it whenever the compositor responds.
- No backend is treated as the "normal" case that others special-case around — the event-based "here's what you actually got" model is the contract for all backends equally.

## 8a. Renderer

- Vulkan is the primary/default backend; DirectX and OpenGL are alternate backend modules behind the same shared contract.
- The standard renderer contract is **fully opaque**: callers interact only with Red Plasma's own rendering vocabulary (e.g. create surface, load mesh, submit draw, present frame) through opaque handles via the handle manager. No backend-specific type (`VkBuffer`, `ID3D12Resource`, etc.) is ever exposed through this contract.
- There is no backend-specific extension mechanism built into the standard contract. A module needing direct access to a specific backend's unique features (e.g. Vulkan-only bindless textures or ray tracing) is free to bypass the standard renderer module entirely and talk to that backend directly as its own module — the standard contract is a good default, not a mandatory path.
- **Not an engine-level decision: 2D vs. 3D rendering path.** Whether 2D rendering is implemented as a thin layer over the 3D pipeline (sprites as textured quads) or as a genuinely separate path is not something the engine core decides or enforces — it's a property of whichever renderer module is loaded. A `renderer_2d_layered` module and a `renderer_2d_native` module could both exist, both valid, both swappable, same as any other renderer choice. The engine has no opinion here by design, consistent with §8a's "module system is the escape hatch" reasoning.

## 9. Editor / tooling boundary

- The engine has no knowledge of or dependency on any editor or tool. A future editor is a separate project, consuming the engine only through its public API — same as any game would.

## Open questions (not yet decided)

None currently — every question raised during design so far has either been decided, or reframed as a module-level choice rather than an engine-level one (see §8a). New questions will be added here as they come up.
