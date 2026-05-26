// ─── Monaco Editor Setup with Custom LLVM Syntax Highlighter ────────────────
let inputEditor;
let outputEditor;
let generatedEditor;
let presets = {};
let currentLang = 'c'; // 'c' or 'llvm'

require.config({ paths: { vs: 'https://cdnjs.cloudflare.com/ajax/libs/monaco-editor/0.39.0/min/vs' } });

require(['vs/editor/editor.main'], function () {
  // 1. Register a custom LLVM IR language for Monaco Editor
  monaco.languages.register({ id: 'llvm' });
  monaco.languages.setMonarchTokensProvider('llvm', {
    tokenizer: {
      root: [
        // Global variables/functions
        [/@[a-zA-Z0-9_.]+/, 'tag'],
        // Local registers/variables
        [/%[a-zA-Z0-9_.]+/, 'variable'],
        // Keywords
        [/\b(define|declare|private|unnamed_addr|constant|global|volatile|inbounds|to)\b/, 'keyword'],
        // Types
        [/\b(i32|i64|i1|i8|ptr|void|double|float)\b/, 'type'],
        // Instructions
        [/\b(add|sub|mul|sdiv|udiv|alloca|store|load|ret|br|phi|icmp|fcmp|call|invoke|getelementptr|bitcast|zext|sext|trunc|select)\b/, 'keyword.flow'],
        // Comparisons
        [/\b(eq|ne|ugt|uge|ult|ule|sgt|sge|slt|sle)\b/, 'operator'],
        // Labels
        [/^[a-zA-Z0-9_]+:/, 'metatag'],
        // Comments
        [/;.*$/, 'comment'],
        // Numbers
        [/\b\d+\b/, 'number'],
      ]
    }
  });

  // Define custom editor theme
  monaco.editor.defineTheme('customTheme', {
    base: 'vs-dark',
    inherit: true,
    rules: [
      { token: 'keyword', foreground: 'c084fc', fontStyle: 'bold' },
      { token: 'keyword.flow', foreground: '22d3ee', fontStyle: 'bold' },
      { token: 'variable', foreground: '818cf8' },
      { token: 'tag', foreground: 'fbbf24' },
      { token: 'type', foreground: '4ade80' },
      { token: 'comment', foreground: '64748b', fontStyle: 'italic' },
      { token: 'number', foreground: '38bdf8' }
    ],
    colors: {
      'editor.background': '#0c0d15',
      'editor.lineHighlightBackground': '#181a28',
      'editorLineNumber.foreground': '#475569',
      'editorLineNumber.activeForeground': '#cbd5e1'
    }
  });

  // 2. Initialize Editors
  inputEditor = monaco.editor.create(document.getElementById('input-editor'), {
    value: `// Write or paste C code here...\n`,
    language: 'c',
    theme: 'customTheme',
    automaticLayout: true,
    minimap: { enabled: false },
    fontSize: 14,
    fontFamily: "'Fira Code', monospace"
  });

  outputEditor = monaco.editor.create(document.getElementById('output-editor'), {
    value: `; Optimized LLVM IR output will appear here...\n`,
    language: 'llvm',
    theme: 'customTheme',
    readOnly: true,
    automaticLayout: true,
    minimap: { enabled: false },
    fontSize: 14,
    fontFamily: "'Fira Code', monospace"
  });

  generatedEditor = monaco.editor.create(document.getElementById('generated-editor'), {
    value: `; Clang-generated LLVM IR will appear here...\n`,
    language: 'llvm',
    theme: 'customTheme',
    readOnly: true,
    automaticLayout: true,
    minimap: { enabled: false },
    fontSize: 14,
    fontFamily: "'Fira Code', monospace"
  });

  // Load Initial Presets
  fetchPresets();
});

// ─── Fetch and Set Presets ───────────────────────────────────────────────────
function fetchPresets() {
  fetch('/api/presets')
    .then(res => res.json())
    .then(data => {
      presets = data;
      // Default to loading test1
      if (presets.test1) {
        inputEditor.setValue(presets.test1);
        document.getElementById('preset-select').value = 'test1';
      }
    })
    .catch(err => console.error('Error fetching presets:', err));
}

