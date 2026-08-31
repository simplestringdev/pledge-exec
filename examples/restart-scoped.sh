# Example: pair pledge-exec with a doas.conf grant to scope what an
# unattended caller can actually do, beyond "this one script only" -
# even the script itself can't reach outside what's unveiled here.
#
# doas.conf:
#   permit nopass agent cmd /usr/local/bin/pledge-exec args \
#       -p "stdio proc exec" -u "/usr/sbin/rcctl:x" -u "/bin/sh:x" -- \
#       /bin/sh /usr/local/sbin/restart-scoped.sh
#
# restart-scoped.sh itself still validates $1 against an allowlist - the
# pledge/unveil layer is defense in depth on top of that check, not a
# replacement for it. It restricts WHAT the process can call
# (proc+exec, nothing that touches the filesystem or network directly),
# not which specific service name gets passed as an argument.

set -eu

case "$1" in
	httpd|smtpd)
		exec /usr/sbin/rcctl restart "$1"
		;;
	*)
		echo "restart-scoped: '$1' is not an allowed service" >&2
		exit 1
		;;
esac
