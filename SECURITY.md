# Security Policy

## Supported Versions

The **last 5 major versions** of hash are supported with security updates:

| Version | Supported          |
| ------- | ------------------ |
| v21.x   | :white_check_mark: |
| v20.x   | :white_check_mark: |
| v19.x   | :white_check_mark: |
| v18.x   | :white_check_mark: |
| < v18   | :x:                |

## Reporting a Vulnerability

We take the security of hash seriously. If you discover a security vulnerability, please report it responsibly.

### How to Report

**Please do NOT report security vulnerabilities through public GitHub issues.**

Instead, please report them via **Security** (tab) > **Advisories** (left nav) > **New draft security advisory** (button)

Include the following information in your report:

- Type of vulnerability (e.g., buffer overflow, command injection, privilege escalation)
- Full path of the source file(s) related to the vulnerability
- Location of the affected source code (tag/branch/commit or direct URL)
- Step-by-step instructions to reproduce the issue
- Proof-of-concept or exploit code (if possible)
- Impact assessment and potential attack scenarios

### What to Expect

- **Acknowledgment:** You will receive an acknowledgment within 48 hours of your report.
- **Initial Assessment:** Within 7 days, we will provide an initial assessment of the report.
- **Updates:** We will keep you informed of our progress toward a fix and announcement.
- **Resolution:** We aim to resolve critical vulnerabilities within 30 days.

### After Resolution

- We will coordinate with you on the disclosure timeline
- You will be credited in the security advisory (unless you prefer to remain anonymous)

## Security Best Practices

### For Users

1. **Keep hash updated:** Always run the latest supported version
2. **Review your .hashrc:** Be cautious with aliases and scripts from untrusted sources
3. **File permissions:** Ensure `~/.hashrc` and `~/.hash_history` have appropriate permissions (600 recommended)
4. **Sensitive commands:** Use a leading space to prevent sensitive commands from being saved to history (when `HISTCONTROL=ignorespace` is set)

### For Contributors

1. **Use safe string functions:** Always use `safe_strcpy`, `safe_strcat`, `safe_strlen` instead of standard C string functions
2. **Bounds checking:** Validate all array indices and buffer sizes
3. **Input validation:** Never trust user input; validate and sanitize all external data
4. **Reentrant functions:** Use thread-safe/reentrant versions of system calls (e.g., `getpwuid_r` instead of `getpwuid`)
5. **Memory management:** Free all allocated memory and avoid use-after-free bugs
6. **Static analysis:** All code must pass SonarQube analysis without security warnings

## Security Features

Hash includes several security-conscious design decisions:

- **Safe string utilities:** Custom bounds-checked string functions prevent buffer overflows
- **Input sanitization:** Command parsing includes proper quote and escape handling
- **History privacy:** Commands prefixed with a space are not saved to history
- **No unsafe functions:** The codebase avoids `gets`, `strcpy`, `strcat`, `sprintf`, and other unsafe C functions
- **Stack protection:** Release builds use `-fstack-protector-strong`
- **Fortify source:** Release builds use `-D_FORTIFY_SOURCE=2`

## Known Limitations

As a shell, hash executes arbitrary commands by design. Users should be aware that:

- Any command entered will be executed with the user's privileges
- Aliases and configuration files can execute arbitrary code
- Scripts sourced with `source` command run in the current shell context

## Security Audits

The codebase undergoes continuous security analysis through:

- **SonarQube/SonarCloud:** Static code analysis on every push
- **GitHub Actions:** Security-focused CI checks including unsafe function detection
- **Compiler warnings:** All code compiles with `-Wall -Wextra -Werror`

## Acknowledgments

We thank the following individuals for responsibly disclosing security issues:

*No security issues have been reported yet.*

---

Back to [README](README.md)
