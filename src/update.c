#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <time.h>
#include <errno.h>
#include <pwd.h>
#include <fcntl.h>
#include <sys/wait.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#include "update.h"
#include "hash.h"
#include "colors.h"
#include "safe_string.h"
#include "execute.h"

// State file for tracking last update check
#define UPDATE_STATE_FILE ".hash_update_state"

// Temporary download location
#define UPDATE_TEMP_DIR "/tmp"

extern int last_command_exit_code;

// Get home directory
static const char *get_home_dir(void) {
    const char *home = getenv("HOME");
    if (home) return home;

    struct passwd pw;
    struct passwd *result = NULL;
    static char buf[1024];

    if (getpwuid_r(getuid(), &pw, buf, sizeof(buf), &result) == 0 && result) {
        return pw.pw_dir;
    }
    return NULL;
}

// Get state file path
static void get_state_path(char *path, size_t size) {
    const char *home = get_home_dir();
    if (home) {
        snprintf(path, size, "%s/%s", home, UPDATE_STATE_FILE);
    } else {
        path[0] = '\0';
    }
}

void update_init(void) {
    // Nothing special needed for now
}

InstallMethod update_detect_install_method(void) {
    char path[1024];

    // Get the path of the currently running executable
    if (update_get_install_path(path, sizeof(path)) != 0) {
        return INSTALL_METHOD_UNKNOWN;
    }

    // Check for package manager database entries
    // This is a heuristic - we check common package manager paths

    // APT (Debian/Ubuntu) - check dpkg database
    FILE *fp = popen("dpkg -S hash-shell 2>/dev/null", "r");
    if (fp) {
        char line[256];
        if (fgets(line, sizeof(line), fp) && strstr(line, "hash-shell")) {
            pclose(fp);
            return INSTALL_METHOD_APT;
        }
        pclose(fp);
    }

    // YUM/DNF (RHEL/CentOS/Fedora) - check rpm database
    fp = popen("rpm -qf /usr/local/bin/hash-shell 2>/dev/null", "r");
    if (fp) {
        char line[256];
        if (fgets(line, sizeof(line), fp) && !strstr(line, "not owned")) {
            pclose(fp);
            // Distinguish between dnf and yum
            if (access("/usr/bin/dnf", X_OK) == 0) {
                return INSTALL_METHOD_DNF;
            }
            return INSTALL_METHOD_YUM;
        }
        pclose(fp);
    }

    // Homebrew (macOS) - check if in Cellar
    if (strstr(path, "/Cellar/") || strstr(path, "/homebrew/")) {
        return INSTALL_METHOD_BREW;
    }

    // Also check brew list
    fp = popen("brew list hash-shell 2>/dev/null", "r");
    if (fp) {
        char line[256];
        if (fgets(line, sizeof(line), fp)) {
            pclose(fp);
            return INSTALL_METHOD_BREW;
        }
        pclose(fp);
    }

    // FreeBSD pkg
    fp = popen("pkg info hash-shell 2>/dev/null", "r");
    if (fp) {
        char line[256];
        if (fgets(line, sizeof(line), fp) && strstr(line, "hash-shell")) {
            pclose(fp);
            return INSTALL_METHOD_PKG;
        }
        pclose(fp);
    }

    // Arch Linux pacman
    fp = popen("pacman -Qo /usr/local/bin/hash-shell 2>/dev/null", "r");
    if (fp) {
        char line[256];
        if (fgets(line, sizeof(line), fp) && strstr(line, "owned by")) {
            pclose(fp);
            return INSTALL_METHOD_PACMAN;
        }
        pclose(fp);
    }

    // openSUSE zypper
    fp = popen("zypper se --installed-only hash-shell 2>/dev/null", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, "hash-shell")) {
                pclose(fp);
                return INSTALL_METHOD_ZYPPER;
            }
        }
        pclose(fp);
    }

    // Snap
    if (strstr(path, "/snap/")) {
        return INSTALL_METHOD_SNAP;
    }

    // Flatpak
    if (strstr(path, "/flatpak/")) {
        return INSTALL_METHOD_FLATPAK;
    }

    // Check if in standard system paths (likely from make install or direct)
    if (strstr(path, "/usr/local/bin") || strstr(path, "/usr/bin")) {
        // Could be make install or direct download
        // We'll treat it as direct (allowing updates)
        return INSTALL_METHOD_DIRECT;
    }

    // If running from build directory or home, likely source build
    const char *home = get_home_dir();
    if (home && strstr(path, home)) {
        return INSTALL_METHOD_SOURCE;
    }

    return INSTALL_METHOD_DIRECT;
}

