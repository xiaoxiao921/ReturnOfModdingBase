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

Copying folders must preserve the folder hierarchy without flattening it.

### `plugins`

Location of `.lua`, `README`, and `manifest.json`.
 
#### First Installation
- Copy content into `plugins/subfolder`.

#### Update
- Erase `plugins/subfolder` directory if it already exists.
- Copy content into `plugins/subfolder` directory.

#### Uninstallation
- Delete `plugins/subfolder` directory.
  
### `plugins_data`

This directory is primarily used for data that must persist between sessions and should not be altered by the user.

To ensure the mod manager does not modify certain data during updates, use a folder called `cache` located at `plugins_data/subfolder/cache`.

#### First Installation
- Copy the contents into the `plugins_data/subfolder` directory.

#### Update
- If the `plugins_data/subfolder/cache` folder does not exist, delete the entire `plugins_data/subfolder` directory. Otherwise, delete everything in the directory except the `cache` folder.
- Copy the new contents into the `plugins_data/subfolder` directory, overwriting any existing files with the same names, including those in the `plugins_data/cache` folder if present in their .zip package.

#### Uninstallation
- If the `plugins_data/subfolder/cache` folder does not exist, delete the entire `plugins_data/subfolder` directory. Otherwise, delete everything in the directory except the `cache` folder.

### `config`

This directory is primarily used for data that must persist between sessions and can be manipulated by the user.

Mod creators are advised not to include pre-created `.cfg` files in their `config` folder within their `.zip` package, as this will cause the mod manager to overwrite any existing user configuration files with the same name (e.g., during updates).

Instead, let your code generate `.cfg` files using the mod API `config.config_file`.

A valid use case for providing pre-made configuration files is when creating a modpack that aims to deliver a curated, set-in-stone experience.

#### First Installation
- Copy the contents into the `config/subfolder` directory.

#### Update
- Copy the contents into the `config/subfolder` directory, overwriting any existing files with the same names.

#### Uninstallation
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
