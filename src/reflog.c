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
#include "tig/git.h"
#include "tig/main.h"
#include "tig/parse.h"

static bool
reflog_json_number(const char *line, const char *name, unsigned long *value)
{
	const char *key = strstr(line, name);
	char *end;

	if (!key)
		return false;
	key += strlen(name);
	while (isspace((unsigned char)*key))
		key++;
	if (*key++ != ':')
		return false;
	while (isspace((unsigned char)*key))
		key++;
	*value = strtoul(key, &end, 10);
	return end > key;
}

static bool
reflog_time(const char *value, time_t *timestamp)
{
	int year, month, day, hour, minute, second;
	long era, year_of_era, day_of_year, day_of_era, days;

	if (sscanf(value, "%d-%d-%dT%d:%d:%d", &year, &month, &day,
		   &hour, &minute, &second) != 6 ||
	    month < 1 || month > 12 || day < 1 || day > 31 ||
	    hour < 0 || hour > 23 || minute < 0 || minute > 59 ||
	    second < 0 || second > 60)
		return false;

	year -= month <= 2;
	era = (year >= 0 ? year : year - 399) / 400;
	year_of_era = year - era * 400;
	day_of_year = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 +
		day - 1;
	day_of_era = year_of_era * 365 + year_of_era / 4 -
		year_of_era / 100 + day_of_year;
	days = era * 146097 + day_of_era - 719468;
	*timestamp = (time_t)days * 86400 + hour * 3600 + minute * 60 + second;
	return true;
}

static bool
reflog_main_read(struct view *view, char *line)
{
	struct buffer buf = { line, strlen(line) };

	return main_read(view, &buf, false);
}

bool
reflog_read_entry(struct view *view, const char *id, const char *ref,
		  const char *author, const char *date,
		  const char *description)
{
	char line[SIZEOF_STR * 2];
	time_t timestamp;

	if (!iscommit(id) || !reflog_time(date, &timestamp) ||
	    !string_format(line, "commit %s", id) ||
	    !reflog_main_read(view, line) ||
	    !string_format(line, "Reflog: %s", ref) ||
	    !reflog_main_read(view, line) ||
	    !string_format(line, "Reflog message: %s", description) ||
	    !reflog_main_read(view, line) ||
	    !string_format(line, "author %s <> %lld +0000", author,
			  (long long)timestamp) ||
	    !reflog_main_read(view, line) ||
	    !string_format(line, "committer %s <> %lld +0000", author,
			  (long long)timestamp) ||
	    !reflog_main_read(view, line))
		return false;

	line[0] = 0;
	return reflog_main_read(view, line) &&
	       string_format(line, "    %s", description) &&
	       reflog_main_read(view, line);
}

static bool
reflog_read(struct view *view, struct buffer *buf, bool force_stop)
{
	char id[SIZEOF_REV];
	char author[SIZEOF_STR];
	char date[SIZEOF_STR];
	char description[SIZEOF_STR];
	char ref[SIZEOF_REF];
	unsigned long num;

	if (!buf)
		return main_read(view, NULL, force_stop);
	if (!parse_json_string(buf->data, "\"after\"", id, sizeof(id), true) ||
	    !parse_json_string(buf->data, "\"author\"", author, sizeof(author), true) ||
	    !parse_json_string(buf->data, "\"time\"", date, sizeof(date), true) ||
	    !parse_json_string(buf->data, "\"description\"", description,
			       sizeof(description), true) ||
	    !reflog_json_number(buf->data, "\"num\"", &num) ||
	    !string_format(ref, "HEAD@{%lu}", num))
		return false;

	return reflog_read_entry(view, id, ref, author, date, description);
}

static enum status_code
reflog_open(struct view *view, enum open_flags flags)
{
	struct main_state *state = view->private;
	const char *reflog_argv[] = {
		"arc", "reflog", "show", "--json", "%(cmdlineargs)",
			"%(revargs)", NULL
	};

	if (is_initial_view(view) && opt_file_args)
		die("No revisions match the given arguments.");

	state->with_graph = false;
	watch_register(&view->watch, WATCH_HEAD | WATCH_REFS);
	return begin_update(view, NULL, reflog_argv, flags);
}

static enum request
reflog_request(struct view *view, enum request request, struct line *line)
{
	struct commit *commit = line->data;

	switch (request) {
	case REQ_ENTER:
	{
		const char *main_argv[] = {
			GIT_MAIN_LOG(encoding_arg, commit_order_arg(),
				"%(mainargs)", "", commit->id, "",
				show_notes_arg(), log_custom_pretty_arg())
		};
		enum open_flags flags = view_is_displayed(view) ? OPEN_SPLIT : OPEN_DEFAULT;

		if (!argv_format(main_view.env, &main_view.argv, main_argv, 0))
			report("Failed to format argument");
		else
			open_main_view(view, flags | OPEN_PREPARED);
		return REQ_NONE;
	}

	default:
		return main_request(view, request, line);
	}
}

static struct view_ops reflog_ops = {
	"reference",
	argv_env.head,
	VIEW_LOG_LIKE | VIEW_REFRESH,
	sizeof(struct main_state),
	reflog_open,
	reflog_read,
	view_column_draw,
	reflog_request,
	view_column_grep,
	main_select,
	main_done,
	view_column_bit(AUTHOR) | view_column_bit(COMMITTER) | view_column_bit(COMMIT_TITLE) |
		view_column_bit(DATE) | view_column_bit(ID) |
		view_column_bit(LINE_NUMBER),
	main_get_column_data,
};

DEFINE_VIEW(reflog);

/* vim: set ts=8 sw=8 noexpandtab: */
