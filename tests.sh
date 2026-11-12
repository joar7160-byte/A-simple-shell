#!/bin/sh
shell_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd) || exit 1
shell="$shell_dir/mysh"
work=$(mktemp -d "${TMPDIR:-/tmp}/mysh-tests.XXXXXX") || exit 1
trap 'rm -rf "$work"' 0
trap 'exit 1' HUP INT TERM
cd "$work" || exit 1
work=$(pwd -P)

HOME=$work
export HOME
passed=0
failed=0

check() {
    printf '%b' "$2" > commands
    printf '%b' "$3" > expected
    if "$shell" < commands > actual 2> errors && cmp -s expected actual; then
        echo "PASS: $1"
        passed=$((passed + 1))
    else
        echo "FAIL: $1"
        diff expected actual
        cat errors
        failed=$((failed + 1))
    fi
}

check 'arguments and comments' '\n# comment\necho hello world # ignored\n' 'hello world\n'
check 'absolute program path' '/bin/echo hello\n' 'hello\n'
check 'final line without newline' 'echo final' 'final\n'
check 'exit with arguments' 'exit anything\necho wrong\n' ''
check 'batch child input' 'cat\necho alive\n' 'alive\n'
mkdir sub
check 'pwd, cd, and HOME' 'pwd\ncd sub\npwd\ncd\npwd\n' "$work\n$work/sub\n$work\n"
check 'built-in argument errors' 'cd one two\npwd extra\nwhich\necho alive\n' 'alive\n'
check 'overwrite and append' 'echo a > out\necho b >> out\ncat < out\necho c > out\ncat out\n' 'a\nb\nc\n'
check 'both redirection orders' 'echo data > out\ncat < out > copy1\ncat > copy2 < out\ncat copy1 copy2\n' 'data\ndata\n'
check 'built-in redirection' 'pwd > location\necho next\ncat location\n' "next\n$work\n"
check 'file errors recover' 'pwd > missing/out\ncat < absent\necho alive\n' 'alive\n'
check 'multiple pipes' 'echo one two three | cat | cat\n' 'one two three\n'
check 'early pipe reader' 'yes | head -n 1\necho alive\n' 'y\nalive\n'
check 'pipeline cd preserves parent' 'cd sub | cat\npwd\n' "$work\n"
touch sub/apple.txt sub/apricot.txt sub/b.log sub/.hidden.txt
check 'multiple wildcard arguments' 'echo sub/a*.txt sub/*.log\n' 'sub/apple.txt sub/apricot.txt sub/b.log\n'
check 'hidden and unmatched patterns' 'echo sub/*.txt\necho sub/.h*.txt\necho absent*.xyz\n' 'sub/apple.txt sub/apricot.txt\nsub/.hidden.txt\nabsent*.xyz\n'
check 'syntax errors recover' 'cat < <\necho x |\n| cat\necho >\necho alive\n' 'alive\n'
check 'unknown commands recover' 'no_such_p3_command_12345\necho alive\n' 'alive\n'
check 'which explicit path' 'which /bin/echo\n' '/bin/echo\n'
check 'exit in pipeline' 'exit | echo finished > result\necho wrong\n' ''
check 'pipeline finishes before exit' 'cat result\n' 'finished\n'

printf 'echo batch\n' > batch.txt
if "$shell" batch.txt > actual 2> errors && [ "$(cat actual)" = batch ]; then
    echo 'PASS: filename input'; passed=$((passed + 1))
else echo 'FAIL: filename input'; failed=$((failed + 1)); fi
if "$shell" nonexistent.txt > actual 2> errors; then
    echo 'FAIL: missing input file'; failed=$((failed + 1))
else echo 'PASS: missing input file'; passed=$((passed + 1)); fi
umask 0
printf 'echo mode > permissions\n' | "$shell" > actual 2> errors
mode=$(ls -l permissions 2>/dev/null)
case "$mode" in
    -rw-r-----*) echo 'PASS: output mode 0640'; passed=$((passed + 1));;
    *) echo 'FAIL: output mode 0640'; failed=$((failed + 1));;
esac
echo "$passed passed; $failed failed. Check interactive behavior manually."
[ "$failed" -eq 0 ]
