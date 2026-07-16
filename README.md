# DBD Reboot

DBD reboot product line for a local-authoritative RTS/MMO sandbox runtime that
is being shaped toward a broader authoritative architecture.

`DBDReboot` is now the active source of truth for the product line.
The legacy Unity project in `My project` is archival reference only for:

- control-room terrain editing ideas
- map-plan concepts
- packaging/operator flow history

The current product direction is:

- `dbd_play_session` is the active local authoritative runtime
- `dbd_client` is the active direct-play native client
- `client/debug_view.html` is an auxiliary browser debug tool, not the main play surface
- `runtime-save/` lives at the repo root and is the shared runtime handoff folder
- runtime commands now flow through `runtime-save/command_spool/`
- Unreal-based C++ client remains a later path, not the current launch surface
- top-down RTS control with per-unit and group command
- 4-unit starting roster
- flexible units with unit skill growth instead of locked classes
- resource hauling, storage, structures, flattening labor, combat, insurance, lineage
- build anywhere in the world, with terrain affecting labor/time instead of hard legality
- persistent chunk-backed world where only resources, corpses, and dropped items reset in opened chunks
- chunks are backend storage/simulation partitions only, never player-facing build parcels or legal zones

## Layout

- `docs/` - architecture, migration, gameplay contracts
- `shared/` - domain and protocol contracts shared by client/server/tooling
- `server/` - native runtime/session code, including `dbd_play_session`
- `client/` - native direct-play client plus auxiliary browser debug tooling
- `assets/characters/` - transparent PNG character bases, clothing layers, equipment layers, and generated composites
- `tools/` - control-room and ops migration notes
- `runtime-save/` - repo-root runtime state, snapshots, status, command spool, and receipts/results

## Launch flow

Current local play flow uses the native runtime. Unity is not required.

Prerequisites:

- CMake 3.20 or newer
- A Windows C++ toolchain that CMake can find, such as Visual Studio Build Tools

Build from the repo root:

```powershell
cmake -S . -B build-native
cmake --build build-native --config Debug
```

Or use the helper:

```powershell
.\build-native.ps1
```

The helper also signs the local debug executables with a user-local
`DBDReboot Local Dev Code Signing` certificate and a public timestamp. On some
Windows Device Guard or Smart App Control policies, this is still not enough:
those policies can require enterprise/reputation-backed signing instead of a
fresh self-signed development certificate.

If you have a real CA, enterprise, or cloud-backed code-signing certificate, use:

```powershell
.\tools\sign-release.ps1 -CertificateThumbprint "YOUR_CERT_THUMBPRINT"
```

Or, if your organization uses a provider-specific trusted-signing wrapper:

```powershell
$env:DBD_TRUSTED_SIGNING_WRAPPER = "C:\path\to\your-trusted-signing-wrapper.ps1"
.\tools\sign-release.ps1 -UseTrustedSigning -TrustedSigningProfile "YOUR_PROFILE"
```

Run the local session and client in two terminals:

```powershell
.\run-play-session.ps1
.\run-client.ps1
```

To try the first Direct3D11 3D client instead of the stable 2D tactical client:

```powershell
.\run-play-session.ps1
.\run-client3d.ps1
```

Those helpers launch:

- `build-native\Debug\dbd_play_session.exe`
- `build-native\Debug\dbd_client.exe`
- `build-native\Debug\dbd_client3d.exe` when using the 3D client helper

Manual launch is fine too. Start `dbd_play_session` first so the
authoritative local runtime can create and update `runtime-save/`, then start
`dbd_client` or `dbd_client3d` to play through that shared folder. Open
`client/debug_view.html` only when you want inspection, regression checks, or
command-queue troubleshooting.

This is the present working product loop. It does not require Unity, and it is
separate from any future Unreal-facing client work.

`dbd_client3d` is the first native 3D visualization path. It uses Win32 +
Direct3D11, polls `world_snapshot.json` and `session_status.json`, renders the
same Depot Run world as simple 3D shapes with placeholder labels, and writes
the existing movement, combat, scouting, hauling, loot, flatten/build, repair,
and Depot Run objective commands through the command spool. It is intentionally
experimental; the 2D `dbd_client` remains the stable comparison surface.
The 3D client also surfaces the thin item loop: depot hover/selection panels
show stored wood/stone/iron and craft readiness, `Q` crafts a Storage Crate,
`Shift+Q` crafts a Field Shovel, `T`/`Shift+T` use shovel/hammer tools, and
`Shift+B` enters install mode for placing a crafted Storage Crate.

