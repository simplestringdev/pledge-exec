/*
 * pledge-exec — generic OpenBSD pledge(2)/unveil(2) wrapper.
 *
 * Restricts what a program CAN do (via pledge) and WHERE it can even see
 * (via unveil) before exec'ing it, without having to write bespoke C for
 * every single script you want to run under those restrictions.
 *
 * Usage:
 *   pledge-exec -p "stdio rpath wpath cpath" -u "/path/one:r" -u "/path/two:rwc" -- program [args...]
 *
 *   -p PROMISES   pledge(2) promise string (required)
 *   -u PATH:PERM  one unveil(2) rule; repeatable. PERM is unveil's own
 *                 permission string (r, w, x, c, or a combination).
 *   --            everything after this is the real program + its argv
 *
 * If no -u is given at all, unveil is skipped entirely (the exec'd
 * program keeps full filesystem visibility, only pledge restricts it).
 * If at least one -u is given, unveil(NULL, NULL) is called after the
 * last rule to lock the view down to exactly what was named - remember
 * this means the program's own binary path needs an "x" rule too (e.g.
 * -u "/usr/bin/myscript:x"), or exec will fail with ENOENT.
 *
 * PROMISES applies to the exec'd program, not to pledge-exec itself -
 * see the comment above the pledge(2) call in main() for why that
 * distinction matters and what pledge-exec pledges for its own short
 * remaining lifetime instead.
 *
 * Compile:  cc -o pledge-exec pledge-exec.c
 * (No external dependencies - this only exists to be small and auditable.)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

static void
usage(const char *argv0)
{
	fprintf(stderr,
	    "usage: %s -p PROMISES [-u PATH:PERM ...] -- PROGRAM [ARGS...]\n",
	    argv0);
	exit(2);
}

int
main(int argc, char *argv[])
{
	const char *promises = NULL;
	int have_unveil = 0;
	int i = 1;

	while (i < argc && strcmp(argv[i], "--") != 0) {
		if (strcmp(argv[i], "-p") == 0) {
			if (++i >= argc)
				usage(argv[0]);
			promises = argv[i++];
		} else if (strcmp(argv[i], "-u") == 0) {
			char *spec, *path, *perm;

			if (++i >= argc)
				usage(argv[0]);
			spec = strdup(argv[i++]);
			if (spec == NULL)
				err(1, "strdup");

			perm = strrchr(spec, ':');
			if (perm == NULL) {
				fprintf(stderr,
				    "pledge-exec: -u argument must be PATH:PERM, got '%s'\n",
				    spec);
				exit(2);
			}
			*perm = '\0';
			perm++;
			path = spec;

			if (unveil(path, perm) == -1)
				err(1, "unveil(\"%s\", \"%s\")", path, perm);
			have_unveil = 1;
			free(spec);
		} else {
			fprintf(stderr, "pledge-exec: unknown argument '%s'\n", argv[i]);
			usage(argv[0]);
		}
	}

	if (promises == NULL)
		usage(argv[0]);

	if (i >= argc || strcmp(argv[i], "--") != 0)
		usage(argv[0]);
	i++; /* skip "--" */

	if (i >= argc) {
		fprintf(stderr, "pledge-exec: no program given after --\n");
		usage(argv[0]);
	}

	if (have_unveil) {
		if (unveil(NULL, NULL) == -1)
			err(1, "unveil(NULL, NULL) (lock)");
	}

	/*
	 * pledge(2) takes two promise sets: the first restricts THIS process
	 * right now, the second - execpromises - is what the exec'd program
	 * inherits after a successful execve(2). Passing NULL for
	 * execpromises does NOT mean "keep the current promises through
	 * exec" - it means the new program starts with NO pledge active at
	 * all (see pledge(2), the "exec" promise description). So the
	 * caller's -p promises belong in execpromises, not in our own; our
	 * own promises here only need enough to reach execvp(): "exec" to
	 * be allowed to call it, "stdio" so err() below can still report a
	 * failure if the exec itself fails (e.g. ENOENT).
	 */
	if (pledge("stdio exec", promises) == -1)
		err(1, "pledge(\"stdio exec\", \"%s\")", promises);

	execvp(argv[i], &argv[i]);
	err(1, "execvp(\"%s\")", argv[i]);
}
