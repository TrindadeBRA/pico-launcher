# Favorites and menu music

## Favorites

- Highlight a game and press **X** to add or remove it. Favorite games show `[Favorito]` in the top screen's filename line.
- The **heart button** beside the gear opens a list of favorites from all folders. Press it again, or press **B**, to return to the folder you were browsing.
- Press **A** or tap an already selected game to launch it as usual. Its original absolute path is used for loading, save files and last-used selection.
- Removing the selected favorite refreshes the list. An empty list explains how to add favorites.
- Missing games are skipped; their saved entries remain so temporarily removed games can reappear. Duplicate names in different folders remain separate entries.
- Favorites are stored in `/_pico/favorites.json`, with a maximum of 512 paths of up to 255 bytes. Rename/move a game and its old favorite no longer resolves; mark it again at the new path.
- Saves write and sync a temporary file before rotating the old list to `favorites.bak`. Loading falls back to the backup if the primary JSON is missing or invalid. Failed writes leave the in-memory selection unchanged and show an error in the top filename line.

## Music

The gear now has a **Musica** row:

1. Musical note: turn menu music on/off.
2. Minus: decrease volume by 10 percentage points.
3. Plus: increase volume by 10 percentage points.
4. Next icon: play the next valid BCSTM file in alphabetical order, wrapping at the end.

The status line shows enabled state and volume, or explains when no valid track exists. Controls support touch or directional focus followed by A. The choice of enabled state and volume is saved on closing the sheet with B, touching outside it, or opening the theme selector. Settings default to music enabled and 100% volume when upgrading from an older config. These controls affect menu music; games retain their own audio controls.

Tracks come from the current theme's `bgm` folder. Startup keeps the existing random choice. Hidden files, directories, unsupported extensions and invalid tracks are skipped. Themes without BCSTM music remain silent. Software volume affects PCM output and takes effect as the buffered audio is replenished; the physical volume control still applies.

## Validation

Run `python3 tests/test_preferences.py`. Host tests compile the production favorites repository and settings serializer against a filesystem double. They exercise persisted reload, case-insensitive FAT identity, duplicate names in separate folders, invalid paths, limits, failed writes/renames, backup recovery, backwards-compatible settings, volume bounds, signed PCM sample gain and focus navigation to/from the music row. They do not establish DS boot, rendering, streaming or hardware input behavior. Those require a console test.

Stopping playback joins the decoder without holding its buffer mutex. Decoded PCM buffers are flushed by buffer address after applying volume. App exit drains pending I/O before destroying controller state or stopping the player.
