# Bash completion for Cra.

_cra()
{
	local current command word
	local commands="blame grep log reflog refs show stash status"
	local options="-C -h --help -v --version"

	current="${COMP_WORDS[COMP_CWORD]}"
	command=
	for word in "${COMP_WORDS[@]:1:COMP_CWORD-1}"; do
		case "$word" in
			-*) ;;
			*) command="$word"; break ;;
		esac
	done

	if [ -z "$command" ]; then
		COMPREPLY=( $(compgen -W "$commands $options" -- "$current") )
	else
		COMPREPLY=( $(compgen -f -- "$current") )
	fi
}

complete -F _cra cra
