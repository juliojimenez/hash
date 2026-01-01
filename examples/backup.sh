#!/usr/local/bin/hash-shell
#
# backup.sh - Example backup script
# Usage: ./backup.sh [source_dir] [backup_dir]
#

# Default values
SOURCE_DIR="${1:-$HOME}"
BACKUP_DIR="${2:-/tmp/backup}"
DATE=$(date +%Y%m%d_%H%M%S)

echo "Backup Script"
echo "============="
echo ""

# Validate source directory
if [ ! -d "$SOURCE_DIR" ]; then
    echo "Error: Source directory '$SOURCE_DIR' does not exist" >&2
    exit 1
fi

# Create backup directory if it doesn't exist
if [ ! -d "$BACKUP_DIR" ]; then
    echo "Creating backup directory: $BACKUP_DIR"
    mkdir -p "$BACKUP_DIR"
    if [ $? -ne 0 ]; then
        echo "Error: Could not create backup directory" >&2
        exit 1
    fi
fi

# Create backup filename
BACKUP_NAME="backup_${DATE}.tar.gz"
BACKUP_PATH="${BACKUP_DIR}/${BACKUP_NAME}"

echo "Source: $SOURCE_DIR"
echo "Destination: $BACKUP_PATH"
echo ""

# Perform backup
echo "Creating backup..."
tar -czf "$BACKUP_PATH" -C "$(dirname "$SOURCE_DIR")" "$(basename "$SOURCE_DIR")" 2>/dev/null

if [ $? -eq 0 ]; then
    echo "Backup completed successfully!"
    echo ""
    
    # Show backup info
    if [ -f "$BACKUP_PATH" ]; then
        SIZE=$(ls -lh "$BACKUP_PATH" | awk '{print $5}')
        echo "Backup file: $BACKUP_PATH"
        echo "Backup size: $SIZE"
    fi
else
    echo "Error: Backup failed" >&2
    exit 1
fi

# Clean up old backups (keep last 5)
echo ""
echo "Cleaning up old backups (keeping last 5)..."
cd "$BACKUP_DIR"
BACKUP_COUNT=$(ls -1 backup_*.tar.gz 2>/dev/null | wc -l)

if [ $BACKUP_COUNT -gt 5 ]; then
    REMOVE_COUNT=$((BACKUP_COUNT - 5))
    echo "Removing $REMOVE_COUNT old backup(s)"
    ls -1t backup_*.tar.gz | tail -n $REMOVE_COUNT | while read old_backup; do
        rm -f "$old_backup"
        echo "  Removed: $old_backup"
    done
fi

echo ""
echo "Done!"
exit 0
