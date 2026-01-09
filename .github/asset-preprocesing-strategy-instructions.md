# LegendsBelow – Asset Processing Strategy

Purpose: describe the automated steps, validation, and outputs required to process gameplay JSON assets into runtime-ready binary data files that can be loaded dynamically without requiring solution recompilation.

Goals:
- Validate and normalize authoring JSON (schema, IDs, references).
- Produce compact, versioned binary assets for fast runtime load.
- Enable rapid iteration: data changes should not require C++ recompilation.
- Generate minimal C++ headers for strongly-typed ID enums only.
- Support hot-reloading of assets during development.

Principles:
- Data-driven: keep canonical data in JSON files, load binary assets at runtime.
- Fast iteration: asset changes should be testable without rebuilding the solution.
- Idempotent: processing must produce the same output given same inputs.
- Fail-fast: strict validation with clear error messages to prevent loading broken assets.
- Backwards-compatible: support asset versioning and migration helpers.
- Runtime flexibility: assets loaded from external files, not compiled into executable.

Pipeline overview (high-level):
1. `validate` — run JSON schema and cross-reference checks.
2. `normalize` — canonicalize fields, enforce ID naming, fill defaults.
3. `pack` — produce binary-packed data files for runtime loading.
4. `bundle` — compress and sign package for distribution (optional).
5. `deploy` — copy assets to game's asset directory for runtime access.

Suggested file layout and conventions:
- Source: `data/*.json` (human-editable canonical assets).
- Intermediate: `build/normalized/*.json` (normalized, validated JSON).
- Binary output: `Assets/Data/<asset_version>/weapons.bin` (runtime binary format) and accompanying `manifest.json`.
- Generated code: `generated/assets_ids.h` (ID enums only, no data embedded).
- Runtime location: Game loads binary assets from `Assets/Data/` directory at startup or on-demand.

Validation rules (minimum):
- Use JSON Schema files stored in `.github/schemas/` for `spells.json`, `weapons.json`, `armour.json`.
- Enforce unique `id` across each domain and optionally globally.
- Cross-reference checks: ensure `appliedStatusIds` exist in a `status` registry or `status_*.json`.
- Field type and range checks (numeric ranges for `manaCost`, `requiredLevel`).

Normalization rules:
- IDs: `lower_snake_case` with prefixes: `spell_`, `weapon_`, `armor_` and suffixes `_l#`/`_t#` as appropriate.
- Fill missing optional fields with documented defaults (e.g., `cooldownSeconds = 0.0`).
- Order object properties in deterministic order for stable diffs.

Binary packing details:
- Binary format: a simple header + indexed records for efficient runtime loading. Header contains:
  - `magic` (4 bytes), `version` (u16), `recordCount` (u32), `stringTableOffset` (u32)
- Records: fixed-layout numeric fields + offsets into a string table to avoid repeated strings.
- Use little-endian and align records to 4 bytes for fast memory mapping.
- Provide both single-file pack and per-domain packs (e.g., `spells.bin`, `weapons.bin`).
- Runtime loader can memory-map files for zero-copy access to reduce startup time.

Generated C++ helpers (minimal):
- `generated/assets_ids.h`:
  - `enum class SpellId : uint32_t { spell_evocation_arcane_bolt_l1 = 0, ... };`
  - `enum class WeaponId : uint32_t { weapon_sword_... = 0, ... };`
  - Hash constexpr functions for string-to-ID lookups during development.
- NO generated registry data: all asset data loaded from binary files at runtime.
- Asset loader API in engine provides type-safe access using the enum IDs.

Developer tools and scripts:
- `scripts/validate_spells.py` (example) — add more domain validators.
- Add `scripts/normalize_assets.py` to canonicalize and write to `build/normalized/`.
- Add `scripts/pack_assets.py` to produce binary `.bin` files and `manifest.json`.

CI integration:
- Add a CI job step `assets:validate` that runs validators on changed assets.
- Add `assets:pack` job that produces binary assets and uploads them as artifacts.
- Generate ID enums only when asset IDs actually change (not on every data change).
- Fail CI when validation fails, but allow iteration on data values.
- Optional: separate fast-iteration builds (skip validation) from release builds (full validation).

