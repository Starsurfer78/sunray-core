
#!/usr/bin/env bash
echo "BASH_VERSION is: ${BASH_VERSION:-}"
if [ -z "${BASH_VERSION:-}" ]; then
    echo "Falling back! \$0 is $0"
    exec bash "$0" "$@"
fi
echo "Running in bash! arg: $1"