// Preset selection change listener
document.getElementById('preset-select').addEventListener('change', (e) => {
  const selected = e.target.value;
  if (selected === 'c-example' && inputEditor) {
    switchLang('c');
    inputEditor.setValue(`#include <stdio.h>\n\nint compute(int a, int b) {\n    // Useful instruction\n    int x = a + b;\n\n    // Dead instructions\n    int d1 = a * 100;\n    int d2 = d1 + 50;\n    int d3 = d2 / 2;\n\n    // Useful instruction\n    int y = x * 2;\n\n    // Dead chain\n    int temp1 = a - b;\n    int temp2 = temp1 * 5;\n    int temp3 = temp2 + 10;\n\n    // Useful branch\n    if (y > 50) {\n        // Dead inside branch\n        int branchDead1 = y * 100;\n        int branchDead2 = branchDead1 + 1;\n        printf("Large value\\n");\n    }\n\n    // Useful instruction\n    int z = y + 10;\n\n    // Dead assignment overwrite\n    int overwrite = 5;\n    overwrite = 10;\n\n    // Useful output\n    return z;\n}\n\nint main() {\n    int result = compute(20, 15);\n    printf("Result = %d\\n", result);\n    return 0;\n}`);
  } else if (presets[selected] && inputEditor) {
    switchLang('llvm');
    inputEditor.setValue(presets[selected]);
  }
});

// ─── Language Toggle Logic ───────────────────────────────────────────────────
const btnLangC = document.getElementById('btn-lang-c');
const btnLangLL = document.getElementById('btn-lang-ll');
const inputPanelTitle = document.getElementById('input-panel-title');

function switchLang(lang) {
  currentLang = lang;
  if (lang === 'c') {
    btnLangC.classList.add('active');
    btnLangLL.classList.remove('active');
    inputPanelTitle.innerText = 'Input C Code';
    monaco.editor.setModelLanguage(inputEditor.getModel(), 'c');
  } else {
    btnLangLL.classList.add('active');
    btnLangC.classList.remove('active');
    inputPanelTitle.innerText = 'Input LLVM IR';
    monaco.editor.setModelLanguage(inputEditor.getModel(), 'llvm');
  }
}

btnLangC.addEventListener('click', () => {
  switchLang('c');
  document.getElementById('preset-select').value = 'c-example';
  document.getElementById('preset-select').dispatchEvent(new Event('change'));
});

btnLangLL.addEventListener('click', () => {
  switchLang('llvm');
  document.getElementById('preset-select').value = 'test1';
  document.getElementById('preset-select').dispatchEvent(new Event('change'));
});

// ─── File Upload Handler ─────────────────────────────────────────────────────
const btnUpload = document.getElementById('btn-upload');
const fileInput = document.getElementById('file-input');

btnUpload.addEventListener('click', () => fileInput.click());

fileInput.addEventListener('change', (e) => {
  const file = e.target.files[0];
  if (!file) return;

  const reader = new FileReader();
  reader.onload = (evt) => {
    if (inputEditor) {
      inputEditor.setValue(evt.target.result);
      // Reset dropdown select
      document.getElementById('preset-select').value = '';
    }
  };
  reader.readAsText(file);
});

// ─── Tab Navigation Logic ────────────────────────────────────────────────────
const tabOptimized = document.getElementById('tab-optimized');
const tabGenerated = document.getElementById('tab-generated');
const tabReport = document.getElementById('tab-report');
const contentOptimized = document.getElementById('content-optimized');
const contentGenerated = document.getElementById('content-generated');
const contentReport = document.getElementById('content-report');

function activateTab(tabBtn, contentDiv) {
  [tabOptimized, tabGenerated, tabReport].forEach(t => t.classList.remove('active'));
  [contentOptimized, contentGenerated, contentReport].forEach(c => c.classList.remove('active'));
  tabBtn.classList.add('active');
  contentDiv.classList.add('active');
}

tabOptimized.addEventListener('click', () => activateTab(tabOptimized, contentOptimized));
tabGenerated.addEventListener('click', () => activateTab(tabGenerated, contentGenerated));
tabReport.addEventListener('click', () => activateTab(tabReport, contentReport));

// ─── Run Optimization ────────────────────────────────────────────────────────
const btnOptimize = document.getElementById('btn-optimize');
const consoleOutput = document.getElementById('console-output');

