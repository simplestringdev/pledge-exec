#!/bin/sh
# Example: run a read-only status script under pledge-exec instead of
# trusting the script to behave. Even if status.sh were compromised or
# had a bug, it physically cannot write anywhere or touch the network -
# the kernel enforces that, not the script's own logic.
#
# Adjust PLEDGE_EXEC and the target script path for your setup.

PLEDGE_EXEC=/usr/local/bin/pledge-exec
SCRIPT=/usr/local/sbin/status.sh

exec "$PLEDGE_EXEC" \
	-p "stdio rpath proc exec" \
	-u "/bin/sh:x" \
	-u "$SCRIPT:x" \
	-u "/bin:x" \
	-u "/usr/bin:x" \
	-u "/etc:r" \
	-u "/var/run:r" \
	-- /bin/sh "$SCRIPT"
