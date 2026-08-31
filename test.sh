#!/bin/sh
# Exercises all four cases that matter for a pledge(2)/unveil(2) wrapper:
# the happy path, a genuine pledge violation, a genuine unveil violation,
# and PATH-search vs. absolute-path exec. Run this after every change -
# it's what caught the real bug described in the README, twice.
#
# Usage: ./test.sh   (run on OpenBSD, after `cc -o pledge-exec pledge-exec.c`)

BIN=./pledge-exec
FAIL=0

check() {
	desc=$1
	expect_status=$2
	shift 2
	out=$("$@" 2>&1)
	status=$?
	if [ "$status" -eq "$expect_status" ]; then
		echo "ok   - $desc (exit $status)"
	else
		echo "FAIL - $desc (expected exit $expect_status, got $status): $out"
		FAIL=1
	fi
}

if [ ! -x "$BIN" ]; then
	echo "build first: cc -o pledge-exec pledge-exec.c" >&2
	exit 2
fi

# 1. Happy path: binary and target file both unveiled, promise granted.
check "happy path succeeds" 0 \
	"$BIN" -p "stdio rpath" -u "/bin/cat:x" -u "/etc/myname:r" -- /bin/cat /etc/myname

# 2. Genuine pledge violation: no rpath granted to the exec'd program.
#    Must fail - if this ever exits 0, the wrapper is not restricting
#    anything and is worse than useless.
check "missing rpath is denied" 1 \
	"$BIN" -p "stdio" -u "/bin/cat:x" -u "/etc/myname:r" -- /bin/cat /etc/myname

# 3. Genuine unveil violation: rpath granted, but the target path was
#    never unveiled. Must fail (ENOENT, since unveil hides even the
#    file's existence - not a crash, just a clean error).
check "un-unveiled path is denied" 1 \
	"$BIN" -p "stdio rpath" -u "/bin/cat:x" -- /bin/cat /etc/myname

# 4. PATH search still works when the containing directory is unveiled
#    with x, not just the exact binary path.
check "PATH search via unveiled directory succeeds" 0 \
	"$BIN" -p "stdio rpath" -u "/bin:x" -u "/etc/myname:r" -- cat /etc/myname

if [ "$FAIL" -eq 0 ]; then
	echo "all tests passed"
else
	echo "some tests FAILED"
	exit 1
fi
