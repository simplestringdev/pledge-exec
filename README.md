# pledge-exec

[![build](https://github.com/simplestringdev/pledge-exec/actions/workflows/build.yml/badge.svg)](https://github.com/simplestringdev/pledge-exec/actions/workflows/build.yml)

A ~100-line C wrapper that applies OpenBSD's `pledge(2)` and `unveil(2)`
to an arbitrary program before exec'ing it, driven entirely by command-line
flags. No bespoke C needed per script you want to sandbox.

```sh
pledge-exec -p "stdio rpath" -u "/bin/cat:x" -u "/etc/myname:r" -- /bin/cat /etc/myname
```

- `-p PROMISES` — the pledge(2) promise string the exec'd program will run under.
- `-u PATH:PERM` — one unveil(2) rule, repeatable. `PERM` is unveil's own
  permission string (`r`, `w`, `x`, `c`, or a combination). The program's own
  binary path needs an `x` rule too, or the exec itself fails.
- `--` — everything after this is the real program and its argv.

If no `-u` is given at all, unveil is skipped entirely (only pledge
restricts the program; it keeps full filesystem visibility). If at least
one `-u` is given, the view is locked to exactly what was named.

## Why this exists

`pledge`/`unveil` are per-process self-restriction calls - normally a
program calls them on itself. Useful as that is, most of the things I
actually want to run under those restrictions are third-party scripts or
binaries I don't want to patch: a cron job, a small service, a tool an
automated agent invokes. `pledge-exec` lets you wrap any of those from the
command line, the same way you'd reach for `nice` or `chroot`.

## The bug that shaped this (and why it matters)

The first version compiled clean and looked correct, but every single
invocation - even the simplest read of a plainly unveiled file - died
with `Abort trap (core dumped)`. Two rounds of wrong hypotheses before
finding the real cause, in order:

1. **Wrong guess: unveil blocking `PATH` search.** `execvp()` resolves
   the program name through `$PATH`, and only one specific file had been
   unveiled - looked like a plausible culprit. Disproven by testing with
   an absolute path instead (`/bin/cat` directly): identical abort. Ruled
   out.

2. **Half-right guess: missing the `exec` promise.** `pledge(2)` requires
   the `"exec"` promise just to be allowed to call `execve(2)` at all,
   and the first version's `-p` string (e.g. `"stdio rpath"`) never
   included it. Adding it made the abort go away - but a follow-up test
   made clear something was still wrong: a promise string with no
   `rpath` at all was somehow still allowed to read a file after exec.
   That shouldn't be possible if the restriction were real.

3. **Actual root cause: `pledge(2)` takes *two* promise sets, and NULL
   for the second one does not mean what it sounds like it means.**

   ```c
   int pledge(const char *promises, const char *execpromises);
   ```

   `promises` restricts the calling process immediately. `execpromises`
   is what a program started via `execve(2)` inherits afterward - and
   passing `NULL` for it does **not** mean "keep the current
   restriction through exec." Per `pledge(2)`:

   > If execpromises has been previously set the new program begins
   > with those promises... Otherwise the new program starts running
   > without pledge active, and hopefully makes a new pledge soon.

   The first version called `pledge(promises, NULL)` - which pledged
   *pledge-exec itself* (a process about to disappear into `execve()`
   anyway) and left the actual target program completely unrestricted.
   That's why a promise string missing `rpath` could still read a file:
   the restriction was never applied to the program that mattered. The
   fix is to put the caller's `-p` string into `execpromises`, and give
   `pledge-exec`'s own short remaining lifetime just enough to reach the
   `execvp()` call (`"stdio exec"`):

   ```c
   if (pledge("stdio exec", promises) == -1)
       err(1, "pledge");
   execvp(argv[i], &argv[i]);
   ```

Every one of these steps was tested for real on OpenBSD, not reasoned
about in the abstract - `test.sh` is the artifact of that process, kept
in the repo so the four cases that actually matter (happy path, a real
pledge violation, a real unveil violation, `PATH` search) stay checked
after any future change:

```
$ ./test.sh
ok   - happy path succeeds (exit 0)
ok   - missing rpath is denied (exit 1)
ok   - un-unveiled path is denied (exit 1)
ok   - PATH search via unveiled directory succeeds (exit 0)
all tests passed
```

## Build

```sh
cc -o pledge-exec pledge-exec.c
```

No dependencies beyond the OpenBSD base system's libc. CI compiles and
runs the full test suite inside a real OpenBSD VM on every push
(`.github/workflows/build.yml`, via `vmactions/openbsd-vm`) - the badge
above is a live check, not a claim.

## Examples

See `examples/` for two small, realistic wrappers: a read-only status
script and a scoped service-restart script, the same shape used to run
an unattended agent process against a real host without giving it a
shell.

## Caveats

- This is base-system OpenBSD only - `pledge`/`unveil` don't exist
  elsewhere.
- `unveil` and `pledge` restrict what the exec'd program *can* do, not
  what arguments a human passes it. `pledge-exec -p "stdio rpath" -u
  "/bin/rm:x" -u "/:r"` still lets `rm` walk the whole unveiled tree
  read-only - unveil grants visibility, it isn't a per-argument ACL.
  Scope the promises and the unveiled paths as tightly as the actual
  task needs.

## License

[MIT](LICENSE)
