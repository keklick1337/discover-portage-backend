# Discover Headers

This directory contains minimal header files from KDE Discover (libdiscover) required to build the Portage backend.

## Why bundled headers?

Gentoo's `kde-plasma/discover` package does not install development headers, only the runtime library (`libDiscoverCommon.so`). To avoid requiring the entire Discover source tree as a submodule, we bundle only the necessary headers here.

## Files included

- `libdiscover/resources/` - Abstract backend interfaces
- `libdiscover/Transaction/` - Transaction handling
- `libdiscover/Category/` - Category definitions
- `libdiscover/ReviewsBackend/` - Rating support
- `libdiscover/DiscoverConfig.h` - Plugin IID definitions
- `libdiscover/discovercommon_export.h` - Export macros

## Source

Headers are copied from KDE Discover repository: https://invent.kde.org/plasma/discover

## License

All headers retain their original SPDX license identifiers from the Discover project.
