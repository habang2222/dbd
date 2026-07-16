# DBD Character Layer Assets

This folder is for character artwork that can be assembled from transparent PNG
layers. The game uses two character outputs:

- `composites/fullbody/` for inventory, equipment, and dress-up views.
- `composites/topdown/` for simplified RTS battlefield units.

The first working format is transparent PNG. PSD files are optional source art
and are not required for the runtime pipeline.

## Folder Layout

- `base/` - body base layers, such as `body_base_front.png`
- `clothes/` - shirts, pants, shoes, outerwear, hair, and similar wearable layers
- `equipment/` - held items, headgear, bags, cloaks, weapons, tools
- `topdown/` - top-down unit source or simplified exports
- `portraits/` - face or inventory portrait art
- `composites/fullbody/` - generated full-body dressed characters
- `composites/topdown/` - generated top-down dressed units
- `manifests/` - JSON layer recipes for generated composites

## Layer Order

The default full-body order is:

1. `body_base`
2. `underwear_or_inner`
3. `pants`
4. `shirt`
5. `outerwear`
6. `shoes`
7. `hair`
8. `headgear`
9. `held_item`
10. `back_item`

Each input PNG should use the same canvas size as the body base whenever
possible. If a layer is smaller, the compose tool can place it with an offset.

## First Test Set

Start with these files:

- `base/body_base_front.png`
- `clothes/shirt_basic_front.png`
- `clothes/pants_basic_front.png`
- `clothes/boots_basic_front.png`

Then copy `manifests/fullbody_template.json` to a new manifest and point each
layer at the files you want to combine.

