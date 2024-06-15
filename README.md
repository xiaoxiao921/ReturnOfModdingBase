# ReturnOfModdingBase

---

## User Interface

Comes with an ImGui user interface.

The default key to open the user interface is `INSERT`.

You can change this key in the `ReturnOfModding/config/ModLoaderTeamName-ModLoaderName-Hotkeys.cfg` file.

If the file isn't there, it will appear once you've launched the game at least once with the mod loader installed.

There's also a small onboarding window that will appear in-game if the mod loader has launched successfully, where you can also edit the keybind.

## Creating mods

- Define a `main.lua` file in which to code your mod.

- Create a `manifest.json` file that follows the Thunderstore Version 1 Manifest format.

- Create a folder whose name follows the GUID format `TeamName-ModName`, for example: `ReturnOfModding-DebugToolkit`.

- Place the `main.lua` file and the `manifest.json` file in the folder you've just created.

- Place the newly created folder in the `plugins` folder in the ReturnOfModding root folder, called `ReturnOfModding`, so the path to your manifest.json should be something like `ReturnOfModding/plugins/ReturnOfModding-DebugToolkit/manifest.json`.

## Folder Convention

The `subfolder` we are referring to in the paragraphs below always follows the GUID format `TeamName-ModName`, for example: `ReturnOfModding-DebugToolkit`.

It also refers to the mod `subfolder` in its respective root folder.

- `plugins`

  Location of `.lua`, `README`, and `manifest.json`.
 
  - First Installation
    - Copy content into `plugins_data/subfolder`.

  - Update
    - Erase `plugins/subfolder` directory if it already exists.
    - Copy content into `plugins/subfolder` directory.

  - Uninstallation
    - Delete `plugins/subfolder` directory.
  
- `plugins_data`

  Mostly used for data that must persist between sessions and that should not be manipulated by the user.

  - First Installation
    - Copy content into `plugins_data/subfolder` directory.

  - Update
    - Copy content into `plugins_data/subfolder` directory and overwrite any existing files of the same names.

  - Uninstallation
    - Delete `plugins_data/subfolder` directory.

- `config`

  Mostly used for data that must persist between sessions and that can be manipulated by the user.
 
  - First Installation
    - Copy content into `config/subfolder` directory.

  - Update
    - Copy content into `config/subfolder` directory and overwrite any existing files of the same names.

  - Uninstallation
    - Do nothing.

## Mod Manager Integration

If you'd like to integrate ReturnOfModding into your mod manager, here are the specifications:

- ReturnOfModding is injected into the game process using DLL hijacking, which is the same technique used by other bootstrappers such as [UnityDoorstop](https://github.com/NeighTools/UnityDoorstop).

  The dll used by this technique depends on the game you're using the mod loader for. This means that the name of the dll may change from one game to another.

- The root folder used by ReturnOfModding (which will then be used to load mods from this folder) can be defined in several ways:

  - Setting the process environment variable: `rom_modding_root_folder <CUSTOM_PATH>`

  - Command line argument when launching the game executable: `--rom_modding_root_folder <CUSTOM_PATH>`

  - If the process environment variable is not defined, the command line arguments are checked. If neither is defined, the ReturnOfModding folder is placed in the game folder, next to the game executable.

- The root folder is always named `ReturnOfModding`.

- See [Folder Convention](#folder-convention) to find out how to manage mod installation, update, and uninstallation.
