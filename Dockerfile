# syntax=docker/dockerfile:1

# =============================================================================
# Stage 1: Builder
# =============================================================================
FROM debian:trixie-slim AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /build

# Copy source files
COPY src/ src/
COPY Makefile .

# Build the shell
RUN make clean && make

# =============================================================================
# Stage 2: Runtime
# =============================================================================
FROM debian:trixie-slim

# Add labels for container metadata
LABEL org.opencontainers.image.title="hash-shell"
LABEL org.opencontainers.image.description="POSIX-compliant command line interpreter"
LABEL org.opencontainers.image.source="https://github.com/juliojimenez/hash"

# Copy binary from builder
COPY --from=builder /build/hash-shell /usr/local/bin/hash-shell

# Copy man page (optional documentation)
COPY debian/hash-shell.1 /usr/local/share/man/man1/hash-shell.1

# Set hash-shell as the default shell
ENV SHELL=/usr/local/bin/hash-shell

# Support both interactive and script execution:
# - Interactive: docker run -it hash-shell
# - Script: docker run hash-shell -c "echo hello"
# - File: docker run -v ./script.sh:/script.sh hash-shell /script.sh
ENTRYPOINT ["/usr/local/bin/hash-shell"]
CMD ["-l"]
