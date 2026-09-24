# SCOURGE VR

A Fallout 4 VR port of **SCOURGE by GELUXRUM**. It recalculates NPC stats using the original mod's race filters, configurable ranges, and multipliers.

**The original SCOURGE is required. Install the original mod first, then install this port after it so the VR DLL overwrites the original DLL.** Keep the original plugins, scripts, YAML files, and MCM files.

## Requirements

- Fallout 4 VR **1.2.72.0**.
- **F4SEVR 0.6.21** and **VR Address Library for F4SEVR**.
- [Original SCOURGE by GELUXRUM](https://www.nexusmods.com/fallout4/mods/60917), with its required data files and dependencies.
- [Original Scaling Flag Remover by GELUXRUM](https://www.nexusmods.com/fallout4/mods/73976), with the DLL supplied by [Scaling Flag Remover VR](https://github.com/brunocatani/ScalingFlagRemover-VR).
- **Daytripper / Fallout VR ESL Support** when using ESL or ESL-flagged plugins.

RobCo Patcher is optional. If installed, use its VR port and configure it as described below.

## Installation

1. Install the original SCOURGE and its requirements. Keep the original data files and required ESPs enabled.
2. Install this port as a separate mod in Mod Organizer 2.
3. Place the port **below the original SCOURGE in MO2's left pane**, allowing it to overwrite `F4SE\Plugins\GLXRM_SCOURGE.dll`.
4. Install the original Scaling Flag Remover, then place its VR port below it in the same way.
5. Launch the game through F4SEVR.

MO2's left pane controls file-overwrite priority. The port replaces the native DLL; it does not replace the original mod's data files or change their ESP load order.

For manual installation, install the original mods first and replace their native DLLs with the corresponding VR DLLs. SCOURGE uses the original filename:

```text
Data\F4SE\Plugins\GLXRM_SCOURGE.dll
```

## Configuration

Configure NPC stat ranges and multipliers through the original SCOURGE MCM. The port reads the original YAML filters from `Data\SCOURGE\`.

SCOURGE does not create or read a separate INI. Its dependency checks are mandatory; the original `GLXRM_SCOURGE.ini` bypass option does not disable them.

### RobCo Patcher compatibility

If RobCo Patcher is installed, set this option in its original INI, `Data\F4SE\Plugins\RobCo_Patcher.ini`:

```ini
[Features]
iEnableReCalculateStatsWithSaveLoad=0
```

With MO2, edit the INI supplied by the mod winning that file conflict.

RobCo's save-load stat recalculation conflicts with SCOURGE. SCOURGE checks the setting at startup and before loading a save. An enabled, missing, invalid, or unreadable setting produces a Windows warning and terminates the game when the warning is dismissed.

Scaling Flag Remover must also be successfully loaded by F4SEVR. A missing or rejected plugin blocks startup with a warning.

## Logs and support

The log is:

```text
Documents\My Games\Fallout4VR\F4SE\GLXRM_SCOURGE.log
```

It reports dependency checks, record and filter loading, spell distribution, and actor recalculation counts. These counts do not establish an individual NPC's final stat values.

For [VR-port issues](https://github.com/brunocatani/Scourge-VR/issues), include this log, `f4sevr.log` from the same game session, relevant settings, and a description of the affected NPC. Direct VR-port bug reports to this repository.

## Building from source

Install Visual Studio 2022 with Desktop development with C++, a Windows SDK, CMake 3.25 or newer, vcpkg, and CommonLibF4VR. Set `VCPKG_ROOT` to your vcpkg checkout and `COMMON_LIB_F4VR_PATH` to CommonLibF4VR's `CommonLibF4` subdirectory.

```powershell
cmake --preset fast
cmake --build --preset fast
ctest --preset fast
```

The preset builds Release with compiler concurrency capped at two processes. The DLL is written to `build-fast/Release/GLXRM_SCOURGE.dll`. Put machine-specific settings and optional deployment paths in an ignored `CMakeUserPresets.json`.

## Credits and permission

- **GELUXRUM** — original creator of SCOURGE. Thank you for creating the mod and granting permission for this VR port. Please support the original mod and its author.
- **brunocatani** — VR adaptation and maintenance.
- **shad0wshayd3** — EditorID and Papyrus helper code, with the existing copyright and license notices retained.
- The **F4SEVR**, **CommonLibF4VR**, **SimpleIni**, and **yaml-cpp** contributors.

The original mod remains a mandatory separate download.

## License

This VR port is distributed under the **GNU General Public License, version 3**. See [LICENSE](LICENSE).

Third-party source files retain their original copyright and license notices. The original mod's separate assets and distribution retain their applicable terms.
