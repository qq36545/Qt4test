# Repository Guidelines

## Project Structure & Module Organization
- Core app entry and shell: `main.cpp`, `mainwindow.h`, `mainwindow.cpp`.
- UI feature widgets live in `widgets/` (e.g., `videogen.*`, `imagegen.*`, `configwidget.*`, `aboutwidget.*`).
- Network/API logic is in `network/` (task polling, upload, video/image API adapters).
- Local persistence is in `database/` (`dbmanager.*`).
- Static assets and app icons are under `resources/`, with Qt resource mapping in `resources.qrc`.
- Stylesheets are in `styles/` (`glassmorphism.qss`, `light.qss`); docs and release notes are in `docs/` and `releases/`.

## Build, Test, and Development Commands
- Configure: `cmake -S . -B build` — generate build files from `CMakeLists.txt`.
- Build: `cmake --build build -j4` — compile the Qt app.
- Run (macOS): `open build/ChickenAI.app`.
- Quick local run script: `bash run.sh` (if environment is already prepared).
- Release preflight: `bash scripts/release_preflight_check.sh` — checks versioning and release readiness.

## Coding Style & Naming Conventions
- Language: C++17 + Qt 6; keep implementation split as `*.h`/`*.cpp` pairs.
- Indentation: 4 spaces; avoid tabs; keep braces and spacing consistent with nearby code.
- Naming: classes use `PascalCase`; methods/variables use `camelCase`; constants/macros use `UPPER_SNAKE_CASE`.
- Keep UI strings user-facing and clear; centralize reusable style/resource paths.
- Prefer minimal, focused changes; do not introduce new dependencies unless necessary.

## Testing Guidelines
- No full automated test suite is currently enforced; use targeted validation.
- For UI/API changes, verify app startup, page switching, generation flows, and history/polling behavior.
- Validate build before PR: `cmake --build build` must pass.
- If adding tests later, place them under `tests/` and name files `test_<module>.cpp`.

## Commit & Pull Request Guidelines
- Use clear, scoped commits, e.g., `fix(network): retry image upload on timeout`.
- Keep each commit focused on one concern (UI, network, DB, or build).
- PRs should include: purpose, key changes, manual verification steps, and screenshots for UI updates.
- Link related issue/docs when applicable (e.g., files under `docs/plans/`).

## Security & Configuration Tips
- Never commit real API keys or tokens; keep secrets in local config/environment.
- Review `WINDOWS_BUILD.md` and installer settings (`installer.nsi`) before cross-platform packaging.
