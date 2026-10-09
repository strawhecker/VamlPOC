
#ifndef VAML_TOOLS_EXPORT_H
#define VAML_TOOLS_EXPORT_H

#ifdef VAML_TOOLS_STATIC_DEFINE
#  define VAML_TOOLS_EXPORT
#  define VAML_TOOLS_NO_EXPORT
#else
#  ifndef VAML_TOOLS_EXPORT
#    ifdef vaml_tools_EXPORTS
        /* We are building this library */
#      define VAML_TOOLS_EXPORT 
#    else
        /* We are using this library */
#      define VAML_TOOLS_EXPORT 
#    endif
#  endif

#  ifndef VAML_TOOLS_NO_EXPORT
#    define VAML_TOOLS_NO_EXPORT 
#  endif
#endif

#ifndef VAML_TOOLS_DEPRECATED
#  define VAML_TOOLS_DEPRECATED __declspec(deprecated)
#endif

#ifndef VAML_TOOLS_DEPRECATED_EXPORT
#  define VAML_TOOLS_DEPRECATED_EXPORT VAML_TOOLS_EXPORT VAML_TOOLS_DEPRECATED
#endif

#ifndef VAML_TOOLS_DEPRECATED_NO_EXPORT
#  define VAML_TOOLS_DEPRECATED_NO_EXPORT VAML_TOOLS_NO_EXPORT VAML_TOOLS_DEPRECATED
#endif

/* NOLINTNEXTLINE(readability-avoid-unconditional-preprocessor-if) */
#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef VAML_TOOLS_NO_DEPRECATED
#    define VAML_TOOLS_NO_DEPRECATED
#  endif
#endif

#endif /* VAML_TOOLS_EXPORT_H */
