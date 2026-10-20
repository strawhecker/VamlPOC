# src/presentation — vaml-presentation library (Sprint 4B S173-S175 OPTIONAL framework).
#
# The OPTIONAL presentation/composition framework ABOVE the VAML primitives. The
# library (vaml-core/vaml-vulkan) stays a NEUTRAL toolkit; the COMPOSITION policy
# lives ONLY here, so a user can DIY their own on the library tools OR adopt the
# framework ("with-or-without-framework" is preserved — not linking vaml-presentation
# = the primitive-only consumer).
#
# Layer: vaml-presentation is a VULKAN-layer target (it wraps vaml-vulkan: S167_1 +
# S166 + S165 + the S169 Device), but it is deliberately NOT appended to
# VAML_CORE_LIBRARIES by the artifact model — so ADR-011's core-link check never
# trips on it and the framework stays optional. It links vaml-vulkan PUBLIC (its
# includes/type surface) so consuming headers (Presenter.h / CompositionEngine.h)
# can reference the vaml::vulkan types.
#
# vaml_add_library() creates the vaml-presentation target (with its generated export
# header vaml/presentation/export.h). The libraries MUST be built via the artifact
# model — see cmake/artifact-model.cmake.
vaml_add_library(presentation
    src/Presenter.cpp
    src/CompositionEngine.cpp
    src/version.cpp
)

# Only vaml-presentation links the framework onto the primitives; it NEVER joins the
# CORE aggregation (ADR-011 stays intact — the framework is Vulkan-layer, optional).
target_link_libraries(vaml-presentation
    PUBLIC vaml-vulkan
)
