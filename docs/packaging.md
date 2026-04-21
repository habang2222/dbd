# DBD Reboot Packaging

## Product split

`DBDReboot` is the canonical codebase for all three products below.

The reboot must package three products independently:

1. `dbd-client`
2. `dbd-server`
3. `dbd-control-room`

## Why this split is mandatory

The Unity line bundled gameplay payload, update transport, and environment wiring too tightly.
The reboot should not repeat that mistake.

- The client is a player install/update problem.
- The server is a deployment/service problem.
- The control room is an operator/web deployment problem.

## Release model

### Client

- installer per platform
- manifest-driven updates
- player config outside binaries
- channel-aware endpoint selection

### Server

- versioned zip/tar/service package
- config and save data outside release folder
- no player-style self-updating install flow

### Control room

- web app or service package
- independent release cadence
- independent config and API endpoint wiring

## Carry forward from the Unity line

- manifest-driven updates are good
- versioned immutable releases are good
- local dev and public release should differ by transport, not by artifact format

## Do not carry forward

- engine-specific payload assumptions
- side-car hand-maintained installer forks
- localhost baked into production artifacts