btnOptimize.addEventListener('click', () => {
  if (!inputEditor) return;

  const inputCode = inputEditor.getValue();
  
  // Set Loading UI state
  btnOptimize.disabled = true;
  btnOptimize.innerHTML = `
    <svg class="icon animate-spin" fill="none" viewBox="0 0 24 24" stroke="currentColor" style="animation: spin 1s linear infinite;">
      <circle cx="12" cy="12" r="10" stroke="currentColor" stroke-width="4" style="opacity: 0.25;" fill="none"></circle>
      <path stroke="currentColor" stroke-linecap="round" stroke-linejoin="round" stroke-width="4" d="M4 12a8 8 0 018-8V0" style="opacity: 0.75;"></path>
    </svg>
    Optimizing Code...
  `;

  // Clear output and console
  outputEditor.setValue('; Working...');
  consoleOutput.innerHTML = 'Executing Dead Code Elimination Pass...';

  // API Call
  fetch('/api/optimize', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ code: inputCode })
  })
  .then(res => {
    if (!res.ok) {
      return res.json().then(errData => { throw new Error(errData.details || errData.error) });
    }
    return res.json();
  })
  .then(data => {
    // 1. Set optimized code
    outputEditor.setValue(data.optimizedCode || '; IR unchanged\n' + inputCode);
    
    // 1.5 Set generated IR code
    if (data.llvmIRInput) {
      generatedEditor.setValue(data.llvmIRInput);
    }

    // Show/hide the 'Generated IR' tab based on whether input was C
    if (data.isC) {
      tabGenerated.style.display = 'inline-block';
    } else {
      tabGenerated.style.display = 'none';
    }

    // 2. Print styled console report
    displayStyledReport(data.report);

    // 3. Update Metrics Dashboard with Count Animation
    animateMetricValue('metric-scanned', data.stats.scanned);
    animateMetricValue('metric-eliminated', data.stats.eliminated);
    document.getElementById('metric-reduction').innerText = data.stats.reduction;
    animateMetricValue('metric-functions', data.stats.functions);

    // 4. Default to optimized IR tab on success
    tabOptimized.click();
  })
  .catch(err => {
    consoleOutput.innerHTML = `<span style="color: #f87171; font-weight: bold;">[ERROR] Optimization Run Failed:</span>\n\n${err.message}`;
    outputEditor.setValue('; Error occurred during optimization. See analysis logs.');
    tabReport.click();
  })
  .finally(() => {
    // Reset button state
    btnOptimize.disabled = false;
    btnOptimize.innerHTML = `
      <svg class="icon animate-spin-hover" fill="none" viewBox="0 0 24 24" stroke="currentColor">
        <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M13 10V3L4 14h7v7l9-11h-7z" />
      </svg>
      Eliminate Dead Code
    `;
  });
});

// ─── Helper: HTML-Escape Console report and Colorize live/dead ──────────────
function displayStyledReport(rawReport) {
  if (!rawReport) {
    consoleOutput.innerText = 'No compilation logs returned.';
    return;
  }

  // HTML Escape
  let escaped = rawReport
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;');

  // Colorize [✗ DEAD] and [✓ LIVE]
  escaped = escaped.replace(/✗ DEAD/g, '<span class="color-dead">✗ DEAD</span>');
  escaped = escaped.replace(/✓ LIVE/g, '<span class="color-live">✓ LIVE</span>');
  
  // Highlight removed lines
  escaped = escaped.replace(/\[-\]/g, '<span class="color-dead">[-]</span>');
  
  // Colorize block titles and pass stage headers
  escaped = escaped.replace(/\[DIE Pass\]/g, '<span class="color-header">[DIE Pass]</span>');
  escaped = escaped.replace(/USE-DEF CHAIN ANALYSIS REPORT/g, '<span class="color-header" style="font-size: 1.05rem;">USE-DEF CHAIN ANALYSIS REPORT</span>');

  consoleOutput.innerHTML = escaped;
}

// ─── Helper: Animated Count Value change ─────────────────────────────────────
function animateMetricValue(id, targetValue) {
  const obj = document.getElementById(id);
  const start = parseInt(obj.innerText, 10) || 0;
  if (start === targetValue) return;

  const duration = 800; // ms
  const startTime = performance.now();

  function update(currentTime) {
    const elapsed = currentTime - startTime;
    const progress = Math.min(elapsed / duration, 1);
    
    // Ease out quad formula
    const easeProgress = progress * (2 - progress);
    const currentValue = Math.floor(start + (targetValue - start) * easeProgress);
    
    obj.innerText = currentValue;

    if (progress < 1) {
      requestAnimationFrame(update);
    } else {
      obj.innerText = targetValue;
    }
  }

  requestAnimationFrame(update);
}