const char *update_install_method_str(InstallMethod method) {
    switch (method) {
        case INSTALL_METHOD_DIRECT:  return "direct download";
        case INSTALL_METHOD_SOURCE:  return "built from source";
        case INSTALL_METHOD_APT:     return "apt (Debian/Ubuntu)";
        case INSTALL_METHOD_YUM:     return "yum (RHEL/CentOS)";
        case INSTALL_METHOD_DNF:     return "dnf (Fedora)";
        case INSTALL_METHOD_BREW:    return "Homebrew";
        case INSTALL_METHOD_PKG:     return "pkg (FreeBSD)";
        case INSTALL_METHOD_PACMAN:  return "pacman (Arch)";
        case INSTALL_METHOD_ZYPPER:  return "zypper (openSUSE)";
        case INSTALL_METHOD_FLATPAK: return "Flatpak";
        case INSTALL_METHOD_SNAP:    return "Snap";
        default:                     return "unknown";
    }
}

int update_get_platform(char *buffer, size_t size) {
    struct utsname info;
    if (uname(&info) != 0) {
        return -1;
    }

    // Determine OS
    const char *os;
    if (strcmp(info.sysname, "Linux") == 0) {
        os = "linux";
    } else if (strcmp(info.sysname, "Darwin") == 0) {
        os = "darwin";
    } else if (strcmp(info.sysname, "FreeBSD") == 0) {
        os = "freebsd";
    } else {
        os = "unknown";
    }

    // Determine architecture
    const char *arch;
    if (strcmp(info.machine, "x86_64") == 0 || strcmp(info.machine, "amd64") == 0) {
        arch = "x86_64";
    } else if (strcmp(info.machine, "aarch64") == 0 || strcmp(info.machine, "arm64") == 0) {
        arch = "arm64";
    } else if (strncmp(info.machine, "arm", 3) == 0) {
        arch = "arm";
    } else {
        arch = info.machine;
    }

    snprintf(buffer, size, "%s-%s", os, arch);
    return 0;
}

int update_get_install_path(char *buffer, size_t size) {
    // Try to get path from /proc/self/exe (Linux)
    ssize_t len = readlink("/proc/self/exe", buffer, size - 1);
    if (len > 0) {
        buffer[len] = '\0';
        return 0;
    }

#ifdef __APPLE__
    // macOS: use _NSGetExecutablePath
    uint32_t bufsize = (uint32_t)size;
    if (_NSGetExecutablePath(buffer, &bufsize) == 0) {
        // Resolve symlinks
        char resolved[1024];
        if (realpath(buffer, resolved)) {
            safe_strcpy(buffer, resolved, size);
        }
        return 0;
    }
#endif

    // Try $_ environment variable (path of last command)
    const char *path = getenv("_");
    if (path && path[0] == '/') {
        safe_strcpy(buffer, path, size);
        return 0;
    }

    // Fallback to common locations
    const char *locations[] = {
        "/usr/local/bin/hash-shell",
        "/usr/bin/hash-shell",
        NULL
    };

    for (int i = 0; locations[i]; i++) {
        if (access(locations[i], X_OK) == 0) {
            safe_strcpy(buffer, locations[i], size);
            return 0;
        }
    }

    return -1;
}

