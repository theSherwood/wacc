const fs = require('fs');
const { execSync } = require('child_process');

// We now generate WASM directly, so no conversion needed
console.log('Loading WASM module...');

// Load and run the WASM module
const wasmBuffer = fs.readFileSync('out.wasm');
const wasmModule = new WebAssembly.Module(wasmBuffer);
const wasmInstance = new WebAssembly.Instance(wasmModule, {});

// Call main function
const result = wasmInstance.exports.main();
console.log('Result from main():', result);
console.log('Expected: 42');
console.log('Test:', result === 42 ? 'PASSED' : 'FAILED');
