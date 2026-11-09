#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <fnmatch.h>
#include "exec.h"
#include "varexpand.h"
#include "safe_string.h"
#include "hash.h"
#include "script.h"
#include "jobs.h"
#include "config.h"
#include "shellvar.h"
#include "ifs.h"
#include "utils.h"
#include "expand.h"

#define MAX_EXPANDED_LENGTH 8192

// A result may not fit the buffer sized from the input: the loop stops
// appending and the expansion is run again with a bigger one, up to this
#define MAX_EXPANSION_SIZE (256u * 1024u * 1024u)

// Track if an unset variable error occurred during expansion
static bool varexpand_error = false;

// Set while expanding a word that is never field-split (an assignment, a
// here-document body, a case word): $* joins with the first IFS character as
// a quoted "$*" would. Only the join is affected; quoting and escapes stay
// those of the word as written.
static bool no_field_splitting = false;

bool varexpand_set_no_split(bool on) {
    bool was = no_field_splitting;
    no_field_splitting = on;
    return was;
}

// Check and report unset variable error when -u is set
// Returns true if error occurred
static bool check_unset_error(const char *var_name) {
    if (shell_option_nounset()) {
        fprintf(stderr, "%s: %s: unbound variable\n", HASH_NAME, var_name);
        varexpand_error = true;
        return true;
    }
    return false;
}

// Get the last expansion error status
static pid_t shell_pid_value = 0;

void shell_pid_init(void) {
    shell_pid_value = getpid();
}

pid_t shell_pid(void) {
    return shell_pid_value > 0 ? shell_pid_value : getpid();
}

bool varexpand_had_error(void) {
    return varexpand_error;
}

// Clear the expansion error flag
void varexpand_clear_error(void) {
    varexpand_error = false;
}

// Check if character is valid in variable name
static int is_varname_char(char c) {
    return isalnum(c) || c == '_';
}

// Get positional parameter value ($1, $2, etc.)
// Returns NULL if not found
static const char *get_positional_param(int n) {
    if (n < 0 || n >= script_state.positional_count) {
        return NULL;
    }
    return script_state.positional_params[n];
}

// Add \x02 marker before each $ in word to indicate quoted context
// Returns a static buffer - caller should use immediately or copy
// A copy of word with every $ marked as quoted (\x02$): the expansions in it
// behave as inside double quotes, which is what a word in a context without
// field splitting (an assignment, a here-document, a case word) needs: "$*"
// joins with the first IFS character, "$@" keeps its fields, values are
// protected. A $ already marked, or protected as a literal (\x01$), is left.
char *varexpand_mark_quoted(const char *word) {
    size_t len = strlen(word);
    char *marked = malloc(len * 2 + 1);
    if (!marked) return NULL;
    size_t out = 0;
    for (const char *p = word; *p; p++) {
        if (*p == '$' && (out == 0 || (marked[out-1] != '\x02' && marked[out-1] != '\x01'))) {
            marked[out++] = '\x02';
        }
        marked[out++] = *p;
    }
    marked[out] = '\0';
    return marked;
}

static char *expand_default_word(const char *word, bool is_quoted, int last_exit_code);
static char *expand_pattern_word(const char *word, int last_exit_code);

// The special parameters that are not positional: $ ? ! - # @ *. They expand
// the same with and without braces, in and out of double quotes.
static bool is_special_param(char c) {
    return c != '\0' && strchr("$?!-#@*", c) != NULL;
}

// Copy a positional parameter into a "$*" / "$@" buffer with its marker-range
// bytes protected (\x01 + byte), so they are not read as the \x04 field
// separators the buffer uses. Returns the bytes written; stops when out of room.
static size_t copy_param_protected(char *dst, size_t room, const char *param) {
    size_t out = 0;
    for (const char *p = param; *p; p++) {
        size_t need = is_marker_byte(*p) ? 2 : 1;
        if (out + need > room) {
            break;
        }
        if (need == 2) {
            dst[out++] = '\x01';
        }
        dst[out++] = *p;
    }
    return out;
}

// Room for every positional parameter with each byte protected, a separator
// between them and the terminator
static size_t positional_params_room(void) {
    size_t room = 2;
    for (int i = 1; i < script_state.positional_count; i++) {
        const char *param = get_positional_param(i);
        room += (param ? 2 * strlen(param) : 0) + 1;
    }
    return room;
}

