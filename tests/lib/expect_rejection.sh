# Arguments: failure message, stderr path, command and its arguments.
# Keep exit in the calling shell: accepting invalid input must stop the test
# even when the caller is a function used as an if condition.
expect_rejection() {
    if (
        shift 2
        "$@"
    ) 2> "$2"; then
        echo "$1" >&2
        exit 1
    fi
}
