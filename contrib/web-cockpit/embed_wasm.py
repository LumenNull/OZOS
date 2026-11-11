# -*- coding: utf-8 -*-
"""
O.Z.O.S - Automated WASM In-Memory Embedder
Converts compiled zero_dct.wasm into an embedded Base64 string within zero_dct_embedded.js.
Ensures zero-toolchain client-side and Node.js execution without external file dependencies.
"""

import base64
import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, '..', '..'))
core_dir = os.path.join(REPO_ROOT, 'core')
web_dir = SCRIPT_DIR

wasm_path = os.path.join(core_dir, 'zero_dct.wasm')
if not os.path.exists(wasm_path):
    wasm_path = os.path.join(web_dir, 'zero_dct.wasm')

with open(wasm_path, 'rb') as f:
    b64_str = base64.b64encode(f.read()).decode('utf-8')

for target_dir in [core_dir, web_dir]:
    if os.path.exists(target_dir):
        out_js = os.path.join(target_dir, 'zero_dct_embedded.js')
        with open(out_js, 'w', encoding='utf-8') as f:
            f.write('// O.Z.O.S Sovereign Embedded WASM Bytecode (100% In-Memory)\n')
            f.write('var OZOS_WASM_B64 = "' + b64_str + '";\n')
            f.write('if (typeof window !== "undefined") window.OZOS_WASM_B64 = OZOS_WASM_B64;\n')
            f.write('if (typeof module !== "undefined") module.exports = OZOS_WASM_B64;\n')
        print(f"Embedded {len(b64_str)} bytes of Base64 WASM into {out_js}")
