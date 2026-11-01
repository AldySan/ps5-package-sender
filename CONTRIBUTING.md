# Contributing

Thanks for your interest in improving PS5 Package Sender. Bug reports, fixes,
and well-scoped features are all welcome.

## Getting started

1. Fork the repository and clone your fork.
2. Create a branch off `dev` (the default branch and the target of every pull
   request).
3. Build and run the test suite to make sure your baseline is clean.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Requirements: Qt 6.5 or newer, CMake 3.21+, and a C++17 compiler. Tests run on
Linux; CI builds and tests Windows and Linux on every pull request.

## What belongs where

* **A bug report** — one clear reproduction, the app version, your OS, and what
  you expected instead. Use the bug report template.
* **A feature request** — the problem you are trying to solve first, the
  proposed solution second. Use the feature request template.
* **A pull request** — one focused change. If a fix and a refactor are both
  needed, send two pull requests.

## Pull request conventions

* Target `dev`, never `main`. `main` only tracks releases.
* Keep each pull request focused on a single concern; unrelated drive-by
  changes make review harder and will be asked to be split out.
* Branch names: `fix/short-description`, `feat/short-description`,
  `docs/short-description`, `test/short-description`.
* Commits: short, professional, imperative subject line in English, for example
  `fix: rescan library after resume`. Do not rewrite history once review has
  started.
* New behavior that a user can observe should come with a test when practical.
* Comments should explain *why* something is done, not narrate *what* the code
  already says. Silence is preferred over noise.
* Keep the surface area in mind: the app supports Windows and Linux only, and
  must stay portable (no installation step, no hard-coded absolute paths).

## Code layout

| Path | Contents |
| --- | --- |
| `src/core` | transfers, receiver HTTP client, file server, discovery, library |
| `src/db` | SQLite storage layer |
| `src/app` | application controller, QML image provider |
| `qml` | Qt Quick interface |
| `tests` | console test binaries run by CTest |

## Licensing

By contributing, you agree that your contributions are licensed under the
GPL-3.0 license that covers this repository.
