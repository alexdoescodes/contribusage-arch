---
type: architecture
status: active
updated: 2026-10-04
tracks: [.github, src, tests, CMakeLists.txt, docs, LICENSE]
tags: [structure]
---
# Repository map

Every top-level entry of the repository and of the vault, and what it is for. `wiki.sh lint` compares the first code block below with the files on disk (the repository root and the vault's top level), so the tree cannot drift unnoticed. The source layout of the Linux port is in its design (`docs/superpowers/specs/2026-10-04-linux-plasmoid-port-design.md` §1); each build target has its page under `modules/`. The macOS layout of SPEC §15.2 is at the git tag `macos-reference`.

```text
contribusage/
├── .claude/settings.json   Claude Code hooks that run the wiki protocol
├── .clang-format           clang-format settings (LLVM base, 4 spaces, 120 columns)
├── .github/               ISSUE_TEMPLATE/ (issue forms: bug, /usage output change, feature; security link), pull_request_template.md; workflows/ CI: package tests (warnings as errors), app build, architecture checks, String Catalog sync check, wiki lint, the GitHub release on a version bump
├── .gitignore
├── AGENTS.md               agent protocol: orient, work, record; publish policy; hard rules
├── CLAUDE.md               imports AGENTS.md for Claude Code
├── CMakeLists.txt          the CMake project: KDE ECM settings, -Werror, src/ and tests/
├── CODE_OF_CONDUCT.md      Contributor Covenant 2.1; reports through GitHub's Report content to the repository admins
├── CONTRIBUTING.md         the contributor guide: build, rules, pull requests
├── LICENSE                 MIT
├── docs/                   superpowers/: specs/ (designs) and plans/ (one implementation plan per milestone) for the Linux plasmoid port; agents/: configuration the engineering agent skills read (issue tracker, triage labels, domain docs; see AGENTS.md § Agent skills); assets/: README art: header SVGs (light and dark) with the icon drawn flat, screenshots, the 1280×640 social preview (SVG source and the PNG uploaded under Settings → Social preview)
├── README.md
├── SECURITY.md             supported versions, private vulnerability reporting, what counts as a vulnerability
├── src/                    core/: the static logic library contribusage_core (Qt6::Core only); applet/: the Plasma widget org.contribusage.widget (QML and its C++ view models)
├── SPEC.md                 the contract: requirements, tasks, research items
├── tests/                  Qt Test suites run by CTest: core/, applet/
└── llm-wiki/               the memory: this Obsidian vault
    ├── .obsidian/          shared vault settings (only app.json is committed)
    ├── _templates/         one template per page type
    ├── architecture/       cross-cutting structure as built
    ├── decisions/          ADRs, NNNN-slug.md
    ├── guides/             how-tos
    ├── index.md            catalog of every page
    ├── log.md              chronological change log
    ├── modules/            one page per build target
    ├── overview.md         hub: what, how the pieces fit
    ├── research/           findings for the spec's R-items
    ├── sessions/           session files, local only (gitignored)
    ├── sources/            summaries of external sources
    └── tools/              wiki.sh and its regression test
```

Folders named in `AGENTS.md` but not yet present (`concepts/`, `raw/`) are created with their first file.
