# Building from Source

Build hash shell from source code on any supported platform.

## Prerequisites

- GCC or Clang compiler
- Make build system
- Git (for cloning)

### Debian/Ubuntu

```bash
sudo apt install build-essential git
```

### macOS

Install Xcode Command Line Tools:

```bash
xcode-select --install
```

### FreeBSD

```bash
pkg install git gmake
```

## Build Steps

```bash
# Clone the repository
git clone https://github.com/juliojimenez/hash.git
cd hash

# Build
make

# Install (to /usr/local/bin)
sudo make install
```

## Build Options

### Debug Build

Build with debug symbols:

```bash
make debug
```

### Clean Build

Remove build artifacts:

```bash
make clean
make
```

### Custom Installation Path

```bash
sudo make install PREFIX=/opt/hash
```

## Running Tests

```bash
# Setup test framework (first time only)
make test-setup

# Run unit tests
make test

# Run integration tests
./test.sh
```

## Verify Installation

```bash
hash-shell --version
hash-shell
```

## Uninstall

```bash
sudo make uninstall
```

## Development

For development setup, see the [Contributing Guide](../contributing/README.md).
