# Update Notifications in Hash Shell

Hash shell can check for and install updates automatically, keeping you on the latest version with the newest features and bug fixes.

## Quick Start

Check for updates manually:

```bash
#> update
Checking for updates...
Update available!
  Current version: v18
  Latest version:  v19
  Release notes:   https://github.com/juliojimenez/hash/releases/tag/v19

Do you want to update now? [y/N] y
Downloading v19...
Verifying checksum...
Checksum verified
Installing to /usr/local/bin/hash-shell...
Successfully updated to v19!

Restart your shell to use the new version.
```

## Commands

### update

Check for and install updates:

```bash
#> update
```

### update --check

Check for updates without installing:

```bash
#> update --check
Update available!
  Current version: v18
  Latest version:  v19
  Release notes:   https://github.com/juliojimenez/hash/releases/tag/v19

Run 'update' to install the update.
```

### update --help

Show help:

```bash
#> update --help
Usage: update [options]

Check for and install hash shell updates.

Options:
  -c, --check    Check for updates without installing
  -f, --force    Force update even if on latest version
  -h, --help     Show this help message
```

## Automatic Update Checks

Hash automatically checks for updates once every 24 hours when you start an interactive session. If an update is available, you'll see a notification:

```
hash v18
Type 'exit' to quit

📦 Update available: v18 → v19
   Run 'update' to install, or 'update --check' for details.

#>
```

This check is non-blocking and happens in the background.

## Installation Methods

Hash intelligently detects how it was installed and provides appropriate update instructions.

### Direct Download / make install

If you installed hash from a GitHub release or via `make install`, the `update` command will:

1. Download the new binary from GitHub
2. Verify the SHA256 checksum
3. Install to the same location (may require `sudo`)

```bash
#> update
Checking for updates...
Update available!
  Current version: v18
  Latest version:  v19

Do you want to update now? [y/N] y
Downloading v19...
Verifying checksum...
Checksum verified
Installing to /usr/local/bin/hash-shell...
Successfully updated to v19!
```

### Package Manager Installations

If hash was installed via a package manager, the `update` command will show you the appropriate command to use:

**APT (Debian/Ubuntu):**
```bash
#> update
hash was installed via apt (Debian/Ubuntu)

To update, use your package manager:

  sudo apt update && sudo apt upgrade hash-shell
```

**Homebrew (macOS):**
```bash
#> update
hash was installed via Homebrew

To update, use your package manager:

  brew upgrade hash-shell
```

**DNF (Fedora):**
```bash
#> update
hash was installed via dnf (Fedora)

To update, use your package manager:

  sudo dnf upgrade hash-shell
```

Supported package managers (**pending**):
- APT (Debian/Ubuntu)
- YUM (RHEL/CentOS)
- DNF (Fedora)
- Homebrew (macOS)
- Pacman (Arch Linux)
- Zypper (openSUSE)
- PKG (FreeBSD)
- Snap
- Flatpak

## Platform Support

The update system automatically detects your operating system and architecture:

| Platform | Architecture | Binary Name |
|----------|--------------|-------------|
| Linux | x86_64 | `hash-shell-vN-linux-x86_64` |
| Linux | ARM64 | `hash-shell-vN-linux-aarch64` |
| macOS | ARM64 (Apple Silicon) | `hash-shell-vN-darwin-arm64` |

## Security

### Checksum Verification

All downloads are verified against SHA256 checksums published with each release:

```
Downloading v19...
Verifying checksum...
Checksum verified
```

If verification fails, you'll see a warning but the update will proceed (you can cancel if concerned).

### HTTPS Only

All downloads use HTTPS from `github.com`.

### No Auto-Updates

Hash never updates itself without your explicit consent. The automatic check only notifies you; installation always requires confirmation.

## Configuration

### Disabling Update Checks

To disable automatic startup update checks, add to your `~/.hashrc`:

```bash
export HASH_DISABLE_UPDATE_CHECK=1
```

Or set it in your environment before starting hash.

### Check Interval

By default, hash checks for updates once every 24 hours. The timestamp of the last check is stored in `~/.hash_update_state`.

## Troubleshooting

### "Failed to check for updates"

This usually means:
1. No internet connection
2. `curl` is not installed
3. GitHub API is unreachable

Install curl if needed:
```bash
# Debian/Ubuntu
sudo apt install curl

# macOS (usually pre-installed)
# Already available

# Fedora
sudo dnf install curl
```

### "Could not determine installation path"

Hash couldn't find where it's installed. This might happen if:
- Running from an unusual location
- The binary was renamed

Try specifying the path manually and reinstalling.

### Permission Denied During Install

If you see permission errors during update:

```
Installing to /usr/local/bin/hash-shell...
mv: cannot move '/tmp/hash-shell-update-12345' to '/usr/local/bin/hash-shell': Permission denied
```

The update command will try to use `sudo` automatically if needed. Make sure you have sudo privileges.

### Update Stuck or Slow

The download depends on your internet connection. For large downloads or slow connections, you might see delays. You can cancel with Ctrl+C and try again later.

## How It Works

1. **Version Check**: Hash queries the GitHub API for the latest release tag
2. **Comparison**: Compares current version against latest
3. **Platform Detection**: Determines your OS and architecture
4. **Download**: Fetches the appropriate binary from GitHub releases
5. **Verification**: Checks SHA256 checksum
6. **Installation**: Moves new binary to install location
7. **Cleanup**: Removes temporary files

## Files

| File | Purpose |
|------|---------|
| `~/.hash_update_state` | Tracks last update check time |

## See Also

- [Installation Guide](../README.md#install)
- [GitHub Releases](https://github.com/juliojimenez/hash/releases)

Back to [README](../README.md)
