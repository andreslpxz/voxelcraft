#!/usr/bin/env python3
"""
Compila los shaders GLSL de VoxelCraft a SPIR-V y genera los headers C
embebidos (`block.vert.h`, `block.frag.h`, ...) que el renderer de Vulkan
espera encontrar cuando se define VOXELCRAFT_EMBEDDED_SHADERS.

Cada header tiene la forma:

    #pragma once
    #include <cstdint>
    static const unsigned char block_vert_spv[] = { 0x03, 0x02, ... };
    static const size_t block_vert_spv_len = sizeof(block_vert_spv);

De esta forma el renderer puede hacer:

    createShaderModule(*ctx_, (const uint32_t*)block_vert_spv, block_vert_spv_len);

Sin depender de que glslc este presente en la maquina del usuario en tiempo
de build - los shaders ya estan compilados y commiteados en el repo.
"""

from __future__ import annotations
import os
import subprocess
import sys
from pathlib import Path

# ---------------------------------------------------------------------------
# Configuracion
# ---------------------------------------------------------------------------
REPO_ROOT   = Path("/home/z/my-project/voxelcraft")
SHADER_DIR  = REPO_ROOT / "src" / "shaders"
OUTPUT_DIR  = SHADER_DIR  # Los headers viven junto a los .vert/.frag
GLSLANG     = Path("/home/z/my-project/tools/glslang/bin/glslangValidator")

SHADERS = [
    "block.vert",
    "block.frag",
    "sky.vert",
    "sky.frag",
    "ui.vert",
    "ui.frag",
]


def compile_glsl_to_spirv(src: Path, dst: Path) -> None:
    """Compila un GLSL a SPIR-V usando glslangValidator."""
    cmd = [
        str(GLSLANG),
        "-V",            # Target Vulkan
        "--quiet",       # Sin output informativo
        "--target-env", "vulkan1.1",  # Compatible con la mayoria de GPUs moviles
        "-o", str(dst),
        str(src),
    ]
    print(f"  [compile] {src.name} -> {dst.name}")
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"ERROR compilando {src}:")
        print(result.stderr)
        sys.exit(1)


def spirv_to_c_header(spirv_path: Path, header_path: Path) -> None:
    """
    Convierte un binario SPIR-V a un header C con el array de bytes y el tamano.

    Genera:
        #pragma once
        #include <cstdint>
        #include <cstddef>
        static const unsigned char <name>_spv[] = { ... };
        static const size_t <name>_spv_len = sizeof(<name>_spv);
    """
    data = spirv_path.read_bytes()
    # Nombre del simbolo: "block.vert" -> "block_vert"
    stem = spirv_path.stem.replace(".", "_")
    symbol = f"{stem}_spv"

    lines = []
    lines.append("#pragma once")
    lines.append("// AUTO-GENERATED - do not edit. Regenerate with scripts/compile_shaders.py")
    lines.append("#include <cstdint>")
    lines.append("#include <cstddef>")
    lines.append("")
    lines.append(f"static const unsigned char {symbol}[] = {{")
    # 12 bytes por linea para mantener legibilidad
    for i in range(0, len(data), 12):
        chunk = data[i:i+12]
        hex_vals = ", ".join(f"0x{b:02x}" for b in chunk)
        lines.append(f"    {hex_vals},")
    lines.append("};")
    lines.append(f"static const size_t {symbol}_len = sizeof({symbol});")
    lines.append("")

    header_path.write_text("\n".join(lines), encoding="utf-8")
    print(f"  [header]   {header_path.name}  ({len(data)} bytes)")


def main() -> None:
    print("VoxelCraft shader compiler")
    print(f"  GLSL dir : {SHADER_DIR}")
    print(f"  Output   : {OUTPUT_DIR}")
    print(f"  glslang  : {GLSLANG}")
    print()

    if not GLSLANG.exists():
        print(f"ERROR: glslangValidator no encontrado en {GLSLANG}")
        sys.exit(1)

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    tmp_dir = Path("/tmp/voxelcraft_shaders")
    tmp_dir.mkdir(parents=True, exist_ok=True)

    for shader in SHADERS:
        src = SHADER_DIR / shader
        if not src.exists():
            print(f"ERROR: No existe {src}")
            sys.exit(1)
        # Compilar a .spv en tmp
        spv_path = tmp_dir / f"{shader}.spv"
        compile_glsl_to_spirv(src, spv_path)
        # Generar header C junto al .vert/.frag
        header_path = OUTPUT_DIR / f"{shader}.h"
        spirv_to_c_header(spv_path, header_path)

    print()
    print("OK - todos los shaders compilados y embebidos como headers C.")


if __name__ == "__main__":
    main()
