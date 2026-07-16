# Client Runtime Notes

`client/` now has two jobs:

- `src/native_client.cpp` builds `dbd_client`, the main direct-play client
- `src/d3d_client.cpp` builds `dbd_client3d`, the first Direct3D11 3D client
- `debug_view.html` is an auxiliary browser tool for inspection and regression checking

All client surfaces read from the repo-root `runtime-save/` folder. The 2D
`dbd_client` remains the stable play entry point, while `dbd_client3d` is the
new 3D shape-based experimental play surface.

## Direct-play client (`dbd_client`)

`dbd_client` is intentionally lightweight and engine-free:

- Win32 window + GDI rendering
- polls `../runtime-save/world_snapshot.json`
- polls `../runtime-save/session_status.json`
- writes command files to `../runtime-save/command_spool/inbox/`
- gives us direct-play selection and orders without Unity or Unreal

Expected launch pairing:

1. Build the native targets from the repo root.
2. Run `dbd_play_session` first so the local authoritative runtime is alive.
3. Run `dbd_client` to connect through the shared files in repo-root `runtime-save/`.

Repo-root commands:

```powershell
.\build-native.ps1
.\tools\run-live-smoke.ps1 -StartSession
.\run-play-session.ps1
.\run-client.ps1
.\run-client3d.ps1
```

Equivalent manual commands:

```powershell
cmake -S . -B build-native
cmake --build build-native --config Debug
.\build-native\Debug\dbd_play_session.exe
.\build-native\Debug\dbd_client.exe
.\build-native\Debug\dbd_client3d.exe
```

