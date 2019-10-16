if ! echo "$PATH" | grep -qE ".*:/scripts/ivy/bin/latest.*" ; 
then
	export PATH="${PATH}:/scripts/ivy/bin/latest"
fi

