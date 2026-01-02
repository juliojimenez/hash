#ifndef UPDATE_H
#define UPDATE_H

#include <stdbool.h>

// Update check interval (24 hours in seconds)
#define UPDATE_CHECK_INTERVAL (24 * 60 * 60)

// GitHub API URL for latest release
#define GITHUB_API_URL "https://api.github.com/repos/juliojimenez/hash/releases/latest"

// GitHub releases download base URL
#define GITHUB_DOWNLOAD_URL "https://github.com/juliojimenez/hash/releases/download"

// Installation methods
typedef enum {
    INSTALL_METHOD_UNKNOWN,
    INSTALL_METHOD_DIRECT,      // Direct download from GitHub
    INSTALL_METHOD_SOURCE,      // Built from source (make install)
    INSTALL_METHOD_APT,         // Debian/Ubuntu apt
    INSTALL_METHOD_YUM,         // RHEL/CentOS yum
    INSTALL_METHOD_DNF,         // Fedora dnf
    INSTALL_METHOD_BREW,        // macOS Homebrew
    INSTALL_METHOD_PKG,         // FreeBSD pkg
    INSTALL_METHOD_PACMAN,      // Arch Linux pacman
    INSTALL_METHOD_ZYPPER,      // openSUSE zypper
    INSTALL_METHOD_FLATPAK,     // Flatpak
    INSTALL_METHOD_SNAP         // Snap
} InstallMethod;

// Update check result
typedef struct {
    bool update_available;
    char latest_version[32];
    char current_version[32];
    char download_url[512];
    char release_notes_url[256];
    InstallMethod install_method;
} UpdateInfo;

/**
 * Initialize update system
 * Creates state directory if needed
 */
void update_init(void);

/**
 * Detect how hash was installed
 * @return Installation method enum
 */
InstallMethod update_detect_install_method(void);

/**
 * Get string representation of install method
 * @param method Installation method
 * @return Human-readable string
 */
const char *update_install_method_str(InstallMethod method);

/**
 * Check for updates (non-blocking if possible)
 * @param info Pointer to UpdateInfo struct to fill
 * @return 0 on success, -1 on error
 */
int update_check(UpdateInfo *info);

/**
 * Check if enough time has passed since last update check
 * @return true if should check for updates
 */
bool update_should_check(void);

/**
 * Record that an update check was performed
 */
void update_record_check(void);

/**
 * Perform the update
 * Downloads new binary, verifies checksum, installs
 * @param info Update information
 * @param interactive If true, prompts for confirmation
 * @return 0 on success, -1 on error
 */
int update_perform(const UpdateInfo *info, bool interactive);

/**
 * Get platform identifier string (e.g., "linux-x86_64", "darwin-arm64")
 * @param buffer Buffer to store result
 * @param size Size of buffer
 * @return 0 on success, -1 on error
 */
int update_get_platform(char *buffer, size_t size);

/**
 * Compare version strings
 * @param v1 First version (e.g., "18")
 * @param v2 Second version (e.g., "19")
 * @return <0 if v1<v2, 0 if equal, >0 if v1>v2
 */
int update_compare_versions(const char *v1, const char *v2);

/**
 * Get the path where hash is installed
 * @param buffer Buffer to store path
 * @param size Size of buffer
 * @return 0 on success, -1 on error
 */
int update_get_install_path(char *buffer, size_t size);

/**
 * Print update instructions for package manager installations
 * @param method Installation method
 */
void update_print_package_manager_instructions(InstallMethod method);

/**
 * Builtin command: check for and perform updates
 * @param args Command arguments
 * @return 1 to continue shell
 */
int shell_update(char **args);

/**
 * Check for updates at startup (called from main)
 * Only checks if interval has passed, shows notification if available
 */
void update_startup_check(void);

#endif // UPDATE_H
