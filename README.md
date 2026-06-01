<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="/program_info/io.github.mintlauncher.MintLauncher.logo-darkmode.svg">
    <source media="(prefers-color-scheme: light)" srcset="/program_info/io.github.mintlauncher.MintLauncher.logo.svg">
    <img alt="Mint Launcher" src="/program_info/io.github.mintlauncher.MintLauncher.logo.svg" width="40%">
  </picture>
</p>

# Mint Launcher

Mint Launcher is a **rebranded fork** of [PineconeMC](https://github.com/ElyPrismLauncher/Launcher) (which adds [Ely.by](https://ely.by/) support to [Prism Launcher](https://prismlauncher.org/)). It is **not** affiliated with Prism Launcher, PineconeMC, Microsoft, or Ely.by.

## Ely.by

- **OAuth2** sign-in is built in (browser flow; no password stored by the launcher).
- **Game sessions** use Ely’s auth stack and the same authlib patching approach as PineconeMC.
- You **must** register your own OAuth2 application at [account.ely.by](https://account.ely.by/), then set `Launcher_ELY_CLIENT_ID` at configure time (see `CMakeLists.txt`) to your **client id** (default placeholder: `mint-launcher`). Use a redirect URI that matches your build (see Prism/Pinecone patterns and [Ely.by OAuth docs](https://docs.ely.by/en/oauth.html)).

## CustomSkinLoader (CSL)

Mint Launcher **automatically installs** [CustomSkinLoader](https://modrinth.com/mod/customskinloader) into each instance that has a **mod loader** (Fabric, Forge, NeoForge, or Quilt). The correct build is chosen from Modrinth for your Minecraft version and loader. Pure vanilla instances (no mod loader) are skipped.

To disable auto-install, set `MintInstallCustomSkinLoader` to `false` in the launcher config (`mintlauncher.cfg`).

## Branding

See **[program_info/BRANDING.md](program_info/BRANDING.md)** for where to place logos, icons, and Windows/macOS artwork before you rebuild.

## Defaults in this tree

- **Meta / legacy FML libraries / news**: Default meta is the Ely/Pinecone mirror (`https://elyprismlauncher.github.io/meta/v1/`) so Ely authlib and authlib-injector resolve correctly. Prism meta is only used as a last-resort fallback (`MetaMirrorNetworkCheck`).
- **Auto-updater**: disabled until you set `Launcher_UPDATER_GITHUB_REPO` to your published fork.
- **Repository**: [github.com/DroppYM/MintLauncher](https://github.com/DroppYM/MintLauncher)
- **Bug tracker**: [github.com/DroppYM/MintLauncher/issues](https://github.com/DroppYM/MintLauncher/issues)

## Build

Follow [Prism Launcher build instructions](https://prismlauncher.org/wiki/development/build-instructions) (CMake presets, Qt 6, etc.). This repo keeps the same overall layout as upstream.

### Before you ship

1. Replace GitHub URLs, domain (`Launcher_Domain`), and artwork if you do not want the Swzo / pinecone-style logos.
2. Obtain your own **CurseForge API** key if you redistribute builds (see comment in `CMakeLists.txt`).
3. Register **Microsoft Azure AD** and **Ely.by** OAuth apps for production client IDs.

## License

GPL-3.0-only — see `COPYING.md`. Upstream copyrights from Prism Launcher, MultiMC, and PineconeMC contributors remain in force.
