# Contributing to contribusage

Thanks for your interest! Bug reports, ideas, fixes, docs and features are all welcome. You don't need to ask before you start: open an issue or a pull request, whichever fits.

Everyone taking part follows the [Code of Conduct](CODE_OF_CONDUCT.md).

## Report a bug or suggest an idea

[Open an issue](https://github.com/kuehn-lars/contribusage/issues/new) and describe what you expected and what happened. For bugs, Settings → Advanced → **Copy Diagnostics** gives us the versions and each provider's state; it never contains a token, but give it a quick read before you paste it.

If the limits suddenly disappear after a Claude Code update, its `/usage` output has probably changed. The output of `claude -p "/usage"`, with anything personal removed, helps a lot.

Found a security problem? Please report it privately, as described in the [security policy](SECURITY.md), instead of in an issue.

## Build and run

You need Arch Linux with KDE Plasma 6. The commands are in [AGENTS.md § Commands](AGENTS.md#commands) and in the README's "Build from source". Run the tests and the format check before opening a pull request.

## Find your way around

- `src/applet/` is the Plasma widget: QML views, their wiring and C++ view models.
- `src/core/` holds the logic (`contribusage_core`), and `tests/` its Qt Test suites. The Swift code the port comes from is at the git tag `macos-reference` (commit `2603a21` on `main`).
- [`SPEC.md`](SPEC.md) describes what the app should do, with numbered requirements and a task list in §17. It's long; search it rather than reading it whole.
- [`llm-wiki/`](llm-wiki/) explains how it's built and why: a page per module, the design decisions, research notes. [`overview.md`](llm-wiki/overview.md) is a good start.

## A few rules

These keep the app safe and trustworthy, so pull requests that break them can't be merged:

- **Hands off other tools' credentials.** The app never reads Claude Code's login and never calls `api.anthropic.com` or `claude.ai`; it only runs the `claude` CLI. [SPEC §2.1](SPEC.md#21-why-principle-2-matters-claude-code) explains why.
- **No telemetry**, and the GitHub token stays in KWallet and out of logs.
- **No personal data in the repository:** no tokens, real transcripts or unscrubbed tool output in fixtures, issues or docs.
- **Builds stay clean:** C++23, `-Werror`, clang-format clean.

## Pull requests

- Branch from `main` and open the pull request against it. Draft pull requests are welcome if you want early feedback.
- Add or update tests for the logic you change. New tests use Qt Test (`QTest`) and are registered in `tests/CMakeLists.txt`.
- Describe what you changed and how you tested it; for UI changes, a screenshot helps.
- If your change completes a task from SPEC §17, tick its box; if it changes how something should behave, update the spec too.

CI runs the build, the tests, the format check and a check of the wiki. One thing it may ask of you:

- **The wiki check** may flag a page under `llm-wiki/` as stale because it describes code you changed. Update the page, or just its `updated:` date if it still holds. Not sure? Leave it, and we'll take care of it before merging.

## Using a coding agent

Go ahead, the project is built that way. Agents find their instructions in [`AGENTS.md`](AGENTS.md) (Claude Code reads it through `CLAUDE.md`), including how to keep the wiki up to date. Review what your agent writes as if you had typed it yourself.

## Bigger changes

Support for another AI coding tool starts in the spec: it has to pass the provider gate in [SPEC §2.4](SPEC.md#24-provider-gate), which checks that the tool's data can be read through official interfaces only. Opening an issue first saves you work here, as it does for anything that changes the design.

## License

Contributions are released under the project's [MIT License](LICENSE).
