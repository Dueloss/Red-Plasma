# Contracts

These are the concrete, binding technical rules every module in Red Plasma must follow. Where `PHILOSOPHY.md` explains *why*, this document defines *what*, precisely enough to implement against.

## 1. Module lifecycle (5-slot table)

Every module — regardless of what it does — exposes exactly five functions, always, in a fixed order. No module may omit any of them. Unused functions return `0` (success) immediately and do nothing.

| Slot | Name        | Meaning |
|------|-------------|---------|
| 0    | `init`      | One-time setup. Allocate via the appropriate allocator module, register with the handle manager if needed, resolve dependencies on other modules. |
| 1    | `run`       | Scheduled, per-tick execution. Driven by the engine's main loop at the current fixed Hz. |
| 2    | `update`    | Reactive execution — called when external state has changed and the module needs to resync its internals. Not on a fixed schedule. |
| 3    | `delete`    | Teardown counterpart to `init`. Frees what it allocated, releases handles it owns. |
| 4    | `interrupt` | Emergency/priority override — stop normal flow and react immediately. Hardware fault lines, OS-level signals (e.g. window close), or software-level forced events (e.g. a multiplayer server forcing a client resync). |

**Rule:** the engine always calls all five slots on every module without checking capability flags. A module declaring "I don't support X" and then implementing it later without updating that declaration is considered a safety hazard (particularly for hardware-facing modules) — so there is no such declaration. The cost of calling a no-op is negligible compared to the cost (and risk) of checking a table to decide whether to call it.

## 1a. Implementation language and ABI

- The **engine core** (loop, module loader, handle manager, allocators) is implemented in **Rust**.
- Every **module**, regardless of what it does, must expose its 5-slot table as a plain **C ABI** — `extern "C"` linkage, no C++ name mangling, no language-specific calling convention. This is the only common ground every systems language can reliably agree on, and it's what makes the module loader able to treat any module identically regardless of implementation language.
- **Module authors may use any language they want internally** (C, C++, Rust, or otherwise), provided the exported boundary is C ABI compliant.
- The engine core's memory-safety guarantees apply only to the core's own code. They do not, and cannot, extend across the FFI boundary into a module's internals. **Module authors are responsible for the safety and correctness of their own code.** Modules are expected to be reviewed/audited before being trusted in a build — Red Plasma does not claim to guarantee module-level safety it has no way to enforce.

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

- All loop timing is stored as **integers** (e.g. nanoseconds/microseconds in a `uint64_t` on modern hardware, scaled down to smaller integer types on constrained hardware) — never floating point.
- The loop runs at a **fixed Hz**, but that Hz is **self-tuning**:
  - Each session, the actual time the loop body takes is measured.
  - That measurement is combined with the previous session's average using an exponential moving average with weight 0.5: `new_average = (old_average + new_measurement) / 2`, implemented as a bit shift (`>> 1`), never a true division.
  - The resulting average is **persisted across sessions** (written on shutdown, read on next launch).
  - Within a single session, the Hz is **held fixed** once read at startup — it does not change mid-session. This keeps physics/simulation deterministic for the duration of a run, while still letting the engine adapt to the hardware it's on, session over session.
- **First launch** (no prior average exists) uses a **developer-defined fixed Hz** as the starting point. Session-mined auto-calibration (using real playtest data to seed the first-ever value) is a planned future enhancement, not part of the initial implementation.
- **Division is not banned**, but is restricted to setup/calibration time (e.g. converting a target Hz into a fixed interval once, at startup). The per-frame hot path should only ever use add/subtract/compare operations.
- Recurring expensive calculations elsewhere in the loop or related math should prefer precomputed lookup tables over runtime calculation, consistent with the engine's general philosophy.

## 6. Events / subscriber system

- A generic publish/subscribe mechanism lives in the engine core. Any module can subscribe to events raised by another module (e.g. a window's `onResize`/`onConfigure` event) rather than each module inventing its own ad-hoc callback registration style.
- Used for, at minimum: window resize/configure notifications, and (planned) handle lifecycle notifications.

## 7. Module loading

- Modules are compiled as shared libraries (`.so` on the primary target, Fedora/Linux).
- Dependency declaration lives in the module manifest (§7a), validated at load time, before `init` is called.
- Mechanism for the lifecycle table itself is **not yet finalized** — see `DESIGN_DECISIONS.md` and `ROADMAP.md`. The intended direction is an assembler-style lookup table: a module exposes one fixed entry point that returns a table of function pointers (the 5-slot lifecycle table at minimum), avoiding repeated string-based symbol lookups after initial load.

## 7a. Module manifest

Before the engine touches a module's lifecycle table, it must validate the module via a manifest, exposed through a **known, fixed exported function** (e.g. `get_module_manifest()`) — never as a separate sidecar file. See `DESIGN_DECISIONS.md` for why a sidecar was rejected.

The manifest contains:

| Field | Purpose |
|---|---|
| **Magic number** | A fixed constant identifying the file as a genuine Red Plasma module at all, checked first, before any other validation is attempted. |
| **Contract version** | A single incrementing integer. The engine has one contract version it expects; the module declares the one it was built against. |
| **Contract-shape hash** *(optional, cheap insurance)* | A hash of the expected function table shape, computed at build time — catches accidental drift (e.g. a miscompiled table) even when the declared version number matches. |
| **Dependencies** | A list of other modules (by name/identifier, optionally with a minimum contract version) this module requires to be loaded first. Checked at load time, before `init` is called. |
| **Author/writer metadata** | Descriptive only. Supports the audit trail (see `PHILOSOPHY.md` §9) — does not gate loading. |

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
