# Security Backporting Guide

Internal reference for backporting security fixes to supported versions.

## Versioning Scheme

| Version | Type |
|---------|------|
| v18 | Regular release |
| v18.1 | Security patch for v18 |
| v18.2 | Additional security patch for v18 |
| v19 | Regular release |

- **Linear versioning** (v18, v19, v20) for regular releases
- **Patch versioning** (v18.1, v18.2) only for security updates

## Supported Versions

Per SECURITY.md, security updates are provided for the **last 5 major versions**.

Example: If current is v22, supported versions are v18-v22.

## Creating Release Branches

Release branches can be created on-demand when a vulnerability is reported.

### From Existing Tags

```bash
git fetch --tags

# Create release branch from tag
git checkout v18 && git checkout -b release/v18
git push origin release/v18
```

### At Release Time (Recommended)

```bash
# When releasing a new version
git checkout main
git tag v23
git checkout -b release/v23
git push origin v23 release/v23
```

## Backporting Workflow

### 1. Acknowledge and Verify

- Respond to reporter within 48 hours
- Reproduce the vulnerability
- Assess severity and affected versions

### 2. Fix on Main

```bash
git checkout main
git checkout -b security/CVE-YYYY-XXXXX

# Make the fix
vim src/affected_file.c

# Test thoroughly
make clean && make && make test
./test.sh

# Commit with detailed message
git commit -m "fix: description of vulnerability fix (CVE-YYYY-XXXXX)

- What was fixed
- How it was fixed
- Severity level

Reported-by: Reporter Name <email>
"

# Merge to main
git checkout main
git merge security/CVE-YYYY-XXXXX
git push origin main
```

### 3. Determine Affected Versions

```bash
# Check if vulnerability exists in each supported version
git checkout v21
grep -n "vulnerable_pattern" src/affected_file.c

git checkout v20
grep -n "vulnerable_pattern" src/affected_file.c

# ... repeat for each version
```

### 4. Backport to Affected Versions

```bash
# Get the fix commit hash
FIX_COMMIT=$(git log main -1 --format="%H")

# For each affected version (newest to oldest)
git checkout release/v21
git cherry-pick $FIX_COMMIT

# If conflicts occur, resolve manually
# The fix logic may need adjustment for older code
vim src/affected_file.c
git add src/affected_file.c
git cherry-pick --continue

# Test on this version
make clean && make && make test

# Repeat for v20, v19, v18...
```

### 5. Tag Security Releases

```bash
git checkout release/v21
git tag -a v21.1 -m "Security release: CVE-YYYY-XXXXX"

git checkout release/v20
git tag -a v20.1 -m "Security release: CVE-YYYY-XXXXX"

# ... repeat for each patched version

# Tag main as well
git checkout main
git tag -a v22.1 -m "Security release: CVE-YYYY-XXXXX"
```

### 6. Push Everything

```bash
# Push release branches
git push origin release/v18 release/v19 release/v20 release/v21

# Push tags
git push origin v18.1 v19.1 v20.1 v21.1 v22.1

# Or push all tags at once
git push origin --tags
```

### 7. Create GitHub Releases

For each security tag:

1. Go to **Releases** → **Draft a new release**
2. Select the security tag (e.g., v21.1)
3. Title: `v21.1 - Security Release`
4. Body:
   ```
   ## Security Fix
   
   This release addresses CVE-YYYY-XXXXX.
   
   **Severity:** HIGH/MEDIUM/LOW
   **Affected versions:** v18-v22
   
   All users on v21 should upgrade to v21.1 immediately.
   
   See [Security Advisory](link) for details.
   ```
5. Check "This is a pre-release" if appropriate
6. Publish

### 8. Create GitHub Security Advisory

1. Go to **Security** → **Advisories** → **New draft advisory**
2. Fill in:
   - **Ecosystem:** Other
   - **Package name:** hash-shell
   - **Affected versions:** >= v18, <= v22
   - **Patched versions:** v18.1, v19.1, v20.1, v21.1, v22.1
   - **Severity:** Select appropriate level
   - **CVE:** Request or enter if assigned
   - **Description:** Detailed vulnerability description
   - **Credits:** Acknowledge reporter
3. Publish when patches are available

### 9. Notify Users

- Update Rede Livre newsletter
- Consider adding notice to README temporarily for critical issues

## Handling Cherry-Pick Conflicts

When code has diverged significantly:

```bash
git cherry-pick $FIX_COMMIT
# CONFLICT in src/parser.c

# Open the file and look for conflict markers
vim src/parser.c

# <<<<<<< HEAD
# (old version code)
# =======
# (new fix code)
# >>>>>>> (commit)

# Manually apply the fix logic to the old code structure
# The exact code may differ, but the fix approach should be the same

# Test thoroughly after resolving
make clean && make && make test
./test.sh

git add src/parser.c
git cherry-pick --continue
```

## If a Version Cannot Be Patched

Sometimes a fix requires infrastructure that doesn't exist in older versions:

1. Document why the version cannot be patched
2. Recommend users upgrade to a patchable version
3. Note this in the security advisory
4. Consider dropping that version from support

## Subsequent Security Patches

If v18.1 already exists and a new vulnerability is found:

```bash
git checkout release/v18
# release/v18 should already point to v18.1

git cherry-pick $NEW_FIX_COMMIT
make test

git tag -a v18.2 -m "Security release: CVE-YYYY-ZZZZZ"
git push origin release/v18 v18.2
```

## Quick Reference

```bash
# Full backport sequence
FIX_COMMIT=$(git log main -1 --format="%H")

for version in 21 20 19 18; do
    echo "=== Backporting to v$version ==="
    git checkout release/v$version || git checkout v$version -b release/v$version
    git cherry-pick $FIX_COMMIT
    make clean && make && make test
    git tag -a v$version.1 -m "Security release"
done

git push origin release/v18 release/v19 release/v20 release/v21
git push origin v18.1 v19.1 v20.1 v21.1
```

## Checklist

- [ ] Vulnerability acknowledged to reporter
- [ ] Vulnerability reproduced and verified
- [ ] Severity assessed
- [ ] Affected versions identified
- [ ] Fix developed and tested on main
- [ ] Fix merged to main
- [ ] Release branches created (if needed)
- [ ] Fix cherry-picked to each affected version
- [ ] Each backport tested
- [ ] Security tags created
- [ ] Branches and tags pushed
- [ ] GitHub releases created
- [ ] GitHub security advisory published
- [ ] Users notified
- [ ] Reporter credited and thanked

## Timeline Goals

| Action | Target |
|--------|--------|
| Acknowledge report | 48 hours |
| Reproduce and assess | 72 hours |
| Patch critical vulnerabilities | 7 days |
| Patch high vulnerabilities | 14 days |
| Patch medium/low vulnerabilities | 30 days |

## Contact

Security reports: See SECURITY.md for contact information.