// Value of the special parameter c. Numbers are written to buf; "$*" and "$@"
// come back in a malloc'd buffer handed to *alloc (the caller frees it when
// the word is done) with the field separators the append step expects: \x04
// between fields, or the first character of IFS for a quoted "$*" (none when
// IFS is empty).
static const char *special_param_value(char c, bool is_quoted, int last_exit_code,
                                       char *buf, size_t bufsz, char **alloc) {
    switch (c) {
        case '?':   // exit status of the last command
            snprintf(buf, bufsz, "%d", last_exit_code);
            return buf;
        case '$':   // process ID of the shell, the same inside its subshells
            snprintf(buf, bufsz, "%d", (int)shell_pid());
            return buf;
        case '!': {  // process ID of the last background job, empty before any
            pid_t bg_pid = jobs_get_last_bg_pid();
            if (bg_pid > 0) {
                snprintf(buf, bufsz, "%d", bg_pid);
                return buf;
            }
            return "";
        }
        case '-':   // the option letters in effect
            shell_options_flags(buf, bufsz);
            return buf;
        case '#': {  // number of positional parameters (excluding $0)
            int count = script_state.positional_count > 0 ? script_state.positional_count - 1 : 0;
            snprintf(buf, bufsz, "%d", count);
            return buf;
        }
        case '*': {
            // POSIX: quoted "$*", or $* where no field splitting happens, joins
            // with the first character of IFS into one field; unquoted, each
            // parameter is its own field (\x04 separator), split again later
            size_t star_cap = positional_params_room();
            char *star_buf = malloc(star_cap);
            size_t pos = 0;
            char sep = '\x04';
            bool use_sep = true;
            if (!star_buf) {
                return "";
            }
            if (is_quoted || no_field_splitting) {
                const char *ifs = ifs_get();
                if (ifs[0] == '\0') {
                    use_sep = false;
                } else {
                    sep = ifs[0];
                }
            }
            for (int i = 1; i < script_state.positional_count && pos < star_cap - 1; i++) {
                const char *param = get_positional_param(i);
                if (param) {
                    if (i > 1 && use_sep && pos < star_cap - 1) {
                        star_buf[pos++] = sep;
                    }
                    pos += copy_param_protected(star_buf + pos, star_cap - 1 - pos, param);
                }
            }
            star_buf[pos] = '\0';
            *alloc = star_buf;
            return star_buf;
        }
        case '@': {
            // Quoted or not, each parameter is its own field (\x04 separator)
            size_t at_cap = positional_params_room();
            char *at_buf = malloc(at_cap);
            size_t pos = 0;
            if (!at_buf) {
                return "";
            }
            for (int i = 1; i < script_state.positional_count && pos < at_cap - 1; i++) {
                const char *param = get_positional_param(i);
                if (param) {
                    if (i > 1 && pos < at_cap - 1) {
                        at_buf[pos++] = '\x04';
                    }
                    pos += copy_param_protected(at_buf + pos, at_cap - 1 - pos, param);
                }
            }
            at_buf[pos] = '\0';
            *alloc = at_buf;
            return at_buf;
        }
        default:
            return NULL;
    }
}

// p points at the $ of a ${...}: the position just past its closing brace.
// The braces nest, and quotes inside count only for finding that brace, so
// a word of its own with quotes in it ("${x%%" "*}") does not end the
// segment the expansion sits in.
static const char *past_brace_expansion(const char *p) {
    p += 2;
    int depth = 1;
    bool sq = false, dq = false;
    while (*p && depth > 0) {
        if (*p == '\\' && p[1] && !sq) {
            p += 2;
            continue;
        }
        if (*p == '\'' && !dq) sq = !sq;
        else if (*p == '"' && !sq) dq = !dq;
        else if (!sq && !dq) {
            if (*p == '{') depth++;
            else if (*p == '}') depth--;
        }
        p++;
    }
    return p;
}

// An unprotected $ that opens a ${...}
static bool opens_brace_expansion(const char *word, const char *p) {
    return *p == '$' && p[1] == '{' && (p == word || p[-1] != '\x01');
}

// The word of ${var-word}, ${var:-word}, ${var+word}, ${var=word} and
// ${var?word}. The preprocessor copies brace contents raw, so the word still
// has its own quotes; they are honoured here. Single quotes make a literal
// segment; double quotes a quoted segment, whose expansions are literal and
// whose characters are protected so they never split fields or glob (this is
// what keeps each parameter of ${1+"$@"} whole); the rest expands and splits
// like any unquoted text. When the whole ${...} is in double quotes the word
// is one quoted segment. The result is marked text for the append step, which
// keeps its \x01 pairs and \x04 separators and drops its \x03 regions.
static char *expand_default_word(const char *word, bool is_quoted, int last_exit_code) {
    size_t cap = strlen(word) * 2 + 64;
    char *out = malloc(cap);
    if (!out) return NULL;
    size_t o = 0;
    // Room for `need` more bytes; the output grows with the expansions, not the word
    #define ROOM(need) do { \
        if (o + (need) + 1 > cap) { \
            size_t ncap = (o + (need) + 1) * 2; \
            char *grown = realloc(out, ncap); \
            if (!grown) { out[o] = '\0'; return out; } \
            out = grown; cap = ncap; \
        } \
    } while (0)
