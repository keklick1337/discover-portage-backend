#!/bin/bash
# Development installation script for Portage backend
# Creates symlinks instead of copying files for easier development

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Directories
BUILD_DIR="$(dirname "$(readlink -f "$0")")/build"
PLUGIN_DIR="/usr/lib64/qt6/plugins/discover"
KAUTH_HELPER_DIR="/usr/lib64/libexec/kauth"
POLKIT_ACTIONS_DIR="/usr/share/polkit-1/actions"
DBUS_SERVICES_DIR="/usr/share/dbus-1/system-services"
DBUS_POLICY_DIR="/usr/share/dbus-1/system.d"

# Files
PLUGIN_SO="$BUILD_DIR/bin/discover/portage-backend.so"
HELPER_BIN="$BUILD_DIR/bin/portage_backend_helper"
ACTIONS_FILE="$(dirname "$(readlink -f "$0")")/src/PortageBackend/config/org.kde.discover.portagebackend.policy"
SERVICE_FILE="$(dirname "$(readlink -f "$0")")/src/PortageBackend/config/org.kde.discover.portagebackend.service"
POLICY_FILE="$(dirname "$(readlink -f "$0")")/src/PortageBackend/config/org.kde.discover.portagebackend.conf"

echo -e "${YELLOW}Portage Backend Development Installer${NC}"
echo "======================================"

# Check if running as root
if [ "$EUID" -ne 0 ]; then 
    echo -e "${RED}Error: This script must be run as root (use sudo)${NC}"
    exit 1
fi

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${RED}Error: Build directory not found: $BUILD_DIR${NC}"
    echo "Please run 'make' first to build the project"
    exit 1
fi

# Check if plugin .so exists
if [ ! -f "$PLUGIN_SO" ]; then
    echo -e "${RED}Error: Plugin not found: $PLUGIN_SO${NC}"
    echo "Please run 'make' first to build the project"
    exit 1
fi

# Check if helper exists
if [ ! -f "$HELPER_BIN" ]; then
    echo -e "${RED}Error: KAuth helper not found: $HELPER_BIN${NC}"
    echo "Please run 'make' first to build the project"
    exit 1
fi

# Create necessary directories
echo -e "${YELLOW}Creating directories...${NC}"
mkdir -p "$PLUGIN_DIR"
mkdir -p "$KAUTH_HELPER_DIR"
mkdir -p "$POLKIT_ACTIONS_DIR"
mkdir -p "$DBUS_SERVICES_DIR"
mkdir -p "$DBUS_POLICY_DIR"

# Install plugin symlink
echo -e "${YELLOW}Installing plugin symlink...${NC}"
PLUGIN_LINK="$PLUGIN_DIR/portage-backend.so"
if [ -L "$PLUGIN_LINK" ]; then
    echo "Removing old symlink: $PLUGIN_LINK"
    rm "$PLUGIN_LINK"
elif [ -f "$PLUGIN_LINK" ]; then
    echo "Backing up existing file: $PLUGIN_LINK -> $PLUGIN_LINK.bkp"
    mv "$PLUGIN_LINK" "$PLUGIN_LINK.bkp"
fi
ln -s "$PLUGIN_SO" "$PLUGIN_LINK"
echo -e "${GREEN}✓${NC} Created symlink: $PLUGIN_LINK -> $PLUGIN_SO"

# Install KAuth helper symlink
echo -e "${YELLOW}Installing KAuth helper symlink...${NC}"
HELPER_LINK="$KAUTH_HELPER_DIR/portage_backend_helper"
if [ -L "$HELPER_LINK" ]; then
    echo "Removing old symlink: $HELPER_LINK"
    rm "$HELPER_LINK"
elif [ -f "$HELPER_LINK" ]; then
    echo "Backing up existing file: $HELPER_LINK -> $HELPER_LINK.bkp"
    mv "$HELPER_LINK" "$HELPER_LINK.bkp"
fi
ln -s "$HELPER_BIN" "$HELPER_LINK"
echo -e "${GREEN}✓${NC} Created symlink: $HELPER_LINK -> $HELPER_BIN"

# Install polkit actions (copy, not symlink - polkit doesn't follow symlinks)
echo -e "${YELLOW}Installing polkit actions...${NC}"
ACTIONS_DEST="$POLKIT_ACTIONS_DIR/org.kde.discover.portagebackend.policy"
if [ -f "$ACTIONS_FILE" ]; then
    cp "$ACTIONS_FILE" "$ACTIONS_DEST"
    chmod 644 "$ACTIONS_DEST"
    echo -e "${GREEN}✓${NC} Copied: $ACTIONS_DEST"
else
    echo -e "${RED}Warning: Actions file not found: $ACTIONS_FILE${NC}"
fi

# Install DBus service (copy, not symlink - DBus doesn't follow symlinks)
echo -e "${YELLOW}Installing DBus service...${NC}"
SERVICE_DEST="$DBUS_SERVICES_DIR/org.kde.discover.portagebackend.service"
if [ -f "$SERVICE_FILE" ]; then
    cp "$SERVICE_FILE" "$SERVICE_DEST"
    chmod 644 "$SERVICE_DEST"
    echo -e "${GREEN}✓${NC} Copied: $SERVICE_DEST"
else
    echo -e "${RED}Warning: Service file not found: $SERVICE_FILE${NC}"
fi

# Install DBus policy (copy, not symlink - DBus doesn't follow symlinks)
echo -e "${YELLOW}Installing DBus policy...${NC}"
POLICY_DEST="$DBUS_POLICY_DIR/org.kde.discover.portagebackend.conf"
if [ -f "$POLICY_FILE" ]; then
    cp "$POLICY_FILE" "$POLICY_DEST"
    chmod 644 "$POLICY_DEST"
    echo -e "${GREEN}✓${NC} Copied: $POLICY_DEST"
else
    echo -e "${RED}Warning: Policy file not found: $POLICY_FILE${NC}"
fi

# Reload DBus and polkit
echo -e "${YELLOW}Reloading system services...${NC}"
if command -v systemctl &> /dev/null; then
    systemctl reload dbus 2>/dev/null || echo "Note: Could not reload DBus"
fi

if command -v polkitd &> /dev/null; then
    pkill -HUP polkitd 2>/dev/null || echo "Note: polkitd not running or could not reload"
fi

echo ""
echo -e "${GREEN}======================================"
echo "Installation complete!"
echo -e "======================================${NC}"
echo ""
echo "Installed symlinks:"
echo "  Plugin:      $PLUGIN_LINK"
echo "  KAuth Helper: $HELPER_LINK"
echo ""
echo "Copied files:"
echo "  Polkit Actions: $ACTIONS_DEST"
echo "  DBus Service:   $SERVICE_DEST"
echo "  DBus Policy:    $POLICY_DEST"
echo ""
echo -e "${YELLOW}Note: After rebuilding (make), the symlinks will automatically"
echo -e "      point to the new binaries. Just restart plasma-discover. ${NC}"
echo ""
echo -e "${YELLOW}To restart Discover:${NC}"
echo "  killall plasma-discover && plasma-discover"
echo ""
