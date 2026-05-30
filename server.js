const express = require('express');
const path = require('path');
const fs = require('fs');
const { exec } = require('child_process');

const app = express();
const PORT = process.env.PORT || 3000;

// Body parser middleware
app.use(express.json({ limit: '5mb' }));
app.use(express.static(path.join(__dirname, 'public')));

// Helper: Ensure the tests folder exists
const testsDir = path.join(__dirname, 'tests');
if (!fs.existsSync(testsDir)) {
  fs.mkdirSync(testsDir, { recursive: true });
}

// ─── API: Get Presets ────────────────────────────────────────────────────────
app.get('/api/presets', (req, res) => {
  const presets = {};
  const testFiles = ['test1.ll', 'test2.ll', 'test3.ll', 'test4.ll', 'test5.ll'];
  
  testFiles.forEach(file => {
    const filePath = path.join(testsDir, file);
    if (fs.existsSync(filePath)) {
      presets[file.replace('.ll', '')] = fs.readFileSync(filePath, 'utf8');
    }
  });

  res.json(presets);
});

// ─── Helper: Detect if input is C code vs LLVM IR ────────────────────────────
function isCCode(code) {
  const trimmed = code.trim();
  // C code indicators
  if (trimmed.startsWith('#include')) return true;
  if (trimmed.startsWith('#define')) return true;
  if (trimmed.startsWith('#pragma')) return true;
  if (/^\s*int\s+main\s*\(/.test(trimmed)) return true;
  if (/^\s*void\s+\w+\s*\(/.test(trimmed)) return true;
  if (/^\s*(int|float|double|char|void|long|unsigned|short)\s+\w+\s*\(/.test(trimmed)) return true;
  if (/^\s*#\s*include\s/.test(trimmed)) return true;
  // LLVM IR indicators (if it starts with these, it's definitely IR)
  if (trimmed.startsWith(';')) return false;
  if (trimmed.startsWith('define ')) return false;
  if (trimmed.startsWith('declare ')) return false;
  if (trimmed.startsWith('@')) return false;
  if (trimmed.startsWith('target ')) return false;
  if (trimmed.startsWith('source_filename')) return false;
  // If it has a lot of C-like constructs
  if (/\bprintf\s*\(/.test(trimmed) && !/declare.*printf/.test(trimmed)) return true;
  if (/\breturn\s+\d+;/.test(trimmed)) return true;
  return false;
}

// ─── Helper: Run a shell command inside WSL returning a promise ──────────────
function wslExec(cmd) {
  return new Promise((resolve, reject) => {
    // Write command to a temporary bash script to avoid Windows CMD quoting issues
    const scriptName = 'temp_run_' + Date.now() + '.sh';
    const scriptPath = path.join(__dirname, 'tests', scriptName);
    const wslScriptPath = '/mnt/d/Downloads/cdlabel/tests/' + scriptName;
    
    fs.writeFileSync(scriptPath, cmd, 'utf8');
    
    // Convert line endings just in case
    const wslCmd = `wsl -e bash -c "dos2unix ${wslScriptPath} > /dev/null 2>&1; bash ${wslScriptPath}"`;
    
    exec(wslCmd, { maxBuffer: 10 * 1024 * 1024 }, (error, stdout, stderr) => {
      // Clean up script
      if (fs.existsSync(scriptPath)) {
        try { fs.unlinkSync(scriptPath); } catch(e) {}
      }
      resolve({ error, stdout, stderr });
    });
  });
}

// ─── API: Optimize Code (C or LLVM IR) ──────────────────────────────────────
app.post('/api/optimize', async (req, res) => {
  const { code } = req.body;
  if (!code) {
    return res.status(400).json({ error: 'No code provided' });
  }

  const isC = isCCode(code);
  const inputCFile = 'web_temp_input.c';
  const inputLLFile = 'web_temp_input.ll';
  const outputLLFile = 'web_temp_output.ll';
  const wslProjectDir = '/mnt/d/Downloads/cdlabel';

  let compilationLog = '';
  let llvmIRInput = '';

  try {
    if (isC) {
      // ── Step 1: Write C code to temp file ──────────────────────────────
      fs.writeFileSync(path.join(testsDir, inputCFile), code, 'utf8');
      compilationLog += '[Web Server] Detected C source code. Compiling to LLVM IR via clang...\n\n';

      // ── Step 2: Compile C → LLVM IR using clang in WSL ─────────────────
      const clangCmd = `cd ${wslProjectDir} && clang-17 -S -emit-llvm -O0 -Xclang -disable-O0-optnone tests/${inputCFile} -o tests/${inputLLFile} 2>&1`;
      const clangResult = await wslExec(clangCmd);

      if (clangResult.stderr) {
        compilationLog += '[clang-17] ' + clangResult.stderr + '\n';
      }
      if (clangResult.stdout) {
        compilationLog += clangResult.stdout + '\n';
      }

      // Check if IR file was generated
      const llFilePath = path.join(testsDir, inputLLFile);
      if (!fs.existsSync(llFilePath)) {
        return res.status(500).json({
          error: 'C compilation failed',
          details: compilationLog + '\nclang did not produce an output .ll file.'
        });
      }

      // ── Step 2.5: Promote Memory to Register (mem2reg) ─────────────────
      // We are skipping mem2reg now so that our custom DIE pass can see
      // and eliminate dead allocas and stores itself!
      /*
      compilationLog += '[Web Server] Running mem2reg to lift local variables to SSA registers...\n';
      const mem2regCmd = `cd ${wslProjectDir} && opt-17 -passes=mem2reg -S tests/${inputLLFile} -o tests/${inputLLFile} 2>&1`;
      const mem2regResult = await wslExec(mem2regCmd);
      if (mem2regResult.stderr) {
        compilationLog += '[opt-17] ' + mem2regResult.stderr + '\n';
      }
      if (mem2regResult.stdout) {
        compilationLog += mem2regResult.stdout + '\n';
      }
      */

      llvmIRInput = fs.readFileSync(llFilePath, 'utf8');
      compilationLog += '[Web Server] Compilation successful. Generated LLVM IR (' + llvmIRInput.split('\n').length + ' lines).\n';
      compilationLog += '═'.repeat(60) + '\n\n';

    } else {
      // ── Direct LLVM IR input ───────────────────────────────────────────
      fs.writeFileSync(path.join(testsDir, inputLLFile), code, 'utf8');
      llvmIRInput = code;
      compilationLog += '[Web Server] Detected LLVM IR input. Skipping compilation step.\n';
      compilationLog += '═'.repeat(60) + '\n\n';
    }

    // ── Step 3: Run the Dead Instruction Eliminator pass ─────────────────
    const dieCmd = `cd ${wslProjectDir} && ./build/die-tool tests/${inputLLFile} -o tests/${outputLLFile} -v 2>&1`;
    const dieResult = await wslExec(dieCmd);

    const report = dieResult.stdout || dieResult.stderr || '';
    compilationLog += report;

    // ── Step 4: Read optimized output ────────────────────────────────────
    let optimizedCode = '';
    const outputFilePath = path.join(testsDir, outputLLFile);
    if (fs.existsSync(outputFilePath)) {
      optimizedCode = fs.readFileSync(outputFilePath, 'utf8');
    }

    // ── Step 5: Parse statistics ─────────────────────────────────────────
    let scanned = 0, eliminated = 0, reduction = '0%', functions = 0;

    const scannedMatch = report.match(/Instructions scanned:\s*(\d+)/i);
    const eliminatedMatch = report.match(/Dead eliminated\s*:\s*(\d+)/i);
    const reductionMatch = report.match(/Reduction\s*:\s*([\d.]+%)/i);
    const functionsMatch = report.match(/Functions processed\s*:\s*(\d+)/i);

    if (scannedMatch) scanned = parseInt(scannedMatch[1], 10);
    if (eliminatedMatch) eliminated = parseInt(eliminatedMatch[1], 10);
    if (reductionMatch) reduction = reductionMatch[1];
    if (functionsMatch) functions = parseInt(functionsMatch[1], 10);

    // ── Step 6: Cleanup temp files ───────────────────────────────────────
    const cleanup = [inputCFile, inputLLFile, outputLLFile];
    cleanup.forEach(f => {
      const fp = path.join(testsDir, f);
      if (fs.existsSync(fp)) { try { fs.unlinkSync(fp); } catch(e) {} }
    });

    // ── Step 7: Return results ───────────────────────────────────────────
    if (dieResult.error && !optimizedCode) {
      return res.status(500).json({
        error: 'Optimization pass failed',
        details: compilationLog
      });
    }

    res.json({
      optimizedCode,
      report: compilationLog,
      llvmIRInput,  // send back the generated IR so user can see it
      isC,
      stats: { scanned, eliminated, reduction, functions }
    });

  } catch (err) {
    return res.status(500).json({
      error: 'Internal server error',
      details: err.message
    });
  }
});

// Start Server
app.listen(PORT, () => {
  console.log(`=======================================================`);
  console.log(`🚀 Dead Code Eliminator Web Server running on:`);
  console.log(`   👉 http://localhost:${PORT}`);
  console.log(`   Supports: C source code & LLVM IR input`);
  console.log(`=======================================================`);
});
