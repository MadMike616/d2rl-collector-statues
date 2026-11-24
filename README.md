# Collector Statues

A D2RLoader plugin by **MadMike** for collectible statue items. Version **1.0.0**. MIT licensed.

Unopened statues share an Intricate Box appearance and a localized Right-Click to Open instruction. Opening reveals the statue's unique name and artwork and plays the game's short identification sound. Statues remain statless, occupy one inventory column by two rows, and keep their identified state between games.

## Features

- Right-click opening with authoritative item ownership and identity checks. Left-click keeps normal selection and movement.
- Retains a valid first click briefly while inventory input is busy, cancelling if the item, pointer, cursor, context or game changes.
- Removes the red Unidentified line only for registered statues; unrelated items retain their normal tooltips.
- Independent extra drops, unaffected by Magic Find, treasure picks or player count.
- One central source list for future statues, including additional entries sharing an act, class or Descent section.
- No configuration file or plugin console commands.

## Drop rules

| Source | Eligible statue | Chance per eligible monster |
|---|---|---|
| Descent floors 1–10 / 11–20 / 21–30 / 31–40 / 41–50 / 51–60 | Catacombs / Jail / Crypt / Caves / Ice Caves / Lava | 0.01% (1 in 10,000) |
| Actual checkpoint bosses on floors 10 / 20 / 30 / 40 / 50 / 60 | Corresponding Descent statue | 0.1% (1 in 1,000), replacing the ordinary floor roll |
| Hell campaign acts I–V | Corresponding act statue | 0.0001% (1 in 1,000,000) |
| Hell Cow Level | Cow statue | 0.0001% (1 in 1,000,000) |
| Hell monsters credited to a player, mercenary or summon | That player's class statue | 0.0001% (1 in 1,000,000) |

Cow Level and all Descent areas, including the entrance, exclude act statues. The entrance has no Descent-floor roll. Class rolls remain eligible there in Hell. Ordinary monsters and escorts on checkpoint floors retain the ordinary floor rate. Each eligible statue receives its own independent roll; more than one box may drop from a kill, and repeat copies are allowed.

Natural drops support local offline games. Opening uses the SDK's authoritative game callback; TCP/IP clients use host requests and success acknowledgements. Controller, shared-stash and TCP/IP behavior depend on available SDK capabilities and have not all received live validation.

## Requirements and installation

- Windows x64 and D2RLoader with Plugin ABI 4 and the required item, inventory, lifecycle, UI-thread, table and mutation services.
- Built with the official PluginSDK 0.4.0. The current native profile was audited for D2R 3.4.0 build 95032 with D2RLoader 1.3.1-beta. Native bytes and structure layouts are validated before use; differing layouts disable the affected feature rather than relying on a game-version gate.
- A compatible mod supplying the registered item bases, unique rows, localization, artwork, loose level table and Descent checkpoint definitions. The DLL alone is not a complete item pack.

Build the DLL in **Release** and install it at `mods/<mod>/d2rloader/plugins/d2rl-collector-statues.dll`. Restart the loader after replacing it. No TOML template or settings changes are required. Client and host must use the same plugin/network protocol version.

The initial catalog contains item codes `s01`–`s20`. Each requires one unique row with `nolimit=1`. Base item rows use `SkipName=1`, disabled native item use, shared box artwork and 1×2 dimensions; identified artwork comes from the unique row. Item names, the shared `GrailerRLCollectorStatue` base label and `GrailerRLCollectorStatueOpen` instruction must be localized by the mod. These key names are retained as the existing data interface.

The Descent integration reads named columns from the active mod's `levels.txt`: `GrailerDescentFloor1`–`GrailerDescentFloor60`, `GrailerDescentEntrance`, and the standard Cow Level definition. Its compiled checkpoint catalog must contain all supported boss variants. Missing or inconsistent item/dungeon data disables extra drops. The plugin does not rewrite game tables or install artwork. When extracting base game files to prepare compatible mod data, use **Ladiks Casc Viewer**.

Audio uses the game's existing identification effect and mixer. Rejected, cancelled or duplicate requests do not issue the opening cue. Unknown native audio code or a mismatched sound row disables the optional cue while preserving opening.

## Adding statues

Append a definition to [src/statue_definitions.h](src/statue_definitions.h), add its mod data/localization/artwork, then rebuild. IDs are stable and independent of array positions; three- and four-character item codes are supported. See [EXTENDING_STATUES.md](EXTENDING_STATUES.md).

## Build

Use CMake 3.29+, MSVC x64, C++20, MASM and the exact official SDK 0.4.0 package. Keep generated output outside the source tree:

```powershell
cmake -S . -B ../build-collector-statues -DD2RLPlugin_DIR="<PluginSDK>/cmake"
cmake --build ../build-collector-statues --config Release
```

The build uses the static MSVC runtime. A direct Developer Command Prompt build is also supported: compile the four C++ files in `src/` with `/std:c++20 /O2 /DNDEBUG /MT`, assemble both `.asm` files, compile `plugin.rc`, and link a DLL with `user32.lib` and `bcrypt.lib`. Supply the SDK include path and keep object/resource/link output in a separate build directory.

## Validation

Python 3 is needed only for synthetic test-fixture generation:

```powershell
cmake -S . -B ../build-collector-statues-tests -DD2RLPlugin_DIR="<PluginSDK>/cmake" -DCOLLECTOR_STATUES_BUILD_TESTS=ON
cmake --build ../build-collector-statues-tests --config Release
ctest --test-dir ../build-collector-statues-tests -C Release --output-on-failure
```

Five suites cover opening and feedback, ownership/session transitions, busy-input retry/cancellation, drop eligibility and native placement, the actual MASM adapter, catalog expansion beyond twenty entries, and native audio validation/fault isolation. Fixtures are generated from minimal synthetic tables; no extracted game files are distributed. Tests never launch the game.

The initial catalog has live confirmation for right-click opening, repeat finds, saved identification, Cow/Descent exclusions, matching floor statues and checkpoint classification. Native feature guards and automated tests do not establish compatibility with every loader or plugin combination. See [HOOK_INVENTORY.md](HOOK_INVENTORY.md) and [NATIVE_CONTRACT.md](NATIVE_CONTRACT.md) for the native integration audit.
