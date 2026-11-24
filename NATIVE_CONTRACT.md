# Native contracts — Collector Statues 1.0.0

The plugin uses SDK-owned mutation transactions, lifecycle events and authoritative/UI work queues. All native profile bytes were independently extracted from a loader-hosted runtime capture for the audited build. No third-party implementation or witness arrays are copied. Native checks fail closed without a game-version gate.

## Opening and input

The process-owned game window's UI thread has one WH_GETMESSAGE hook. A plain right-button click is owned only for an unopened registered unique in supported storage, with the same foreground root and a tooltip within four screen pixels / 400 ms. Visible hovers continue refreshing while input is transiently busy. A retained click waits through later SDK UI updates for at most 1,500 ms / 256 attempts; it never sleeps or spins on the UI thread. Pointer/item/cursor/context changes, timeout and game exit cancel it. Monotonic request serials prevent stale callbacks from consuming newer work.

Native read-only UI helpers at RVA 0xCE500 and RVA 0x8460D0 distinguish context admission from transient readiness. Legacy mode 0x0C is not treated as a shopping indicator because it may remain set in normal inventory. Configuration and player trade are excluded; ownership/storage checks exclude vendor-owned items. Generic mouse Activate events are not consumed because they do not expose a physical button. Controller activation uses the SDK event.

Authoritative work obtains the local server player or resolves the requesting network peer, enumerates owned storage and requires exactly one matching code/class/page/cell. UI handles, runtime IDs and seeds are not compared to server-domain copies. Only identification is edited through ItemService. The 36-byte protocol payload contains identity plus an opaque sound ticket split into two u32 halves. Compatibility token 0x5354415455450003 requires matching peers; message 2 acknowledges successful identification with an eight-byte ticket.

## Tooltip adapters

Two exact five-byte CALL sites redirect only the Unidentified lookup: RVA 0x2BE178 and RVA 0x2BEDFC, with continuations at RVA 0x2BE17D and RVA 0x2BEE01. Captured register liveness places the item in RBX for builder A and RSI for builder B. The MASM adapters preserve native pre-CALL stack/shadow-space assumptions and forward to the original string target at RVA 0x3E2B730 for every other item/string.

Bounded reads validate native unit type/class at offsets 0/4 and data bank at 0x1BD against complete getter witnesses. SDK lookups supply each registered code's compiled class in banks 1–3. Incomplete bank resolution preserves normal labels. Only localization string ID 3455 for a known registered class is replaced with empty text.

## Drop observer and placement

The death-dispatch CALL at RVA 0x447E17 targets RVA 0x441300. Its adapter preserves flags, volatile GPRs, XMM0–5 and native stack arguments, then pushes the original return RVA 0x447E1C before entering the native target. This preserves caller-sensitive downstream behavior. Other treasure calls do not pass this observation site.

Validated native unit, owner, room/path, active-game, bank and difficulty layouts identify offline eligible kills. Compiled MonStats stride 0x1FC and Levels stride 0x18C are checked. Boss names are matched by identifiers rather than assumed appended class IDs. Catalog arrays, code recognition and eligibility derive from `src/statue_definitions.h`; IDs are resolved to catalog positions rather than subtracted from array indexes.

A later authoritative callback resolves fresh server units/rooms. It rejects a changed session or level and validates positive coordinates against the selected room. A vanished corpse uses the current matching player room, or current player coordinates in that room. No room/monster pointer is retained across the queue.

Byte-guarded helpers create an unplaced Unique by numeric class ID at RVA 0x43D530, clear only identified flag 0x10 through RVA 0x36D8F0, then publish it with placement at RVA 0x43DD00. Room containment at RVA 0x2F0680 and item-data/quality/static-path witnesses corroborate layout. Quality, identification, room and coordinates are verified after publication. Unexpected native state disables further drops until restart. Each registered code requires exactly one repeat-enabled source unique row. The plugin leaves normal treasure and the game's RNG untouched.

## Opening sound

Optional audio calls native playback at RVA 0x1A0780 (complete 1,143-byte logical body across chained unwind ranges) and sound-row lookup at RVA 0xD96A0 (179 bytes). `src/sound_profile.h` contains the independent complete witnesses. The captured native item-transfer call at RVA 0x15F9F1–0x15FA0D corroborates the ABI: ECX sound ID, null RDX unit, zero R8D/R9D and a zero fifth stack argument. Playback returns an opaque u64 voice handle whose lifetime remains with the game.

Lookup `(13, nullptr, 1)` obtains the raw row. Its 32-byte name at +2 must be NUL-terminated and equal `cursor_identify_item`; normal playback subsequently follows native volume/mute and HD-redirection rules. Unknown code, wrong data or native audio exceptions disable only the optional cue.

Only successful authoritative identification issues feedback. A bounded ticket buffer and monotonic tickets reject duplicate/replayed requests. GameLeft/unload clear queued sounds, and UI playback requires active gameplay and a current local player. Remote success targets the opening client. Audio adds helper calls but no native hook or asset dependency.

## Validation scope

Five automated suites exercise the actual callbacks with SDK/native doubles, the production MASM bridge and synthetic data. Unit-test observations compile out of the installed DLL. Live validation confirms the initial opening/drop/persistence scenarios documented in the README; controller, shared-stash and TCP/IP combinations require further live coverage. No compatibility guarantee is inferred for unvalidated native layouts or other plugins.
