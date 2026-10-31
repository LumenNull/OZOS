import base64
import os

dir_path = r'c:\Users\9htru\OneDrive\Desktop\Commercial_Projects_Archive\OZOS\core'
wasm_path = os.path.join(dir_path, 'zero_dct.wasm')
out_js = os.path.join(dir_path, 'zero_dct_embedded.js')

with open(wasm_path, 'rb') as f:
    b64_str = base64.b64encode(f.read()).decode('utf-8')

with open(out_js, 'w', encoding='utf-8') as f:
    f.write('// O.Z.O.S Sovereign Embedded WASM Bytecode (100% In-Memory)\n')
    f.write('var OZOS_WASM_B64 = "' + b64_str + '";\n')
    f.write('if (typeof window !== "undefined") window.OZOS_WASM_B64 = OZOS_WASM_B64;\n')
    f.write('if (typeof module !== "undefined") module.exports = OZOS_WASM_B64;\n')

print(f"Embedded {len(b64_str)} bytes of Base64 WASM into {out_js}")
