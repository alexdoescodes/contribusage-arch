# Domain Docs

How the engineering skills should consume this repo's domain documentation when exploring the codebase. This repo is **single-context**. Its domain docs live in the `llm-wiki/` vault and in `SPEC.md`, not in `GLOSSARY.md` or `docs/adr/`.

## Before exploring, read these

- **`llm-wiki/index.md`**: one line per vault page; pick the pages the task touches.
- **`llm-wiki/concepts/glossary.md`**: the project's vocabulary, once it exists.
- **`llm-wiki/decisions/`**: read the ADRs that touch the area you're about to work in. `ADR-NNN` in the spec is `decisions/0NNN-…`.
- **`SPEC.md`**: the contract. Grep it for requirement IDs (`FR-`, `NFR-`, `US-`) and section numbers rather than reading it whole.

If the glossary doesn't exist yet, **proceed silently**. Don't flag its absence, and don't suggest creating it upfront. The `/domain-modeling` skill (reached via `/grill-with-docs` and `/improve-codebase-architecture`) creates it lazily when terms actually get resolved.

## Where new domain docs go

These follow the vault's page rules in `AGENTS.md`, and `llm-wiki/tools/wiki.sh lint` must stay clean:

- **Glossary**: `llm-wiki/concepts/glossary.md`, with frontmatter `type: concept` and a line in `llm-wiki/index.md`. Creating it also creates `concepts/`, which goes into `llm-wiki/architecture/repo-map.md`.
- **ADRs**: `llm-wiki/decisions/NNNN-slug.md`, from `llm-wiki/_templates/decision.md`, with `aliases: [ADR-NNN]` and a line in `llm-wiki/index.md`.

## Use the glossary's vocabulary

When your output names a domain concept (in an issue title, a refactor proposal, a hypothesis, a test name), use the term as defined in the glossary. Don't drift to synonyms the glossary explicitly avoids.

If the concept you need isn't in the glossary yet, that's a signal: either you're inventing language the project doesn't use (reconsider) or there's a real gap (note it for `/domain-modeling`).

## Flag ADR conflicts

If your output contradicts an existing ADR, surface it explicitly rather than silently overriding:

> _Contradicts ADR-005 (JSON file persistence), but worth reopening because…_
