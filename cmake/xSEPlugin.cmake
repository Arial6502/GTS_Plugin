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

set(GTS_COMMON_COMPILE_OPTIONS
    /MP
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
)

set(GTS_WARNING_OPTIONS
    /W1
    /WX
    /external:anglebrackets
    /external:W0
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
    /w15205 # delete of an abstract class with a non-virtual destructor (UB)
    /w14548 # expression before comma has no effect
    /w14319 # zero-extending to a greater size
    /w14062 # unhandled enumerator in switch with no default label
    /w14388 # signed/unsigned mismatch in comparison
    /w14267 # conversion from size_t, possible loss of data
    /w14191 # unsafe function-pointer conversion
    /wd4200 # nonstandard extension used : zero-sized array in struct/union
    /wd4100 # 'identifier' : unreferenced formal parameter
    /wd4101 # 'identifier': unreferenced local variable
    /wd4458 # declaration of 'identifier' hides class member
    /wd4459 # declaration of 'identifier' hides global declaration
    /wd4456 # declaration of 'identifier' hides previous local declaration
    /wd4457 # declaration of 'identifier' hides function parameter
    /wd4189 # 'identifier' : local variable is initialized but not referenced
)

target_compile_options(
    "${PROJECT_NAME}"
    PRIVATE
    ${GTS_COMMON_COMPILE_OPTIONS}
    ${GTS_WARNING_OPTIONS}
)

# --- Vendored third-party libraries ---
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/lib")
target_link_libraries("${PROJECT_NAME}" PRIVATE imgui)

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