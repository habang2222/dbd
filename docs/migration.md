# Legacy Migration Notes

## Keep from the Unity line

- map-plan as an operator-authored world influence concept
- terrain raise/lower/contour visualization ideas
- runtime-config lessons about a single config source
- packaging lessons: client, server, and ops tools must version separately

## Do not carry forward

- empty scene bootstrap assumptions
- relay-first gameplay architecture
- control-room-first product framing
- Unity-specific distribution scripts as the new source of truth
- direct reuse of the old runtime UI stack

## Reboot baseline

- server-authoritative world first
- Unreal client later, not required to define server model today
- control room returns only after core war/economy play is alive
- continuous world with chunked persistence under the hood
- building anywhere, with labor/time/terrain prep as the real constraint
- flexible units with stat growth and permanent death
