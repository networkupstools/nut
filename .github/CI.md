# Working on CI (GitHub Actions)

This covers the GitHub Actions workflows and helper scripts under `.github/`.

## Checks before pushing changes to `.github/`

- **zizmor**, with the same version and settings as the "GHA-06: zizmor"
  check (see `.github/workflows/06-zizmor.yml`):

      pipx run zizmor==1.25.2 --min-severity medium .github/

  Record deliberate exceptions, with the reason, in `.github/zizmor.yml`.

- **actionlint** on the workflows, e.g.:

      docker run --rm -v "$PWD:/repo" -w /repo rhysd/actionlint:1.7.7 .github/workflows/*.yml

  Some shellcheck style notes on existing code are known; do not add new ones.

- **`tsc --strict`** for `.github/scripts/*.js`, see the header of
  `.github/scripts/pr-comments.js` for the command. There is no `package.json`
  on purpose: NUT is a C project, so the TypeScript tooling is installed ad hoc
  (`npm install --no-save` in a scratch directory).

## Rules

- **Never trust user-controlled input:** PR titles and descriptions, branch
  names, commit messages, comments, and anything produced by PR builds
  (artifacts and their names). Do not expand `${{ ... }}` of such values into
  `run:` or `script:`; pass them via `env:` and treat them as data.
- **Least privilege:** top-level `permissions: {}`, and each job only gets the
  permissions it needs. PR code runs only on `pull_request`, with a read-only
  token. Jobs with write permissions (e.g. on `workflow_run`) never check out
  or run PR code. Use `persist-credentials: false` on checkouts.
- **Avoid `pull_request_target`** if possible. If it is really needed, follow
  [Securely using pull_request_target](https://docs.github.com/en/actions/reference/security/securely-using-pull_request_target).
- **Caches:** only trusted runs (master branch pushes, scheduled runs) save
  caches that master branch builds restore.
- **No new third-party actions,** unless their benefit clearly outweighs the
  risk of a compromised action; prefer `run:` steps or `actions/github-script`.
  If one is needed, pin it to a full commit SHA with the version in a comment
  (enforced by zizmor, updated by Dependabot).

## References

- [Secure use reference](https://docs.github.com/en/actions/reference/security/secure-use)
- [Securely using pull_request_target](https://docs.github.com/en/actions/reference/security/securely-using-pull_request_target)
