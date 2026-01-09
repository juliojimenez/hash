# FreeBSD Port for Hash Shell

This directory contains the files needed to create a FreeBSD port for hash-shell.

## Files

- `Makefile` - The port Makefile
- `pkg-descr` - Package description

## Submitting to FreeBSD Ports

### Prerequisites

You need access to a FreeBSD system (physical or VM) with the ports tree installed.

### Step 1: Set Up Ports Tree

```bash
# Using git (recommended)
cd /usr
sudo git clone https://git.FreeBSD.org/ports.git /usr/ports

# Or using portsnap
sudo portsnap fetch extract
```

### Step 2: Create Port Directory

```bash
sudo mkdir -p /usr/ports/shells/hash-shell
sudo cp Makefile pkg-descr /usr/ports/shells/hash-shell/
cd /usr/ports/shells/hash-shell
```

### Step 3: Generate distinfo

```bash
sudo make makesum
```

This downloads the source and generates checksums in `distinfo`.

### Step 4: Test the Port

```bash
# Install portlint
sudo pkg install portlint

# Lint check
portlint -AC

# Build
sudo make clean
sudo make

# Test staging
sudo make stage
sudo make stage-qa

# Create package without installing
sudo make package

# Test install/deinstall cycle
sudo make install
hash-shell --version  # or just run it
sudo make deinstall
```

### Step 5: Create Submission Patch

```bash
cd /usr/ports

# Add files to git
sudo git add shells/hash-shell

# Review changes
git diff --staged

# Create patch file
git diff --staged > ~/hash-shell-port.diff
```

### Step 6: Submit to FreeBSD Bugzilla

1. Go to https://bugs.freebsd.org/submit/
2. Select:
   - **Product**: Ports & Packages
   - **Component**: Individual Port(s)
3. Fill in:
   - **Summary**: `shells/hash-shell: New port - Modern shell with git integration`
   - **Description**: Brief description of the shell and its features
4. Attach: `hash-shell-port.diff`
5. Add keyword: `new-port`

### Timeline

- Initial review: days to weeks
- You may be asked for changes
- Once approved, committed to ports tree
- Available via `pkg` after next quarterly release (or immediately from ports)

## Updating the Port

When releasing a new version:

1. Update `DISTVERSION` in `Makefile`
2. Run `make makesum` to regenerate `distinfo`
3. Test as above
4. Submit update PR to Bugzilla

## Resources

- [FreeBSD Porter's Handbook](https://docs.freebsd.org/en/books/porters-handbook/)
- [Quick Porting Guide](https://docs.freebsd.org/en/books/porters-handbook/quick-porting/)
- [FreeBSD Bugzilla](https://bugs.freebsd.org/)
