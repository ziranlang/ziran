# Changelog

## [0.3.0] - 2026-10-23
### Changed
- Portable executables and libraries now use `.zx`, the `zi2zx` tool, and the `zx` target. Source stays `.zi` and checked intermediate modules stay `.zir`. Rebuild previous `.zib` programs; renaming a file does not convert it.

### Fixed
- Go output no longer needs temporary filesystem space and reports output write failures instead of producing incomplete source files.

## [0.2.0] - 2026-09-28
### Added
- `ziran version` (and `--version`) prints the compiler version.
- Release tags now follow the compiler, so `ref = "v0.2.0"` in `ziran.toml` pins a release.

### Changed
- A clean build of the compiler is much faster and uses at most eight cores.

### Fixed
- Building `ziran` from scratch on older `make` versions no longer fails to link.

Earlier versions were tagged `v0.1.x` without a changelog.
