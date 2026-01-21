#ifndef IFS_H
#define IFS_H

// Default IFS value (space, tab, newline) per POSIX
#define DEFAULT_IFS " \t\n"

// Get current IFS value from shell variable or default
// Returns DEFAULT_IFS if IFS is unset
// Returns empty string if IFS is set to empty (disables splitting)
const char *ifs_get(void);

// Check if character is an IFS whitespace character
// IFS whitespace chars are: space, tab, newline that appear in IFS
int ifs_is_whitespace(char c, const char *ifs);

// Perform IFS word splitting on argument array
// Splits content within \x03...\x03 markers on IFS characters
// May grow the argument array (one arg can become multiple)
// Returns 0 on success, -1 on error
// If splitting occurred, *args_ptr is updated to new array
int ifs_split_args(char ***args_ptr, int *arg_count);

#endif
