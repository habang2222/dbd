# Pixel Placeholder Assets

This folder contains deliberately tiny 32x32 pixel placeholders for the first
DBDReboot visual pass.

They are not final art. They exist so `dbd_client3d` and future UI surfaces can
refer to stable placeholder identities while the game still uses simple
Minecraft-like voxel shapes in-world.

Current ids:

- `person.png`
- `raider.png`
- `wood.png`
- `stone.png`
- `iron.png`
- `depot.png`
- `site.png`
- `structure.png`
- `cargo.png`
- `marker.png`

Regenerate them with:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\generate-pixel-assets.ps1
```

