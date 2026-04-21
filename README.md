# DBD Reboot

DBD reboot product line for a server-authoritative RTS/MMO sandbox.

This folder is intentionally separate from the legacy Unity project in
`My project`. The Unity codebase remains a reference/prototype source for:

- control-room terrain editing ideas
- map-plan concepts
- packaging/operator flow history

The new product direction is:

- Unreal-based C++ client later
- C++ authoritative dedicated server first
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
- `server/` - authoritative simulation bootstrap
- `client/` - Unreal client integration notes and future glue layer
- `tools/` - control-room and ops migration notes

## Current status

This reboot line is a scaffold, not a finished game. It defines the first
authoritative data model and protocol surface so implementation can begin
without inheriting Unity-specific runtime assumptions.

Current scaffold focus:

- continuous terrain evaluation instead of patch-gated placement
- all flattening and construction resolves through accumulated labor over time, and multiple units speed it up
- workers can be added to or pulled from live jobs without resetting progress for everyone else
- under-construction sites are vulnerable world objects that can be contested and destroyed
- permanent unit death with partial cargo drop persistence
- unit skill growth for strength, harvesting, hauling, construction, combat, survival
- unit/squad authority can be delegated for shared wars
- chunk-backed file saves preserve flattened ground, sites, structures, storage, units, authority, and insurance
