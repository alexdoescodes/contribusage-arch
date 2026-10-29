---
type: architecture
status: active
updated: 2026-10-30
tracks: [.github, src, tests, CMakeLists.txt, LICENSE]
tags: [structure]
---
# Repository map

Every top-level entry of the repository and of the vault, and what it is for. `wiki.sh lint` compares the first code block below with the files on disk (the repository root and the vault's top level), so the tree cannot drift unnoticed. Each build target has its page under `modules/`. The macOS layout of SPEC §15.2 is at the git tag `macos-reference` (commit `2603a21` on `main`).

```text
contribusage/
├── .claude/settings.json   Claude Code hooks that run the wiki protocol
├── .clang-format           clang-format settings (LLVM base, 4 spaces, 120 columns)
├── .github/               ISSUE_TEMPLATE/ (issue forms: bug, /usage output change, feature; security link), pull_request_template.md; workflows/ CI: build with -Werror, Qt Test suites and clang-format in an Arch Linux container, wiki lint and its tooling test
├── .gitignore
├── AGENTS.md               agent protocol: orient, work, record; publish policy; hard rules
├── CLAUDE.md               imports AGENTS.md for Claude Code
├── CMakeLists.txt          the CMake project: KDE ECM settings, -Werror, src/ and tests/
├── CODE_OF_CONDUCT.md      Contributor Covenant 2.1; reports through GitHub's Report content to the repository admins
├── CONTRIBUTING.md         the contributor guide: build, rules, pull requests
├── LICENSE                 MIT
├── docs/                   assets/: README art: header SVGs, screenshots rendered off-screen from the built applet, the social preview
├── README.md
├── SECURITY.md             supported versions, private vulnerability reporting, what counts as a vulnerability
├── packaging/              arch/: the PKGBUILD (plasma6-applets-contribusage) that builds this checkout, and its .SRCINFO
├── src/                    core/: the static logic library contribusage_core (Qt6::Core only: models, persistence, schedule, layout, planner); claudecode/: the Claude Code provider, contribusage_claudecode (parser, locator, probe, limits job, transcripts and activity job, views); github/: GitHub contributions, contribusage_github; platform/: the live seams, contribusage_platform (QNetworkAccessManager transport, KWallet secret store, desktop notifier, suspend/network/power conditions); applet/: the Plasma widget org.contribusage.widget (QML and its C++ view models)
├── SPEC.md                 the contract: requirements
├── tests/                  a few Qt Test suites run by CTest: parsing, persistence, schedule, GitHub client, applet smoke tests; support/ (fakes), packaging/ (the install-layout check)
└── llm-wiki/               the memory: this Obsidian vault
    ├── .obsidian/          shared vault settings (only app.json is committed)
    ├── _templates/         the session-file template
    ├── architecture/       cross-cutting structure as built
    ├── decisions/          ADRs, NNNN-slug.md
    ├── guides/             how-tos
    ├── index.md            catalog of every page
    ├── log.md              chronological change log
    ├── modules/            one page per build target
    ├── overview.md         hub: what, how the pieces fit
    ├── research/           measured findings
    ├── sessions/           session files, local only (gitignored)
    └── tools/              wiki.sh and its regression test
```

Folders named in `AGENTS.md` but not yet present (`concepts/`, `raw/`) are created with their first file.
