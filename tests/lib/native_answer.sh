# Run an integer Answer() == 42 assertion on one native backend. Arguments:
# target, root, output, input, module, Go entry name, Go failure message.
# A subshell keeps the caller's source/saved matrix variables intact.
native_answer_42() (
    native_target=$1
    native_root=$2
    native_output=$3
    native_input=$4
    native_module=$5
    native_go_entry=$6
    native_go_message=$7
    if test "$native_target" = go; then
        "$ziran" build --target=go --pkg main --root "$native_root" \
            -o "$native_output" "$native_input"
        printf 'package main\nfunc main() { if %s() != 42 { panic("%s") } }\n' \
            "$native_go_entry" "$native_go_message" > "$native_output/main.go"
        GO111MODULE=off go run "$native_output"/*.go
    else
        "$ziran" build "--target=$native_target" --root "$native_root" \
            -o "$native_output" "$native_input"
        case "$native_target" in
            c)
                native_extension=c; native_header=h; native_parameters=void
                native_compiler=${CC:-cc}; native_standard=c11 ;;
            *) exit 1 ;;
        esac
        printf '#include "%s.%s"\nint main(%s) { return Answer() == 42 ? 0 : 1; }\n' \
            "$native_module" "$native_header" "$native_parameters" \
            > "$native_output/main.$native_extension"
        "$native_compiler" "-std=$native_standard" -I"$repo/include" -I"$native_output" \
            "$native_output"/*."$native_extension" -o "$native_output/app"
        "$native_output/app"
    fi
)