int update_compare_versions(const char *v1, const char *v2) {
    // Simple numeric comparison
    // Handles versions like "18", "19", etc.
    // Also handles "v18", "v19"

    // Skip 'v' prefix if present
    if (v1[0] == 'v' || v1[0] == 'V') v1++;
    if (v2[0] == 'v' || v2[0] == 'V') v2++;

    int n1 = atoi(v1);
    int n2 = atoi(v2);

    return n1 - n2;
}

bool update_should_check(void) {
    char state_path[1024];
    get_state_path(state_path, sizeof(state_path));

    if (state_path[0] == '\0') {
        return true;  // Can't determine, check anyway
    }

    struct stat st;
    if (stat(state_path, &st) != 0) {
        return true;  // File doesn't exist, should check
    }

    time_t now = time(NULL);
    time_t last_check = st.st_mtime;

    return (now - last_check) >= UPDATE_CHECK_INTERVAL;
}

void update_record_check(void) {
    char state_path[1024];
    get_state_path(state_path, sizeof(state_path));

    if (state_path[0] == '\0') return;

    // Touch the file (create or update mtime)
    int fd = open(state_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        // Write current version to the file
        dprintf(fd, "last_check=%ld\nversion=%s\n", time(NULL), HASH_VERSION);
        close(fd);
    }
}

// Helper to run curl and capture output
static int run_curl(const char *url, char *output, size_t output_size) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "curl -sL -H 'Accept: application/vnd.github.v3+json' '%s' 2>/dev/null",
             url);

    FILE *fp = popen(cmd, "r");
    if (!fp) return -1;

    size_t total = 0;
    size_t n;
    while ((n = fread(output + total, 1, output_size - total - 1, fp)) > 0) {
        total += n;
        if (total >= output_size - 1) break;
    }
    output[total] = '\0';

    int status = pclose(fp);

    // Check for successful exit
    if (status != -1 && WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);
        // Accept exit code 0 (success) or 23 (write error - happens when
        // response is larger than our buffer, but we got enough data)
        if (exit_code == 0 || exit_code == 23) {
            return 0;
        }
    }

    // If pclose returned -1 but we got data that looks like JSON, consider it success
    // This can happen on macOS due to signal handling quirks
    if (status == -1 && total > 0 && output[0] == '{') {
        return 0;
    }

    return -1;
}

// Simple JSON string extraction (no full parser needed)
static int extract_json_string(const char *json, const char *key, char *value, size_t size) {
    // Look for "key": "value"
    char search[128];
    snprintf(search, sizeof(search), "\"%s\":", key);

    const char *p = strstr(json, search);
    if (!p) return -1;

    p += strlen(search);

    // Skip whitespace
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n')) p++;

    if (*p != '"') return -1;
    p++;  // Skip opening quote

    size_t i = 0;
    while (*p && *p != '"' && i < size - 1) {
        if (*p == '\\' && *(p + 1)) {
            p++;  // Skip escape
        }
        value[i++] = *p++;
    }
    value[i] = '\0';

    return 0;
}

int update_check(UpdateInfo *info) {
    if (!info) return -1;

    memset(info, 0, sizeof(UpdateInfo));
    safe_strcpy(info->current_version, HASH_VERSION, sizeof(info->current_version));
    info->install_method = update_detect_install_method();

    // Check if curl is available
    if (system("which curl >/dev/null 2>&1") != 0) {
        return -1;  // curl not available
    }

    // Fetch latest release info from GitHub
    char json[8192];
    if (run_curl(GITHUB_API_URL, json, sizeof(json)) != 0) {
        return -1;
    }

    // Extract tag name (version)
    char tag_name[64];
    if (extract_json_string(json, "tag_name", tag_name, sizeof(tag_name)) != 0) {
        return -1;
    }

    safe_strcpy(info->latest_version, tag_name, sizeof(info->latest_version));

    // Extract HTML URL for release notes
    extract_json_string(json, "html_url", info->release_notes_url, sizeof(info->release_notes_url));

    // Compare versions
    info->update_available = update_compare_versions(info->current_version, info->latest_version) < 0;

    if (info->update_available) {
        // Build download URL based on platform
        char platform[64];
        if (update_get_platform(platform, sizeof(platform)) == 0) {
            snprintf(info->download_url, sizeof(info->download_url),
                     "%s/%s/hash-shell-%s-%s",
                     GITHUB_DOWNLOAD_URL, tag_name, tag_name, platform);
        }
    }

    return 0;
}

