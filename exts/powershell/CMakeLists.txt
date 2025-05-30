project(PowershellExt LANGUAGES CXX)

set(MANAGED_LIBRARY_PUBLISH_DIR "${CMAKE_HOME_DIRECTORY}/ManagedLibrary/publish") # Path from dotnet publish -o

# --- Define the shared library target ---
# The first argument is the target name. CMake will automatically name the file
# appropriately for the platform (e.g., my_cool_plugin.dll, libmy_cool_plugin.so, libmy_cool_plugin.dylib).
add_library(PowershellExt SHARED
		PowershellExt.cpp PowershellExt.h
)

# Assuming MainApp.exe is in CMAKE_BINARY_DIR for single-config MinGW:
set(PLUGIN_BASE_OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins")
set(PLUGIN_SUBDIR "ext")
set(PLUGIN_FULL_OUTPUT_DIR "${PLUGIN_BASE_OUTPUT_DIR}/${PLUGIN_SUBDIR}")

# --- Include Directories ---
target_include_directories(PowershellExt PUBLIC
		"${CMAKE_DOTNET_TARGET_FRAMEWORK}"
		"${CMAKE_SOURCE_DIR}/include"
)

#    CMake can usually handle the .lib extension directly.
#    Provide the full path to nethost.lib.
set(NET_HOST_DLL "${CMAKE_DOTNET_TARGET_FRAMEWORK}\\nethost.dll")

message("***** NET_HOST_DLL: ${NET_HOST_DLL} *****")

string(REPLACE "\\" "/" FRAMEWORK_LINK_DIR "${CMAKE_DOTNET_TARGET_FRAMEWORK}")

if (EXISTS "${CMAKE_DOTNET_TARGET_FRAMEWORK}\\nethost.lib")
	target_link_directories(PowershellExt PRIVATE "${FRAMEWORK_LINK_DIR}")
	target_link_libraries(PowershellExt PRIVATE nethost)
else ()
	message(FATAL_ERROR "nethost.lib not found. Please check your .NET SDK installation.")
endif ()

# Check if the library file exists
foreach (NET_LIB nethost.lib nethost.dll)
	if (EXISTS "${FRAMEWORK_LINK_DIR}/${NET_LIB}")
		# Link the nethost library to your PowershellExt target
		message("${FRAMEWORK_LINK_DIR}/${NET_LIB}")
		add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
				COMMAND ${CMAKE_COMMAND} -E make_directory "${PLUGIN_FULL_OUTPUT_DIR}" # Ensure directory exists
				COMMAND ${CMAKE_COMMAND} -E copy
				"${FRAMEWORK_LINK_DIR}/${NET_LIB}"
				"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
				COMMENT "Copying ${NET_HOST_DLL} to ${PLUGIN_SUBDIR}"
				VERBATIM)
	else ()
		message(FATAL_ERROR "nethost.dll not found at ${NET_HOST_DLL}. Please check CMAKE_DOTNET_TARGET_FRAMEWORK and your .NET SDK installation.")
	endif ()
endforeach (NET_LIB)


# On Windows, this ensures that the functions are exported from the DLL.
# On other platforms (Linux, macOS), __attribute__((visibility("default"))) is typically used.
if (WIN32)
	target_compile_definitions(PowershellExt PRIVATE POWERSHELL_PLUGIN_EXPORTS)
endif ()

# Glob all files in the source assets directory
file(GLOB ASSET_FILES "${MANAGED_LIBRARY_PUBLISH_DIR}/*") # Gets files directly in the folder

# Ensure destination directory exists
file(MAKE_DIRECTORY "${PLUGIN_BASE_OUTPUT_DIR}/managed")

# Loop through the globbed files and copy them
foreach (ASSET_FILE ${ASSET_FILES})
	file(COPY "${ASSET_FILE}" DESTINATION "${PLUGIN_BASE_OUTPUT_DIR}/managed")
endforeach ()

# On Windows, also place the PDB (Program Database for debugging) file alongside the DLL.
set_target_properties(PowershellExt PROPERTIES
		LIBRARY_OUTPUT_DIRECTORY "${PLUGIN_FULL_OUTPUT_DIR}"
		RUNTIME_OUTPUT_DIRECTORY "${PLUGIN_FULL_OUTPUT_DIR}"
		PDB_OUTPUT_DIRECTORY "${PLUGIN_FULL_OUTPUT_DIR}"
)
