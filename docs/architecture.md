# DBD Reboot Architecture

## Product thesis

DBD is no longer centered on an operator tool. The core product is a
server-authoritative sandbox war world where:

- each player starts with 4 units
- players can directly command units one by one
- groups can be formed and reassigned to other players
- units are flexible actors with trainable stats, not permanently locked classes
- resources must be gathered, moved, and stored
- structures can be installed anywhere, with terrain changing labor/time instead of hard legality
- flattening is a timed labor job that can be accelerated by assigning multiple units
- all flattening and construction resolve through accumulated labor over time
- losses matter, but insurance softens catastrophic recovery
- insurance can soften economic loss, but it does not reverse permanent death or restore lost skill growth
- lineage is part of military and economic value, not cosmetic flavor
- chunking exists for persistence and simulation cost, not as a visible build-zone system, build parcel, or legal territory unit

## Runtime split

### Dedicated server

The server is the source of truth for:

- unit ownership and controller assignment
- squad membership
- movement targets and current orders
- unit skills, growth, and current work assignments
- harvesting, hauling, and storage resolution
- structure placement, flattening labor, under-construction sites, and destruction
- combat, damage, permanent death, and dropped cargo
- unified attack resolution across units, construction sites, and completed structures
- minimum combat legality checks: alive targets, friendly-fire block, and attack range
- insurance contracts and payouts
- lineage state and offspring resolution
- region risk/reward modifiers
- chunk persistence and daily maintenance rules

### Client

The client is responsible for:

- top-down camera and selection UX
- local command composition
- rendering world, units, structures, resources, and terrain state
- visual prediction only where safe
- sending command requests to the server
- rendering authority-transfer state clearly

## First implementation slice

The first vertical slice should prove:

1. server boots a single map with three risk bands
2. one player spawns with 4 units
3. units can be selected individually and as groups
4. any unit can be reassigned between harvesting, hauling, scouting, escorting, flattening, and building
5. resources can be stored in a depot
6. a flatten command can prepare a structure pad anywhere in the world
7. multi-unit labor measurably speeds up flattening and building
8. workers can be partially reassigned off a live job without resetting all progress
9. a structure site remains vulnerable while under construction and can be damaged or destroyed
10. another player can contest the same area and pressure labor allocation
11. permanent death destroys trained unit value and produces cargo loss plus insurance payout
12. delegated units can still be moved and committed to battle under granted authority
13. saving and reloading the world keeps flattened ground, sites, structures, storage, units, authority, and insurance

## Migration stance

`DBDReboot` is the product base. The Unity project is archival only.
Reuse concept-level ideas only:

- contour/map-plan editing ideas
- control-room event concepts
- update/ops lessons

Do not treat the Unity runtime architecture as an active dependency.
