# Installing Hash Shell via APT

Hash shell can be installed on Debian and Ubuntu systems using APT with automatic updates.

## APT Repository (Recommended)

Add the Hash Shell repository for automatic updates via `apt upgrade`:

```bash
# Add the repository
echo "deb [trusted=yes] https://juliojimenez.github.io/hash/ stable main" | sudo tee /etc/apt/sources.list.d/hash-shell.list

# Update package list
sudo apt update

# Install hash-shell
sudo apt install hash-shell
```

### Upgrade

Once installed via the repository, upgrade like any other package:

```bash
sudo apt update
sudo apt upgrade hash-shell
```

Or upgrade all packages:

```bash
sudo apt update
sudo apt upgrade
```

## Direct Download (Alternative)

Download and install the `.deb` package directly from GitHub releases:

### Ubuntu 24.04 (Noble)

```bash
curl -LO https://github.com/juliojimenez/hash/releases/latest/download/hash-shell_*~noble_amd64.deb
sudo dpkg -i hash-shell_*~noble_amd64.deb
```

### Ubuntu 22.04 (Jammy)

```bash
curl -LO https://github.com/juliojimenez/hash/releases/latest/download/hash-shell_*~jammy_amd64.deb
sudo dpkg -i hash-shell_*~jammy_amd64.deb
```

### Debian 12 (Bookworm)

```bash
curl -LO https://github.com/juliojimenez/hash/releases/latest/download/hash-shell_*~bookworm_amd64.deb
sudo dpkg -i hash-shell_*~bookworm_amd64.deb
```

**Note:** Direct downloads won't receive automatic updates. Use the APT repository method above for automatic updates.

## Verify Installation

```bash
# Start hash shell
hash-shell

# You should see:
# hash v29
# Type 'exit' to quit
```

## Set as Default Shell

```bash
# Add to /etc/shells if not present
echo "/usr/bin/hash-shell" | sudo tee -a /etc/shells

# Change your default shell
chsh -s /usr/bin/hash-shell
```

Log out and log back in for changes to take effect.

## Uninstall

```bash
# Remove the package
sudo apt remove hash-shell

# Remove repository (optional)
sudo rm /etc/apt/sources.list.d/hash-shell.list
sudo apt update
```

## Building From Source

If you prefer to build from source:

```bash
# Install build dependencies
sudo apt install build-essential git

# Clone and build
git clone https://github.com/juliojimenez/hash.git
cd hash
make
sudo make install
```

## Building Debian Package Locally

```bash
# Install packaging tools
sudo apt install build-essential debhelper devscripts

# Clone repository
git clone https://github.com/juliojimenez/hash.git
cd hash

# Build the package
dpkg-buildpackage -us -uc -b

# Install
sudo dpkg -i ../hash-shell_*.deb
```

## Supported Distributions

The APT repository provides packages for:

| Distribution | Codename | Architecture |
|--------------|----------|--------------|
| Ubuntu 24.04 | noble | amd64 |
| Ubuntu 22.04 | jammy | amd64 |
| Debian 12 | bookworm | amd64 |

ARM64 packages coming in a future release.

## Troubleshooting

### "Repository does not have a Release file"

Make sure you're using the correct URL:

```bash
# Check the source list
cat /etc/apt/sources.list.d/hash-shell.list

# Should contain:
# deb [trusted=yes] https://juliojimenez.github.io/hash/ stable main
```

### Package Not Found After Adding Repository

```bash
sudo apt update
```

### Dependency Issues

```bash
sudo apt --fix-broken install
```

### GPG Key Warning

The repository uses `[trusted=yes]` for simplicity. This is safe for personal use but means apt won't verify package signatures. If you prefer signed packages, let us know via GitHub issues.

### Permission Denied When Running

```bash
ls -la /usr/bin/hash-shell
# Should show: -rwxr-xr-x
```

## See Also

- [Main Installation Guide](../README.md#install)
- [Building from Source](../README.md#from-source)
- [Homebrew Installation](HOMEBREW.md) (macOS)

Back to [README](../README.md)
