const fs = require("fs");

// Get the filename from the first positional argument or default to "out.wasm"
const wasmFilename = process.argv[2] || "out.wasm";

try {
  // Load and run the WASM module
  const wasmBuffer = fs.readFileSync(wasmFilename);
  const wasmModule = new WebAssembly.Module(wasmBuffer);
  const wasmInstance = new WebAssembly.Instance(wasmModule, {});

  // Call main function
  const result = wasmInstance.exports.main();

  console.log(result);
} catch (error) {
  console.error(`Failed to load or run WASM file "${wasmFilename}":`, error.message);
  process.exit(1);
}
