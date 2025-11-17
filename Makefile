# Makefile for Portage Backend for KDE Discover
# Development building and installation of the project

.PHONY: all build install clean help dependencies uninstall rebuild test check debug info

# Variables
BUILD_DIR := build
SRC_DIR := src
INSTALL_PREFIX := /usr
BUILD_TYPE := Release

# Use MAKEOPTS from /etc/portage/make.conf if available
# Otherwise fallback to nproc
MAKEOPTS ?= -j$(shell nproc 2>/dev/null || echo 1)

all: build

help:
	@echo "Available commands:"
	@echo "  make dependencies   - Install dependencies (requires Gentoo)"
	@echo "  make build          - Build the project"
	@echo "  make install        - Install the backend (requires sudo)"
	@echo "  make clean          - Clean build files"
	@echo "  make rebuild        - Rebuild from scratch"
	@echo "  make uninstall      - Uninstall the backend"
	@echo "  make help           - Show this help"

dependencies:
	@echo "Installing dependencies for Gentoo..."
	@if [ -f /usr/bin/emerge ]; then \
		emerge --ask app-portage/portage-utils app-portage/eix app-portage/gentoolkit; \
		emerge --ask kde-plasma/discover kde-frameworks/kcoreaddons kde-frameworks/ki18n kde-frameworks/kconfig; \
	else \
		echo "Error: emerge not found. This command only works on Gentoo Linux."; \
		exit 1; \
	fi

build: $(BUILD_DIR)/Makefile
	@echo "Building the project..."
	cmake --build $(BUILD_DIR) $(MAKEOPTS)

$(BUILD_DIR)/Makefile:
	@echo "Configuring CMake..."
	@mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake ../$(SRC_DIR) \
		-DCMAKE_INSTALL_PREFIX=$(INSTALL_PREFIX) \
		-DKDE_INSTALL_USE_QT_SYS_PATHS=ON \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON

install: build
	@echo "Installing the backend..."
	@if [ ! -d $(BUILD_DIR) ]; then \
		echo "Error: Build directory not found. Run 'make build' first."; \
		exit 1; \
	fi
	@if [ "$$(id -u)" -eq 0 ]; then \
		cmake --install $(BUILD_DIR); \
	else \
		sudo cmake --install $(BUILD_DIR); \
	fi
	@echo ""
	@echo "=== Installation complete! ==="
	@echo ""
	@echo "Installed files:"
	@echo "  Plugin:     \$$(find $(INSTALL_PREFIX)/lib* -path '*/qt6/plugins/discover/portage-backend.so' 2>/dev/null | head -1 || echo '$(INSTALL_PREFIX)/lib*/qt6/plugins/discover/portage-backend.so')"
	@echo "  Helper:     $(INSTALL_PREFIX)/libexec/kf6/kauth/portage_backend_helper"
	@echo "  Policy:     $(INSTALL_PREFIX)/share/polkit-1/actions/org.kde.discover.portagebackend.policy"
	@echo "  DBus svc:   $(INSTALL_PREFIX)/share/dbus-1/system-services/org.kde.discover.portagebackend.service"
	@echo "  DBus conf:  $(INSTALL_PREFIX)/share/dbus-1/system.d/org.kde.discover.portagebackend.conf"
	@echo ""
	@echo "Next steps:"
	@echo "  1. Restart DBus: sudo systemctl restart dbus"
	@echo "  2. Restart Discover: killall plasma-discover 2>/dev/null; plasma-discover &"
	@echo ""

uninstall:
	@echo "Uninstalling the backend..."
	@PLUGIN_PATH="$$(find $(INSTALL_PREFIX)/lib* -path '*/qt6/plugins/discover/portage-backend.so' 2>/dev/null | head -1)"; \
	UNINSTALL_FILES="\
		$$PLUGIN_PATH \
		$(INSTALL_PREFIX)/libexec/kf6/kauth/portage_backend_helper \
		$(INSTALL_PREFIX)/share/polkit-1/actions/org.kde.discover.portagebackend.policy \
		$(INSTALL_PREFIX)/share/dbus-1/system-services/org.kde.discover.portagebackend.service \
		$(INSTALL_PREFIX)/share/dbus-1/system.d/org.kde.discover.portagebackend.conf"; \
	if [ "$$(id -u)" -eq 0 ]; then \
		for file in $$UNINSTALL_FILES; do \
			if [ -n "$$file" ] && [ -f "$$file" ]; then \
				rm -f "$$file" && echo "Removed: $$file"; \
			fi; \
		done; \
	else \
		for file in $$UNINSTALL_FILES; do \
			if [ -n "$$file" ] && [ -f "$$file" ]; then \
				sudo rm -f "$$file" && echo "Removed: $$file"; \
			fi; \
		done; \
	fi
	@echo ""
	@echo "=== Uninstallation complete! ==="
	@echo "Restart DBus and Discover to apply changes:"
	@echo "  sudo systemctl restart dbus"
	@echo "  killall plasma-discover 2>/dev/null; plasma-discover &"
	@echo ""

clean:
	@echo "Cleaning build files..."
	@rm -rf $(BUILD_DIR)
	@echo "Cleaning complete."

rebuild: clean build

test: build
	@echo "Running tests..."
	cd $(BUILD_DIR) && ctest --output-on-failure

check:
	@echo "Checking code with clang-format..."
	@find src -name "*.cpp" -o -name "*.h" | xargs clang-format -i

debug:
	@echo "Creating debug build..."
	@mkdir -p $(BUILD_DIR)
	cmake -S $(SRC_DIR) -B $(BUILD_DIR) \
		-DCMAKE_INSTALL_PREFIX=$(INSTALL_PREFIX) \
		-DKDE_INSTALL_USE_QT_SYS_PATHS=ON \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	cmake --build $(BUILD_DIR) $(MAKEOPTS)

info:
	@echo "=== Project Information ==="
	@echo "Name: Portage Backend for KDE Discover"
	@echo "Version: 1.0.0"
	@echo "Build directory: $(BUILD_DIR)"
	@echo "Source directory: $(SRC_DIR)"
	@echo "Install prefix: $(INSTALL_PREFIX)"
	@echo "Build type: $(BUILD_TYPE)"
	@echo ""
	@echo "Dependency check:"
	@which emerge > /dev/null 2>&1 && echo "  ✓ emerge found" || echo "  ✗ emerge not found"
	@which qlist > /dev/null 2>&1 && echo "  ✓ qlist found" || echo "  ✗ qlist not found (recommended)"
	@which eix > /dev/null 2>&1 && echo "  ✓ eix found" || echo "  ✗ eix not found (optional)"
	@which equery > /dev/null 2>&1 && echo "  ✓ equery found" || echo "  ✗ equery not found (recommended)"
	@which cmake > /dev/null 2>&1 && echo "  ✓ cmake found" || echo "  ✗ cmake not found"
	@sh -c '\
	if ldconfig -p 2>/dev/null | grep -q "libDiscoverCommon.so"; then \
		echo "  ✓ libDiscoverCommon.so found"; \
	elif [ -f /usr/lib64/plasma-discover/libDiscoverCommon.so ] || [ -f /usr/lib/plasma-discover/libDiscoverCommon.so ] || [ -f /usr/lib/libDiscoverCommon.so ] || [ -f /usr/lib64/libDiscoverCommon.so ]; then \
		echo "  ✓ libDiscoverCommon.so found"; \
	else \
		echo "  ✗ libDiscoverCommon.so not found (install kde-plasma/discover)"; \
	fi'
