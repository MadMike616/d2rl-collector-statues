# Hook Inventory — Collector Statues

## Plugin metadata

| Field | Value |
|---|---|
| Plugin name | Collector Statues |
| Plugin ID | collector-statues |
| Release version | 1.0.0 (source upload; no GitHub Release) |
| Inventory updated | 2026-11-24 |
| Source revision | 1.0.0 source tree |
| D2R build(s) audited | 3.4.0 build 95032; native byte/layout admission required |
| D2RLoader SDK / Plugin ABI | SDK 0.4.0 / Plugin ABI 4 |
| Audit status | Complete |

## Scope and method

Inspected all source in `src/`, `CMakeLists.txt` and `tests/`. The initial source upload retains the four install sites and assembly adapters validated during development; there are no hook-site changes. Expected bytes, register liveness, field layouts and native helper contracts were independently audited from a loader-hosted capture. Ordinary SDK lifecycle, UI/game queue, table, network and tooltip callbacks are excluded unless they install a target. Native helper calls, including audio playback, are not hook sites. Development commands, hover traces and session dumps have been removed from the plugin; unit-test observations compile out.

## Hook sites

| Site ID | Type | Target | Address / identifier | Operation | Guard / validation | Source reference | Notes |
|---|---|---|---|---|---|---|---|
| H-01 | Win32 input-message | Game-window UI-thread message queue | N/A | WH_GETMESSAGE owns qualified statue right-button down/up | Same foreground root, unmodified input, 400 ms / four-pixel tooltip, supported storage, guarded context/readiness and UI item/cursor revalidation | `src/plugin.cpp` — `InstallMouse`, `MouseMessage`, `MouseOpen`, `AdvanceMouseRequest` | Busy input waits at most 1.5 s / 256 later SDK UI updates; serials reject stale work. State clears on GameLeft, hook is removed on unload. |
| H-02 | CALL | Unidentified lookup in native tooltip builder A | RVA 0x2BE178 | Redirects string lookup through item-aware MASM adapter | Exact CALL, surrounding branch/register-liveness and native type/class/bank witnesses; complete per-bank SDK class catalog | `src/native_tooltip.cpp` — `Install`, `CollectorStatuesLookup`; `src/tooltip_thunks.asm` — `CollectorStatuesTooltipA` | Original target retained for other items/strings; continues at RVA 0x2BE17D. |
| H-03 | CALL | Unidentified lookup in native tooltip builder B | RVA 0x2BEDFC | Redirects string lookup through item-aware MASM adapter | Exact CALL, surrounding branch/register-liveness and native type/class/bank witnesses; complete per-bank SDK class catalog | `src/native_tooltip.cpp` — `Install`, `CollectorStatuesLookup`; `src/tooltip_thunks.asm` — `CollectorStatuesTooltipB` | Original target retained for other items/strings; continues at RVA 0x2BEE01. |
| H-04 | CALL | Monster-death treasure dispatch | RVA 0x447E17 | Observes one death and reproduces native dispatch to RVA 0x441300 | Exact CALL window, offline game, unit/bank/difficulty, ownership, room/path, server lookup, active-game and compiled/source catalog checks | `src/statue_drops.cpp` — `Install`, `CollectorStatuesObserveDeath`; `src/drop_thunk.asm` — `CollectorStatuesDeath` | Preserves flags, volatile GPRs, XMM0–5 and stack arguments; original return RVA 0x447E1C is preserved. |

## Known collision overlaps

None identified in the current inventory; this is not a compatibility guarantee.

| Other plugin | Shared target | Relationship | Evidence / notes |
|---|---|---|---|

## Audit notes

H-04 is upstream of the native entry targets used by Endgame Descent at RVA 0x441300 and Random Treasure Drop at RVA 0x4404F0. These are distinct install sites; the original return address is deliberately retained for caller-sensitive code. The Windows hook shares a message channel with input plugins and forwards unowned messages.

Read-only UI helpers at RVA 0xCE500 / 0x8460D0, class/bank getters and creation/placement helpers are guarded but install no target. Optional sound calls playback at RVA 0x1A0780 and row lookup at RVA 0xD96A0, with full byte witnesses and runtime sound-name validation. Modified or unknown audio code disables only the cue. All item mutations run in authoritative SDK callbacks; queued UI work and success feedback are session-bound.

Unvalidated builds, controller/shared-stash/TCP-IP combinations and binary-only external plugins remain compatibility limitations. See `NATIVE_CONTRACT.md` for native layouts, ABI and validation scope.
