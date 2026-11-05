# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
# SPDX-FileCopyrightText: 2026 PeriBluGaming
#
# Configure guard for the local psxrecomp patches in patches/psxrecomp/.
#
# mods/src/widescreen.c calls psx_mod_tag_reveal_edge_quad(), which patch 0001
# adds to the pinned framework. Without it the build would only fail at link
# time with an undefined symbol, so stop here with the fix instead. This checks
# that every piece the mod needs is present (header, declaration, definition,
# mod wrapper); scripts/apply_framework_patches.sh verifies the exact content.
# The files are configure dependencies, so removing the patch later re-runs
# this check on the next build.
function(sledstorm_require_framework_patches psxrecomp_root)
    set(_needs
        "runtime/include/ws_reveal_edge.h|WS_REVEAL_EDGE_LEFT"
        "runtime/include/mod_plugins.h|psx_mod_tag_reveal_edge_quad("
        "runtime/src/gpu.c|void gpu_ws_tag_reveal_edge_quad("
        "runtime/src/mod_runtime.cpp|psx_mod_tag_reveal_edge_quad(uint32_t primitive, int edges)")
    set(_missing "")
    foreach(_need IN LISTS _needs)
        string(REPLACE "|" ";" _parts "${_need}")
        list(GET _parts 0 _file)
        list(GET _parts 1 _text)
        set(_path "${psxrecomp_root}/${_file}")
        if(EXISTS "${_path}")
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_path}")
            file(READ "${_path}" _content)
            string(FIND "${_content}" "${_text}" _at)
            if(_at EQUAL -1)
                list(APPEND _missing "${_file} (no '${_text}')")
            endif()
        else()
            list(APPEND _missing "${_file} (missing)")
        endif()
    endforeach()
    if(_missing)
        string(REPLACE ";" "\n  " _list "${_missing}")
        message(FATAL_ERROR
            "psxrecomp at ${psxrecomp_root} lacks the local patches this title "
            "needs (patches/psxrecomp/):\n  ${_list}\n"
            "Apply them from the repository root with:\n"
            "  bash scripts/apply_framework_patches.sh\n"
            "The script checks the pinned revision (patches/psxrecomp/BASE) and "
            "refuses a modified framework checkout.")
    endif()
endfunction()
