# Safe String Utilities

Hash shell includes a set of safe string handling functions designed to prevent buffer overflows and satisfy static analysis tools like SonarQube.

## Why Safe String Functions?

Standard C string functions have issues:

| Function | Problem | Safe Alternative |
|----------|---------|------------------|
| `strcpy` | No bounds checking | `safe_strcpy` |
| `strcat` | No bounds checking | `safe_strcat` |
| `strncpy` | May not null-terminate | `safe_strcpy` |
| `strlen` | Can read past buffer if not null-terminated | `safe_strlen` |
| `strcmp` | No length limit | `safe_strcmp` |

## API Reference

### safe_strcpy

```c
size_t safe_strcpy(char *dst, const char *src, size_t size);
```

**Safe alternative to `strcpy` and `strncpy`.**

**Features:**
- Always null-terminates
- Never overflows buffer
- Returns source length (for truncation detection)

**Parameters:**
- `dst` - Destination buffer
- `src` - Source string
- `size` - Size of destination buffer (including space for null terminator)

**Returns:**
- Length of source string (allows truncation detection)

**Example:**
```c
char buffer[10];

// Safe copy
safe_strcpy(buffer, "hello", sizeof(buffer));
// buffer = "hello\0"

// Handles truncation
size_t src_len = safe_strcpy(buffer, "hello world", sizeof(buffer));
// buffer = "hello wor\0"
// src_len = 11 (indicates truncation occurred)

if (src_len >= sizeof(buffer)) {
    // String was truncated!
}
```

**Why it's safe:**
```c
// Problem with strncpy:
strncpy(dst, src, size);
dst[size - 1] = '\0';  // Must manually null-terminate

// safe_strcpy guarantees null-termination:
safe_strcpy(dst, src, size);  // Always null-terminated
```

### safe_strlen

```c
size_t safe_strlen(const char *str, size_t maxlen);
```

**Bounds-checked string length.**

**Features:**
- Won't read past maxlen
- Handles NULL pointers
- Prevents unbounded memory reads

**Parameters:**
- `str` - String to measure
- `maxlen` - Maximum bytes to search

**Returns:**
- Length of string (up to maxlen)

**Example:**
```c
char buffer[100];
fgets(buffer, sizeof(buffer), fp);

// Safe length check
size_t len = safe_strlen(buffer, sizeof(buffer));

// Compare to unsafe:
// strlen(buffer);  // Could read past buffer if not null-terminated
```

### safe_strcat

```c
size_t safe_strcat(char *dst, const char *src, size_t size);
```

**Safe string concatenation.**

**Features:**
- Always null-terminates
- Won't overflow buffer
- Finds dst end safely

**Parameters:**
- `dst` - Destination buffer (with existing content)
- `src` - String to append
- `size` - Total size of destination buffer

**Returns:**
- Total length after append

**Example:**
```c
char buffer[20] = "hello";

safe_strcat(buffer, " world", sizeof(buffer));
// buffer = "hello world\0"

safe_strcat(buffer, " extra text", sizeof(buffer));
// buffer = "hello world ext\0" (truncated safely)
```

### safe_strcmp

```c
int safe_strcmp(const char *s1, const char *s2, size_t maxlen);
```

**Length-limited string comparison.**

**Features:**
- Won't compare beyond maxlen
- Handles NULL pointers
- Prevents unbounded reads

**Parameters:**
- `s1` - First string
- `s2` - Second string
- `maxlen` - Maximum characters to compare

**Returns:**
- 0 if equal (within maxlen)
- Non-zero if different

**Example:**
```c
// Compare up to 100 chars
if (safe_strcmp(str1, str2, 100) == 0) {
    printf("Strings match!\n");
}

// Compare prefixes
if (safe_strcmp(cmd, "help", 4) == 0) {
    // Matches "help", "helper", "helping", etc.
}
```

## Usage Patterns

