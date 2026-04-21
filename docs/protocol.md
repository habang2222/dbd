# DBD Reboot Protocol

## Goal

Keep the first network contract small and server-authoritative.

Clients send:

- session intent
- world join intent
- unit/group orders
- authority transfer requests
- harvesting/hauling/building/flattening requests
- insurance and offspring requests

Servers send:

- login result
- world snapshot
- authoritative world events and progress notifications
- later: deltas and entity/tile replication

## First message families

### Session

- `HelloMessage`
- `LoginMessage`
- `LoginResultMessage`
- `JoinWorldMessage`

### World state

- `WorldSnapshotMessage`
- `EventNoticeMessage`
- `ErrorNoticeMessage`

### Player intent

- `UnitOrderRequestMessage`
- `GroupAssignRequestMessage`
- `AuthorityTransferRequestMessage`
- `HarvestRequestMessage`
- `HaulRequestMessage`
- `StartFlattenJobMessage`
- `AssignUnitsToFlattenJobMessage`
- `StartConstructionMessage`
- `AssignUnitsToConstructionMessage`
- `AttackRequestMessage`
- `InsuranceQuoteRequestMessage`
- `InsuranceBindRequestMessage`
- `OffspringRequestMessage`

### Work and loss events

- These are server-originated authoritative notifications, not client requests.
- `FlattenJobProgressMessage`
- `FlattenJobCompletedMessage`
- `ConstructionProgressMessage`
- `ConstructionCompletedMessage`
- `ConstructionDamagedMessage`
- `ConstructionDestroyedMessage`
- `UnitSkillProgressedMessage`
- `UnitDiedPermanentMessage`
- `DroppedCargoSpawnedMessage`
- `DroppedCargoDestroyedMessage`

Attack requests target one of:
- unit
- construction site
- completed structure

## Authority rules

- Clients never directly mutate world state.
- Ownership and command authority are separate.
- A player may own a unit but temporarily grant command authority to another player.
- The server validates every order against ownership, authority scope, and current unit state.

## First replication rule

The first vertical slice can start with:

1. login
2. full world snapshot on join
3. command request/response events
4. periodic authoritative full-state refresh for a tiny prototype

Then move to deltas once the domain is stable.
