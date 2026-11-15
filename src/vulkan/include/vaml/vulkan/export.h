
#ifndef VAML_VULKAN_EXPORT_H
#define VAML_VULKAN_EXPORT_H

#ifdef VAML_VULKAN_STATIC_DEFINE
#  define VAML_VULKAN_EXPORT
#  define VAML_VULKAN_NO_EXPORT
#else
#  ifndef VAML_VULKAN_EXPORT
#    ifdef vaml_vulkan_EXPORTS
        /* We are building this library */
#      define VAML_VULKAN_EXPORT 
#    else
        /* We are using this library */
#      define VAML_VULKAN_EXPORT 
#    endif
#  endif

#  ifndef VAML_VULKAN_NO_EXPORT
#    define VAML_VULKAN_NO_EXPORT 
#  endif
#endif

#ifndef VAML_VULKAN_DEPRECATED
#  define VAML_VULKAN_DEPRECATED __attribute__ ((__deprecated__))
#endif

#ifndef VAML_VULKAN_DEPRECATED_EXPORT
#  define VAML_VULKAN_DEPRECATED_EXPORT VAML_VULKAN_EXPORT VAML_VULKAN_DEPRECATED
#endif

#ifndef VAML_VULKAN_DEPRECATED_NO_EXPORT
#  define VAML_VULKAN_DEPRECATED_NO_EXPORT VAML_VULKAN_NO_EXPORT VAML_VULKAN_DEPRECATED
#endif

/* NOLINTNEXTLINE(readability-avoid-unconditional-preprocessor-if) */
#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef VAML_VULKAN_NO_DEPRECATED
#    define VAML_VULKAN_NO_DEPRECATED
#  endif
#endif

#endif /* VAML_VULKAN_EXPORT_H */