#define PUT(ch) do { ROOM(1); out[o++] = (ch); } while (0)

    // When the whole ${...} sits in double quotes, the word is double-quoted
    // text: a single quote in it is a character, not a quote ("${y:-'a'}" is
    // 'a' with the quotes, as in dash and bash; #408). Double quotes in it
    // still quote, as they do in those shells. The pattern words of
    // ${var#word} keep their quotes as quotes either way (expand_pattern_word).
    const char *p = word;
    while (*p) {
        const char *start;
        char *seg;
        if (*p == '\'' && !is_quoted) {
            // Literal segment: every character protected, nothing expands
            p++;
            start = p;
            while (*p && *p != '\'') p++;
            for (const char *q = start; q < p; q++) {
                PUT('\x01');
                PUT(*q);
            }
            if (*p) p++;
            continue;
        }
        bool dquote = (*p == '"');
        if (dquote) {
            p++;
            start = p;
            while (*p && *p != '"') {
                if (opens_brace_expansion(word, p)) {
                    p = past_brace_expansion(p);
                    continue;
                }
                if (*p == '\\' && p[1]) p++;
                p++;
            }
            seg = strndup(start, (size_t)(p - start));
            if (*p) p++;
        } else {
            // Unquoted run. A backslash escape \X is a literal X (quote removal):
            // protect it as \x01X before the expansion, which would otherwise keep
            // the backslash (`${t:+a\}b}` must give a}b) or fold \\ and \$ its own way
            start = p;
            while (*p && (is_quoted || *p != '\'') && *p != '"') {
                if (opens_brace_expansion(word, p)) {
                    p = past_brace_expansion(p);
                    continue;
                }
                if (*p == '\\' && p[1]) p++;
                p++;
            }
            // When the whole ${...} is in double quotes this run is double-quoted
            // text: only \$ \` \" \\ are escapes and any other backslash stays
            size_t n = (size_t)(p - start);
            seg = malloc(n * 3 + 1);
            if (seg) {
                size_t k = 0;
                for (const char *q = start; q < p; q++) {
                    if (*q == '\\' && q[1] && q + 1 < p) {
                        if (is_quoted && !char_in_string(q[1], "$`\"\\")) {
                            seg[k++] = '\x01';
                            seg[k++] = '\\';
                            continue;               // the backslash is literal; X follows on its own
                        }
                        seg[k++] = '\x01';
                        seg[k++] = *++q;
                    } else {
                        seg[k++] = *q;
                    }
                }
                seg[k] = '\0';
            }
        }
        if (!seg) break;
        // Quoted here, or the whole ${...} in quotes: expansions are literal and
        // the output never splits or globs. Otherwise it splits like any text.
        bool protect = dquote || is_quoted;
        char *marked = protect ? varexpand_mark_quoted(seg) : NULL;
        char *e = varexpand_expand(protect ? (marked ? marked : seg) : seg, last_exit_code);
        free(marked);
        free(seg);
        if (!e) continue;
        for (const char *q = e; *q; q++) {
            if (*q == '\x01' && q[1]) {
                PUT(*q);                // an already protected byte
                PUT(q[1]);
                q++;
            } else if (*q == '\x04') {
                PUT(*q);                // "$@" field separator
            } else if (*q == '\x03') {
                ;                       // a $(...) region: the word is one region (or none, quoted)
            } else if (protect) {
                PUT('\x01');
                PUT(*q);
            } else {
                PUT(*q);
            }
        }
        free(e);
    }
#undef PUT
#undef ROOM
    out[o] = '\0';
    return out;
}

// The word of ${var#word}, ${var##word}, ${var%word} and ${var%%word} as an
// fnmatch pattern. The word keeps its own quotes; it is expanded once (the
// command substitutions and arithmetic in it were already replaced by the
// earlier passes), and what came from quotes matches literally while the
// result of an unquoted expansion is pattern text, as POSIX describes.
static char *expand_pattern_word(const char *word, int last_exit_code) {
    size_t cap = strlen(word) * 2 + 64;
    char *out = malloc(cap);
    if (!out) return NULL;
    size_t o = 0;
    #define ROOM(need) do { \
        if (o + (need) + 1 > cap) { \
            size_t ncap = (o + (need) + 1) * 2; \
            char *grown = realloc(out, ncap); \
            if (!grown) { out[o] = '\0'; return out; } \
            out = grown; cap = ncap; \
        } \
    } while (0)
