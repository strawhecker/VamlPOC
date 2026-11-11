
#ifndef VAML_CORE_EXPORT_H
#define VAML_CORE_EXPORT_H

#ifdef VAML_CORE_STATIC_DEFINE
#  define VAML_CORE_EXPORT
#  define VAML_CORE_NO_EXPORT
#else
#  ifndef VAML_CORE_EXPORT
#    ifdef vaml_core_EXPORTS
        /* We are building this library */
#      define VAML_CORE_EXPORT 
#    else
        /* We are using this library */
#      define VAML_CORE_EXPORT 
#    endif
#  endif

#  ifndef VAML_CORE_NO_EXPORT
#    define VAML_CORE_NO_EXPORT 
#  endif
#endif

#ifndef VAML_CORE_DEPRECATED
#  define VAML_CORE_DEPRECATED __declspec(deprecated)
#endif

#ifndef VAML_CORE_DEPRECATED_EXPORT
#  define VAML_CORE_DEPRECATED_EXPORT VAML_CORE_EXPORT VAML_CORE_DEPRECATED
#endif

#ifndef VAML_CORE_DEPRECATED_NO_EXPORT
#  define VAML_CORE_DEPRECATED_NO_EXPORT VAML_CORE_NO_EXPORT VAML_CORE_DEPRECATED
#endif

/* NOLINTNEXTLINE(readability-avoid-unconditional-preprocessor-if) */
#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef VAML_CORE_NO_DEPRECATED
#    define VAML_CORE_NO_DEPRECATED
#  endif
#endif

#endif /* VAML_CORE_EXPORT_H */
