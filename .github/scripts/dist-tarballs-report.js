// @ts-check
/*
 * Report results of the dist tarballs build of the "GHA-01: Tarballs"
 * workflow (.github/workflows/01-make-dist.yml): find and clean up the
 * tarball artifacts, and report a link to them as a PR comment, in the
 * job summary and as a commit status. Loaded by its actions/github-script
 * steps like this:
 *
 *   const report = require('./.github/scripts/dist-tarballs-report.js');
 *   await report.someFunction({ github, context, core });
 *
 * Those steps run with write permissions (also for pull_request_target
 * events), so this file is checked out from the trusted default branch
 * there, never from the PR source. Values from the workflow are passed
 * in via environment variables (see "Env:" below), not expanded into
 * script code. See pr-comments.js for how to type-check this.
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
 * URL of the current workflow run.
 * @param {GitHubArgs['context']} context
 * @returns {string}
 */
function runUrl(context) {
	const { owner, repo } = context.repo;
	return `${context.serverUrl}/${owner}/${repo}/actions/runs/${context.runId}`;
}

/**
 * Post the PR comment saying that tarballs are being prepared.
 * Env: COMMIT_REF
 * @param {GitHubArgs} args
 */
async function commentPreparing({ github, context }) {
	await prComments.upsertPrComment({ github, context }, COMMENT_MARKER,
		`Preparing a ZIP file with standard source tarball and another tarball with pre-built docs for commit ${env('COMMIT_REF')} ...`,
		OLD_COMMENT_MARKERS);
}

/**
 * Find the tarballs artifact uploaded by the "Worker" job of this run,
 * and set step outputs "artifact_url", "real_artifact_url", "artifact_id".
 * Env: ARTIFACT_NAME
 * @param {GitHubCoreArgs} args
 */
async function findArtifact({ github, context, core }) {
	const { owner, repo } = context.repo;
	const name = env('ARTIFACT_NAME');
	const res = await github.rest.actions.listWorkflowRunArtifacts(
		{ owner, repo, run_id: context.runId, name });
	const artifact = res.data.artifacts[0];
	if (!artifact) {
		core.setFailed(`Artifact ${name} not found in this workflow run`);
		return;
	}
	const url = `${runUrl(context)}/artifacts/${artifact.id}`;
	core.setOutput('artifact_url', url);
	core.setOutput('real_artifact_url', url);
	core.setOutput('artifact_id', `${artifact.id}`);
	console.log(`Detected artifact ${artifact.id} from this workflow run ${context.runId}`);
}

/**
 * Delete older artifacts with the same name (from earlier runs for this
 * feature branch or PR), keeping the one from this run.
 * Env: ARTIFACT_NAME, ARTIFACT_ID
 * @param {GitHubArgs} args
 */
async function deleteOldArtifacts({ github, context }) {
	const { owner, repo } = context.repo;
	const name = env('ARTIFACT_NAME');
	const keep = env('ARTIFACT_ID');
	const artifacts = await github.paginate(github.rest.actions.listArtifactsForRepo,
		{ owner, repo, name, per_page: 100 });
	for (const a of artifacts) {
		if (a.name !== name || `${a.id}` === keep) {
			continue;
		}
		console.log(`Deleting artifact ${a.id}`);
		await github.rest.actions.deleteArtifact({ owner, repo, artifact_id: a.id });
	}
}

/**
 * Update the PR comment with the tarball link, or with failure details.
 * Env: ARTIFACT_NAME, ARTIFACT_URL, COMMIT_REF, WORKER_RESULT
 * @param {GitHubArgs} args
 */
async function commentResult({ github, context }) {
	const artifactUrl = env('ARTIFACT_URL');
	let body;
	if (artifactUrl) {
		body = `A ZIP file with standard source tarball and another tarball with pre-built docs for commit ${env('COMMIT_REF')} is temporarily available: [${env('ARTIFACT_NAME')}](${artifactUrl}).`;
		if (env('WORKER_RESULT') !== 'success') {
			body += `\n\n:warning: Some checks in the [tarball build workflow run](${runUrl(context)}) failed.`;
		}
	} else {
		body = `:x: Could not prepare the tarballs for commit ${env('COMMIT_REF')}, see the [tarball build workflow run](${runUrl(context)}).`;
	}
	await prComments.upsertPrComment({ github, context }, COMMENT_MARKER, body, OLD_COMMENT_MARKERS);
}

/**
 * Report the tarball link in the job summary, and as a commit status
 * (unlike for check runs created with GITHUB_TOKEN, its target_url is
 * honored, so "Details" in the PR checks list opens the artifact).
 * Env: ARTIFACT_NAME, ARTIFACT_URL, COMMIT_REF, WORKER_RESULT
 * @param {GitHubCoreArgs} args
 */
async function reportSummaryAndStatus({ github, context, core }) {
	const { owner, repo } = context.repo;
	const artifactUrl = env('ARTIFACT_URL');
	const artifactName = env('ARTIFACT_NAME');
	const sha = env('COMMIT_REF');
	await core.summary
		.addHeading('Dist and Docs tarballs', 3)
		.addTable([
			[{ data: 'Item', header: true }, { data: 'Value', header: true }],
			['Commit', sha],
			['Tarballs', artifactUrl ? `<a href="${artifactUrl}">${artifactName}</a>` : 'not available'],
			['Build worker', env('WORKER_RESULT') || 'not started'],
		])
		.write();
	await github.rest.repos.createCommitStatus({
		owner, repo, sha,
		context: STATUS_CONTEXT,
		state: artifactUrl ? 'success' : 'failure',
		target_url: artifactUrl || runUrl(context),
		description: artifactUrl
			? `Dist and Docs tarballs for ${sha.substring(0, 9)}`
			: `No tarballs for ${sha.substring(0, 9)}, see the workflow run`,
	});
}

module.exports = {
	commentPreparing,
	commentResult,
	deleteOldArtifacts,
	findArtifact,
	reportSummaryAndStatus,
};
