# ReturnOfModding

---

## User Interface

- Comes with an ImGui user interface. The default key to open the user interface is `INSERT`. You can change this key in the `ReturnOfModding/config/ModLoaderTeamName-ModLoaderName-Hotkeys.cfg` file. If the file isn't there, it will appear once you've launched the game at least once with the mod loader installed. There's also a small onboarding window that will appear in-game if the mod loader has launched successfully, where you can also edit the keybind.

## Creating mods

- Define a `main.lua` file in which to code your mod.

- Create a `manifest.json` file that follows the Thunderstore Version 1 Manifest format.

- Create a folder whose name follows the GUID format `TeamName-ModName`, for example: `ReturnOfModding-DebugToolkit`.

- Place the `main.lua` file and the `manifest.json` file in the folder you've just created.

- Place the newly created folder in the `plugins` folder in the ReturnOfModding root folder, called `ReturnOfModding`, so the path to your manifest.json should be something like `ReturnOfModding/plugins/ReturnOfModding-DebugToolkit/manifest.json`.

## Mod Manager Integration

If you'd like to integrate ReturnOfModding into your mod manager, here are the specifications:

ReturnOfModding is injected into the game process using DLL hijacking, which is the same technique used by other bootstrappers such as [UnityDoorstop](https://github.com/NeighTools/UnityDoorstop).

The root folder used by ReturnOfModding (which will then be used to load mods from this folder) can be defined in several ways:

- Setting the process environment variable: `rom_modding_root_folder <CUSTOM_PATH>`

- Command line argument when launching the game executable: `--rom_modding_root_folder <CUSTOM_PATH>`

- If the process environment variable is not defined, the command line arguments are checked. If neither is defined, the ReturnOfModding folder is placed in the game folder, next to the game executable.

### Folder Convention:

The subfolder we are referring to in the paragraphs below always follows the GUID format `TeamName-ModName`, for example: `ReturnOfModding-DebugToolkit`.

- `plugins`: Location of .lua, README, manifest.json files. The subfolder dedicated to the plugin is removed upon uninstallation by the mod manager.
- `plugins_data`: Used for data that must persist between sessions, mod assets, and other data that should not be manipulated by the user. The subfolder dedicated to the plugin is removed upon uninstallation by the mod manager.
- `config`: Used for data that must persist between sessions and that can be manipulated by the user. The subfolder for the plugin is **never** removed upon uninstallation by the mod manager.
