# Security policy

## Supported versions

There are no releases of the Linux port yet; security fixes land on `main`. Please check that a problem still occurs there before you report it.

## Report a vulnerability

Please report security problems privately, not in a public issue:

1. Open the repository's [**Security** tab](https://github.com/alexdoescodes/contribusage-arch/security).
2. Click **Report a vulnerability** and describe what you found: the version, the steps to reproduce, and what an attacker could do with it.

Only the maintainers see the report. You will get an answer as soon as possible, usually within a week. We'll work on a fix with you in the private advisory, and credit you when it is published, unless you prefer otherwise.

## What counts

contribusage promises a few things about safety (see [SPEC §2](SPEC.md#2-constitution-non-negotiable-principles) and [§14](SPEC.md#14-security-and-privacy)). Anything that breaks one of them is a vulnerability:

- The app reads a coding tool's credentials, such as Claude Code's credential file or its Secret Service or KWallet items, or calls a vendor backend such as `api.anthropic.com` or `claude.ai`.
- The GitHub token leaves KWallet other than in requests to `api.github.com`: into logs, files, diagnostics, the clipboard or another server.
- Prompt or message text from transcripts is kept, logged or sent anywhere.
- The app talks to any server other than `api.github.com`.
- The app runs anything other than what SPEC §14 allows (the resolved `claude` with the arguments in SPEC §8.1.1, and the PATH lookup), or input from a file or another process can change what it runs.
- The app reads files outside the roots SPEC §14 declares, or writes outside its own folder without asking you.
- A downloaded release is not the build CI made from the tagged commit.

## Verify a download

There are no Linux releases yet. Until the package arrives (SPEC §17.8, L-7), build from source.

## Not a vulnerability

The widget runs inside plasmashell with your user's rights and is not sandboxed ([ADR-006](llm-wiki/decisions/0006-no-app-sandbox.md)): it has to run `claude` and read `~/.claude`. That is expected and not a vulnerability.
