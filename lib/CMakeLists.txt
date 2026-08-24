# #######################################################################################################################
# # Vendored third-party libraries
# #######################################################################################################################

if(NOT DEFINED GTS_COMMON_COMPILE_OPTIONS)
    message(FATAL_ERROR "GTS_COMMON_COMPILE_OPTIONS is not set -- lib/ must be added after it is defined.")
endif()

# --- Dear ImGui - https://github.com/ocornut/imgui ---

file(GLOB IMGUI_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/imgui/*.cpp")
file(GLOB IMGUI_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/imgui/*.h")

add_library(imgui STATIC ${IMGUI_SOURCES} ${IMGUI_HEADERS})

target_compile_definitions(
    imgui
    PUBLIC
    IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    IMGUI_DEFINE_MATH_OPERATORS
)

get_target_property(GTS_CLIB_DEFINITIONS CommonLibSSE::CommonLibSSE INTERFACE_COMPILE_DEFINITIONS)

foreach(GTS_DEFINITION IN LISTS GTS_CLIB_DEFINITIONS)
    if(GTS_DEFINITION MATCHES "^(WINVER|_WIN32_WINNT)=")
        target_compile_definitions(imgui PRIVATE "${GTS_DEFINITION}")
        message(STATUS "Vendored: imgui inherits ${GTS_DEFINITION} from CommonLibSSE")
    endif()
endforeach()

target_include_directories(imgui SYSTEM PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_compile_options(
    imgui
    PRIVATE
    ${GTS_COMMON_COMPILE_OPTIONS}
    /W0
)

# Matches CMAKE_CXX_STANDARD
target_compile_features(imgui PRIVATE cxx_std_23)

set_target_properties(imgui PROPERTIES FOLDER "ExternalDependencies")

list(LENGTH IMGUI_SOURCES IMGUI_SOURCE_COUNT)
message(STATUS "Vendored: imgui (${IMGUI_SOURCE_COUNT} sources, /W0)")
