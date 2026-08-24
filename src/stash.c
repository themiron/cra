/* Copyright (c) 2006-2026 Jonas Fonseca <jonas.fonseca@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "tig/display.h"
#include "tig/draw.h"
#include "tig/main.h"
#include "tig/diff.h"
#include "tig/parse.h"
#include "tig/reflog.h"
#include "tig/repo.h"

struct stash_state {
	struct main_state main;
	char id[SIZEOF_REV];
	char author[SIZEOF_STR];
	char date[SIZEOF_STR];
	char description[SIZEOF_STR];
	unsigned long num;
};

static bool
stash_read(struct view *view, struct buffer *buf, bool force_stop)
{
	struct stash_state *state = view->private;
	char *line;

	if (!buf)
		return main_read(view, NULL, force_stop);

	line = buf->data;
	while (isspace((unsigned char)*line))
		line++;
	if (!strcmp(line, "{")) {
		state->id[0] = 0;
		state->author[0] = 0;
		state->date[0] = 0;
		state->description[0] = 0;
		return true;
	}
	if (!strcmp(line, "},") || !strcmp(line, "}")) {
		char ref[SIZEOF_REF];

		if (!string_format(ref, "refs/stash@{%lu}", state->num++) ||
		    !state->id[0] || !state->author[0] || !state->date[0] ||
		    !state->description[0])
			return false;
		return reflog_read_entry(view, state->id, ref, state->author,
					 state->date, state->description);
	}

	if (!prefixcmp(line, "\"id\""))
		return parse_json_string(line, "\"id\"", state->id,
					 sizeof(state->id), true);
	if (!prefixcmp(line, "\"author\""))
		return parse_json_string(line, "\"author\"", state->author,
					 sizeof(state->author), true);
	if (!prefixcmp(line, "\"time\""))
		return parse_json_string(line, "\"time\"", state->date,
					 sizeof(state->date), true);
	if (!prefixcmp(line, "\"description\""))
		return parse_json_string(line, "\"description\"",
					 state->description,
					 sizeof(state->description), true);
	return true;
}

static enum status_code
stash_open(struct view *view, enum open_flags flags)
{
	static const char *stash_argv[] = {
		"arc", "stash", "list", "--json", NULL
	};
	const char **argv = NULL;
	struct main_state *state = view->private;
	enum status_code code;

	if (!(repo.is_inside_work_tree || *repo.worktree))
		return error("The stash view requires a working tree");

	/* git stash list only works well with commit limiting options,
	 * so filter --all, --branches, --remotes and revisions from
	 * %(revargs). */
	if (!argv_append_array(&argv, stash_argv))
		return ERROR_OUT_OF_MEMORY;
	if (opt_rev_args) {
		int i;
		for (i = 0; opt_rev_args[i]; i++) {
			const char *arg = opt_rev_args[i];
			if (arg[0] == '-' && strcmp(arg, "--all") &&
			    strcmp(arg, "--branches") && strcmp(arg, "--remotes"))
				argv_append(&argv, arg);
		}
	}

	state->with_graph = false;
	watch_register(&view->watch, WATCH_STASH);
	code = begin_update(view, NULL, argv, flags | OPEN_RELOAD);
	argv_free(argv);
	free(argv);
	return code;
}

static void
stash_select(struct view *view, struct line *line)
{
	struct main_state *state = view->private;

	main_select(view, line);
	assert(state->reflogs >= line->lineno);
	string_ncopy(view->env->stash, state->reflog[line->lineno - 1] + STRING_SIZE("refs/"),
		     strlen(state->reflog[line->lineno - 1]) - STRING_SIZE("refs/"));
	string_copy(view->ref, view->env->stash);
	view->env->blob[0] = 0;
}

static enum request
stash_request(struct view *view, enum request request, struct line *line)
{
	enum open_flags flags = (view_is_displayed(view) && request != REQ_VIEW_DIFF)
				? OPEN_SPLIT : OPEN_DEFAULT;
	struct view *diff = &diff_view;

	switch (request) {
	case REQ_VIEW_DIFF:
	case REQ_ENTER:
		if (!view_is_displayed(diff) ||
		    strcmp(view->env->stash, diff->ref)) {
			const char *diff_argv[] = {
				"arc", "stash", "show", "--git", diff_context_arg(),
					"%(cmdlineargs)", "%(stash)", NULL
			};

			if (!argv_format(diff_view.env, &diff_view.argv, diff_argv, 0))
				report("Failed to format argument");
			else
				open_diff_view(view, flags | OPEN_PREPARED);
		}
		return REQ_NONE;

	default:
		return main_request(view, request, line);
	}
}

static struct view_ops stash_ops = {
	"stash",
	"",
	VIEW_SEND_CHILD_ENTER | VIEW_REFRESH,
	sizeof(struct stash_state),
	stash_open,
	stash_read,
	view_column_draw,
	stash_request,
	view_column_grep,
	stash_select,
	main_done,
	view_column_bit(AUTHOR) | view_column_bit(COMMIT_TITLE) |
		view_column_bit(DATE) | view_column_bit(ID) |
		view_column_bit(LINE_NUMBER),
	main_get_column_data,
};

DEFINE_VIEW(stash);

/* vim: set ts=8 sw=8 noexpandtab: */
