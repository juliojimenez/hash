# Installation Overview

Hash can be installed on Linux, macOS, and FreeBSD through various methods.

## Quick Install

| Platform | Method | Command |
|----------|--------|---------|
| macOS | Homebrew | `brew install juliojimenez/hash/hash-shell` |
| Debian/Ubuntu | APT | [See APT instructions](apt.md) |
| All platforms | From source | `make && sudo make install` |
| Docker | Pull image | `docker pull juliojimenez/hash-shell` |

## Choose Your Installation Method

- **[Linux](linux.md)** - Binary releases for x86_64 and ARM64
- **[macOS](macos.md)** - Homebrew or binary release
- **[FreeBSD](freebsd.md)** - Binary release or ports
- **[APT Repository](apt.md)** - Debian/Ubuntu with automatic updates
- **[Docker](docker.md)** - Containerized hash shell
- **[From Source](source.md)** - Build from source code

## System Requirements

- 64-bit operating system (x86_64 or ARM64)
- Linux kernel 3.2+ / macOS 10.15+ / FreeBSD 12+
- libc (glibc on Linux, system libc on macOS/BSD)
- Terminal with ANSI color support (optional, for colored output)

## Verify Installation

After installation, verify hash is working:

```bash
hash-shell --version
# hash v33

hash-shell
# hash v33
# Type 'exit' to quit
#>
```

## Next Steps

After installing, see [Getting Started](../getting-started/README.md) to configure your shell.