Keep `dbd_play_session` running while the client is open. The session updates
`runtime-save\world_snapshot.json` and `runtime-save\session_status.json`; the
client polls both files and writes orders into
`runtime-save\command_spool\inbox\`.

## 3D client (`dbd_client3d`)

`dbd_client3d` is the first native 3D visualization pass:

- Win32 + Direct3D11
- fixed quarter-view camera
- WASD camera pan and mouse wheel zoom
- polls `world_snapshot.json` and `session_status.json`
- renders units, resources, storage, construction sites, structures, and dropped cargo as simple 3D shapes
- left click ray-picks a friendly unit or targets the current command mode
- Shift/Ctrl + left click toggles additional selected units while in move/select mode
- right click ground sends `formation_move`; Shift+right click sends `queue_move`
- right click the `OBJ` Iron node sends `harvest`
- placeholder world labels show `person`, `raider`, `wood`, `stone`, `iron`, `depot`, `site`, `flatten`, `cargo`, and `contact`
- hotkeys cover the first 3D pass for attack, intercept, harvest, loot, haul route, guard, scout, patrol, flatten, build, repair, supply repair, retreat, return, stop, clear queue, emergency deposit, drop cargo, and route-threat response
- HUD shows Depot Run progress, reason, selected unit state, alerts, command mode, queued count, route threat, and Iron cargo state

It is still the experimental 3D surface, but it now exercises the same existing
server command spool as the stable 2D client. Use it to verify that Depot Run can
be read and played in 3D before adding real character or item art.

`tools\run-live-smoke.ps1` is optional, but useful before direct play. It starts
or attaches to a play session, sends real movement, retreat, focus fire,
intercept, guard, hold, stop, harvest, storage, flattening, construction,
attack, formation movement, queued movement, and optional loot commands through the same command spool, then records
`runtime-save\live_smoke_report.json`.

Resource nodes are marked by risk/reward: green `L` starter nodes are safer and
lower yield, yellow `M` mid-field nodes are richer and contested, and red `H`
frontier nodes are the highest yield marker. The side panel also summarizes the
best visible node so the player can decide when to leave the starter pocket.
In `Depot Run v0`, the objective Iron node is additionally marked `OBJ`.
Right-clicking that objective node with units selected issues harvest orders,
units carrying Iron Fitting are tagged as objective cargo, and the client shows
a simple Depot Run complete/failed banner when the scenario resolves. The banner
and session panel include Iron secured, units lost, depot HP, time remaining,
forward depot state, and the explicit scenario reason so a run has a readable
outcome instead of just ending.
Depot Run terminal reasons currently resolve as `iron_secured`,
`forward_depot_complete`, `primary_depot_lost`, `all_founder_units_lost`, then
`time_expired`, so a completed objective is not overwritten by a same-tick
failure in this first vertical slice.

Combat readability is intentionally simple. The side panel summarizes selected
`Short / Mid / Long` composition, suggests the current band role, and shows
whether the selected group can retreat cleanly or is overloaded/low on stamina.

Flatten/build pad candidates are marked directly on the board as `PAD` labels.
They show progress, assigned unit count, ready state, and exposure in the side
panel so construction location becomes a visible tactical choice without adding
a separate editor.

Current controls:

- Left click: select own unit
- Ctrl + left click: add to selection
- Left drag: box select
- `Ctrl+1` / `Ctrl+2` / `Ctrl+3` / `Ctrl+4`: save current selection as a control group
- `1` / `2` / `3` / `4`: recall a control group
- double-tap `1` / `2` / `3` / `4`: jump the camera to that group's current center
- `Shift+1` / `Shift+2` / `Shift+3` / `Shift+4`: add a control group to the current selection
- Right click: formation move selected units so they spread around the target instead of stacking; right-click the `OBJ` Iron node to harvest it
- Right click a `wood`, `stone`, or `iron` resource while units are selected: issue a harvest order
- Shift + right click: queue a move after the current order
- `Tab` or the `Inventory` button: show/hide the selected inventory/cargo panel
- `M`: move/select mode
- `1`: focus-fire mode, then left click target (`A` pans the camera left)
- `I`: intercept mode, then left click a hostile moving unit
- `O`: scout-area mode, then left click a forward point
- `Shift+O`: queue scout-area after the current order
- `P`: patrol-route mode, then click point A and point B
- `Shift+P`: queue patrol-route after the current order
- `N`: cycle the selected contact (`Shift+N` cycles backward)
- `J`: send selected units to investigate the selected visible/stale contact
- `C`: harvest mode, then left click resource node to issue one-shot harvest orders for all selected units
- `H`: haul-route mode; click a resource node or dropped cargo, then click a depot or ground to use nearest depot
- `L`: loot mode, then left click dropped cargo to load it onto selected units
- `Y`: emergency deposit selected cargo carriers to the nearest owned depot
- `U`: drop selected units' carried cargo at their current position
- `K`: guard the most threatened friendly supply route with selected units
- `Shift+K`: intercept the enemy threatening the most threatened friendly supply route
- `G`: guard mode; click friendly unit to escort, click construction/structure to guard, or click ground to hold
- `Z`: repair mode; click damaged friendly structure or construction site to restore health
- `Shift+Z`: supply repair mode; click damaged friendly structure or construction site to stage repair materials nearby
- `F`: flatten mode, then left click world position
- `B`: build mode, then left click world position
- `Shift+B`: install mode; click world position to place a crafted Storage Crate
- `Q`: craft one Storage Crate from owned depot materials
- `Shift+Q`: craft one Field Shovel from owned depot materials
- `T`: use one Field Shovel with the current selection
- `Shift+T`: use one Field Hammer with the current selection
- `R`: send selected units back to storage
- `E`: emergency retreat selected units toward friendly storage/safe fallback
- `V`: hold selected units at their current position
- `X`: stop selected units and clear their current tactical/work order plus queued orders
- `Shift+X`: clear only queued orders while preserving current active orders
- `+` / `-`: flatten radius
- Mouse wheel: zoom
- Middle mouse drag: pan
- `Q` / `W` / `S` / `D`: camera nudge
- `Esc`: clear selection and return to move mode
- `F5`: cycle automation editor scope (selected units / active group / squad)
- `F6`: cycle automation target squad (`Shift+F6` cycles backward)
- `F1` / `F2` / `F3` / `F4`: apply Gather / Escort / Scout / Hold automation preset to the current automation editor target
- `Insert` / `Delete`: add/remove the highlighted automation rule
- `Up` / `Down`: change highlighted automation rule
- `Left` / `Right`: adjust highlighted rule threshold (`Shift` for larger step)
- `Ctrl+Z`: cycle highlighted rule trigger
- `X`: cycle highlighted rule action
- `C`: toggle highlighted rule enabled state
- `Enter`: write a `set_unit_automation` or `set_squad_automation` command payload for the current automation editor target

This client still renders a simplified tactical board, but it is no longer just
an experiment or inspector. It is the main direct-play runtime surface for
DBDReboot right now.

Combat readability is intentionally plain but functional:

- units, construction sites, and structures show HP bars
- selected units show their combat band: `Short`, `Mid`, or `Long`
- selected units show their current `VIS` vision range
- selected groups show their Short/Mid/Long composition count
- attacking units get an `ATK` label
- scouting units get `SCT`; patrol units get `PAT`
- guard/hold units get `GRD` or `HOLD` labels
- selected low-health or overloaded combat units are marked with retreat risk hints
- the side panel shows the latest session combat summary and sharper combat alert
- the side panel shows `Contacts: visible / stale` from the server's scouting cache
- the side panel shows the selected contact, last-seen tick, spotting unit, and contact alert
- visible contacts draw as red contact boxes; stale contacts draw as dim last-seen boxes
- selected contacts draw with a yellow outline and can be investigated with `J`
- selected units with queued orders draw a dotted yellow line and `Q` marker to their next queued target

Combat band intent:

- `Short`: short reach, strongest damage once it closes
- `Mid`: stable general-purpose range and damage
- `Long`: longest reach, weaker damage, best as backline pressure

Scouting/contact rules:

- units refresh player-owned contacts using survival-based vision range
- visible enemies become `visible` contacts
- enemies that leave sight remain as `stale` last-seen positions for a short time
- `A` focus fire and `I` intercept require a visible or recent contact
- `O` scout area sends units to a forward area and keeps them cycling around it
- `P` patrol route makes units move between two points while refreshing contacts
- Shift-queued move/scout/patrol orders are capped at two queued orders per unit; direct tactical orders such as retreat, stop, guard, and focus fire clear the queue
- `Shift+X` writes `clear_queue` and removes queued orders without stopping the unit's current action
- queued orders can be interrupted by contact according to automation rules: gather/scout presets retreat, escort presets engage, and hold presets anchor in place
- selected unit rows show `Queue interrupted`, `Queue held`, or `Retreat override` when the server reports why a queue changed
- route haulers show `Route safe`, `Route threatened`, or `Route critical`, and escorts targeting a route hauler are labeled as `Escorting hauler`
- threatened route haulers also show the fast response hints `Y deposit`, `U drop`, `G escort`, and `E retreat`
- `N` selects a contact and `J` writes an `investigate_contact` command for selected units
- old/stale contact orders degrade into movement toward the last seen position instead of perfect attacks
- full fog-of-war, stealth, detection equipment, and sound scouting are not implemented yet

The intended use is free play, not a forced mission script. You can open with
gathering, hauling, flattening, construction, probing the raider, defending,
or disengaging in whatever order makes sense for the current state.

The current session seed is asymmetric on purpose: your starter node and depot
are relatively safe, but the richer mid-field node and the best early
flatten/build lines sit close enough to Raider space that they become contested
quickly if you push out.

## Command spool notes

When the client issues an order, it writes one JSON file to
`runtime-save\command_spool\inbox\`. `dbd_play_session` moves that file through
`processing\`, writes status to `receipts\`, writes final results to
`results\`, and moves the processed payload to `archive\`.

Useful checks while debugging:

- If units do not react, confirm `dbd_play_session` is still running.
- Check `runtime-save\session_status.json` for the latest command message.
- Check `runtime-save\command_spool\receipts\` and `results\` for command outcomes.
- Check `runtime-save\live_smoke_report.json` after running the live smoke harness.
- Files left in `inbox\` usually mean the play session is not running or cannot claim them.

## Auxiliary debug view (`debug_view.html`)

`debug_view.html` still matters, but as a tool alongside the playable client:

- server state inspection
- regression output checking
- command spool troubleshooting
- snapshot/report sanity checks

Use it when you need visibility into the runtime, not as the default way to
launch or play.
