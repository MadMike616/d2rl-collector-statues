# Adding collector statues

The only plugin registration list is `CollectorStatues::Definitions` in [src/statue_definitions.h](src/statue_definitions.h). Opening, native Unidentified-label suppression, compiled item-class lookup, repeat-unique validation, natural drops and the temporary test command all read this list. Array sizes and the eligible-drop list follow its length. There is no separate twenty-item limit or two-award limit.

The catalog is compiled into the DLL. Changes require a Release rebuild; there is no editable runtime catalog or new configuration file.

## Register an item

Append one entry with a new, permanent, nonzero ID, an unused three- or four-character item code and one drop rule. These examples are illustrative and are not installed items:

```cpp
{21,D2RL::Items::MakeItemCode("s21"),Act(0)},
{42,D2RL::Items::MakeItemCode("g021"),Class(4,2000000)},
{43,D2RL::Items::MakeItemCode("g022"),Descent(11,20,10000,1000)},
{44,D2RL::Items::MakeItemCode("g023"),Cow()},
{45,D2RL::Items::MakeItemCode("g024"),{}}, // Opening only; no natural drop.
```

IDs are independent of item codes and array positions. They need not be consecutive. Do not change or reuse the existing twenty IDs/codes. Keep existing entries in order and append new ones, so the existing random-roll order is preserved when possible.

| Rule | Matching kills | Default chance |
|---|---|---|
| `Descent(firstFloor, lastFloor, normal, checkpoint)` | Inclusive floor range, using the existing dungeon and checkpoint recognition | 1/10,000 ordinary; 1/1,000 checkpoint |
| `Act(zeroBasedAct, denominator)` | That campaign act in Hell, excluding Cow Level and all Descent areas | 1/1,000,000 |
| `Cow(denominator)` | Cow Level monsters in Hell | 1/1,000,000 |
| `Class(zeroBasedClass, denominator)` | Hell kills credited to that class, including its mercenaries/summons | 1/1,000,000 |
| `{}` | Registered collectible with opening/tooltip handling, but no automatic drop | None |

Act indexes are 0–4. Class indexes are Amazon 0, Sorceress 1, Necromancer 2, Paladin 3, Barbarian 4, Druid 5, Assassin 6 and Warlock 7. Supported dungeon floors remain 1–60. These are validated game/dungeon limits, not statue-count limits. New game classes, dungeon structures or entirely new drop-source kinds require a separate compatibility/eligibility implementation.

Each matching statue receives its own independent roll. Multiple entries may share an act, class or floor range; this does not split or dilute the individual probabilities. Zero checkpoint denominator means use that entry's ordinary denominator on checkpoint kills too. Zero ordinary denominators, duplicate IDs/codes, malformed codes and invalid selectors fail the compile-time validation.

## Add the mod data and artwork

1. Append the base row to `misc.txt` and its matching Unique-quality row to `uniqueitems.txt`. Map columns by exact header names, preserve existing rows/order and use `*Creator=MadMike` where that column already exists. Check the code against every active item table, not just this plugin's list.
2. Follow the existing statue rows: one inventory column by two rows, no bonuses, native item use disabled, shared Intricate Box base name/artwork and `SkipName=1`. Give the unique row its own localization key and identified artwork; set `nolimit=1` so repeat copies remain unique. Keep the existing statue item type and ground-art conventions.
3. Add the display name in English and every other supported localization language. The plugin's shared localized `GrailerRLCollectorStatueOpen` instruction does not need another copy per statue.
4. Add the loose HD `.sprite` and `.lowend.sprite` artwork, classic `.dc6` fallback and required item-art mappings/ground definitions. Reuse the shared unopened-box sprites. Images remain mod assets and are not embedded in the DLL.

Every registered code must resolve in a supported compiled item bank and have exactly one repeat-enabled unique row. Incomplete or duplicated definitions disable drops rather than generating a wrong item. Incomplete per-bank class lookup also retains the normal Unidentified tooltip label. An opening-only entry still needs valid base/unique rows and artwork.

## Verify and install

Build both the Release configuration outside the source tree using the existing SDK/toolchain. Run opening/feedback, drop-policy/native-placement, MASM-bridge, catalog-extension and native-audio tests. The catalog tests use an expanded temporary list with sparse IDs, arbitrary three/four-character codes and more than two simultaneous awards; these are not game items.

Install in a separate development mod first. Use the loader's normal unidentified-unique spawning tools to check name/artwork, first-click opening, normal left-click movement, repeat copies and save/rejoin persistence. For predictable drop testing, use a temporary private catalog with denominator 1, rebuild, verify area/difficulty/class exclusions, then restore the intended probabilities before building the installed DLL. Opening-only registrations are tested through manual spawning.

Build and distribute only Release DLLs. No custom drop-test command or development traces are included in the plugin.
