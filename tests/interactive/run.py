#!/usr/bin/env python3
"""Interactive tests: drive hash-shell in a pseudo-terminal and check what it prints.

    python3 tests/interactive/run.py [--shell PATH] [--timeout SECONDS] [-v] [--keep] [--list] [NAME...]

Each case is a text file in tests/interactive/cases/. A case is a list of
directives, one per line, run in order; blank lines and lines starting with #
are ignored. Everything after the directive's first space is its argument,
verbatim, with Python escapes decoded (\\x03 is Ctrl-C, \\x04 is Ctrl-D,
\\x12 is Ctrl-R, \\r is Enter, \\x1b[A is the Up arrow).

Before the shell starts:
  env NAME=value       add to the shell's environment
  write FILE text      append a line to FILE under the session's HOME
  rc text              shorthand for `write .hashrc text`
  login                start the shell as a login shell (-l)

During the session:
  send text            wait for a prompt, then type text and press Enter
  raw text             once the output is quiet, send text as typed, with no Enter
  expect text          wait until text appears in the output after the previous match
  absent text          once the output is quiet, text must not have appeared so far
  wait seconds         keep reading output for that long

After the session (the case has to make the shell exit):
  status N             the shell's exit status
  file FILE has text   a file under HOME contains text
  file FILE lacks text a file under HOME does not contain text (a missing file lacks everything)

Every case gets a fresh temporary HOME. The shell runs with TERM=dumb,
HISTFILE=$HOME/.hash_history, HASH_DISABLE_UPDATE_CHECK=1, PS1="hash> " and
PS2="... " unless `env` lines say otherwise. `send` waits until the shell is
quiet with the PS1 or PS2 text at the end of its output before typing; `raw`
only waits for quiet, so use it at a prompt or on a line being edited, not
while a command runs. Output is matched after terminal escape sequences and
carriage returns are removed, so the words the shell prints are what is
checked, not how it redraws them.

The shell echoes every typed line, so to check that a command did not run,
type something whose output differs from its spelling:
`send echo DIS''CARDED` prints DISCARDED only if it ran.
"""
import argparse
import codecs
import fcntl
import os
import pty
import re
import select
import shutil
import signal
import struct
import sys
import tempfile
import termios
import time

HERE = os.path.dirname(os.path.abspath(__file__))
CASES = os.path.join(HERE, "cases")
DEFAULT_SHELL = os.path.join(HERE, "..", "..", "hash-shell")
DEFAULT_PS1 = "hash> "
DEFAULT_PS2 = "... "
# The only variables the shell inherits from the caller
INHERIT = ("PATH", "LANG", "LC_ALL", "LC_CTYPE", "USER", "LOGNAME", "TMPDIR", "SHELL")
# CSI sequences, OSC strings and two-byte escapes
ESCAPES = re.compile(rb"\x1b\[[0-9;?]*[ -/]*[@-~]|\x1b\][^\x07\x1b]*(?:\x07|\x1b\\)|\x1b[@-Z\\-_]")
BEFORE = ("env", "write", "rc", "login")
DURING = ("send", "raw", "expect", "absent", "wait")
AFTER = ("status", "file")


def unescape(text):
    return codecs.decode(text.encode("latin-1", "backslashreplace"), "unicode_escape")


def clean(raw):
    return ESCAPES.sub(b"", raw).replace(b"\r", b"").decode("utf-8", "replace")


class Failure(Exception):
    pass


