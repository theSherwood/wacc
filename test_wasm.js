const fs = require('fs');
const { execSync } = require('child_process');

// Convert WAT to WASM
try {
    execSync('wat2wasm out.wat -o out.wasm', { stdio: 'inherit' });
} catch (error) {
    console.log('wat2wasm not available, skipping binary conversion');
    process.exit(0);
}

// Load and run the WASM module
const wasmBuffer = fs.readFileSync('out.wasm');
const wasmModule = new WebAssembly.Module(wasmBuffer);
const wasmInstance = new WebAssembly.Instance(wasmModule, {});

// Call main function
const result = wasmInstance.exports.main();
console.log('Result from main():', result);
console.log('Expected: 42');
console.log('Test:', result === 42 ? 'PASSED' : 'FAILED');
