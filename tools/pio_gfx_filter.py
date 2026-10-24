"""PlatformIO pre-build script: compiles only the GFX Library for Arduino sources
that the firmware actually uses.

The library ships ~65 display drivers and ~25 data buses, and PlatformIO compiles
all of them (each object is ~6 MB because every file pulls in U8g2lib.h). The
CYD only needs the ILI9341 driver over ESP32 SPI. This script drops every other
library source from the build; it does not touch any file on disk.

Enabled per environment in platformio.ini:

  extra_scripts = pre:tools/pio_gfx_filter.py
  custom_gfx_sources =
    Arduino_GFX.cpp
    display/Arduino_ILI9341.cpp

custom_gfx_sources lists paths relative to the library's src/ folder. If it is
empty or missing, the whole library is built as usual.

If the link fails with "undefined reference to Arduino_Xxx", add the .cpp that
defines Arduino_Xxx to custom_gfx_sources.
"""

#region Imports
import os

Import("env")  # noqa: F821 - injected by PlatformIO/SCons
#endregion

#region Configuration
GFX_LIBRARY_FOLDER_NAME = "GFX Library for Arduino"
GFX_SOURCES_OPTION_NAME = "custom_gfx_sources"
#endregion

#region Internal helpers

def read_kept_gfx_sources(build_env) -> set:
    """Returns the custom_gfx_sources entries as normalized relative paths."""
    raw_option_value = build_env.GetProjectOption(GFX_SOURCES_OPTION_NAME, "")
    kept_sources = set()
    for option_line in raw_option_value.splitlines():
        relative_path = option_line.strip().replace("\\", "/")
        if relative_path:
            kept_sources.add(relative_path.lower())
    return kept_sources


def get_path_inside_gfx_library(source_absolute_path: str):
    """Returns the path relative to the GFX library's src/ folder, or None if the
    file does not belong to that library."""
    normalized_path = source_absolute_path.replace("\\", "/")
    library_src_marker = "/" + GFX_LIBRARY_FOLDER_NAME + "/src/"
    marker_index = normalized_path.find(library_src_marker)
    if marker_index < 0:
        return None
    return normalized_path[marker_index + len(library_src_marker):]

#endregion

#region Register build middleware
kept_gfx_sources = read_kept_gfx_sources(env)  # noqa: F821

if kept_gfx_sources:
    print("GFX filter: building only " + ", ".join(sorted(kept_gfx_sources)))

    def skip_unused_gfx_source(build_env, source_node):
        #region Leave files from other libraries and the project alone
        path_inside_library = get_path_inside_gfx_library(
            source_node.srcnode().get_abspath())
        if path_inside_library is None:
            return source_node
        #endregion

        #region Keep only the listed library sources
        if path_inside_library.lower() in kept_gfx_sources:
            return source_node
        return None  # None removes the file from the build
        #endregion

    env.AddBuildMiddleware(skip_unused_gfx_source,  # noqa: F821
                           "*" + GFX_LIBRARY_FOLDER_NAME + "*")
#endregion
