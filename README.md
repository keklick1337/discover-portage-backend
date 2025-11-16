# Portage Backend for KDE Discover

A native Portage package manager backend for KDE Discover on Gentoo Linux systems.

## Overview

This project provides full Portage integration into KDE Discover, allowing Gentoo users to manage their system packages through the familiar Discover graphical interface instead of command-line tools like `emerge`.

## Features

### Implemented

- **Package Browsing**: View all available packages from Gentoo repositories
- **Installed Packages**: Display all currently installed packages with version information
- **Package Search**: Search packages by name or category
- **Category Navigation**: Browse packages organized by Portage categories (net-im, dev-util, etc.)

## Building and Installation

### Quick Start

```bash
# Clone with submodules
git clone --recursive https://github.com/keklick1337/discover-portage-backend.git
cd discover-portage-backend

# Install dependencies (optional if you already have KDE Plasma)
make dependencies

# Build everything
make discover-deps
make build

# Install
sudo make install

# Restart Discover
killall plasma-discover
plasma-discover
```

### Manual Installation

For development, use `dev_install.sh` script that creates symlinks:

```bash
make build
sudo ./dev_install.sh
# After changes: make && killall plasma-discover && plasma-discover
```

### Building for Different Discover Versions

To build against a different version of Discover, update the version in `src/DiscoverConfig.h`:

```cpp
#define DISCOVER_PLUGIN_IID "org.kde.discover.6.4.5.AbstractResourcesBackendFactory"
#define DISCOVER_NOTIFIER_IID "org.kde.discover.6.4.5.BackendNotifierModule"
// Change both "6.4.5" to your Discover version
```

## TODO

### High Priority

- [x] Implement portage backend service with KAuth
- [x] Implement actual package installation
- [x] Implement package removal
- [x] Implement version selecting for install
- [ ] Add update check functionality
- [ ] Implement package updates
- [x] QML injector to top bar

### USE Flags

- [x] Read USE flags from installed packages
- [x] Display available USE flags for installed package
- [ ] Display available USE flags any package (ebuild parsing?)
- [x] Read configured USE flags from `/etc/portage/package.use/`
- [ ] Display message when USE flags was changed
- [ ] Write USE flag changes
- [ ] USE flag description tooltips
- [x] Interactive USE flag editor UI
- [ ] Validate USE flag conflicts

### Package Information

- [x] Basic package metadata
- [ ] Package dependencies display
- [ ] Reverse dependencies
- [ ] Package file list

### Advanced Features

- [ ] Package masking
- [x] Package unmasking
- [ ] Tasks lists and task monitoring
- [ ] Package search by description
- [ ] Message box with error details
- [ ] World file integration
- [ ] News reader (Gentoo news items)
- [ ] Sync repository functionality

## Contributing

Contributions are welcome! This project is open for community contributions.

### How to Contribute

1. Fork the repository
2. Create a feature branch
3. Commit your changes
4. Push to the branch
5. Open a Pull Request

### Areas Needing Help

- Testing on different Gentoo configurations
- UI/UX improvements
- Performance optimization
- Documentation
- Translation support
- TODO list

### Development Guidelines

- Follow existing code style
- Add comments for complex logic
- Test on a real Gentoo system before submitting
- Update README if adding new features

---

**Note**: This is early-stage software. While package browsing work, actual package installation/removal via emerge is not yet fully implemented. Use with caution on production systems.
