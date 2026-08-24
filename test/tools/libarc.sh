#!/bin/sh
#
# Arc test helpers.

mock_arc()
{
	head="${1-feature/test}"
	tag="${2-}"
	show_commit="${3-}"
	executable 'arc' <<EOF
#!/bin/sh

printf '%s\n' "\$*" >> "$HOME/arc-commands"

case "\${1:-}" in
root)
	pwd -P
	;;
rev-parse)
	case "\${2:-}" in
	--show-prefix) printf '\n' ;;
	*) exit 1 ;;
	esac
	;;
info)
	case "\${2:-}" in
	--json)
		printf '%s\n' \
			'{' \
			'  "repository":"example",' \
			'  "remote":"trunk",' \
			'  "branch":"$head",' \
			'  "hash":"1111111111111111111111111111111111111111"' \
			'}'
		;;
	--ahead-behind) ;;
	*) exit 1 ;;
	esac
	;;
config)
	[ "\${2:-}" = --list ] || exit 1
	;;
branch)
	[ "\${2:-}" = --all ] && [ "\${3:-}" = --verbose ] || exit 1
	if [ -n "$head" ]; then
		printf '%s\n' \
			'* $head 1111111111111111111111111111111111111111' \
			'  example/trunk 2222222222222222222222222222222222222222'
	fi
	;;
tag)
	[ "\${2:-}" = --points-at ] && [ -n "\${3:-}" ] || exit 1
	[ -z "$tag" ] || printf '%s\n' "$tag"
	;;
show)
	[ -n "$show_commit" ] || exit 1
	printf '%s\n' \
		'commit $show_commit' \
		'Author: A. U. Thor <author@example.com>' \
		'Date:   Thu Jan 1 00:00:00 1970 +0000' \
		'' \
		'    Tagged commit'
	;;
status)
	[ "\${2:-}" = --short ] && [ "\${3:-}" = -u ] &&
		[ "\${4:-}" = all ] || exit 1
	;;
*)
	printf 'mock arc: unsupported command: %s\n' "\$*" >&2
	exit 1
	;;
esac
EOF
}

use_arc_repo()
{
	arc_bin="$(PATH="$original_path" command -v arc 2>/dev/null || true)"
	repo_dir="${CRA_TEST_REPO:-}"

	[ -n "$arc_bin" ] || die 'The live Arc tests require arc in PATH'
	[ -n "$repo_dir" ] || die 'Set CRA_TEST_REPO to an Arc working directory'
	[ -d "$repo_dir" ] || die "Arc test directory does not exist: $repo_dir"
	repo_dir="$(cd "$repo_dir" >/dev/null && pwd -P)"

	executable 'arc' <<EOF
#!/bin/sh
HOME="$original_home" exec "$arc_bin" "\$@"
EOF
	work_dir="$repo_dir"
}
