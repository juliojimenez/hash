# Debian/APT Packaging

This directory contains Debian packaging files for building `.deb` packages.

## Files

| File | Purpose |
|------|---------|
| `control` | Package metadata, dependencies, description |
| `changelog` | Version history and release notes |
| `rules` | Build instructions (Makefile) |
| `copyright` | License information (Apache 2.0) |
| `compat` | Debhelper compatibility level |
| `source/format` | Source package format |
| `hash-shell.1` | Man page |
| `hash-shell.install` | Installation paths |
| `hash-shell.manpages` | Man page registration |

## Building Locally

```bash
# Install dependencies
sudo apt install build-essential debhelper devscripts dpkg-dev

# Build (from repository root)
dpkg-buildpackage -us -uc -b

# Package will be at ../hash-shell_*.deb
sudo dpkg -i ../hash-shell_*.deb
```

## GitHub Actions

The `.github/workflows/debian.yml` workflow:

1. Builds packages for Ubuntu 22.04, Ubuntu 24.04, and Debian Bookworm
2. Publishes an APT repository to GitHub Pages
3. Uploads `.deb` files to GitHub Releases

### Enabling GitHub Pages

For the APT repository to work, GitHub Pages must be configured:

1. Go to repository Settings → Pages
2. Set Source to "GitHub Actions"
3. The workflow will deploy automatically on release

## User Installation

Once published, users can install with:

```bash
echo "deb [trusted=yes] https://juliojimenez.github.io/hash/ stable main" | sudo tee /etc/apt/sources.list.d/hash-shell.list
sudo apt update
sudo apt install hash-shell
```

## Updating the Package

When releasing a new version:

1. Update version in `changelog` (or let the workflow handle it via tag)
2. Create a new GitHub release with tag `vXX`
3. The workflow builds and publishes automatically

## Version Numbering

The package version follows: `VERSION-REVISION~CODENAME`

- `VERSION` - From git tag (e.g., `21`)
- `REVISION` - Debian revision (usually `1`)
- `CODENAME` - Distribution codename (e.g., `jammy`, `noble`, `bookworm`)

Example: `21-1~noble` for version 21 on Ubuntu 24.04