void update_print_package_manager_instructions(InstallMethod method) {
    color_info("hash was installed via %s", update_install_method_str(method));
    printf("\nTo update, use your package manager:\n\n");

    switch (method) {
        case INSTALL_METHOD_APT:
            color_print(COLOR_CYAN, "  sudo apt update && sudo apt upgrade hash-shell\n");
            break;
        case INSTALL_METHOD_YUM:
            color_print(COLOR_CYAN, "  sudo yum update hash-shell\n");
            break;
        case INSTALL_METHOD_DNF:
            color_print(COLOR_CYAN, "  sudo dnf upgrade hash-shell\n");
            break;
        case INSTALL_METHOD_BREW:
            color_print(COLOR_CYAN, "  brew upgrade hash-shell\n");
            break;
        case INSTALL_METHOD_PKG:
            color_print(COLOR_CYAN, "  sudo pkg upgrade hash-shell\n");
            break;
        case INSTALL_METHOD_PACMAN:
            color_print(COLOR_CYAN, "  sudo pacman -Syu hash-shell\n");
            break;
        case INSTALL_METHOD_ZYPPER:
            color_print(COLOR_CYAN, "  sudo zypper update hash-shell\n");
            break;
        case INSTALL_METHOD_FLATPAK:
            color_print(COLOR_CYAN, "  flatpak update hash-shell\n");
            break;
        case INSTALL_METHOD_SNAP:
            color_print(COLOR_CYAN, "  sudo snap refresh hash-shell\n");
            break;
        case INSTALL_METHOD_SOURCE:
            printf("  ");
            color_print(COLOR_CYAN, "cd /path/to/hash && git pull && make clean && make && sudo make install\n");
            break;
        default:
            break;
    }
    printf("\n");
}