#define PUT(ch) do { ROOM(1); out[o++] = (ch); } while (0)
#define PUT_LITERAL(ch) do { if (char_in_string((ch), "*?[]\\")) PUT('\\'); PUT(ch); } while (0)

    const char *p = word;
    while (*p) {
        const char *start;
        char *seg;
        if (*p == '\'') {
            p++;
            start = p;
            while (*p && *p != '\'') p++;
            for (const char *q = start; q < p; q++) PUT_LITERAL(*q);
            if (*p) p++;
            continue;
        }
        bool dquote = (*p == '"');
        if (dquote) {
            p++;
            start = p;
            while (*p && *p != '"') {
                if (opens_brace_expansion(word, p)) {
                    p = past_brace_expansion(p);
                    continue;
                }
                if (*p == '\\' && p[1]) p++;
                p++;
            }
            seg = strndup(start, (size_t)(p - start));
            if (*p) p++;
        } else {
            // Unquoted run. A backslash escape \X is a literal X in the pattern
            // (`*\\` ends in one backslash): protect it as \x01X before the
            // expansion, which would otherwise fold \\ into \ and \$ into $
            start = p;
            while (*p && *p != '\'' && *p != '"') {
                if (opens_brace_expansion(word, p)) {
                    p = past_brace_expansion(p);
                    continue;
                }
                if (*p == '\\' && p[1]) p++;
                p++;
            }
            size_t n = (size_t)(p - start);
            seg = malloc(n * 2 + 1);
            if (seg) {
                size_t k = 0;
                for (const char *q = start; q < p; q++) {
                    if (*q == '\\' && q[1] && q + 1 < p) {
                        seg[k++] = '\x01';
                        seg[k++] = *++q;
                    } else {
                        seg[k++] = *q;
                    }
                }
                seg[k] = '\0';
            }
        }
        if (!seg) break;
        char *marked = dquote ? varexpand_mark_quoted(seg) : NULL;
        char *e = varexpand_expand(dquote ? (marked ? marked : seg) : seg, last_exit_code);
        free(marked);
        free(seg);
        if (!e) continue;
        for (const char *q = e; *q; q++) {
            if (*q == '\x01' && q[1]) {
                PUT_LITERAL(q[1]);          // protected: literal in the pattern
                q++;
            } else if (*q == '\x03' || *q == '\x04') {
                ;                           // field markers mean nothing in a pattern
            } else if (dquote) {
                PUT_LITERAL(*q);            // quoted text matches itself
            } else {
                PUT(*q);                    // unquoted: pattern text as written or expanded
            }
        }
        free(e);
    }
    out[o] = '\0';
    // A [ that never closes is a literal [ (bash and dash treat it so; fnmatch
    // would fail the whole pattern): escape it
    for (size_t i = 0; i < o; i++) {
        if (out[i] == '\\' && out[i + 1]) {
            i++;
            continue;
        }
        if (out[i] != '[') continue;
        size_t j = i + 1;
        if (out[j] == '!' || out[j] == '^') j++;
        if (out[j] == ']') j++;             // a ] right after the opening is a member
        while (j < o && out[j] != ']') j++;
        if (j < o) {
            i = j;                          // a bracket expression: skip it whole
            continue;
        }
        ROOM(1);
        memmove(out + i + 1, out + i, o - i);
        out[i] = '\\';
        o++;
        out[o] = '\0';
        i++;
    }
#undef PUT_LITERAL
#undef PUT
#undef ROOM
    out[o] = '\0';
    return out;
}

