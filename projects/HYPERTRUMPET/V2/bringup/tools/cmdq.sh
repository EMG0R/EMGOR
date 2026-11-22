#!/bin/zsh
# cmdq.sh "cmd1" "cmd2" ...: queue serial commands through the open web tab (POST /cmd), paced
for c in "$@"; do curl -s -X POST -H 'x-ht: 1' --data-binary "$c" http://127.0.0.1:8731/cmd >/dev/null; sleep 0.25; done
