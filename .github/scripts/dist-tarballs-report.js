// @ts-check
/*
 * Report results of the "GHA-01: Tarballs" workflow (01-make-dist.yml):
 * a PR comment with a link to the tarballs artifact, a commit status, and
 * clean-up of older artifacts. Called by the "GHA-01: Tarballs Report"
 * workflow (01-make-dist-report.yml) when a GHA-01 run is requested or
 * completed:
 *
 *   const report = require('./.github/scripts/dist-tarballs-report.js');
 *   await report.run({ github, context, core });
 *
 * That workflow has write permissions and runs for builds of untrusted
 * PR code, so this file comes from the trusted default branch, and all
 * details of the GHA-01 run (passed via environment variables, see "Env:"
 * below) are treated as data: the PR is looked up via the API and must
 * match the run's head commit, and the artifact name is computed here.
 * See pr-comments.js for how to type-check this.
 */

const prComments = require('./pr-comments.js');

/**
 * @typedef {import('@actions/github-script').AsyncFunctionArguments} AsyncFunctionArguments
 * @typedef {Pick<AsyncFunctionArguments, 'github' | 'context'>} GitHubArgs
 * @typedef {Pick<AsyncFunctionArguments, 'github' | 'context' | 'core'>} GitHubCoreArgs
 */

/** Hidden marker by which our PR comment is found and updated */
const COMMENT_MARKER = '<!-- GHA-01:tarball-url -->';

/** Marker of comments posted earlier by thollander/actions-comment-pull-request */
const OLD_COMMENT_MARKERS = ['<!-- thollander/actions-comment-pull-request "tarball-url" -->'];

/** Name of the commit status with the tarball link */
const STATUS_CONTEXT = 'GHA-01: Tarballs / download link';

/**
 * Read an environment variable set by the workflow step.
 * @param {string} name
 * @returns {string}
 */
function env(name) {
	return process.env[name] || '';
}

/**
 * The open PR whose head is the commit built by the GHA-01 run, if that
 * run was for a pull_request event. Looked up by head owner and branch,
 * as workflow_run events do not list PRs from forks.
 * Env: RUN_EVENT, HEAD_OWNER, HEAD_BRANCH, HEAD_SHA
 * @param {GitHubArgs} args
 * @returns {Promise<number | undefined>}
 */
async function findPr({ github, context }) {
	if (env('RUN_EVENT') !== 'pull_request') {
		return undefined;
	}
	const { owner, repo } = context.repo;
	const pulls = await github.paginate(github.rest.pulls.list,
		{ owner, repo, state: 'open', head: `${env('HEAD_OWNER')}:${env('HEAD_BRANCH')}`, per_page: 100 });
	const pr = pulls.find(p => p.head.sha === env('HEAD_SHA'));
	if (!pr) {
		console.log(`No open PR found with head commit ${env('HEAD_SHA')} (maybe updated since)`);
	}
	return pr?.number;
}

/**
 * Artifact name as set by 01-make-dist.yml: by PR number, or by branch
 * or tag name (with characters not valid in artifact names replaced).
 * Env: HEAD_BRANCH
 * @param {number | undefined} pr
 * @returns {string}
 */
function artifactName(pr) {
	const name = pr ? `PR-${pr}` : env('HEAD_BRANCH');
	return `NUT-tarballs-${name.replace(/[^A-Za-z0-9._+-]/g, '_')}`;
}

/**
 * The tarballs artifact uploaded by the GHA-01 run, if any.
 * Env: RUN_ID
 * @param {GitHubArgs} args
 * @param {string} name
 */
async function findArtifact({ github, context }, name) {
	const { owner, repo } = context.repo;
	const res = await github.rest.actions.listWorkflowRunArtifacts(
		{ owner, repo, run_id: Number(env('RUN_ID')), name });
	return res.data.artifacts[0];
}

/**
 * Delete older artifacts with the same name (from earlier runs for this
 * PR or feature branch), keeping the given one. Not for master or tags.
 * Env: RUN_EVENT, HEAD_BRANCH
 * @param {GitHubArgs} args
 * @param {string} name
 * @param {number} keep - artifact ID
 */
async function deleteOldArtifacts({ github, context }, name, keep) {
	const { owner, repo } = context.repo;
	if (env('RUN_EVENT') !== 'pull_request') {
		if (env('HEAD_BRANCH') === 'master') {
			return;
		}
		try {
			await github.rest.git.getRef({ owner, repo, ref: `tags/${env('HEAD_BRANCH')}` });
			return;
		} catch (e) {
			// Not a tag, so a feature branch
		}
	}
	const artifacts = await github.paginate(github.rest.actions.listArtifactsForRepo,
		{ owner, repo, name, per_page: 100 });
	for (const a of artifacts) {
		if (a.name === name && a.id !== keep) {
			console.log(`Deleting artifact ${a.id}`);
			await github.rest.actions.deleteArtifact({ owner, repo, artifact_id: a.id });
		}
	}
}

/**
 * Handle a requested or completed GHA-01 run.
 * Env: RUN_ACTION, RUN_CONCLUSION, RUN_URL, HEAD_SHA (and those above)
 * @param {GitHubCoreArgs} args
 */
async function run({ github, context, core }) {
	const { owner, repo } = context.repo;
	const sha = env('HEAD_SHA');
	const runLink = `[tarball build workflow run](${env('RUN_URL')})`;
	const pr = await findPr({ github, context });

	if (env('RUN_ACTION') === 'requested') {
		if (pr) {
			// AppVeyor posts new comments for new commits
			await prComments.deletePrCommentsBy({ github, context }, pr, 'AppVeyorBot');
			await prComments.upsertPrComment({ github, context }, pr, COMMENT_MARKER,
				`Preparing a ZIP file with standard source tarball and another tarball with pre-built docs for commit ${sha} ...`,
				OLD_COMMENT_MARKERS);
		}
		return;
	}

	const name = artifactName(pr);
	const artifact = await findArtifact({ github, context }, name);
	const url = artifact ? `${env('RUN_URL')}/artifacts/${artifact.id}` : '';
	if (artifact) {
		await deleteOldArtifacts({ github, context }, name, artifact.id);
	}

	if (pr) {
		let body;
		if (url) {
			body = `A ZIP file with standard source tarball and another tarball with pre-built docs for commit ${sha} is temporarily available: [${name}.zip](${url}).`;
			if (env('RUN_CONCLUSION') !== 'success') {
				body += `\n\n:warning: Some checks in the ${runLink} failed.`;
			}
		} else {
			body = `:x: Could not prepare the tarballs for commit ${sha}, see the ${runLink}.`;
		}
		await prComments.upsertPrComment({ github, context }, pr, COMMENT_MARKER, body, OLD_COMMENT_MARKERS);
	}

	// Unlike for check runs created with GITHUB_TOKEN, the target_url of a
	// commit status is honored: "Details" in the PR checks opens the artifact
	await github.rest.repos.createCommitStatus({
		owner, repo, sha,
		context: STATUS_CONTEXT,
		state: url ? 'success' : 'failure',
		target_url: url || env('RUN_URL'),
		description: url
			? `Dist and Docs tarballs for ${sha.substring(0, 9)}`
			: `No tarballs for ${sha.substring(0, 9)}, see the workflow run`,
	});
	core.info(url ? `Reported ${url}` : 'No tarballs artifact found');
}

module.exports = {
	artifactName,
	run,
};