// Expand environment variables in a string
// One attempt with a buffer of result_size bytes. Every append stops at the
// end of the buffer, so *truncated reports input left unread or a result that
// reached the end; the caller then tries again with more room.
static char *expand_once(const char *str, int last_exit_code, size_t result_size, bool *truncated) {
    char *result = malloc(result_size);
    if (!result) return NULL;

    size_t out_pos = 0;
    const char *p = str;

    // Buffers one ${...} may need; they live until its value has been
    // appended. (They were static once, and a ${a#${b#c}} nested through
    // expand_pattern_word() wrote the inner expansion over the outer's.)
    char *word_buf = NULL;      // the word of ${var-word} and friends
    char *expanded_buf = NULL;  // that word expanded
    char *pattern_buf = NULL;   // the value being trimmed by # or %
    char *params_buf = NULL;    // "$*" or "$@"

    while (*p && out_pos < result_size - 1) {
        if (*p == '\x01' && *(p + 1) == '\\' && *(p + 2) == '\\') {
            // Protected double backslash from single quotes - output both literally
            result[out_pos++] = '\\';
            result[out_pos++] = '\\';
            p += 3;
            continue;
        } else if (*p == '\x01' && *(p + 1) == '\\' && char_in_string(*(p + 2), "$`")) {
            // A literal backslash (unquoted \\) before $ or `: keep the
            // backslash; what follows is expanded as usual
            result[out_pos++] = '\\';
            p += 2;
            continue;
        } else if (*p == '\\' && *(p + 1) == '\\') {
            // Double backslash outside single quotes - output one backslash and continue
            // This allows \\$ to become \ followed by expanded $var
            result[out_pos++] = '\\';
            p += 2;
            continue;
        } else if (char_in_string(*p, "\x01\\") && *(p + 1) == '$') {
            // Single-quoted or escaped dollar sign - output $ literally (no expansion)
            result[out_pos++] = '$';
            p += 2;
        } else if (*p == '\\' && *(p + 1) == '`') {
            // Escaped backtick - output ` literally (for heredocs)
            result[out_pos++] = '`';
            p += 2;
        } else if (*p == '\x01' && *(p + 1) && out_pos < result_size - 2) {
            // Any other protected byte: keep the pair whole so the byte after
            // the marker (a real \x02, say) is not read as a marker itself
            result[out_pos++] = *p++;
            result[out_pos++] = *p++;
        } else if ((*p == '\x02' && *(p + 1) == '$') || *p == '$') {
            // Variable expansion - check for quoted marker first
            bool is_quoted = (*p == '\x02');
            if (is_quoted) p++;  // Skip \x02 marker if present
            p++;  // Skip $

            const char *var_value = NULL;
            char var_name[256] = {0};
            bool is_literal_dollar = false;  // Track if this is just a literal $, not an expansion
            const char *premarked = NULL;    // a "$*"/"$@" buffer: its bytes are already protected
            const char *premarked_word = NULL;  // a ${var:-word} word: expanded text, markers and all

            // Special parameters ($ ? ! - # @ *). Inside double quotes the parser
            // puts \x02 before $ and \x01 before ? and *; skip that marker.
            const char *special = p;
            if ((*special == '\x01' || *special == '\x02') && *(special + 1)) {
                special++;
            }
            if (is_special_param(*special)) {
                var_value = special_param_value(*special, is_quoted, last_exit_code, var_name, sizeof(var_name), &params_buf);
                if (*special == '@' || *special == '*') {
                    premarked = var_value;
                }
                p = special + 1;
            } else if (*p == '0') {
                // $0 - script name or shell name
                const char *param0 = get_positional_param(0);
                var_value = param0 ? param0 : HASH_NAME;
                p++;
            } else if (*p == '{') {
                // ${VAR} or ${VAR-default} or ${VAR:-default} or ${#VAR} etc syntax
                p++;  // Skip {
                const char *brace_start = p;

                // Check for ${#VAR} - string length syntax
                bool get_length = false;
                if (*p == '#' && *(p + 1) == '}') {
                    // ${#} - same as $#, number of positional parameters
                    int count = script_state.positional_count > 0 ? script_state.positional_count - 1 : 0;
                    snprintf(var_name, sizeof(var_name), "%d", count);
                    var_value = var_name;
                    p += 2;  // Skip # and }
                    goto append_value;
                } else if (*p == '#') {
                    // ${#var} - get length of variable
                    get_length = true;
                    p++;  // Skip #
                }

                // The name: a special parameter (${$} ${?} ${!} ${-} ${@} ${*}, and
                // ${#X} for their lengths) is one character; otherwise letters,
                // digits and underscores
                size_t name_len = 0;
                bool is_special = false;
                const char *special = p;
                if ((*special == '\x01' || *special == '\x02') && *(special + 1)) {
                    special++;
                }
                if (is_special_param(*special) && *special != '#') {
                    var_name[0] = *special;
                    var_name[1] = '\0';
                    name_len = 1;
                    is_special = true;
                    p = special + 1;
                } else {
                    while (*p && is_varname_char(*p) && name_len < sizeof(var_name) - 1) {
                        var_name[name_len++] = *p++;
                    }
                    var_name[name_len] = '\0';
                }

                // Check for modifiers: - + = ? # % (and : prefix)
                char modifier = 0;
                bool check_null = false;
                bool double_modifier = false;  // For ## and %%
                size_t word_cap = strlen(p) + 1;   // the word cannot outgrow what follows
                word_buf = malloc(word_cap);
                if (!word_buf) {
                    break;
                }
                char *word = word_buf;
                word[0] = '\0';

                if (*p == ':') {
                    check_null = true;
                    p++;
                }

                if (char_in_string(*p, "-+=?#%")) {
                    modifier = *p++;
                    // Check for ## or %%
                    if (char_in_string(modifier, "#%") && *p == modifier) {
                        double_modifier = true;
                        p++;
                    }

                    // Parse the word/pattern until the closing brace; the word keeps
                    // its own quotes, and a } inside them does not close
                    size_t word_len = 0;
                    int brace_depth = 1;
                    bool w_single = false, w_double = false;
                    while (*p && brace_depth > 0 && word_len < word_cap - 1) {
                        if (*p == '\\' && p[1] && !w_single && word_len < word_cap - 2) {
                            word[word_len++] = *p++;
                            word[word_len++] = *p++;
                            continue;
                        }
                        if (*p == '\'' && !w_double) w_single = !w_single;
                        else if (*p == '"' && !w_single) w_double = !w_double;
                        else if (!w_single && !w_double) {
                            if (*p == '{') brace_depth++;
                            else if (*p == '}') {
                                brace_depth--;
                                if (brace_depth == 0) break;
                            }
                        }
                        word[word_len++] = *p++;
                    }
                    word[word_len] = '\0';
                }

                if (*p != '}') {
                    // ${.sh.version+s}, ${foo*}, ${1x}: not a parameter expansion
                    // (bash and dash reject it; hash used to print the text)
                    const char *close = strchr(p, '}');
                    size_t shown = close ? (size_t)(close - brace_start) + 1 : strlen(brace_start);
                    fprintf(stderr, "%s: ${%.*s: bad substitution\n", HASH_NAME, (int)shown, brace_start);
                    varexpand_error = true;
                    var_value = "";
                    p = close ? close + 1 : p + strlen(p);
                } else {
                    p++;  // Skip }

                    if (name_len > 0) {
                        // Get the variable value
                        const char *val = NULL;

                        // Check if it's a positional parameter
                        int is_positional = 0;
                        if (!is_special) {
                            is_positional = 1;
                            for (size_t i = 0; i < name_len; i++) {
                                if (!isdigit(var_name[i])) {
                                    is_positional = 0;
                                    break;
                                }
                            }
                        }
                        static char special_buf[256];
                        if (is_special) {
                            val = special_param_value(var_name[0], is_quoted, last_exit_code,
                                                      special_buf, sizeof(special_buf), &params_buf);
                            if (var_name[0] == '@' || var_name[0] == '*') {
                                premarked = val;
                            }
                        } else if (is_positional) {
                            int param_num = atoi(var_name);
                            val = get_positional_param(param_num);
                        } else if (strcmp(var_name, "LINENO") == 0) {
                            val = exec_lineno_string();
                        } else {
                            val = shellvar_get(var_name);
                        }

                        // Determine if variable is "unset" or "null"
                        bool is_unset = (val == NULL);
                        bool is_null = (val != NULL && val[0] == '\0');

                        // Apply modifier
                        // Note: POSIX requires the word to be expanded when used
                        const char *effective_word = word;

                        if (modifier == '-') {
                            // ${var-word}: use word if unset
                            // ${var:-word}: use word if unset or null
                            if (is_unset || (check_null && is_null)) {
                                // Expand the word with its own quotes honoured
                                char *expanded_word = expand_default_word(word, is_quoted, last_exit_code);
                                if (expanded_word) {
                                    expanded_buf = expanded_word;
                                    effective_word = expanded_buf;
                                }
                                var_value = effective_word;
                                premarked_word = effective_word;
                            } else {
                                var_value = val;
                            }
                        } else if (modifier == '+') {
                            // ${var+word}: use word if set
                            // ${var:+word}: use word if set and not null
                            if (!is_unset && (!check_null || !is_null)) {
                                // Expand the word with its own quotes honoured
                                char *expanded_word = expand_default_word(word, is_quoted, last_exit_code);
                                if (expanded_word) {
                                    expanded_buf = expanded_word;
                                    effective_word = expanded_buf;
                                }
                                var_value = effective_word;
                                premarked_word = effective_word;
                            } else {
                                var_value = "";
                            }
                        } else if (modifier == '=' && is_special) {
                            // ${?=word}: a special parameter cannot be assigned
                            fprintf(stderr, "%s: ${%s=%s}: cannot assign in this way\n", HASH_NAME, var_name, word);
                            varexpand_error = true;
                            var_value = "";
                        } else if (modifier == '=') {
                            // ${var=word}: assign word if unset
                            // ${var:=word}: assign word if unset or null
                            if (is_unset || (check_null && is_null)) {
                                // The value assigned is never field-split, so $* in the
                                // word joins with the first IFS character. The expansion's
                                // result is that value, appended
                                // like any other: split and globbed when the ${...} is
                                // unquoted (POSIX 2.6.2). It is stored through the variable
                                // table (not setenv alone, which $var does not read back)
                                // and without the markers the expansion carries; v51 stored
                                // "\x03hi there\x03" for ${x:=$(echo hi there)}
                                bool was = varexpand_set_no_split(true);
                                char *expanded_word = expand_default_word(word, is_quoted, last_exit_code);
                                varexpand_set_no_split(was);
                                if (expanded_word) {
                                    strip_quote_markers(expanded_word);
                                    shellvar_set(var_name, expanded_word);
                                    expanded_buf = expanded_word;
                                    var_value = expanded_buf;
                                } else {
                                    var_value = "";
                                }
                            } else {
                                var_value = val;
                            }
                        } else if (modifier == '?') {
                            // ${var?word}: error if unset
                            // ${var:?word}: error if unset or null
                            if (is_unset || (check_null && is_null)) {
                                // Expand the word for the message, then drop the markers
                                char *expanded_word = expand_default_word(word, is_quoted, last_exit_code);
                                if (expanded_word) {
                                    expanded_buf = expanded_word;
                                    strip_quote_markers(expanded_buf);
                                    effective_word = expanded_buf;
                                }
                                if (effective_word[0]) {
                                    fprintf(stderr, "%s: %s\n", var_name, effective_word);
                                } else {
                                    fprintf(stderr, "%s: parameter not set\n", var_name);
                                }
                                varexpand_error = true;
                                var_value = "";
                            } else {
                                var_value = val;
                            }
                        } else if (modifier == '#') {
                            // ${var#pattern}: remove smallest prefix matching pattern
                            // ${var##pattern}: remove largest prefix matching pattern
                            if (val) {
                                pattern_buf = strdup(val);
                                if (!pattern_buf) {
                                    break;
                                }
                                char *pattern_result = pattern_buf;

                                size_t val_len = strlen(val);
                                size_t match_len = 0;

                                // The word, expanded, as an fnmatch pattern
                                char *fnmatch_pattern = expand_pattern_word(word, last_exit_code);

                                // Try matching pattern at start of string
                                // For ##, find longest match; for #, find shortest
                                // (which may be empty: ${t#*} removes nothing)
                                if (fnmatch_pattern && double_modifier) {
                                    // Longest match - try from end backwards
                                    for (size_t i = val_len; i > 0; i--) {
                                        char saved = pattern_result[i];
                                        pattern_result[i] = '\0';
                                        if (fnmatch(fnmatch_pattern, pattern_result, 0) == 0) {
                                            match_len = i;
                                            pattern_result[i] = saved;
                                            break;
                                        }
                                        pattern_result[i] = saved;
                                    }
                                } else if (fnmatch_pattern) {
                                    // Shortest match - try from the empty prefix forwards
                                    for (size_t i = 0; i <= val_len; i++) {
                                        char saved = pattern_result[i];
                                        pattern_result[i] = '\0';
                                        if (fnmatch(fnmatch_pattern, pattern_result, 0) == 0) {
                                            match_len = i;
                                            pattern_result[i] = saved;
                                            break;
                                        }
                                        pattern_result[i] = saved;
                                    }
                                }
                                free(fnmatch_pattern);

                                // Result is the part after the match
                                memmove(pattern_result, val + match_len, val_len - match_len + 1);
                                var_value = pattern_result;
                            } else {
                                var_value = "";
                            }
                        } else if (modifier == '%') {
                            // ${var%pattern}: remove smallest suffix matching pattern
                            // ${var%%pattern}: remove largest suffix matching pattern
                            if (val) {
                                pattern_buf = strdup(val);
                                if (!pattern_buf) {
                                    break;
                                }
                                char *pattern_result = pattern_buf;

                                size_t val_len = strlen(val);
                                size_t keep_len = val_len;  // How much to keep

                                // The word, expanded, as an fnmatch pattern
                                char *fnmatch_pattern = expand_pattern_word(word, last_exit_code);

                                // Try matching pattern at end of string: a suffix starts at
                                // index i, so the longest is the smallest i that matches
                                // and the shortest the largest (val_len is the empty suffix)
                                if (fnmatch_pattern && double_modifier) {
                                    for (size_t i = 0; i <= val_len; i++) {
                                        if (fnmatch(fnmatch_pattern, val + i, 0) == 0) {
                                            keep_len = i;
                                            break;
                                        }
                                    }
                                } else if (fnmatch_pattern) {
                                    for (size_t i = val_len + 1; i-- > 0;) {
                                        if (fnmatch(fnmatch_pattern, val + i, 0) == 0) {
                                            keep_len = i;
                                            break;
                                        }
                                    }
                                }
                                free(fnmatch_pattern);

                                // Result is the part before the match
                                pattern_result[keep_len] = '\0';
                                var_value = pattern_result;
                            } else {
                                var_value = "";
                            }
                        } else {
                            // No modifier, simple expansion
                            if (is_unset && check_unset_error(var_name)) {
                                // Unset variable with -u option
                                var_value = "";
                            } else {
                                var_value = val;
                            }
                        }

                        // Handle ${#var} - return length instead of value
                        if (get_length) {
                            size_t len = var_value ? strlen(var_value) : 0;
                            snprintf(var_name, sizeof(var_name), "%zu", len);
                            var_value = var_name;
                        }
                    } else {
                        // Empty variable name ${} is a syntax error
                        fprintf(stderr, "%s: ${}: bad substitution\n", HASH_NAME);
                        varexpand_error = true;
                        var_value = "";
                    }
                }
            } else if (isdigit(*p)) {
                // $N positional parameter (single digit for POSIX compliance)
                int param_num = *p - '0';
                p++;
                var_value = get_positional_param(param_num);
                // Check for unset positional parameter (except $0)
                if (var_value == NULL && param_num > 0) {
                    char param_name[16];
                    snprintf(param_name, sizeof(param_name), "%d", param_num);
                    check_unset_error(param_name);
                }
            } else if (is_varname_char(*p)) {
                // $VAR syntax
                size_t name_len = 0;
                while (*p && is_varname_char(*p) && name_len < sizeof(var_name) - 1) {
                    var_name[name_len++] = *p++;
                }
                var_name[name_len] = '\0';

                if (name_len > 0) {
                    var_value = (strcmp(var_name, "LINENO") == 0) ? exec_lineno_string() : shellvar_get(var_name);
                    // Check for unset variable with -u option
                    if (var_value == NULL) {
                        check_unset_error(var_name);
                    }
                }
            } else {
                // Just a $ followed by something else, keep the $
                if (out_pos < result_size - 1) {
                    result[out_pos++] = '$';
                }
                is_literal_dollar = true;  // Not a variable expansion
            }

append_value:
            // Append variable value if found, or handle empty/unset expansion
            // Skip if this was just a literal $ (not a variable expansion)
            if (is_literal_dollar) {
                // Do nothing - literal $ was already output
            } else if (var_value) {
                size_t val_len = strlen(var_value);
                if (val_len > 0) {
                    // A "$@"/"$*" buffer or an expanded ${var:-word} word is already
                    // marked: its \x01 pairs and \x04 separators pass as they are. The
                    // word's own \x03 regions (from a $(...) inside it) are dropped: the
                    // whole word is one expansion here, wrapped once below when unquoted,
                    // and never split when quoted.
                    bool keep_pairs = (premarked != NULL && var_value == premarked) ||
                                      (premarked_word != NULL && var_value == premarked_word);
                    bool drop_regions = (premarked_word != NULL && var_value == premarked_word);
                    if (is_quoted) {
                        // Append with markers to prevent glob expansion and quote interpretation
                        for (size_t i = 0; i < val_len && out_pos < result_size - 2; i++) {
                            char c = var_value[i];
                            if (keep_pairs && c == '\x01' && i + 1 < val_len) {
                                result[out_pos++] = c;          // an already protected byte
                                result[out_pos++] = var_value[++i];
                                continue;
                            }
                            if (drop_regions && c == '\x03') {
                                continue;
                            }
                            if (keep_pairs && is_marker_byte(c)) {
                                result[out_pos++] = c;          // a marker of the word's own
                                continue;
                            }
                            if (char_in_string(c, "*?[]'\"\\<|>&~") || is_marker_byte(c)) {
                                // Add marker before special characters (globs, quotes, redirects, operators, tilde, bracket close)
                                // and before a real marker-range byte, which travels as \x01 + byte
                                result[out_pos++] = '\x01';
                            }
                            result[out_pos++] = c;
                        }
                    } else {
                        // Unquoted - wrap with \x03 markers for IFS splitting; a
                        // real marker-range byte in the value travels as \x01 + byte.
                        // An unquoted $* or $@ is one field per parameter (\x04 between
                        // them), and POSIX splits each of those fields again: every
                        // parameter gets a region of its own, \x03p1\x03\x04\x03p2\x03.
                        // The same for the fields of an expanded ${var-word}: those from
                        // a quoted "$@" inside it carry protected bytes and never split
                        bool per_field = keep_pairs;
                        if (out_pos + 2 < result_size - 1) {
                            result[out_pos++] = '\x03';  // Start marker
                            for (size_t i = 0; i < val_len && out_pos < result_size - 5; i++) {
                                char c = var_value[i];
                                if (keep_pairs && c == '\x01' && i + 1 < val_len) {
                                    result[out_pos++] = c;      // an already protected byte
                                    result[out_pos++] = var_value[++i];
                                    continue;
                                }
                                if (drop_regions && c == '\x03') {
                                    continue;
                                }
                                if (per_field && c == '\x04') {
                                    result[out_pos++] = '\x03';  // close this parameter's region
                                    result[out_pos++] = '\x04';
                                    result[out_pos++] = '\x03';  // open the next one
                                    continue;
                                }
                                if (!keep_pairs && is_marker_byte(c)) {
                                    result[out_pos++] = '\x01';
                                }
                                result[out_pos++] = c;          // a \x04 separator stays raw
                            }
                            result[out_pos++] = '\x03';  // End marker
                        }
                    }
                } else if (!is_quoted && out_pos < result_size - 2) {
                    // Empty unquoted expansion - add markers so IFS splitting can remove it
                    // POSIX: empty unquoted expansion produces no field
                    result[out_pos++] = '\x03';
                    result[out_pos++] = '\x03';
                } else if (is_quoted && var_name[0] == '@' && var_name[1] == '\0' &&
                           script_state.positional_count <= 1 && out_pos < result_size - 2) {
                    // "$@" with no positional parameters is no field at all (POSIX),
                    // unlike "", "$empty" or "$@" with one empty parameter: mark it
                    // like an empty unquoted expansion so field splitting removes it
                    result[out_pos++] = '\x03';
                    result[out_pos++] = '\x03';
                }
            } else if (!is_quoted && out_pos < result_size - 2) {
                // Unset variable in unquoted context - add empty markers for removal
                // POSIX: unset variable in unquoted expansion produces no field
                result[out_pos++] = '\x03';
                result[out_pos++] = '\x03';
            }
            // Quoted unset variables expand to empty string (kept as argument)

        } else {
            // Regular character
            result[out_pos++] = *p++;
        }
        free(word_buf);
        free(expanded_buf);
        free(pattern_buf);
        free(params_buf);
        word_buf = expanded_buf = pattern_buf = params_buf = NULL;
    }
    free(word_buf);
    free(expanded_buf);
    free(pattern_buf);
    free(params_buf);

    result[out_pos] = '\0';
    *truncated = (*p != '\0') || (out_pos + 64 >= result_size);
    return result;
}

