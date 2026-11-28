# Instructions for coding agents

These instructions apply throughout the Network UPS Tools (NUT) repository.
They supplement, but do not replace, the project's authoritative guidance.

## Read the project guidance

Before editing, read and apply the relevant guidance below. Check the final
diff against those requirements before submission:

- `docs/developers.txt` for coding style, portability, build and contribution
  requirements.
- `docs/developer-guide.txt` for NUT architecture, design and data flow.
- `.github/pull_request_template.md` for the current contribution checklist.
- `SECURITY.md` before investigating or reporting a possible vulnerability.
- `.github/CI.md` before changing GitHub Actions workflows or scripts under
  `.github/`.

Follow more specific guidance in those files if this summary differs from it.

## Work from evidence

- Verify claims against the current source and applicable specifications,
  consulting relevant history where needed. Trace affected callers, consumers
  and data flow, then change the layer which owns the behaviour.
- Reuse established NUT helpers, types, probes and patterns before adding new
  code. Check nearby code for applicable precedent. Check that optional
  features and their detection mechanisms are available before use.

## Keep changes focused and portable

- Keep each change narrow and cohesive. Exclude unrelated cleanup, formatting
  and speculative refactoring.
- Follow affected-file style and the portability requirements in
  `docs/developers.txt`, including plain ASCII in source comments and AsciiDoc
  where ASCII suffices.
- Preserve still-valid behavioral qualifications and rationale when
  restructuring code or documentation. Relocate useful comments with the code
  they explain, and review removed text for lost information.
- When adding, removing or renaming files, or changing templates, update the
  applicable `Makefile.am`, `configure.ac`, distribution lists and generation
  rules. Update source templates rather than generated files unless project
  practice requires a generated artifact to be tracked.
- Update manuals, compatibility data, version markers, `NEWS.adoc`,
  `UPGRADING.adoc` and acknowledgements only where required by the affected
  change. With `NEWS.adoc` note that many changes are grouped by subject,
  such as improvements for a specific driver or tool program, library or
  common ecosystem -- do not introduce dangling paragraphs where they can
  rather be bullet points in an existing list. Be sure to only add entries to
  the upcoming release (topmost chapter in `NEWS.adoc` and `UPGRADING.adoc`).
- Where `aspell` is available, verify documentation changes with
  `make spellcheck-quick`. Note that dictionary defaults are `en_US`,
  so notably many verbs should end with `-ize` (`initialize`, etc.)
- When a release note references the PR being opened, include the expected
  PR number in the initial commit. Just before opening the PR, check the
  latest issue or PR number in the upstream repository (including closed
  items) and add one. Verify the assigned number afterward and correct the
  reference if it differs. This usually saves a commit and another CI run.

## Validate the affected behaviour

- For a bug fix, where practical, show that the same check exposes the
  reported behavior before the change and passes afterward.
- Select checks from the affected compatibility requirements, including older
  supported tools and feature-disabled paths. Apply these requirements to tests
  and build scripts too.
- Run affected tests and builds, and run `make distcheck-light` before
  upstreaming. Include relevant documentation and distribution checks when
  their inputs or metadata change. For documentation changes, run the project
  spellcheck and account for the dictionary versions exercised by CI.
- Report expected compatibility, what was actually tested, material failures
  and untested limitations. Do not claim hardware, platform or runtime
  acceptance from CI jobs which did not exercise it.
- Diagnose the first relevant CI failure before changing code, distinguishing
  the proposed change from baseline or CI-environment failures.
- When introducing test programs or scripts, prefer adding them to the `tests/`
  directory and not into arbitrary locations in the code base. Update needed
  dependency or symlink recipes in `tests/Makefile.am` accordingly. List any
  created files or symlinks in `CLEANFILES`.
- If using `mkdtemp` or similar methods to create test data and configurations,
  be sure to base them under the build directory -- e.g. by exporting `TMPDIR`,
  `TEMP` or `TMP` environment variable(s) from the Makefile goal.
- When adding new items into the automake standard flow as `TESTS += program`
  in a `Makefile.am`, update the nearby `.gitignore` file with a triplet based
  on the program name:
```
/program
/program.log
/program.trs
```

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
