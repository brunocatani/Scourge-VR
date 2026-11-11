# Changelog

## Unreleased

First release of the Fallout 4 VR port of SCOURGE by GELUXRUM. It replaces the
original mod's `GLXRM_SCOURGE.dll`; the original mod's plugins, scripts, YAML
filters and MCM files stay required.

- Recalculates NPC stats with the original mod's race filters, ranges and
  multipliers, configured through the original MCM, on Fallout 4 VR 1.2.72.0
  with F4SEVR 0.6.21.
- Filters match the original mod: an NPC's race is the actor's own race, never
  the race of the leveled record it came from, so leveled humans no longer pass
  through a synth filter; keyword and faction tests work as in the original.
- Recalculation runs through the original mod's spell effect and its manual
  `SCOURGE_Recalculate_Global`, as in the original, with no extra sweep at load.
- The dependency checks are always on: a missing or rejected Scaling Flag
  Remover, or RobCo Patcher's save-load stat recalculation turned on, shows a
  warning and closes the game when the warning is dismissed.
