
#ifndef DISCOVERCOMMON_EXPORT_H
#define DISCOVERCOMMON_EXPORT_H

#ifdef DISCOVERCOMMON_STATIC_DEFINE
#  define DISCOVERCOMMON_EXPORT
#  define DISCOVERCOMMON_NO_EXPORT
#else
#  ifndef DISCOVERCOMMON_EXPORT
#    ifdef DiscoverCommon_EXPORTS
        /* We are building this library */
#      define DISCOVERCOMMON_EXPORT __attribute__((visibility("default")))
#    else
        /* We are using this library */
#      define DISCOVERCOMMON_EXPORT __attribute__((visibility("default")))
#    endif
#  endif

#  ifndef DISCOVERCOMMON_NO_EXPORT
#    define DISCOVERCOMMON_NO_EXPORT __attribute__((visibility("hidden")))
#  endif
#endif

#ifndef DISCOVERCOMMON_DEPRECATED
#  define DISCOVERCOMMON_DEPRECATED __attribute__ ((__deprecated__))
#endif

#ifndef DISCOVERCOMMON_DEPRECATED_EXPORT
#  define DISCOVERCOMMON_DEPRECATED_EXPORT DISCOVERCOMMON_EXPORT DISCOVERCOMMON_DEPRECATED
#endif

#ifndef DISCOVERCOMMON_DEPRECATED_NO_EXPORT
#  define DISCOVERCOMMON_DEPRECATED_NO_EXPORT DISCOVERCOMMON_NO_EXPORT DISCOVERCOMMON_DEPRECATED
#endif

/* NOLINTNEXTLINE(readability-avoid-unconditional-preprocessor-if) */
#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef DISCOVERCOMMON_NO_DEPRECATED
#    define DISCOVERCOMMON_NO_DEPRECATED
#  endif
#endif

#endif /* DISCOVERCOMMON_EXPORT_H */
