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

## TODO

### High Priority

- [ ] Implement actual package installation
- [ ] Implement package removal
- [ ] Add update check functionality
- [ ] Implement package updates

### USE Flags

- [x] Read USE flags from installed packages
- [ ] Display available USE flags
- [x] Read configured USE flags from `/etc/portage/package.use/`
- [ ] Write USE flag changes
- [ ] USE flag description tooltips
- [ ] Interactive USE flag editor UI
- [ ] Validate USE flag conflicts

### Package Information

- [x] Basic package metadata
- [ ] Package dependencies display
- [ ] Reverse dependencies
- [ ] Package file list

### Advanced Features

- [ ] Package masking/unmasking
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
