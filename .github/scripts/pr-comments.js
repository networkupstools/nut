// @ts-check
/*
 * Generic helpers to manage comments on a pull request, for use in
 * actions/github-script steps, e.g.:
 *
 *   const prComments = require('./.github/scripts/pr-comments.js');
 *   await prComments.deletePrCommentsBy({ github, context }, 123, 'AppVeyorBot');
 *
 * Steps using this need "pull-requests: write" permission, so this file
 * must be checked out from the trusted default branch there, never from
 * a PR source.
 *
 * For type checking (not needed to run it in a workflow), in a scratch
 * directory with a copy of the .github/scripts/ files (the types of
 * actions/github-script are not published on npm, only on GitHub):
 *   npm install --no-save typescript @types/node \
 *       "@actions/github-script@github:actions/github-script"
 *   npx tsc --noEmit --allowJs --checkJs --strict --skipLibCheck \
 *       --module node16 --moduleResolution node16 --types node \
 *       .github/scripts/*.js
 */

/**
 * @typedef {import('@actions/github-script').AsyncFunctionArguments} AsyncFunctionArguments
 * @typedef {Pick<AsyncFunctionArguments, 'github' | 'context'>} GitHubArgs
 */

/** Account which posts comments with the GITHUB_TOKEN of a workflow */
const WORKFLOW_COMMENT_AUTHOR = 'github-actions[bot]';

/**
 * List all comments of a pull request.
 * @param {GitHubArgs} args
 * @param {number} issue_number - the PR number
 */
async function listPrComments({ github, context }, issue_number) {
	const { owner, repo } = context.repo;
	return github.paginate(github.rest.issues.listComments,
		{ owner, repo, issue_number, per_page: 100 });
}

/**
 * Create a comment of this workflow on the PR, identified by a hidden
 * marker (an HTML comment, added to the body), or update it if it
 * exists already, so the PR has one such comment that stays current.
 * @param {GitHubArgs} args
 * @param {number} issue_number - the PR number
 * @param {string} marker - e.g. "<!-- my-workflow:my-comment -->"
 * @param {string} body - Markdown text
 * @param {string[]} [oldMarkers] - markers of earlier versions of this
 *   comment (e.g. by another tool) to take over and update as well
 */
async function upsertPrComment({ github, context }, issue_number, marker, body, oldMarkers = []) {
	const { owner, repo } = context.repo;
	const fullBody = `${body}\n\n${marker}`;
	const markers = [marker, ...oldMarkers];
	const comments = await listPrComments({ github, context }, issue_number);
	const old = comments.find(c => c.user?.login === WORKFLOW_COMMENT_AUTHOR
		&& markers.some(m => c.body?.includes(m)));
	if (old) {
		await github.rest.issues.updateComment({ owner, repo, comment_id: old.id, body: fullBody });
		console.log(`Updated comment ${old.id}`);
	} else {
		const res = await github.rest.issues.createComment(
			{ owner, repo, issue_number, body: fullBody });
		console.log(`Created comment ${res.data.id}`);
	}
}

/**
 * Delete all comments on the PR posted by the given account, e.g.
 * outdated build reports of another CI system.
 * @param {GitHubArgs} args
 * @param {number} issue_number - the PR number
 * @param {string} login - e.g. "AppVeyorBot"
 */
async function deletePrCommentsBy({ github, context }, issue_number, login) {
	const { owner, repo } = context.repo;
	const comments = await listPrComments({ github, context }, issue_number);
	for (const c of comments.filter(c => c.user?.login === login)) {
		await github.rest.issues.deleteComment({ owner, repo, comment_id: c.id });
		console.log(`Deleted comment ${c.id} by ${login}`);
	}
}

module.exports = {
	deletePrCommentsBy,
	listPrComments,
	upsertPrComment,
};
