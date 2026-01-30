# syntax=docker/dockerfile:1

# =============================================================================
# Stage 1: Builder (Alpine/musl)
# =============================================================================
FROM alpine:edge AS builder

# Install build dependencies
RUN apk add --no-cache build-base

# Set working directory
WORKDIR /build

# Copy source files
COPY src/ src/
COPY Makefile .

# Build the shell
RUN make clean && make

# =============================================================================
# Stage 2: Runtime (Alpine - minimal with utilities)
# =============================================================================
FROM alpine:edge

# Add labels for container metadata
LABEL org.opencontainers.image.title="hash-shell"
LABEL org.opencontainers.image.description="A modern command line interpreter for Linux, macOS, and BSD."
LABEL org.opencontainers.image.source="https://github.com/juliojimenez/hash"

# Copy binary from builder
COPY --from=builder /build/hash-shell /usr/local/bin/hash-shell

# Set hash-shell as the default shell
ENV SHELL=/usr/local/bin/hash-shell

# Support both interactive and script execution:
# - Interactive: docker run -it hash-shell
# - Script: docker run hash-shell -c "echo hello"
# - File: docker run -v ./script.sh:/script.sh hash-shell /script.sh
ENTRYPOINT ["/usr/local/bin/hash-shell"]
CMD ["-l"]
