# Security Policy

## Supported Versions

The last 5 major versions receive security updates:

| Version | Supported |
|---------|-----------|
| v33.x | Yes |
| v32.x | Yes |
| v31.x | Yes |
| v30.x | Yes |
| v29.x | Yes |
| < v29 | No |

## Reporting Vulnerabilities

**Do NOT report security vulnerabilities through public issues.**

Report via: **Settings > Security > Advisories > New draft security advisory**

Include:
- Type of vulnerability
- Affected source files
- Steps to reproduce
- Proof-of-concept (if possible)
- Impact assessment

### Response Timeline

- **Acknowledgment**: 48 hours
- **Initial Assessment**: 7 days
- **Resolution**: 30 days (critical)

## Security Best Practices

### For Users

1. Keep hash updated
2. Review .hashrc from untrusted sources
3. Set proper permissions on config files (600)
4. Use leading space for sensitive commands

### For Contributors

1. Use safe string functions (`safe_strcpy`, `safe_strcat`)
2. Validate all array indices and buffer sizes
3. Never trust user input
4. Use reentrant functions (`getpwuid_r` not `getpwuid`)
5. Free all allocated memory
6. Pass SonarQube analysis

## Security Features

Hash includes:

- Safe string utilities (bounds-checked)
- Input sanitization in parser
- History privacy (space prefix)
- No unsafe C functions (`gets`, `strcpy`, etc.)
- Stack protection (`-fstack-protector-strong`)
- Fortify source (`-D_FORTIFY_SOURCE=2`)

## Known Limitations

As a shell, hash executes arbitrary commands by design:

- Commands run with user privileges
- Aliases can execute arbitrary code
- Sourced scripts run in current context
