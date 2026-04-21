# Unreal Client Notes

The reboot client is expected to be an Unreal C++ project later.

## First client responsibilities

- top-down RTS camera
- click selection and box selection
- per-unit and group command UI
- authority transfer UI for handing units/groups to another player
- world rendering for terrain, resources, units, structures, and region risk overlays
- command submission to the authoritative server

## First Unreal systems to create

- `DBDWorldSubsystem`
- `DBDSelectionComponent`
- `DBDOrderPreviewComponent`
- `DBDTopDownCameraPawn`
- `DBDUnitActor`
- `DBDStructureActor`
- `DBDResourceNodeActor`

The Unreal client should not own simulation truth. It is a command-and-render client.
