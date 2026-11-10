# Contributing to PS5 Package Sender

Thanks for wanting to help. Bug reports, fixes, documentation improvements
and well-scoped features are all welcome, and first-time contributors are
very much invited. If you get stuck anywhere in this guide, open an issue
and ask; that counts as contributing too.

## Ways to help

- **Report bugs.** When something breaks, the
  [bug report template](.github/ISSUE_TEMPLATE/bug_report.md) lists the
  details that make a report actionable.
- **Suggest improvements.** Describe the problem you are trying to solve
  first; the [feature request template](.github/ISSUE_TEMPLATE/feature_request.md)
  shows how.
- **Start small.** A typo fix or a small failing test is a fine first
  contribution, and a good way to learn the workflow before taking on
  something bigger.

## What you need

- A GitHub account and a working `git`.
- Qt 6.5 or newer (Quick, QuickControls2, Sql, Network, Concurrent, Svg),
  CMake 3.21+, and a C++17 compiler. On Linux, your distribution's Qt 6
  development packages cover this.
- A jailbroken PS5 helps for end-to-end checks, but it is not required:
  the test suite exercises the core logic on your PC.

## Get the code

1. Fork the repository on GitHub, then clone your fork:

   ```sh
   git clone https://github.com/<your-name>/ps5-package-sender.git
   cd ps5-package-sender
   ```

2. Keep a link to the original repository so you can pull updates:

   ```sh
   git remote add upstream https://github.com/AldySan/ps5-package-sender.git
   ```

3. Create a branch off `dev`. It is the default branch and the target of
   every pull request:

   ```sh
   git checkout -b fix/short-description origin/dev
   ```

Branch names follow one pattern: `fix/…`, `feat/…`, `docs/…` or `test/…`
followed by a short description.

## Build and test

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

The app binary lands in `build/bin/ps5-package-sender`. Tests run on
Linux; CI builds and tests Windows and Linux on every pull request, so a
green local run before you push saves everyone time.

## Finding your way around

| Path | Contents |
| --- | --- |
| `src/core` | transfers, receiver HTTP client, file server, discovery, library, update checks |
| `src/db` | SQLite storage layer |
| `src/app` | application controller, QML image provider |
| `qml/pages` | the four screens: library, queue, history, settings |
| `qml/components` | reusable controls: buttons, text fields, toasts |
| `tests` | console test binaries run by CTest |
| `DESIGN.md` | design direction and the accessibility floor |

## Code style

There is no formatter configuration in the repository, so the honest rule
is: match the file you are editing. The patterns already in use:

- Four-space indentation, no tabs. Classes are `PascalCase`, functions and
  variables are `camelCase`, data members carry an `m_` prefix.
- Qt idioms throughout: `QStringLiteral` and `QLatin1String` for string
  literals, `Q_PROPERTY` and `Q_INVOKABLE` on the QML-facing
  `Application` class.
- Every user-facing string in QML is wrapped in `qsTr()`, and UI copy
  avoids em dashes (the copy audit in `anti-slop/` records why).
- Colors, spacing and type sizes come from the `Theme` singleton rather
  than ad-hoc values, and contrast must stay WCAG AA in both themes; see
  [DESIGN.md](DESIGN.md).
- Comments explain *why* something is done, not what the code already
  says. Silence is preferred over noise.
- New behavior a user can observe should come with a test when practical.
  Each test is a small console binary registered in `CMakeLists.txt`.

## Commit messages

Write a short, lowercase, imperative subject in English, with an optional
scope. The history already follows one shape:

```
fix: rescan library after resume
feat(queue): warn when the console lacks room for an install
docs: add community health files
```

Common types: `fix`, `feat`, `docs`, `test`, `refactor`, `chore`. Avoid
rewriting history once review has started.

## Sending a pull request

1. Update your branch with the latest `dev` if it has moved
   (`git fetch upstream && git rebase upstream/dev` or a merge, whichever
   you prefer).
2. Push the branch to your fork and open a pull request targeting `dev`,
   never `main`. `main` only tracks releases.
3. Fill in the pull request template: summary, related issues, type of
   change, how you tested, and the checklist.
4. Keep each pull request focused on a single concern. If a fix and a
   refactor are both needed, send two pull requests; unrelated drive-by
   changes will be asked to be split out.
5. Expect a review, and treat review comments as collaboration, not
   criticism. Everyone involved wants the change to land in good shape.

## Licensing

By contributing, you agree that your contributions are licensed under the
GPL-3.0 license that covers this repository.

All participation is covered by the [Code of Conduct](CODE_OF_CONDUCT.md).
