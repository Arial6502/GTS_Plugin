find_package(CommonLibSSE CONFIG REQUIRED)


#mimicks add_commonlibsse_plugin but without the PluginInfo Insertion
add_library("${PROJECT_NAME}" SHARED ${headers} ${sources})
target_compile_definitions("${PROJECT_NAME}"
    PRIVATE 
    __CMAKE_COMMONLIBSSE_PLUGIN=1
    ENABLE_COMMONLIBSSE_TESTING=1
)
target_link_libraries("${PROJECT_NAME}" PUBLIC CommonLibSSE::CommonLibSSE)
target_include_directories("${PROJECT_NAME}" PUBLIC ${CommonLibSSE_INCLUDE_DIRS})

add_library("${PROJECT_NAME}::${PROJECT_NAME}" ALIAS "${PROJECT_NAME}")

target_compile_features(
	"${PROJECT_NAME}"
	PRIVATE
	cxx_std_23
)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include(AddCXXFiles)
add_cxx_files("${PROJECT_NAME}")

target_precompile_headers(
	"${PROJECT_NAME}"
	PRIVATE
	src/PCH.hpp
)

# Build DLL RC
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.rc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/version.rc"
    @ONLY
)
target_sources(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/version.rc")

# Build Version.hpp from project info.
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Version.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/src/Version.hpp"
    @ONLY
)
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/src")

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME ON)

add_compile_definitions(NOMINMAX)
add_compile_definitions(_UNICODE)

# --- Common compiler options for all configurations ---
target_compile_options(
    "${PROJECT_NAME}"
    PRIVATE
    /MP
    $<$<BOOL:${GTS_STRICT_COMPILE}>:/W4;/WX>
    $<$<NOT:$<BOOL:${GTS_STRICT_COMPILE}>>:/W1>
    /permissive-
    /utf-8
    /Zc:alignedNew
    /Zc:auto
    /Zc:__cplusplus
    /Zc:externC
    /Zc:externConstexpr
    /Zc:forScope
    /Zc:hiddenFriend
    /Zc:implicitNoexcept
    /Zc:lambda
    /Zc:noexceptTypes
    /Zc:preprocessor
    /Zc:referenceBinding
    /Zc:rvalueCast
    /Zc:sizedDealloc
    /Zc:strictStrings
    /Zc:ternary
    /Zc:threadSafeInit
    /Zc:trigraphs
    /Zc:wchar_t
    #/Zc:char8_t- JSONCpp needs it

    # --- External header noise suppression ---
    # Required for the warnings below to be usable. Promoting a warning to level 1
    # promotes it everywhere, including inside CommonLibSSE, the STL and vcpkg
    # headers, which would bury our own hits and (under GTS_STRICT_COMPILE) fail
    # the build on code we do not own. This project includes third-party headers
    # with <> and its own with "", so the angle-bracket rule maps cleanly.
    # Caveat: vendored ImGui in src/UI/Lib is quoted, so it stays in scope.
    /external:anglebrackets
    /external:W0

    # --- Correctness warnings ---
    # These are off by default or sit at /W3-/W4. /w1NNNN forces each to level 1
    # so it is reported even in the default /W1 build. Selected for bug-finding
    # value only: nothing here is about style, naming or unused entities.
    /w14263 # member function does not override any base class virtual function
    /w14264 # no override available for virtual member function; function is hidden
    /w14265 # class has virtual functions but destructor is not virtual
    /w14266 # no override available for virtual member function from base
    /w15204 # class has virtual functions but its trivial destructor is not virtual
    /w15038 # data member will be initialized after another (reordered init)
    /w15262 # implicit fall-through between switch cases
    /w15263 # calling std::move on a temporary prevents copy elision
    /w14296 # expression is always true or always false
    /w14555 # result of expression not used
    /w14701 # potentially uninitialized local variable used
    /w14703 # potentially uninitialized local pointer variable used
    /w14826 # conversion is sign-extended, may cause unexpected runtime behaviour
    /w14928 # illegal copy-initialization; more than one user-defined conversion
    /w14946 # reinterpret_cast used between related classes
    /w14287 # unsigned/negative constant mismatch

    /wd4200 # nonstandard extension used : zero-sized array in struct/union
    /wd4100 # 'identifier' : unreferenced formal parameter
    /wd4101 # 'identifier': unreferenced local variable
    /wd4458 # declaration of 'identifier' hides class member
    /wd4459 # declaration of 'identifier' hides global declaration
    /wd4456 # declaration of 'identifier' hides previous local declaration
    /wd4457 # declaration of 'identifier' hides function parameter
    /wd4189 # 'identifier' : local variable is initialized but not referenced
)

# --- Linker Options ---
target_link_options(
	${PROJECT_NAME}
	PRIVATE
	/WX
)

if(CMAKE_GENERATOR MATCHES "Visual Studio" AND TARGET CommonLibSSE)
    set_target_properties(CommonLibSSE PROPERTIES
        FOLDER "ExternalDependencies"
    )
endif()

target_include_directories(
	${PROJECT_NAME}
	PRIVATE
	${CMAKE_CURRENT_BINARY_DIR}/cmake
	${CMAKE_CURRENT_SOURCE_DIR}/src
)