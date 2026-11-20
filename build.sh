#!/usr/bin/env bash
# Builds FEXL Training Tools (build/dinput8.dll) with clang-cl and lld-link, on Linux or Windows.
#
#   FEXL_DEPS=<dir> ./build.sh
#
#   FEXL_DEPS   x64-windows build of Dear ImGui (with its Win32 and DirectX 11 backends) and
#               MinHook: <dir>/include holds imgui.h, imgui_impl_*.h and MinHook.h, and
#               <dir>/lib holds imgui.lib and minhook.x64.lib (a vcpkg x64-windows tree works).
#   CLANG_CL    clang-cl to use (default: clang-cl on PATH)
#   LLD_LINK    lld-link to use (default: lld-link on PATH)
#
# Cross-building from Linux also needs the Windows SDK and MSVC headers and libraries where
# clang-cl and lld-link look for them (for example set up with xwin). Objects are rebuilt only
# when their source or a header changed.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
deps="${FEXL_DEPS:?set FEXL_DEPS to the folder holding ImGui and MinHook (see build.sh)}"
cl="${CLANG_CL:-clang-cl}"
link="${LLD_LINK:-lld-link}"
out="$here/build"
mkdir -p "$out"
flags=(/nologo /c /O2 /W3 /EHs-c- /D_HAS_EXCEPTIONS=0 /D_CRT_SECURE_NO_WARNINGS /std:c++20 /MD /utf-8 -Wno-msvc-not-found)
newest_header=$(find "$here/src" -name '*.hxx' -printf '%T@\n' | sort -n | tail -1)
objects=()
while IFS= read -r source; do
  rel="${source#$here/src/}"
  obj="$out/obj/${rel%.cxx}.obj"
  mkdir -p "$(dirname "$obj")"
  objects+=("$obj")
  if [ ! -f "$obj" ] || [ "$source" -nt "$obj" ] || \
     awk -v h="$newest_header" -v o="$(stat -c %Y "$obj")" 'BEGIN{exit !(h > o)}'; then
    "$cl" "${flags[@]}" "/I$here/src" "/I$deps/include" "$source" "/Fo$obj"
  fi
done < <(find "$here/src" -name '*.cxx' | sort)
"$link" /NOLOGO /DLL "/OUT:$out/dinput8.dll" "/IMPLIB:$out/dinput8.lib" "${objects[@]}" \
  "$deps/lib/imgui.lib" "$deps/lib/minhook.x64.lib" d3d11.lib dxgi.lib user32.lib gdi32.lib dwmapi.lib imm32.lib
ls -la "$out/dinput8.dll"