### Pattern 1: Copying Untrusted Input

```c
// Bad
char buffer[100];
strcpy(buffer, user_input);  // Buffer overflow!

// Good
char buffer[100];
safe_strcpy(buffer, user_input, sizeof(buffer));
```

### Pattern 2: Building Strings Safely

```c
// Bad
char path[256];
strcpy(path, home_dir);
strcat(path, "/.config");
strcat(path, "/app.conf");  // Could overflow!

// Good
char path[256];
safe_strcpy(path, home_dir, sizeof(path));
safe_strcat(path, "/.config", sizeof(path));
safe_strcat(path, "/app.conf", sizeof(path));
```

### Pattern 3: Measuring Tainted Strings

```c
// Bad
char buffer[100];
fgets(buffer, sizeof(buffer), fp);
size_t len = strlen(buffer);  // What if buffer isn't null-terminated?

// Good
char buffer[100];
fgets(buffer, sizeof(buffer), fp);
buffer[sizeof(buffer) - 1] = '\0';  // Ensure null-termination
size_t len = safe_strlen(buffer, sizeof(buffer));
```

### Pattern 4: Detecting Truncation

```c
char buffer[20];
size_t src_len = safe_strcpy(buffer, long_string, sizeof(buffer));

if (src_len >= sizeof(buffer)) {
    fprintf(stderr, "Warning: String was truncated\n");
}
```

## Static Analyzer Benefits

These functions make static analyzers happy because:

1. **Explicit bounds checking** - Every function checks bounds explicitly
2. **Guaranteed null-termination** - No manual null-termination needed
3. **No undefined behavior** - All edge cases handled
4. **Clear intent** - Code shows what safety checks are being done

**SonarQube issues resolved:**
- ❌ "Array may overflow"
- ❌ "String may not be null-terminated"
- ❌ "Unbounded memory read"
- ❌ "Tainted index"

**After using safe functions:**
- ✅ Explicit bounds checking
- ✅ Guaranteed null-termination
- ✅ Bounded memory access
- ✅ Safe index usage

## Performance

These functions are **slightly slower** than raw string functions because of explicit bounds checking, but:

- Overhead is negligible for typical use cases
- Safety is worth the tiny performance cost
- Modern compilers optimize them well
- In critical paths, you can still use raw functions with careful manual checking

## Testing

The module includes comprehensive unit tests:

```bash
make test
```

Tests cover:
- ✅ Normal usage
- ✅ Truncation
- ✅ Empty strings
- ✅ NULL pointers
- ✅ Edge cases (size=1, etc.)
- ✅ Bounds checking

## When to Use

**Always use safe functions for:**
- User input
- File content
- Network data
- Environment variables
- Untrusted/tainted data

**Can use standard functions for:**
- String literals: `strcpy(buf, "literal")`
- Well-known bounded operations
- Performance-critical tight loops (with manual checking)

## Comparison with Other Approaches

### vs strlcpy/strlcat (BSD)

```c
// BSD (not portable)
strlcpy(dst, src, size);

// hash (portable)
safe_strcpy(dst, src, size);
```

Our functions work the same but are available everywhere.

### vs snprintf

```c
// snprintf for copying (works but verbose)
snprintf(dst, size, "%s", src);

// safe_strcpy (clearer intent)
safe_strcpy(dst, src, size);
```

### vs manual strncpy + null termination

```c
// Manual (error-prone)
strncpy(dst, src, size - 1);
dst[size - 1] = '\0';

// safe_strcpy (automatic)
safe_strcpy(dst, src, size);
```

## See Also

- [String Safety Best Practices](https://cwe.mitre.org/data/definitions/120.html)
- [CERT C Coding Standard STR31-C](https://wiki.sei.cmu.edu/confluence/display/c/STR31-C.+Guarantee+that+storage+for+strings+has+sufficient+space+for+character+data+and+the+null+terminator)

Back to [README](../README.md)

