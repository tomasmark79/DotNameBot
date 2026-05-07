# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build System

Meson + Ninja with a Makefile convenience wrapper. C++20 required. Development environment is Nix-based (direnv auto-activates via `.envrc`).

```bash
make build          # Release build (native)
make debug          # Debug build (native)
make build-clang    # Release build using Clang
make test           # Run all tests (auto-builds debug first)
make test-verbose   # Verbose test output
make format         # Run clang-format on all sources
make check          # Run clang-tidy static analysis
```

Cross-compilation targets: `make cross-aarch64`, `make cross-windows`, `make cross-wasm`.

## Running Tests

Tests use Google Test and are native-only (cross builds skip tests).

```bash
make test                                                        # All standard tests
make test-verbose                                                # With verbose output
meson test -C build/builddir-debug -v                           # Manual verbose
meson test -C build/builddir-debug --suite live                 # Live RSS tests (needs internet, 120s timeout)
```

Test files in `tests/`: `RssManagerTest.cpp`, `AssetManagerTest.cpp`, `ConsoleLoggerTest.cpp`, `FileReaderTest.cpp`, `RssFeedLiveTest.cpp`.

## Architecture

**Application lifecycle:**

```
Application.cpp → UtilsFactory (creates ApplicationContext)
                → ServiceContainer (dependency injection)
                → Orchestrator<ILifeCycle> (manages threads/signals)
                    └── DiscordBot (the ILifeCycle implementation)
```

1. `Application.cpp` builds an `ApplicationContext` (logger, asset manager, platform info, custom strings), registers services into `ServiceContainer`, creates `Orchestrator<ILifeCycle>`, adds `DiscordBot`, then blocks on `sigwait(SIGINT/SIGTERM)` for graceful shutdown.
2. `Orchestrator` calls `initialize()` + `start()` on each registered `ILifeCycle` in threads and handles teardown.
3. `ServiceContainer` is a type-erased registry — services are registered by type and retrieved by the same type token.

**DiscordBot** (`src/lib/DiscordBot/`) is the core: it holds a `dpp::cluster`, registers slash commands (`/rss_add`, `/rss_list`, `/rss_remove`, `/rand`, `/rename_channel`, `/btcprice`, `/ethprice`), and schedules three periodic background tasks:
- RSS refetch every `FETCH_INTERVAL_SECONDS` (3600 s)
- Channel rename every `RENAME_INTERVAL_SECONDS` (7200 s)
- BTC/ETH price update every `BTCPRICE_INTERVAL_SECONDS` (300 s), with EMA-based trend detection (EMA_SHORT=3, EMA_LONG=12)

**RssManager** (`src/lib/Rss/`) parses RSS and ATOM feeds with tinyxml2, decodes HTML entities (numeric, hex, named), persists feed URLs and seen-item hashes to JSON files, and deduplicates items across runs. It filters per Discord channel.

**Supporting libraries** (`src/lib/`):
- `EmojiModuleLib` — loads `assets/emoji-test.txt`, returns random emoji
- `NameGen` — random channel names from `assets/adjectives.txt` + `assets/nouns.txt`
- `Crypto/CryptoUtils` — fetches BTC/ETH prices from Binance Klines API via libcurl
- `Utils/` — `ILogger`/`ConsoleLogger`/`NullLogger`, `IAssetManager`/`AssetManager`, `FileReader`/`FileWriter`, JSON serialization, platform detection (Unix/Windows/Emscripten)

## Key Constants (DiscordBot)

| Constant | Value | Meaning |
|---|---|---|
| `MAX_DISCORD_MESSAGE_LENGTH` | 2000 | Discord message cap |
| `LOG_CHANNEL_ID` | 1454003952533242010 | Hardcoded log channel |
| `FETCH_INTERVAL_SECONDS` | 3600 | RSS poll interval |
| `RENAME_INTERVAL_SECONDS` | 7200 | Channel rename interval |
| `BTCPRICE_INTERVAL_SECONDS` | 300 | Price update interval |

## Key Dependencies

- **dpp** — D++ Discord bot library
- **tinyxml2** — RSS/ATOM XML parsing
- **libcurl** — HTTP for RSS and crypto API
- **nlohmann_json** — JSON (header-only)
- **fmt** — String formatting
- **openssl**, **zlib** — SSL and compression

## Code Style

LLVM-based clang-format (see `.clang-format`). Clang-tidy enforces `performance-*`, `readability-*`, `modernize-*` (excluding `modernize-use-trailing-return-type`). Run `make format` before committing and `make check` to catch tidy warnings.
