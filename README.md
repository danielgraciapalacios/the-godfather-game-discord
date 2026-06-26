# The Godfather Discord Rich Presence

![Example](https://i.imgur.com/5rHS8wF.png)

Discord Rich Presence for **The Godfather: The Game** (PC, EA, 2006) — including the Spanish edition *El Padrino*.

It's a native `.asi` plugin (32-bit DLL) that loads automatically when you start the game and displays your Discord profile with the status text.

![Discord status](https://img.shields.io/badge/Discord-Rich%20Presence-5865F2?logo=discord&logoColor=white)
![Platform](https://img.shields.io/badge/plataforma-Windows%20x86-blue)
![License](https://img.shields.io/badge/licencia-MIT-green)

---

## What does it show?


Your Discord profile will display **"Playing The Godfather"** with:

- A status line (currently static: **"New York"**).

- The elapsed session time (automatically tracked by Discord).

> **Project status:** functional basic version. Detecting the specific **neighborhood/district** (Hell's Kitchen, Brooklyn, SoHo, etc.) by reading the game's memory is
> planned as a later phase. See [docs/](docs/).

--

## Requirements

- **Windows** (the game is x86 from 2006).

- **Discord desktop** open and logged in.

- The game with the **[Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader)** installed (usually found as `dinput8.dll` in the game's root directory). This is what loads
the `.asi` plugins. Many modern installations (with SilentPatch, WidescreenFix, etc.) already have it.

---

## Installation (for users)

1. Download `GodfatherDiscordRPC.asi` (from the *Releases* section, or compile it — see below).

2. Copy it to the `scripts\` folder of your game installation, along with the other `.asi` files

(e.g., `C:\The Godfather\scripts\`).

3. *(Optional)* also copy `GodfatherDiscordRPC.ini` to the same folder if you want to customize the text or use your own Discord Client ID.

4. Open Discord and launch the game. Done!

**Required files in `scripts\`:**

| File | Required | Purpose |

---|---|---|

| `GodfatherDiscordRPC.asi` | Yes | The plugin |

`GodfatherDiscordRPC.ini` | Optional | Customization; without it, default values ​​are used |

---

## Configuration (`GodfatherDiscordRPC.ini`)

```ini
[Discord]
client_id = 1520195420398289017

[Settings]
poll_interval_ms = 2000
state = New York
```

- `client_id`: Discord application ID. Create your own in the

[Discord Developer Portal](https://discord.com/developers/applications) if you want it to appear with your own name after "Playing...".

- `poll_interval_ms`: How often it checks the status/retries the connection.

- `state`: The displayed status text.

If the `.ini` file is missing or a key is empty, the compiled default values ​​are used.

---

## Compile from source code

You need **Visual Studio Build Tools 2022** (C++ workflow with x86 target) and **CMake**.

``powershell
# Quick way (use the included script)
.\build.cmd

# Run the tests
.\build.cmd test
```

Or manually, from a *Developer Command Prompt for VS 2022* on x86 architecture:

```powershell
`cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release`
`cmake --build build`
```

The result is `build\GodfatherDiscordRPC.asi` (32-bit DLL).

` ... ---

## How it works

- The plugin is injected into `godfather.exe` via the Ultimate ASI Loader.

- It launches a background thread that connects to Discord using its local IPC (named pipe `\\.\pipe\discord-ipc-N`), performs the handshake, and sends the activity (`SET_ACTIVITY`). No SDK or external dependencies are required.

- If Discord is not open, it periodically re-tries the connection without interfering with the game.

- All operations are isolated to ensure they never affect game stability.

It generates a diagnostic log in `GodfatherDiscordRPC.log`, along with the `.asi` file.

---

## Project Structure

```
src/ C++ Source Code (Logger, Config, DiscordIPC, dllmain)
test/ Tests: unit_tests (logic) + ipc_host_test (Discord without the game)
dist/ Example GodfatherDiscordRPC.ini
docs/ Design and Implementation Plan
build.cmd x86 Build Script
```

---

## Legal Notice

This is an **unofficial** project, with no affiliation with Electronic Arts or the owners of the trademark *The Godfather*. "The Godfather" is a trademark of its respective owners. This software only adds Discord integration and does not include or distribute any game resources.

## License

[MIT](LICENSE).