int update_perform(const UpdateInfo *info, bool interactive) {
    if (!info || !info->update_available) {
        color_info("No update available. You are running the latest version (v%s).",
                   info ? info->current_version : HASH_VERSION);
        return 0;
    }

    // Check installation method
    if (info->install_method != INSTALL_METHOD_DIRECT &&
        info->install_method != INSTALL_METHOD_UNKNOWN) {
        update_print_package_manager_instructions(info->install_method);
        return 0;
    }

    // Interactive confirmation
    if (interactive) {
        printf("\n");
        color_print(COLOR_BOLD COLOR_CYAN, "Update available!\n");
        printf("  Current version: v%s\n", info->current_version);
        printf("  Latest version:  %s\n", info->latest_version);
        if (info->release_notes_url[0]) {
            printf("  Release notes:   %s\n", info->release_notes_url);
        }
        printf("\n");

        printf("Do you want to update now? [y/N] ");
        fflush(stdout);

        char response[16];
        if (!fgets(response, sizeof(response), stdin)) {
            return -1;
        }

        if (response[0] != 'y' && response[0] != 'Y') {
            printf("Update cancelled.\n");
            return 0;
        }
    }

    // Get install path
    char install_path[1024];
    if (update_get_install_path(install_path, sizeof(install_path)) != 0) {
        color_error("Could not determine installation path");
        return -1;
    }

    // Create temp file path
    char temp_path[1024];
    snprintf(temp_path, sizeof(temp_path), "%s/hash-shell-update-%d",
             UPDATE_TEMP_DIR, getpid());

    printf("Downloading %s...\n", info->latest_version);

    // Download new binary
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "curl -sL -o '%s' '%s'", temp_path, info->download_url);
    if (system(cmd) != 0) {
        color_error("Download failed");
        unlink(temp_path);
        return -1;
    }

    // Verify download (basic check - file exists and has content)
    struct stat st;
    if (stat(temp_path, &st) != 0 || st.st_size < 1000) {
        color_error("Downloaded file appears invalid");
        unlink(temp_path);
        return -1;
    }

    // Download and verify checksum
    char checksum_url[600];
    char checksum_path[1040];  // temp_path (1024) + ".sha256" (7) + null
    snprintf(checksum_url, sizeof(checksum_url), "%s.sha256", info->download_url);
    snprintf(checksum_path, sizeof(checksum_path), "%s.sha256", temp_path);

    printf("Verifying checksum...\n");
    snprintf(cmd, sizeof(cmd), "curl -sL -o '%s' '%s'", checksum_path, checksum_url);
    if (system(cmd) == 0) {
        // Verify checksum by comparing hashes directly (filename-independent)
        char verify_cmd[4096];
        #ifdef __APPLE__
        snprintf(verify_cmd, sizeof(verify_cmd),
                 "expected=$(cut -d' ' -f1 '%s') && actual=$(shasum -a 256 '%s' | cut -d' ' -f1) && [ \"$expected\" = \"$actual\" ]",
                 checksum_path, temp_path);
        #else
        snprintf(verify_cmd, sizeof(verify_cmd),
                 "expected=$(cut -d' ' -f1 '%s') && actual=$(sha256sum '%s' | cut -d' ' -f1) && [ \"$expected\" = \"$actual\" ]",
                 checksum_path, temp_path);
        #endif

        if (system(verify_cmd) != 0) {
            color_warning("Checksum verification failed (continuing anyway)");
        } else {
            color_success("Checksum verified");
        }
        unlink(checksum_path);
    }

    // Make executable
    if (chmod(temp_path, 0755) != 0) {
        color_error("Failed to set executable permission");
        unlink(temp_path);
        return -1;
    }

    // Check if we need sudo
    bool need_sudo = (access(install_path, W_OK) != 0);

    // Also check if the directory is writable (for new installs)
    if (!need_sudo) {
        char install_dir[1024];
        safe_strcpy(install_dir, install_path, sizeof(install_dir));
        char *last_slash = strrchr(install_dir, '/');
        if (last_slash) {
            *last_slash = '\0';
            if (access(install_dir, W_OK) != 0) {
                need_sudo = true;
            }
        }
    }

    printf("Installing to %s...\n", install_path);

    // Move to install location
    char move_cmd[2200];  // Larger buffer for two paths + command
    if (need_sudo) {
        printf("\n");
        color_warning("Elevated permissions required to install to %s", install_path);
        printf("Please enter your password when prompted.\n\n");
        snprintf(move_cmd, sizeof(move_cmd), "sudo -p 'Password: ' mv '%s' '%s'", temp_path, install_path);
    } else {
        snprintf(move_cmd, sizeof(move_cmd), "mv '%s' '%s'", temp_path, install_path);
    }

    if (system(move_cmd) != 0) {
        if (need_sudo) {
            // Provide manual instructions instead of deleting the file
            printf("\n");
            color_error("Installation failed. You can install manually with:");
            printf("\n");
            color_print(COLOR_CYAN, "  sudo mv '%s' '%s'\n", temp_path, install_path);
            printf("\nThe downloaded binary is preserved at: %s\n", temp_path);
        } else {
            color_error("Installation failed");
            unlink(temp_path);
        }
        return -1;
    }

    color_success("Successfully updated to %s!", info->latest_version);
    printf("\nRestart your shell to use the new version.\n");

    return 0;
}

