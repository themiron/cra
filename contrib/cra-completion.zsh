#compdef cra

_cra()
{
	local -a commands
	commands=(
		'blame:show file attribution'
		'grep:search repository files'
		'log:show commit log'
		'reflog:show reference log'
		'refs:show references'
		'show:show a commit'
		'stash:show stashes'
		'status:show working-tree status'
	)

	_arguments \
		'-C[start in directory]:directory:_directories' \
		'(-h --help)'{-h,--help}'[show help]' \
		'(-v --version)'{-v,--version}'[show version]' \
		'1:command:->command' \
		'*:argument:_files'

	case "$state" in
		command) _describe 'command' commands ;;
	esac
}

_cra "$@"
