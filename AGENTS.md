# Instructions for coding agents

These instructions apply throughout the Network UPS Tools (NUT) repository.
They supplement, but do not replace, the project's authoritative guidance.

## Read the project guidance

Before proposing a change, read the parts relevant to it:

- `docs/developers.txt` for coding style, portability, build and contribution
  requirements.
- `docs/developer-guide.txt` for NUT architecture, design and data flow.
- `.github/pull_request_template.md` for the current contribution checklist.
- `SECURITY.md` before investigating or reporting a possible vulnerability.

Follow more specific guidance in those files if this summary differs from it.

## Work from evidence

- Verify claims against the current source and applicable specifications,
  consulting relevant history where needed. Trace affected callers, consumers
  and data flow, then change the layer which owns the behaviour.
- Reuse established NUT helpers, types, probes and patterns before adding new
  code. Check nearby code for applicable precedent.

## Keep changes focused and portable

- Keep each change narrow and cohesive. Exclude unrelated cleanup, formatting
  and speculative refactoring.
- Follow affected-file style and the portability requirements in
  `docs/developers.txt`, including plain ASCII in source comments and AsciiDoc
  where ASCII suffices.
- When adding, removing or renaming files, or changing templates, update the
  applicable `Makefile.am`, `configure.ac`, distribution lists and generation
  rules. Update source templates rather than generated files unless project
  practice requires a generated artefact to be tracked.
- Update manuals, compatibility data, version markers, `NEWS.adoc`,
  `UPGRADING.adoc` and acknowledgements only where required by the affected
  change.

## Validate the affected behaviour

- Run affected tests and builds, and run `make distcheck-light` before
  upstreaming. Include relevant documentation and distribution checks when
  their inputs or metadata change.
- Report expected compatibility, what was actually tested, material failures
  and untested limitations. Do not claim hardware, platform or runtime
  acceptance from CI jobs which did not exercise it.
- Diagnose the first relevant CI failure before changing code, distinguishing
  the proposed change from baseline or CI-environment failures.

## Preserve human responsibility

- A human contributor must review and take responsibility for every proposed
  change and for the right to submit it under the project's licence.
- Do not add a `Signed-off-by` trailer on the contributor's behalf. The human
  contributor must consciously provide the required DCO sign-off for each
  commit.
- Disclose AI or coding-helper use as required by the current pull-request
  template, identifying the exact tool and model actually used where
  available. This does not prescribe a tool or model and does not replace
  human review.
