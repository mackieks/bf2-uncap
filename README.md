# swbf2_uncap

Removes the hard **200 fps cap** in STAR WARS Battlefront II (2017).

The game sleeps the render thread so that no frame takes less than 5 ms, whatever `GameTime.MaxVariableFps` is set to. This mod removes that minimum frame time, so you can run uncapped or set your own cap in your GPU driver (NVIDIA Control Panel, AMD Software, RTSS, etc.).

## Install

1. Download `swbf2_uncap.zip` from [Releases](../../releases).
2. Extract `dinput8.dll` and `swbf2_uncap.ini` into the game folder, next to `starwarsbattlefrontii.exe`.
3. Start the game as usual. Works with or without Frosty Mod Manager.

To check that it's working, open `swbf2_uncap.log` in the game folder. It should contain `interval 50000 -> 0`. The in-game overlay (`PerfOverlay.DrawFps 1` in the console) should show more than 200 fps on the CPU line.

## Configure

Edit `swbf2_uncap.ini`:

| Setting | Meaning |
|---|---|
| `Interval=0` | Minimum frame time in QueryPerformanceCounter ticks, at 10,000,000 per second. `0` means no cap, `25000` caps at 400 fps, `50000` is the stock 200 fps. |
| `Enabled=1` | Set to `0` to turn the mod off without removing it. |

## Uninstall

Delete `dinput8.dll` and `swbf2_uncap.ini` from the game folder.

## Compatibility

- **Game build:** tested only on the October 2024 PC build (`starwarsbattlefrontii.exe` MD5 `76a8ee9a94ef97360a9a3e5974fd5753`). On any other build, the mod checks the game's memory layout, finds it doesn't match, writes nothing, and logs `unsupported game build`.
- **Frosty Mod Manager:** compatible. Frosty uses `CryptBase.dll`, and this mod uses `dinput8.dll`.
- **Other mods named `dinput8.dll`:** only one can be installed at a time.

## Online play

The mod only changes how long the render thread waits between frames. It doesn't touch the 30 Hz simulation tick, networking, or any game data, and it patches no code.

It does write to the game's memory from inside the game process. As far as we know, Battlefront II has no client-side anti-cheat (FairFight runs on the server), but this hasn't been verified. **Use online at your own risk.**

## How it works

`dinput8.dll` is a proxy. The game loads it from its own folder, and every DirectInput call is passed through to the real `System32\dinput8.dll`. Alongside that:

- A low-priority background thread finds the render limiter and sets its frame interval to the configured value.
- It re-checks every 0.5 s, so the value stays applied if the game resets it.
- It does nothing in any program other than `starwarsbattlefrontii.exe`.

See [docs/TECHNICAL.md](docs/TECHNICAL.md) for the reverse-engineering details and addresses.

## Build from source

Requires Visual Studio 2019 or later with the "Desktop development with C++" workload. Run:

```
build.bat
```

The build produces `build\dinput8.dll` and the release archive `dist\swbf2_uncap.zip`.

## License

[MIT](LICENSE)

## Disclaimer

Not affiliated with or endorsed by EA, DICE, Motive or Lucasfilm. STAR WARS Battlefront II is a trademark of its respective owners.
