const fs = require("fs");

// Load and run the WASM module
const wasmBuffer = fs.readFileSync("out.wasm");
const wasmModule = new WebAssembly.Module(wasmBuffer);
const wasmInstance = new WebAssembly.Instance(wasmModule, {});

// Call main function
const result = wasmInstance.exports.main();

console.log(result);