int shell_update(char **args) {
    bool check_only = false;
    bool force = false;

    // Parse arguments
    for (int i = 1; args[i] != NULL; i++) {
        if (strcmp(args[i], "--check") == 0 || strcmp(args[i], "-c") == 0) {
            check_only = true;
        } else if (strcmp(args[i], "--force") == 0 || strcmp(args[i], "-f") == 0) {
            force = true;
        } else if (strcmp(args[i], "--help") == 0 || strcmp(args[i], "-h") == 0) {
            printf("Usage: update [options]\n");
            printf("\n");
            printf("Check for and install hash shell updates.\n");
            printf("\n");
            printf("Options:\n");
            printf("  -c, --check    Check for updates without installing\n");
            printf("  -f, --force    Force update even if on latest version\n");
            printf("  -h, --help     Show this help message\n");
            printf("\n");
            printf("Note: If hash was installed via a package manager (apt, brew, etc.),\n");
            printf("      this command will show the appropriate update instructions.\n");
            last_command_exit_code = 0;
            return 1;
        } else {
            color_error("update: unknown option: %s", args[i]);
            last_command_exit_code = 1;
            return 1;
        }
    }

    // Check for updates
    printf("Checking for updates...\n");

    UpdateInfo info;
    if (update_check(&info) != 0) {
        color_error("Failed to check for updates. Please check your internet connection.");
        last_command_exit_code = 1;
        return 1;
    }

    update_record_check();

    if (check_only) {
        if (info.update_available) {
            color_print(COLOR_BOLD COLOR_CYAN, "Update available!\n");
            printf("  Current version: v%s\n", info.current_version);
            printf("  Latest version:  %s\n", info.latest_version);
            if (info.release_notes_url[0]) {
                printf("  Release notes:   %s\n", info.release_notes_url);
            }
            printf("\nRun 'update' to install the update.\n");
            last_command_exit_code = 0;
        } else {
            color_success("You are running the latest version (v%s).", info.current_version);
            last_command_exit_code = 0;
        }
        return 1;
    }

    if (force) {
        info.update_available = true;
        // Build download URL if not already set
        if (info.download_url[0] == '\0') {
            char platform[64];
            if (update_get_platform(platform, sizeof(platform)) == 0) {
                snprintf(info.download_url, sizeof(info.download_url),
                        "%s/%s/hash-shell-%s-%s",
                        GITHUB_DOWNLOAD_URL, info.latest_version, info.latest_version, platform);
            }
        }
    }

    if (!info.update_available && !force) {
        color_success("You are running the latest version (v%s).", info.current_version);
        last_command_exit_code = 0;
        return 1;
    }

    // Perform update
    int result = update_perform(&info, true);
    last_command_exit_code = (result == 0) ? 0 : 1;
    return 1;
}

void update_startup_check(void) {
    // Check if updates are disabled via environment variable
    const char *disabled = getenv("HASH_DISABLE_UPDATE_CHECK");
    if (disabled && (disabled[0] == '1' || disabled[0] == 'y' || disabled[0] == 'Y')) {
        return;
    }

    // Only check if interval has passed
    if (!update_should_check()) {
        return;
    }

    // Perform check silently
    UpdateInfo info;
    if (update_check(&info) != 0) {
        return;  // Silently fail
    }

    update_record_check();

    // Show notification if update available
    if (info.update_available) {
        printf("\n");
        color_print(COLOR_BOLD COLOR_YELLOW, "📦 ");
        color_print(COLOR_YELLOW, "Update available: v%s → %s\n",
                    info.current_version, info.latest_version);
        printf("   Run ");
        color_print(COLOR_CYAN, "'update'");
        printf(" to install, or ");
        color_print(COLOR_CYAN, "'update --check'");
        printf(" for details.\n\n");
    }
}