Before opening the client, you can run the live command-spool smoke harness:

```powershell
.\tools\run-live-smoke.ps1 -StartSession
```

That starts a temporary `dbd_play_session`, sends real movement, tactical,
economy, flattening, construction, combat, and loot commands through
`runtime-save\command_spool\inbox\`, waits for command results, checks snapshot
changes, then writes `runtime-save\live_smoke_report.json`. Use this when you
want to confirm the live session path works without manually clicking through
the client. If you already have a play session running, omit `-StartSession`.

## Character layer assets

Character art is prepared as transparent PNG layers rather than one fixed final
image. The first pipeline supports full-body inventory/dress-up composites and
separate top-down unit composites for the RTS battlefield.

Put source art under `assets/characters/`:

- `base/` for body bases such as `body_base_front.png`
- `clothes/` for shirts, pants, shoes, hair, and outerwear
- `equipment/` for held items, headgear, bags, weapons, and tools
- `topdown/` for top-down source layers
- `composites/` for generated dressed outputs

Layer recipes live in `assets/characters/manifests/`. To compose a dressed
character after adding matching PNG files:

```powershell
.\tools\compose-character.ps1 -ManifestPath assets\characters\manifests\fullbody_template.json
```

The default layer order is body, innerwear, pants, shirt, outerwear, shoes,
hair, headgear, held item, and back item. All layers should preferably share the
same transparent canvas size as the body base.

The current play-session seed is free-play oriented: the founder opens in a
safer starter pocket, while the richer mid-field resource line and promising
construction ground sit close enough to raider territory to become contested
without forcing an opening fight.

`dbd_play_session` now frames that seed as the first playable vertical slice:
`Depot Run v0`. This is not a forced quest chain; it is a 10-minute test match
overlay for judging whether the existing RTS sandbox parts create a readable
game. The objective is to secure the supply line by either storing 15 `Iron
Fitting` in owned storage or completing a forward storage depot before the timer
expires. Losing all founder units or losing the primary depot fails the slice.
`session_status.json` exposes the objective under `scenario`, including the
current Iron Fitting target node id and position, and the native client shows the
current objective state in the session panel. In the native client, the objective
Iron node is marked `OBJ`; right-clicking that node with units selected sends
them to harvest it. Units carrying Iron Fitting are tagged as objective cargo,
and the client shows a simple complete/failed banner when the Depot Run terminal
state is reached. The scenario status also exposes `reason` plus result-summary
fields such as units lost and primary depot health so the client can show why a
run ended and what condition the supply line survived in.
Depot Run v0 resolves terminal reasons in a fixed order: `iron_secured`,
`forward_depot_complete`, `primary_depot_lost`, `all_founder_units_lost`, then
`time_expired`. The live smoke harness force-checks the three failure reasons
with debug-only commands so success and failure both stay closed as the sandbox
rules change.

Resource nodes now expose that risk/reward ladder directly. Starter nodes are
low-risk and low-yield, mid-field nodes are visibly richer inside raider pressure
range, and frontier nodes carry the highest yield marker for later high-risk
expeditions. Resource node snapshots also expose the item they produce, and the
native client labels nodes with their risk band, product name, and approximate
yield multiplier so the player can choose whether to stay safe or push outward.

Build pad candidates are also surfaced as tactical choices. The play session
seeds contested flatten pads near the mid-field line, and the native client
labels each pad with progress, assigned workers, ready state, and nearby exposure
so a player can decide whether to finish the pad, guard it, or abandon it.
Raider pressure now reacts to active mid-field flatten labor too: once founder
units are assigned to a contested pad, raiders can pressure those workers before
the automatic construction stage even begins.

Combat currently uses tick-based sustained damage. Units can attack units,
construction sites, and structures; guard/hold orders respond to nearby threats
without turning into a full pursuit AI. Combat bands are fixed unit tendencies:
`Short` has the shortest reach and highest damage, `Mid` is the stable baseline,
and `Long` reaches farthest with lower damage. The client surfaces combat through
HP bars, band labels, tactical order labels, and the session `combatSummary`
plus sharper `combatAlert` field. The client also summarizes selected band mix
and retreat readiness so the player can read “pin with short range, cover with
long range, or pull out now” without adding a deeper weapon system yet.

Scouting/contact information is now part of the server loop. Each living unit
refreshes its owner's `knownContacts` from survival-based vision range. Visible
contacts stay marked as `visible`; contacts that leave sight remain as `stale`
last-seen positions for a short time. `focus_fire` and `intercept_unit` require a
currently visible or recent contact. Old contacts do not grant perfect attacks:
the order turns into movement toward the last seen position instead. Full
fog-of-war and stealth are still intentionally out of scope for this slice.
Players can now explicitly send units forward with `scout_area` and
`patrol_route`; both are movement/scouting orders that feed the same contact
cache instead of creating a separate sensor system. `investigate_contact` lets
selected units move to a visible or stale contact's last-seen position, keeping
the target id as tactical context without granting perfect tracking.

RTS movement has a first formation/queue pass. `formation_move` spreads selected
units around a destination with simple offsets so multi-unit orders do not stack
on one point. `queue_move`, `queue_scout_area`, and `queue_patrol_route` add a
short per-unit order queue capped at two entries. When a normal movement order
finishes, the next queued order starts automatically. Direct tactical/survival
orders such as `stop`, `retreat_selected`, `focus_fire`, `guard_unit`, and
`guard_site` clear queued orders because they are treated as higher-priority
battlefield decisions. `clear_queue` removes queued orders while preserving the
current active order, which is exposed in the client as `Shift+X`.

Queued orders now react to contact through the existing automation rules instead
of blindly continuing or always auto-fighting. A gather/scout style unit can
drop its queue and retreat, an escort can break queue to engage nearby contact,
and a hold unit anchors in place instead of chasing across the map. Snapshots
expose `tacticalState` and `queueInterruptReason` so the client can explain
whether a queue was started, interrupted by enemy contact, held, or overridden by
low-health retreat.

Logistics now has a first repeatable supply route command. `haul_route` lets
selected haulers cycle between a resource node or dropped cargo pile and an
owned storage site. The route remains visible through `routeActive`,
`routeSourceKind`, `routeSourceId`, `routeStorageId`, and `routePhase`; direct
tactical commands such as stop, retreat, focus fire, and guard still interrupt
the route when the player needs to react.

Supply routes now report battlefield risk instead of behaving like invisible
background jobs. Route haulers expose `routeThreatLevel` (`safe`,
`threatened`, or `critical`) plus `routeThreatEnemyId` when nearby enemies,
contested sources, exposed storage, or overloaded cargo make the route dangerous.
`session_status.json` adds `supplyAlert`, and the native client calls out
threatened haulers and escorts so a player can decide whether to keep hauling,
retreat, intercept, or guard the line.

Threatened haulers now have two direct survival responses. `emergency_deposit`
breaks the active route and sends cargo carriers to the nearest owned depot,
while `drop_cargo` turns carried cargo into a recoverable dropped pile at the
unit's current position. These are player-issued tactical choices: dropping
cargo can save a slow hauler, but the pile can be looted or hauled again by
either side.

Items now have a thin shared definition table for naming and classifying cargo.
This first pass only defines construction/material/tool metadata such as
display name, category, unit weight, base value, stackability, and max stack.
It does not add equipment slots, durability, crafting recipes, or gameplay
effects yet. Existing `CargoStack` save/load, haul, loot, drop, and storage
flows remain unchanged, and snapshots expose the default item definitions so
tools and clients can start resolving item ids into readable names. Harvested
resource nodes now produce registered construction material ids instead of raw
node ids: low-risk nodes produce `Basic Wood`, mid-field nodes produce `Stone
Block`, and high-risk nodes produce `Iron Fitting`.
Dropped cargo snapshots also expose a representative `primaryItemName`, so the
client can label loot piles without adding a full inventory UI yet. Unit cargo
and storage sites expose the same representative item name so haulers and depot
stock can be read at a glance.

Supply threats also have fast response orders. `guard_threatened_route` attaches
selected responders to the most dangerous friendly route hauler, while
`intercept_route_threat` sends selected units toward that route's current threat
enemy. Both commands use the existing guard/intercept rules; they are direct RTS
shortcuts, not background AI.

Repair is a direct battlefield recovery order. `repair_structure` and
`repair_construction_site` send selected units to restore health on damaged
friendly structures or construction sites. Repair does not increase construction
progress; it only restores health, consumes owner storage resources, and stalls
with `Repair stalled: no materials` when stored materials run out. In the native
client this is exposed as `Z Repair`. `supply_repair` is the deliberately thin
field resupply layer: it stages stored resources as a nearby dropped repair pile
so stalled repairs can resume. It is exposed as `Shift+Z Supply Repair` and does
not create an automatic supply route or a separate repair economy.

If Windows blocks a built executable with an application control policy message,
run `diagnose-run-policy.ps1`. If it reports `signature=Valid` but Code Integrity
says `Enterprise signing level requirements`, the code is built and signed, but
the machine policy requires a stronger trusted signer than the local development
certificate.

Free local unblock path:

```powershell
.\open-smart-app-control-settings.ps1
```

That helper opens Windows Security and explains the tradeoff. It does not
silently change policy. The free fix for this development laptop is to turn
Smart App Control off, but Microsoft documents that Smart App Control may not be
turnable back on without resetting or reinstalling Windows. Treat that as a
deliberate development-machine choice, not a shipping/signing solution.

## Release signing

`build-native.ps1` is the developer convenience path. It builds Debug output and
signs with a local self-signed certificate so the repo has a repeatable local
workflow.

`tools\sign-release.ps1` is the release/trusted-signing path. It supports:

- a CA or enterprise code-signing certificate installed in `CurrentUser\My` or `LocalMachine\My`
- a provider wrapper exposed through `DBD_TRUSTED_SIGNING_WRAPPER`

Minimum release-signing inputs:

- certificate thumbprint or subject, or `-UseTrustedSigning`
- timestamp server
- target configuration (`Debug` or `Release`)

Minimum operator prerequisites:

- a public CA, enterprise CA, or trusted-signing account that the target WDAC policy trusts
- the signing certificate installed in the Windows certificate store, or a provider wrapper script/exe
- Windows SDK `signtool.exe`

Verification flow:

```powershell
Get-AuthenticodeSignature .\build-native\Debug\dbd_play_session.exe,
  .\build-native\Debug\dbd_client.exe,
  .\build-native\Debug\dbd_server.exe | Format-List