class Session:
    def __init__(self, shell, home, env, login, timeout):
        self.timeout = timeout
        self.raw = b""
        self.cursor = 0  # where in the clean output the next expect starts looking
        self.eof = False
        self.exit_status = None
        self.signal = None
        argv = [shell, "-l"] if login else [shell]
        self.pid, self.fd = pty.fork()
        if self.pid == 0:
            try:
                os.chdir(home)
                os.execve(shell, argv, env)
            finally:
                os._exit(127)
        fcntl.ioctl(self.fd, termios.TIOCSWINSZ, struct.pack("HHHH", 24, 80, 0, 0))

    def text(self):
        return clean(self.raw)

    def read(self, timeout):
        """Collect what the shell printed within timeout seconds; True if anything came."""
        if self.eof:
            time.sleep(min(timeout, 0.05))
            return False
        ready, _, _ = select.select([self.fd], [], [], timeout)
        if not ready:
            return False
        try:
            data = os.read(self.fd, 65536)
        except OSError:  # EIO once the shell is gone (Linux)
            data = b""
        if not data:
            self.eof = True
            return False
        self.raw += data
        return True

    def settle(self, idle=0.2):
        """Read until the shell has been quiet for `idle` seconds."""
        deadline = time.monotonic() + self.timeout
        while time.monotonic() < deadline and not self.eof:
            if not self.read(idle):
                return

    def wait_for(self, text):
        deadline = time.monotonic() + self.timeout
        while True:
            index = self.text().find(text, self.cursor)
            if index >= 0:
                self.cursor = index + len(text)
                return
            if self.eof:
                raise Failure("expected %r after the output ended" % text)
            if time.monotonic() >= deadline:
                raise Failure("expected %r within %gs" % (text, self.timeout))
            self.read(0.1)

    def at_prompt(self, prompts):
        """True once the shell has been quiet for a moment with a prompt at the end of its output.

        The line editor redraws the typed line when Enter is pressed, so the
        output can end with the prompt text for an instant while the shell is
        about to switch terminal modes; waiting for quiet makes sure the prompt
        is the one the shell is reading at. (Input typed during that switch is
        kept since #342, so this is about reading the transcript, not losing
        keys.)"""
        quiet = not self.read(0.2)
        out = self.text()
        return quiet and any(out.endswith(p) for p in prompts)

    def wait_ready(self, prompts):
        """Wait for the first prompt, or for the shell to leave on its own."""
        deadline = time.monotonic() + self.timeout
        while time.monotonic() < deadline and not self.eof:
            if self.at_prompt(prompts):
                return

    def wait_prompt(self, prompts, line):
        deadline = time.monotonic() + self.timeout
        while True:
            if self.at_prompt(prompts):
                return
            if self.eof:
                raise Failure("the shell exited before %r could be typed" % line)
            if time.monotonic() >= deadline:
                raise Failure("no prompt within %gs before typing %r" % (self.timeout, line))

    def write(self, data):
        try:
            os.write(self.fd, data)
        except OSError as error:
            raise Failure("could not type %r: %s" % (data, error))

    def finish(self):
        """Wait for the shell to exit; kill it if it does not."""
        deadline = time.monotonic() + self.timeout
        while True:
            self.read(0.1)
            pid, status = os.waitpid(self.pid, os.WNOHANG)
            if pid:
                break
            if time.monotonic() >= deadline:
                os.kill(self.pid, signal.SIGKILL)
                os.waitpid(self.pid, 0)
                raise Failure("the shell was still running %gs after the last directive" % self.timeout)
        # Drain whatever is left in the pty
        while self.read(0.1):
            pass
        if os.WIFSIGNALED(status):
            self.signal = os.WTERMSIG(status)
            raise Failure("the shell died from signal %d" % self.signal)
        self.exit_status = os.WEXITSTATUS(status)

    def close(self):
        try:
            os.close(self.fd)
        except OSError:
            pass


def parse_case(path):
    """Split a case file into its before, during and after directives."""
    before, during, after = [], [], []
    with open(path, encoding="utf-8") as handle:
        for number, line in enumerate(handle, 1):
            line = line.rstrip("\n")
            if not line.strip() or line.lstrip().startswith("#"):
                continue
            name, _, argument = line.partition(" ")
            where = "%s:%d" % (os.path.basename(path), number)
            if name in BEFORE:
                before.append((name, argument, where))
            elif name in DURING:
                during.append((name, argument, where))
            elif name in AFTER:
                after.append((name, argument, where))
            else:
                raise Failure("%s: unknown directive %r" % (where, name))
    return before, during, after


def prepare(home, before):
    env = {key: os.environ[key] for key in INHERIT if key in os.environ}
    env.update({
        "HOME": home,
        "TERM": "dumb",
        "HISTFILE": os.path.join(home, ".hash_history"),
        "HASH_DISABLE_UPDATE_CHECK": "1",
        "PS1": DEFAULT_PS1,
        "PS2": DEFAULT_PS2,
    })
    login = False
    for name, argument, where in before:
        if name == "env":
            key, equals, value = argument.partition("=")
            if not key or not equals:
                raise Failure("%s: env needs NAME=value" % where)
            env[key] = unescape(value)
        elif name in ("write", "rc"):
            if name == "rc":
                target, text = ".hashrc", argument
            else:
                target, _, text = argument.partition(" ")
            if not target or "/" in target:
                raise Failure("%s: write needs a file name under HOME" % where)
            with open(os.path.join(home, target), "a", encoding="utf-8") as handle:
                handle.write(unescape(text) + "\n")
        elif name == "login":
            login = True
    return env, login


