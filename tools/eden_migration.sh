#!/bin/bash
set -euo pipefail

REPO_ROOT="/workspaces/SuyuEclipse"
EDEN_SRC="${REPO_ROOT}/externals/eden-src"
TARGET_SRC="${REPO_ROOT}/src"

# Create backup of src directory
BACKUP_TIMESTAMP=$(date +%Y%m%d_%H%M%S)
BACKUP_DIR="${REPO_ROOT}/src.bak.${BACKUP_TIMESTAMP}"
echo "Creating backup of src directory to ${BACKUP_DIR}"
cp -r "${TARGET_SRC}" "${BACKUP_DIR}"

# Function to copy new files only (skip if target exists)
copy_new_files() {
    local src_dir="$1"
    local dst_dir="$2"
    
    # Use rsync with --ignore-existing to only copy new files
    rsync -av --ignore-existing "${src_dir}/" "${dst_dir}/"
}

# Copy Eden source files that don't exist in src/
echo "Copying new files from Eden to src directory..."
copy_new_files "${EDEN_SRC}" "${TARGET_SRC}"

# Generate list of potentially conflicting files
echo "Generating conflict report..."
CONFLICT_REPORT="${REPO_ROOT}/eden_conflicts_${BACKUP_TIMESTAMP}.txt"
{
    echo "Eden Migration Conflict Report - $(date)"
    echo "========================================"
    echo
    echo "Files that exist in both trees (potential conflicts):"
    echo "---------------------------------------------------"
    find "${TARGET_SRC}" "${EDEN_SRC}" -type f -exec basename {} \; | sort | uniq -d
    
    echo
    echo "Full paths of potentially conflicting files:"
    echo "------------------------------------------"
    while IFS= read -r file; do
        echo "File: ${file}"
        echo "  Eden: ${EDEN_SRC}/${file}"
        echo "  Suyu: ${TARGET_SRC}/${file}"
        echo
    done < <(find "${TARGET_SRC}" "${EDEN_SRC}" -type f -exec basename {} \; | sort | uniq -d)
} > "${CONFLICT_REPORT}"

echo "Migration complete!"
echo "1. New files from Eden have been copied to src/"
echo "2. Original src/ backed up to ${BACKUP_DIR}"
echo "3. Conflict report generated at ${CONFLICT_REPORT}"