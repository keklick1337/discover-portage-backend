# Makefile for Portage Backend for KDE Discover
# Development building and installation of the project

.PHONY: all build install clean help dependencies uninstall rebuild test check debug info discover-deps clean-discover

# Variables
BUILD_DIR := build
SRC_DIR := src
DISCOVER_DIR := discover
DISCOVER_BUILD_DIR := $(DISCOVER_DIR)/build
INSTALL_PREFIX := /usr
BUILD_TYPE := Release
JOBS := $(shell nproc)

all: build

help:
	@echo "Available commands:"
	@echo "  make discover-deps  - Build Discover library (DiscoverCommon)"
	@echo "  make clean-discover - Clean Discover library build"
	@echo "  make dependencies   - Install dependencies (requires Gentoo)"
	@echo "  make build          - Build the project"
	@echo "  make install        - Install the backend (requires sudo)"
	@echo "  make clean          - Clean build files"
	@echo "  make rebuild        - Rebuild from scratch"
	@echo "  make uninstall      - Uninstall the backend"
	@echo "  make help           - Show this help"

discover-deps:
	@if [ -f $(DISCOVER_BUILD_DIR)/lib/libDiscoverCommon.so ]; then \
		echo "DiscoverCommon library already built, skipping..."; \
	else \
		echo "Building Discover library (DiscoverCommon)..."; \
		mkdir -p $(DISCOVER_BUILD_DIR); \
		cd $(DISCOVER_BUILD_DIR) && \
			if [ ! -f Makefile ]; then \
				echo "Configuring Discover with CMake..."; \
				cmake .. -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCMAKE_INSTALL_PREFIX=$(INSTALL_PREFIX); \
			fi && \
		echo "Building DiscoverCommon library..." && \
		$(MAKE) -j$(JOBS) DiscoverCommon && \
		echo "DiscoverCommon library built successfully!"; \
	fi

dependencies:
	@echo "Installing dependencies for Gentoo..."
	@if [ -f /usr/bin/emerge ]; then \
		emerge --ask app-portage/portage-utils app-portage/eix app-portage/gentoolkit; \
		emerge --ask kde-plasma/discover kde-frameworks/kcoreaddons kde-frameworks/ki18n kde-frameworks/kconfig; \
	else \
		echo "Error: emerge not found. This command only works on Gentoo Linux."; \
		exit 1; \
	fi

build: discover-deps $(BUILD_DIR)/Makefile
	@echo "Building the project..."
	$(MAKE) -C $(BUILD_DIR) -j$(JOBS)

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
		$(MAKE) -C $(BUILD_DIR) install; \
	else \
		sudo $(MAKE) -C $(BUILD_DIR) install; \
	fi
	@echo ""
	@echo "=== Installation complete! ==="
	@echo ""
	@echo "Installed files:"
	@echo "  Plugin:     $(INSTALL_PREFIX)/lib64/qt6/plugins/discover/portage-backend.so"
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
	@UNINSTALL_CMD=""; \
	if [ -d $(BUILD_DIR) ]; then \
		UNINSTALL_CMD="$(MAKE) -C $(BUILD_DIR) uninstall 2>/dev/null"; \
	fi; \
	if [ "$$(id -u)" -eq 0 ]; then \
		$$UNINSTALL_CMD || true; \
		rm -f $(INSTALL_PREFIX)/lib64/qt6/plugins/discover/portage-backend.so; \
		rm -f $(INSTALL_PREFIX)/libexec/kf6/kauth/portage_backend_helper; \
		rm -f $(INSTALL_PREFIX)/share/polkit-1/actions/org.kde.discover.portagebackend.policy; \
		rm -f $(INSTALL_PREFIX)/share/dbus-1/system-services/org.kde.discover.portagebackend.service; \
		rm -f $(INSTALL_PREFIX)/share/dbus-1/system.d/org.kde.discover.portagebackend.conf; \
	else \
		sudo sh -c "$$UNINSTALL_CMD || true"; \
		sudo rm -f $(INSTALL_PREFIX)/lib64/qt6/plugins/discover/portage-backend.so; \
		sudo rm -f $(INSTALL_PREFIX)/libexec/kf6/kauth/portage_backend_helper; \
		sudo rm -f $(INSTALL_PREFIX)/share/polkit-1/actions/org.kde.discover.portagebackend.policy; \
		sudo rm -f $(INSTALL_PREFIX)/share/dbus-1/system-services/org.kde.discover.portagebackend.service; \
		sudo rm -f $(INSTALL_PREFIX)/share/dbus-1/system.d/org.kde.discover.portagebackend.conf; \
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

clean-discover:
	@echo "Cleaning Discover library build..."
	@rm -rf $(DISCOVER_BUILD_DIR)
	@echo "Discover library cleaned."

rebuild: clean build

test: build
	@echo "Running tests..."
	@cd $(BUILD_DIR) && ctest --output-on-failure

check:
	@echo "Checking code with clang-format..."
	@find src -name "*.cpp" -o -name "*.h" | xargs clang-format -i

debug:
	@echo "Creating debug build..."
	@mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake ../$(SRC_DIR) \
		-DCMAKE_INSTALL_PREFIX=$(INSTALL_PREFIX) \
		-DKDE_INSTALL_USE_QT_SYS_PATHS=ON \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	$(MAKE) -C $(BUILD_DIR) -j$(JOBS)

info:
	@echo "=== Project Information ==="
	@echo "Name: Portage Backend for KDE Discover"
	@echo "Version: 1.0.0"
	@echo "Build directory: $(BUILD_DIR)"
	@echo "Source directory: $(SRC_DIR)"
	@echo "Install prefix: $(INSTALL_PREFIX)"
	@echo "Build type: $(BUILD_TYPE)"
	@echo "Number of jobs: $(JOBS)"
	@echo ""
	@echo "Dependency check:"
	@which emerge > /dev/null 2>&1 && echo "  ✓ emerge found" || echo "  ✗ emerge not found"
	@which qlist > /dev/null 2>&1 && echo "  ✓ qlist found" || echo "  ✗ qlist not found (recommended)"
	@which eix > /dev/null 2>&1 && echo "  ✓ eix found" || echo "  ✗ eix not found (optional)"
	@which equery > /dev/null 2>&1 && echo "  ✓ equery found" || echo "  ✗ equery not found (recommended)"
	@which cmake > /dev/null 2>&1 && echo "  ✓ cmake found" || echo "  ✗ cmake not found"
