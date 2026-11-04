# Technical notes

All addresses are for `starwarsbattlefrontii.exe` with MD5 `76a8ee9a94ef97360a9a3e5974fd5753` (October 2024 build). The executable has a fixed image base of `0x140000000` and no relocations, so these addresses are the same on every launch.

## Where the 200 fps cap comes from

The cap is a **hardcoded 5 ms minimum frame time on the render thread**. It is separate from `GameTime.MaxVariableFps`, which is why that console variable works below 200 but has no effect above it.

1. **Render thread loop.** The loop is at `0x140BB1450` (profiler label `"Rendering"`). Each frame it calls `0x1417854D0`, which:
   - asks up to 4 pacing objects (`owner+0x8A8[i] -> +0x50`) for a sleep time through virtual slot `+0x40`;
   - calls `Sleep(seconds * 1000)`.
2. **The limiter.** The active pacing object has vtable `0x143162C68`. Its slot `+0x40` is a thunk to `0x147FFE580`, which:
   - returns `(deadline - now) / QPC frequency` as the sleep time, where `deadline` is at `+0x7068`;
   - then sets `deadline = max(now + interval, deadline + interval)`, where `interval` is at `+0x7060` in QueryPerformanceCounter ticks.
3. **Stock value.** `interval` is **50000 ticks**. With the usual 10 MHz QPC frequency that is **5.000 ms, or 200 fps**.
4. **How to reach it.**
   - The owner is a static object at `0x144090F70` (vtable `0x143164610`).
   - The pointer chain is `[0x144090F70 + 0x8A8] -> +0x50 -> limiter`.
   - The only writer of `interval` is a small setter at `0x148014FE0`, reached through a thunk at `0x141785BF0` in vtable slot `+0x1C8`. Its callers are in obfuscated code and were not traced.

## Other timers (not changed)

- **Simulation.** The 30 Hz simulation tick runs on its own waitable-timer loop: `"GameSimulation"`, `0x14017A400` -> `0x146A0D980` -> `0x146A3FEB0`.
- **MaxVariableFps.** `GameTimeSettings` (vtable `0x142FD11C0`, constructor `0x146A595A0`) holds `MaxVariableFps` at `+0x2C`, with a default of 200.0. It is applied separately from the render limiter above.

## Why a proxy DLL

- **Binary patching isn't practical.** The game's code is encrypted on disk by EA's DRM (section `.ooa`) and only decrypted at runtime. Patching the file would mean defeating that protection.
- **A Frosty mod can't do it.** Frosty edits game data assets. The 5 ms floor isn't known to come from any asset.
- **A data write is enough.** Setting `interval` to 0 makes the limiter return 0 seconds, because the deadline never moves past "now". The DLL only changes this one value. It uses the same atomic exchange as the game's own setter and modifies no code.

## How it was found

1. Read-only sampling of every game thread's instruction pointer and stack showed the render thread spending about 88% of its time in `Sleep()` under `0x1417854D0`.
2. Following the live vtables led to `0x147FFE580` and the 50000-tick interval.
3. A live test confirmed it: writing 25000 gave 400 fps, and writing 0 removed the cap. `MaxVariableFps` had no effect on this limiter.

## Porting to a new game build

After a game update, every address above will probably move. To find them again:

1. Sample the render thread's stack while it sleeps to find the function that calls `Sleep`.
2. Follow its object to the vtable slot that returns the sleep time.
3. In that function, look for an "interval" field: a 64-bit QPC tick count that equals 50000 in the stock game.
4. Update the constants at the top of `src/swbf2_uncap.cpp`.