Versioning and migrations:
- Store an `assetVersion` in the `manifest.json` produced by the pipeline.
- When schema changes are introduced, provide a migration script under `.github/migrations/v{N}_to_v{N+1}.py` that can upgrade JSON sources or produce compatibility metadata.

Testing and QA:
- Unit tests for validators and packer code (Python `pytest` or C++ tests).
- Sample runtime loader test that loads the produced `.bin` and verifies a few known IDs and numeric fields.
- Smoke test in CI that verifies generated enums compile (optional step in C++ build matrix).

Performance and size considerations:
- Deduplicate repeated strings via a string table in the binary pack.
- Use compression (zstd) for distribution artifacts; keep runtime format uncompressed for fast mmap.
- Provide both compressed distribution and uncompressed runtime-ready files.

Security and integrity:
- Produce SHA256 checksums for all produced artifacts and include in `manifest.json`.
- Optionally sign artifacts with a CI-managed key.

Developer workflow (local - fast iteration):
1. Edit `data/*.json`.
2. Run `python scripts/validate_spells.py` (optional for quick validation).
3. Run `python scripts/pack_assets.py` to update binary files in `Assets/Data/`.
4. Game automatically reloads assets on next startup (or via hot-reload if implemented).
5. Test immediately without C++ recompilation.
6. Commit source JSON changes. Binary files can be committed or generated by CI.

Alternative workflow (validation-heavy):
1. Edit `data/*.json`.
2. Run `python scripts/validate_all.py` for comprehensive validation.
3. Run `python scripts/normalize_assets.py` if normalization is needed.
4. Run `python scripts/pack_assets.py`.
5. Test game changes.

Hot-reload workflow (advanced):
1. Edit `data/*.json`.
2. File watcher automatically runs packer when JSON files change.
3. Game detects asset file changes and reloads in real-time.
4. Immediate feedback without restart or recompilation.

Examples and reference commands (CLI):
```
# quick validation during development
python scripts/validate_spells.py
python scripts/validate_items.py

# fast binary packing for immediate testing
python scripts/pack_assets.py --input data --out Assets/Data

# full pipeline with normalization
python scripts/normalize_assets.py --input data --out build/normalized
python scripts/pack_assets.py --input build/normalized --out Assets/Data

# generate ID enums when asset IDs change
python scripts/generate_ids.py --input data --out generated/assets_ids.h

# file watching for auto-rebuild (development)
python scripts/watch_assets.py --input data --out Assets/Data
```

Runtime asset loading:
- Game engine initializes asset manager on startup.
- Asset manager loads binary files from `Assets/Data/` directory.
- Each asset type (spells, weapons, etc.) has dedicated loader.
- Assets remain in memory for fast access during gameplay.
- Optional: implement asset hot-reloading by monitoring file timestamps.

Notes:
- Keep the canonical JSON files small and human-readable; the binary packs are the runtime artifacts.
- Generated C++ headers contain only ID enums, not asset data, minimizing recompilation needs.
- Binary assets are loaded at runtime, enabling rapid iteration and data-driven development.
- Consider implementing asset hot-reload for even faster iteration during development.
- Asset validation can be optional during development, mandatory in CI/release builds.

Performance considerations:
- Binary format optimized for memory mapping and cache efficiency.
- String deduplication reduces memory footprint.
- Asset loading can be asynchronous or on-demand for large datasets.
- Consider asset dependency resolution for complex inter-asset references.

Development best practices:
- Use file watchers to automatically repack assets during development.
- Implement asset validation in IDE/editor extensions for immediate feedback.
- Keep development and release asset processing pipelines consistent.
- Version assets independently from code to allow content updates without code changes.

Maintenance:
- Document schema changes in `.github/schemas/CHANGELOG.md`.
- Rotate migration scripts and keep them minimal.

Contact/ownership:
- Assign asset pipeline ownership in project docs; this team maintains validators, packers, and migration scripts.
