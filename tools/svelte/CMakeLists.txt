# The host of the Svelte applications that render with Studio Kit, which the renderers' tests run beside the programs
# that the applications were ported from. It declares those programs itself, so it holds the sections of
# examples/studio_gallery, whose sources it shares with the example.
if (PR_JS)
    set(gallery ${PROJECT_SOURCE_DIR}/examples/studio_gallery)
    add_executable(svelte_kit_host kit_host.cpp
        ${gallery}/gallery_parts.cpp
        ${gallery}/gallery_state.cpp
        ${gallery}/gallery_view.cpp
        ${gallery}/section_components.cpp
        ${gallery}/section_controls.cpp
        ${gallery}/section_data.cpp
        ${gallery}/section_feedback.cpp
        ${gallery}/section_foundations.cpp
    )
    target_include_directories(svelte_kit_host PRIVATE ${PROJECT_SOURCE_DIR})
    target_compile_definitions(svelte_kit_host PRIVATE PR_FONT_BUNDLE_DIR="${PR_FONT_BUNDLE_DIR}")
    target_link_libraries(svelte_kit_host PRIVATE prax::studio_kit_quickjs prax::studio_kit prax::studio_app
                                                  prax::html_quickjs prax::html prax::dom prax::font_bundle
                                                  prax::font prax::text prax::draw prax::layout prax::ui
                                                  prax::ui_cpp prax::app prax::compress prax::image
                                                  prax::core_cpp harness harness_png pr_warnings)
endif()