.\diagnose-run-policy.ps1
```

If `Get-AuthenticodeSignature` returns `Valid` but `diagnose-run-policy.ps1` still
shows `Enterprise signing level requirements`, the signer chain still does not
match what the machine policy trusts.

## Runtime handoff

`runtime-save/` is the local handoff folder between the authoritative session,
the direct-play client, and debug tooling.

- `world_snapshot.json` - current world export polled by `dbd_client`, including unit state and `knownContacts`
- `session_status.json` - tick, mode, last command status, `combatSummary`, optional `combatAlert`, `contactSummary`, and optional `contactAlert`
- `session_manifest.json` - player metadata for the current session
- `regression_report.json` - debug/regression report placeholder for tooling
- `live_smoke_report.json` - live play-session command-spool smoke result
- `chunk_*.txt` and `world_meta.txt` - persisted chunk-backed world data
- `autosave/` - latest autosaved persisted world files
- `command_spool/` - file-backed command queue

`command_spool/` has five subfolders:

- `inbox/` - clients write new `.json` command files here
- `processing/` - `dbd_play_session` claims commands here while applying them
- `receipts/` - command claim/completion status
- `results/` - final command result payloads
- `archive/` - processed command payloads

The native client writes commands such as `move`, `attack`, `focus_fire`,
`intercept_unit`, `retreat_selected`, `guard_unit`, `guard_site`,
`hold_position`, `stop`, `harvest`, `loot`, `haul_route`, `return_to_storage`,
`formation_move`, `queue_move`, `clear_queue`, `scout_area`, `queue_scout_area`,
`patrol_route`, `queue_patrol_route`, `investigate_contact`, `start_flatten`, and `start_construction` into
`runtime-save\command_spool\inbox\`. The play session claims each file,
applies it to the authoritative world, writes a receipt/result, then archives
the command file.

## Current status

This reboot line is a scaffold, not a finished game. It defines the first
runtime/data model and protocol surface so implementation can continue from the
reboot codebase instead of inheriting Unity runtime assumptions.

Current scaffold focus:

- continuous terrain evaluation instead of patch-gated placement
- all flattening and construction resolves through accumulated labor over time, and multiple units speed it up
- workers can be added to or pulled from live jobs without resetting progress for everyone else
- under-construction sites are vulnerable world objects that can be contested and destroyed
- permanent unit death with partial cargo drop persistence
- unit skill growth for strength, harvesting, hauling, construction, combat, survival
- unit/squad authority can be delegated for shared wars
- chunk-backed file saves preserve flattened ground, sites, structures, storage, units, authority, and insurance
