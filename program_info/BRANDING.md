# Mint Launcher branding assets

Replace the files below, then rebuild the launcher (`cmake --build` / your usual preset). Windows icons are regenerated from SVG during the build when `rsvg-convert` or Inkscape is available; otherwise copy a ready-made `.ico` into `program_info/mintlauncher.ico`.

## Primary artwork (edit these first)

| File | Used for |
|------|----------|
| `io.github.mintlauncher.MintLauncher.svg` | Main app icon (in-app, Linux, generic) |
| `io.github.mintlauncher.MintLauncher.logo.svg` | Light-mode logo (About, README, welcome) |
| `io.github.mintlauncher.MintLauncher.logo-darkmode.svg` | Dark-mode logo |
| `mintlauncher.ico` | Windows executable and installer icon |
| `MintLauncher.icon/` | macOS `.icns` bundle (`Assets/*.svg` + `icon.json`) |

## Optional / platform extras

| File | Used for |
|------|----------|
| `io.github.mintlauncher.MintLauncher.bigsur.svg` | macOS Big Sur–style variant |
| `instance_icons.svg` | Default instance icon sheet in the launcher |
| `io.github.mintlauncher.MintLauncher_256.png` | 256×256 PNG (generated from SVG if tools exist) |
| `io.github.mintlauncher.MintLauncher.desktop.in` | Linux desktop entry (name/icon) |
| `io.github.mintlauncher.MintLauncher.metainfo.xml.in` | Flathub / app store metadata |
| `mintlauncher.rc.in` | Windows version resource block |
| `mintlauncher.qrc.in` | Qt resource bundle (logos embedded in the binary) |

## In-app icon themes (instance list, mods, etc.)

Not under `program_info/` — user and built-in themes live in:

- **Source (shipped defaults):** `launcher/resources/iconthemes/`
- **User overrides:** `%AppData%\MintLauncher\iconthemes\` (Windows) or `~/.local/share/MintLauncher/iconthemes/`

## Widget / Qt styles

- **Source:** `launcher/resources/themes/`
- **User overrides:** `%AppData%\MintLauncher\themes\`

## Background “cat packs” (disabled in Mint Launcher)

Mint Launcher hides the old MultiMC background cat. Custom images can still be dropped in `%AppData%\MintLauncher\catpacks\` if you re-enable the feature in a custom build.

## CMake branding strings

Edit `program_info/CMakeLists.txt` for display name, App ID, domain, and Git URLs (`Launcher_DisplayName`, `Launcher_AppID`, `Launcher_Domain`, `Launcher_Git`).
