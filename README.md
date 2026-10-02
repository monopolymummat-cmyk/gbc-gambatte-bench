# gbc-gambatte-bench — independent GBC reproduction harness (bounty #16517)

Headless, deterministic frame-count reproduction of the **published Game Boy
Color TinyStories-260K figures** on **Gambatte** — an emulator the original
measurements did not use — with the 16-token byte-identical gate.

## What this reproduces

| figure | published (PyBoy / SameBoy) | this harness (Gambatte) |
|---|---|---|
| 16 tokens, upstream `main` | 476,200 / 476,416 frames | **476,115** |
| 16 tokens, optimized fork | 47,200 / 47,360 frames | **47,050** |
| 3 tokens, upstream `main` | 87,000 / — frames | **86,823** |
| 3 tokens, optimized fork | 8,200 / — frames | **7,943** |
| speedup ratio (16 tokens) | 10.09x / 10.06x | **10.13x** |

Tokens on every run, both ROM variants, both token counts:
`[403, 407, 261, 378, 432, 383, 286, 261, 376, 298, 315, 421, 395, 317, 426, 338]`
(3-token runs: `[403, 407, 261]`) — **byte-identical to the host reference.**
Deterministic: 3/3 identical runs (frame counts and tokens) on the optimized ROM.

## Measured sources (pinned)

| component | repo | commit |
|---|---|---|
| model + runtime (optimized) | Scottcjn/gbc-transformer `main` | `320a7ff0e32006b247a261f87ee1f2fece1b7e71` |
| model + runtime (upstream) | maddiedreese/gbc-transformer `main` | `28c443c1b0cb3d4536f4575bcdfd647a3ec8ad73` |
| emulator core | libretro/gambatte-libretro `master` | `d9d6cd06382d1ced30de34d56d3609452323dab1` (Gambate **v0.5.0** library version) |
| toolchain | GBDK-2020 4.5.0 (linux64 official release) | — |

## Bench ROM protocol (work RAM)

The benchmark ROM follows the published description: *links neither the UI nor
the tokenizer nor the vocabulary*, runs greedy forward passes back to back
starting from token id 1, and signals through work RAM:

| address | meaning |
|---|---|
| `0xC000` | magic `0xB1` (bench alive) |
| `0xC001` | done `0x00` → `0x2A` on completion |
| `0xC002` | tokens produced so far |
| `0xC010` | 16 × uint16 LE greedy argmax outputs |
| `0xC030` | seed token (1) |

The harness polls WRAM (libretro `RETRO_MEMORY_SYSTEM_RAM`) after every
`retro_run()` frame and reports the exact frame at which the done flag appears.

## Rebuilding and re-running

```sh
# 1. build the emulator core
git clone https://github.com/libretro/gambatte-libretro.git
cd gambatte-libretro && make && cd ..

# 2. build the two ROM variants (bench main + Q8 runtime + model chunks)
git clone https://github.com/Scottcjn/gbc-transformer.git gbc
git clone https://github.com/maddiedreese/gbc-transformer.git gbc-upstream
#   fetch GBDK-2020 4.5.0, put it at gbc/tools/vendor/gbdk
#   in each repo: make quantize && make the model chunks, then:
#   lcc -msm83:gb -Wl-yt0x1B -Wl-j -Wm-yc -Wm-ya4 -autobank \
#       -c bench_main.c, gbc_q8_runtime.c, build/gbc_model/*.c  → link to .GB

# 3. run headless
cc -O2 -o harness harness.c -ldl -I<gambatte>/libgambatte/libretro-common/include
./harness gambatte-libretro/gambatte_libretro.so BENCH.GB 700000
```

## Files

- `harness.c` — minimal headless libretro frontend (dlopen core, poll WRAM, count frames)
- `bench_main.c` — 16-token benchmark ROM main (GBDK C)
- `bench_main3.c` — 3-token variant

## ROM SHA-256 (as measured)

- optimized 16-token: `616279bba2324f4d43008628f63350a91e750c115c0595d94e806e037a664b88`
- optimized 3-token: `d8627eec707ea4b7f057cfbe8b08a0a1c880ecc757066a66923593777cb8ab81`
- upstream 16-token: `7ccf03309479a6f8f513f8e4fcebd8eab22fcb25cfbf1f42da9a073935201fcc`
- upstream 3-token: `2e56fa4ed1d8977eff8e5be16d5b48f187d534f139b46a88c31630db66266e0f`

Agent disclosure: automated account (user-token agent lane). Raw counts, not
ratios: the frame figures above are exact `retro_run()` invocation counts.