def run_session(session, prompts, during):
    for name, argument, where in during:
        try:
            if name == "send":
                line = unescape(argument)
                session.wait_prompt(prompts, line)
                session.write(line.encode("utf-8") + b"\r")
            elif name == "raw":
                session.settle()
                session.write(unescape(argument).encode("utf-8"))
            elif name == "expect":
                session.wait_for(unescape(argument))
            elif name == "absent":
                session.settle()
                text = unescape(argument)
                if text in session.text():
                    raise Failure("%r appeared" % text)
            elif name == "wait":
                deadline = time.monotonic() + float(argument)
                while time.monotonic() < deadline:
                    session.read(0.1)
        except Failure as failure:
            raise Failure("%s: %s" % (where, failure))


def check_after(session, home, after):
    for name, argument, where in after:
        if name == "status":
            wanted = int(argument)
            if session.exit_status != wanted:
                raise Failure("%s: exit status %d, expected %d" % (where, session.exit_status, wanted))
        elif name == "file":
            target, _, rest = argument.partition(" ")
            verb, _, text = rest.partition(" ")
            if verb not in ("has", "lacks") or not target or "/" in target:
                raise Failure("%s: file needs `FILE has text` or `FILE lacks text`" % where)
            path = os.path.join(home, target)
            try:
                with open(path, encoding="utf-8", errors="replace") as handle:
                    content = handle.read()
            except FileNotFoundError:
                content = None
            text = unescape(text)
            if verb == "has":
                if content is None:
                    raise Failure("%s: %s does not exist" % (where, target))
                if text not in content:
                    raise Failure("%s: %s does not contain %r; it holds:\n%s" % (where, target, text, content))
            elif content is not None and text in content:
                raise Failure("%s: %s contains %r" % (where, target, text))


def run_case(path, shell, timeout, keep):
    """Run one case; returns (failure message or None, transcript, home)."""
    home = tempfile.mkdtemp(prefix="hash-interactive-")
    session = None
    try:
        before, during, after = parse_case(path)
        env, login = prepare(home, before)
        prompts = [env["PS1"], env["PS2"]]
        session = Session(shell, home, env, login, timeout)
        session.wait_ready(prompts)
        run_session(session, prompts, during)
        session.finish()
        check_after(session, home, after)
        return None, session.text(), home
    except Failure as failure:
        return str(failure), session.text() if session else "", home
    finally:
        if session:
            session.close()
            if session.exit_status is None and session.signal is None:
                try:
                    os.kill(session.pid, signal.SIGKILL)
                    os.waitpid(session.pid, 0)
                except OSError:
                    pass
        if not keep:
            shutil.rmtree(home, ignore_errors=True)


def main():
    parser = argparse.ArgumentParser(description="Run hash-shell's interactive (pty) tests.",
                                     epilog=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("names", nargs="*", help="run only cases whose name contains one of these")
    parser.add_argument("--shell", default=DEFAULT_SHELL, help="shell binary (default: ./hash-shell)")
    parser.add_argument("--timeout", type=float, default=10.0, help="seconds to wait for a prompt or an expect")
    parser.add_argument("-v", "--verbose", action="store_true", help="print the transcript of passing cases too")
    parser.add_argument("--keep", action="store_true", help="keep each case's HOME directory and print its path")
    parser.add_argument("--list", action="store_true", help="list the cases and exit")
    args = parser.parse_args()

    cases = sorted(f for f in os.listdir(CASES) if f.endswith(".txt"))
    if args.names:
        cases = [c for c in cases if any(n in c for n in args.names)]
    if args.list:
        for case in cases:
            print(case[:-4])
        return 0
    if not cases:
        print("no cases match", file=sys.stderr)
        return 2
    shell = os.path.abspath(args.shell)
    if not os.access(shell, os.X_OK):
        print("not executable: %s (build hash first or pass --shell)" % shell, file=sys.stderr)
        return 2

    passed = failed = 0
    for case in cases:
        name = case[:-4]
        failure, transcript, home = run_case(os.path.join(CASES, case), shell, args.timeout, args.keep)
        if failure is None:
            passed += 1
            print("PASS %s" % name)
        else:
            failed += 1
            print("FAIL %s: %s" % (name, failure))
        if failure is not None or args.verbose:
            for line in transcript.splitlines():
                print("    | " + line)
            if args.keep:
                print("    HOME kept at %s" % home)
    print("\nInteractive tests: %d passed, %d failed" % (passed, failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
