# Security Releases

Maintainer reference for shipping a security fix. hash is developed fix-forward
(see [SECURITY.md](../SECURITY.md)): the fix lands on `main` and goes out in the
next release, which every user is expected to upgrade to. There are no release
branches, no backports and no patch versions.

## Versioning

Releases are plain integers (`vNN`). A security fix does not get a `vNN.1`; it is
part of the next release, which may contain nothing else if the fix is urgent.

## Procedure

### 1. Acknowledge and verify

- Respond to the reporter within 48 hours through the draft advisory they opened
  (**Security** → **Advisories**), or open one for them if the report came another way.
- Reproduce the problem on the latest release and on `main`.
- Assess severity and note which releases are affected; this goes into the advisory.

### 2. Fix on main

Work in a branch and open a pull request like any other change, with a test that
fails before the fix. Keep the description factual but free of exploitation detail
until the advisory is published. If the fix must stay private until release, use
the advisory's temporary private fork instead of a public branch.

```bash
make clean && make CFLAGS="-Wall -Wextra -Werror -std=gnu99"
make test && ./test.sh
```

The sanitizer, Valgrind and fuzzing workflows must stay green on the pull request.

### 3. Release

Cut the next release as described in "Cutting a Release" in
[CONTRIBUTING.md](../CONTRIBUTING.md):

```bash
make release VERSION=NN          # bumps every version string, writes debian/changelog
# merge the release pull request, then
git fetch origin && git tag -a vNN -m vNN origin/main && git push origin vNN
```

The tag push builds the binaries, creates the GitHub release and runs the Debian,
Docker and Homebrew workflows. Check all four finished before announcing.

### 4. Publish the advisory

In the draft advisory set:

- **Ecosystem:** Other; **Package name:** hash-shell
- **Affected versions:** `<= vNN-1` (or the exact range if the bug is newer)
- **Patched versions:** `vNN`
- **Severity**, a CVE if one was requested, a description, and credit for the reporter
  unless they asked to stay anonymous

Publish it once the release is available on every channel.

### 5. Notify

- Post in GitHub Discussions (Announcements) with the advisory link and the one-line
  upgrade command for each channel.
- For a critical issue, add a temporary notice to the top of `README.md` and remove it
  with the following release.

## Timeline goals

| Action | Target |
|--------|--------|
| Acknowledge report | 48 hours |
| Reproduce and assess | 72 hours |
| Release a fix for a critical vulnerability | 7 days |
| Release a fix for a high vulnerability | 14 days |
| Release a fix for a medium or low vulnerability | 30 days |

## Checklist

- [ ] Report acknowledged and reproduced
- [ ] Severity and affected releases recorded in the draft advisory
- [ ] Fix merged to `main` with a test
- [ ] Release `vNN` tagged; binaries, Debian, Docker and Homebrew all green
- [ ] Advisory published with patched version `vNN`
- [ ] Announcement posted; reporter credited