// Expand environment variables in a string. The buffer is sized from the
// input first; a value longer than that (a 10000-byte "$v", $(set) in a big
// environment) used to be cut at 8 KB, so when an attempt fills its buffer
// the expansion runs again with four times the room.
char *varexpand_expand(const char *str, int last_exit_code) {
    if (!str) return NULL;
    size_t result_size = strlen(str) * 2 + 4096;
    if (result_size < MAX_EXPANDED_LENGTH) {
        result_size = MAX_EXPANDED_LENGTH;
    }
    for (;;) {
        bool truncated = false;
        char *result = expand_once(str, last_exit_code, result_size, &truncated);
        if (!result || !truncated || result_size >= MAX_EXPANSION_SIZE) {
            return result;
        }
        free(result);
        result_size *= 4;
    }
}

// Expand variables in all arguments
int varexpand_args(char **args, int last_exit_code) {
    if (!args) return -1;

    for (int i = 0; args[i] != NULL; i++) {
        // Check if this argument contains $
        if (strchr(args[i], '$') != NULL) {
            char *expanded = varexpand_expand(args[i], last_exit_code);
            if (expanded) {
                // Replace argument with expanded version
                // Note: args[i] points into line buffer, don't free it
                args[i] = expanded;
            }
        }
    }

    return 0;
}
