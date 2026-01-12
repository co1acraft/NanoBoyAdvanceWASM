/**
 * NanoBoyAdvance WASM Frontend
 * Main JavaScript module for the emulator interface
 */

// Screen dimensions
const SCREEN_WIDTH = 240;
const SCREEN_HEIGHT = 160;

// Key mappings: keyboard code -> GBA key index
const KEY_MAP = {
  'KeyX': 0,        // A
  'KeyZ': 1,        // B
  'ShiftRight': 2,  // Select
  'ShiftLeft': 2,   // Select (alternative)
  'Enter': 3,       // Start
  'ArrowRight': 4,  // Right
  'ArrowLeft': 5,   // Left
  'ArrowUp': 6,     // Up
  'ArrowDown': 7,   // Down
  'KeyS': 8,        // R
  'KeyA': 9,        // L
};

class NBAEmulator {
  constructor() {
    this.emulator = null;
    this.module = null;
    this.canvas = document.getElementById('screen');
    this.ctx = this.canvas.getContext('2d');
    this.imageData = this.ctx.createImageData(SCREEN_WIDTH, SCREEN_HEIGHT);
    this.running = false;
    this.animationId = null;
    this.audioContext = null;
    this.audioWorklet = null;
    this.biosLoaded = false;
    this.romLoaded = false;
    this.romName = '';
    
    this.init();
  }

  async init() {
    // Initialize UI
    this.setupEventListeners();
    
    // Load WASM module
    try {
      this.updateLoadingText('Loading WASM module...');
      this.module = await createNBA();
      this.emulator = new this.module.Emulator();
      
      this.hideLoading();
      console.log('NanoBoyAdvance WASM loaded successfully');
      
      // Check for saved BIOS in IndexedDB
      await this.loadSavedBIOS();
    } catch (error) {
      this.updateLoadingText(`Error: ${error.message}`);
      console.error('Failed to load WASM module:', error);
    }
  }

  setupEventListeners() {
    // File inputs
    document.getElementById('bios-input').addEventListener('change', (e) => this.handleBIOSLoad(e));
    document.getElementById('rom-input').addEventListener('change', (e) => this.handleROMLoad(e));
    
    // Buttons
    document.getElementById('start-btn').addEventListener('click', () => this.start());
    document.getElementById('pause-btn').addEventListener('click', () => this.togglePause());
    document.getElementById('reset-btn').addEventListener('click', () => this.reset());
    document.getElementById('fullscreen-btn').addEventListener('click', () => this.toggleFullscreen());
    
    // Settings
    document.getElementById('skip-bios').addEventListener('change', (e) => {
      if (this.emulator) {
        this.emulator.setSkipBIOS(e.target.checked);
      }
    });
    
    document.getElementById('volume').addEventListener('input', (e) => {
      if (this.emulator) {
        this.emulator.setVolume(parseInt(e.target.value));
      }
    });
    
    document.getElementById('scale').addEventListener('change', (e) => {
      const scale = parseInt(e.target.value);
      this.canvas.style.width = `${SCREEN_WIDTH * scale}px`;
      this.canvas.style.height = `${SCREEN_HEIGHT * scale}px`;
    });
    
    // Save controls
    document.getElementById('save-btn').addEventListener('click', () => this.saveToStorage());
    document.getElementById('load-save-btn').addEventListener('click', () => this.loadFromStorage());
    document.getElementById('export-save-btn').addEventListener('click', () => this.exportSave());
    document.getElementById('import-save-btn').addEventListener('click', () => {
      document.getElementById('import-save-input').click();
    });
    document.getElementById('import-save-input').addEventListener('change', (e) => this.importSave(e));
    
    // Keyboard input
    document.addEventListener('keydown', (e) => this.handleKeyDown(e));
    document.addEventListener('keyup', (e) => this.handleKeyUp(e));
    
    // Prevent arrow key scrolling
    window.addEventListener('keydown', (e) => {
      if (['ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight'].includes(e.code)) {
        e.preventDefault();
      }
    });
  }

  async handleBIOSLoad(event) {
    const file = event.target.files[0];
    if (!file) return;

    try {
      const arrayBuffer = await file.arrayBuffer();
      const data = new Uint8Array(arrayBuffer);
      
      if (this.emulator.loadBIOS(data)) {
        this.biosLoaded = true;
        document.getElementById('bios-status').textContent = '●';
        document.getElementById('bios-status').classList.add('loaded');
        
        // Save BIOS to IndexedDB for future use
        await this.saveBIOSToStorage(data);
        
        this.updateButtons();
        console.log('BIOS loaded successfully');
      } else {
        alert('Invalid BIOS file. Please use a valid GBA BIOS (16KB).');
      }
    } catch (error) {
      alert(`Failed to load BIOS: ${error.message}`);
    }
  }

  async handleROMLoad(event) {
    const file = event.target.files[0];
    if (!file) return;

    try {
      const arrayBuffer = await file.arrayBuffer();
      let data = new Uint8Array(arrayBuffer);
      
      // Handle ZIP files
      if (file.name.toLowerCase().endsWith('.zip')) {
        // For now, we don't support ZIP extraction in WASM
        alert('ZIP files are not supported in the web version. Please extract the ROM first.');
        return;
      }
      
      if (this.emulator.loadROM(data, 0)) { // 0 = auto-detect backup type
        this.romLoaded = true;
        this.romName = file.name.replace(/\.[^/.]+$/, '');
        document.getElementById('rom-status').textContent = '●';
        document.getElementById('rom-status').classList.add('loaded');
        
        // Try to load save from storage
        await this.loadFromStorage();
        
        this.updateButtons();
        console.log('ROM loaded successfully');
      } else {
        alert('Invalid ROM file.');
      }
    } catch (error) {
      alert(`Failed to load ROM: ${error.message}`);
    }
  }

  updateButtons() {
    const ready = this.biosLoaded && this.romLoaded;
    document.getElementById('start-btn').disabled = !ready;
    document.getElementById('pause-btn').disabled = !this.running;
    document.getElementById('reset-btn').disabled = !ready;
    document.getElementById('save-btn').disabled = !this.romLoaded;
    document.getElementById('load-save-btn').disabled = !this.romLoaded;
    document.getElementById('export-save-btn').disabled = !this.romLoaded;
    document.getElementById('import-save-btn').disabled = !this.romLoaded;
  }

  async initAudio() {
    if (this.audioContext) return;

    try {
      this.audioContext = new AudioContext({
        sampleRate: this.emulator.getAudioSampleRate()
      });

      // Use ScriptProcessorNode for compatibility (AudioWorklet would be better for production)
      const bufferSize = 2048;
      this.audioProcessor = this.audioContext.createScriptProcessor(bufferSize, 0, 2);
      
      const audioBuffer = this.module._malloc(bufferSize * 2 * 2); // stereo, 16-bit
      
      this.audioProcessor.onaudioprocess = (event) => {
        if (!this.running) return;
        
        const left = event.outputBuffer.getChannelData(0);
        const right = event.outputBuffer.getChannelData(1);
        const samples = left.length;
        
        // Fill buffer from emulator
        this.emulator.fillAudioBuffer(audioBuffer, samples);
        
        // Convert from s16 to float using fresh HEAP16 view (needed with ALLOW_MEMORY_GROWTH)
        const heap16 = new Int16Array(this.module.HEAPU8.buffer);
        const audioOffset = audioBuffer >> 1; // byte offset to i16 index
        for (let i = 0; i < samples; i++) {
          left[i] = heap16[audioOffset + i * 2] / 32768;
          right[i] = heap16[audioOffset + i * 2 + 1] / 32768;
        }
      };
      
      this.audioProcessor.connect(this.audioContext.destination);
      console.log('Audio initialized at', this.audioContext.sampleRate, 'Hz');
    } catch (error) {
      console.error('Failed to initialize audio:', error);
    }
  }

  async start() {
    if (!this.emulator.isReady()) {
      alert('Please load both BIOS and ROM files first.');
      return;
    }

    // Apply settings
    this.emulator.setSkipBIOS(document.getElementById('skip-bios').checked);
    this.emulator.setVolume(parseInt(document.getElementById('volume').value));
    
    // Reset and start
    this.emulator.reset();
    
    // Initialize audio
    await this.initAudio();
    if (this.audioContext && this.audioContext.state === 'suspended') {
      await this.audioContext.resume();
    }
    
    this.emulator.setAudioPause(false);
    this.running = true;
    this.updateButtons();
    
    // Start emulation loop
    this.runFrame();
  }

  runFrame() {
    if (!this.running) return;

    // Run one frame
    this.emulator.runFrame();

    // Render if new frame available
    if (this.emulator.hasNewFrame()) {
      this.renderFrame();
    }

    // Schedule next frame
    this.animationId = requestAnimationFrame(() => this.runFrame());
  }

  renderFrame() {
    const bufferPtr = this.emulator.getFrameBufferPtr();
    // Access WASM heap - with ALLOW_MEMORY_GROWTH, need to get fresh view each time
    const heapU8 = new Uint8Array(this.module.HEAPU8.buffer);
    const heap = heapU8.subarray(bufferPtr, bufferPtr + SCREEN_WIDTH * SCREEN_HEIGHT * 4);
    
    // Copy to ImageData
    this.imageData.data.set(heap);
    
    // Draw to canvas
    this.ctx.putImageData(this.imageData, 0, 0);
  }

  togglePause() {
    if (this.running) {
      this.running = false;
      this.emulator.setAudioPause(true);
      if (this.animationId) {
        cancelAnimationFrame(this.animationId);
        this.animationId = null;
      }
      document.getElementById('pause-btn').textContent = 'Resume';
    } else if (this.emulator.isReady()) {
      this.running = true;
      this.emulator.setAudioPause(false);
      this.runFrame();
      document.getElementById('pause-btn').textContent = 'Pause';
    }
    this.updateButtons();
  }

  reset() {
    this.emulator.reset();
    if (!this.running && this.emulator.isReady()) {
      this.start();
    }
  }

  toggleFullscreen() {
    const container = document.querySelector('.emulator-container');
    if (document.fullscreenElement) {
      document.exitFullscreen();
    } else {
      container.requestFullscreen();
    }
  }

  handleKeyDown(event) {
    const key = KEY_MAP[event.code];
    if (key !== undefined && this.emulator) {
      event.preventDefault();
      this.emulator.setKeyState(key, true);
    }
  }

  handleKeyUp(event) {
    const key = KEY_MAP[event.code];
    if (key !== undefined && this.emulator) {
      event.preventDefault();
      this.emulator.setKeyState(key, false);
    }
  }

  // Save/Load functionality using IndexedDB
  async openDB() {
    return new Promise((resolve, reject) => {
      const request = indexedDB.open('NanoBoyAdvance', 1);
      request.onerror = () => reject(request.error);
      request.onsuccess = () => resolve(request.result);
      request.onupgradeneeded = (event) => {
        const db = event.target.result;
        if (!db.objectStoreNames.contains('saves')) {
          db.createObjectStore('saves');
        }
        if (!db.objectStoreNames.contains('bios')) {
          db.createObjectStore('bios');
        }
      };
    });
  }

  async saveBIOSToStorage(data) {
    try {
      const db = await this.openDB();
      const tx = db.transaction('bios', 'readwrite');
      const store = tx.objectStore('bios');
      store.put(data, 'gba_bios');
      await new Promise((resolve, reject) => {
        tx.oncomplete = resolve;
        tx.onerror = () => reject(tx.error);
      });
      console.log('BIOS saved to storage');
    } catch (error) {
      console.error('Failed to save BIOS:', error);
    }
  }

  async loadSavedBIOS() {
    try {
      const db = await this.openDB();
      const tx = db.transaction('bios', 'readonly');
      const store = tx.objectStore('bios');
      const request = store.get('gba_bios');
      
      await new Promise((resolve, reject) => {
        request.onsuccess = resolve;
        request.onerror = () => reject(request.error);
      });
      
      if (request.result) {
        const data = request.result;
        if (this.emulator.loadBIOS(data)) {
          this.biosLoaded = true;
          document.getElementById('bios-status').textContent = '●';
          document.getElementById('bios-status').classList.add('loaded');
          this.updateButtons();
          console.log('BIOS loaded from storage');
        }
      }
    } catch (error) {
      console.log('No saved BIOS found');
    }
  }

  async saveToStorage() {
    if (!this.romName) return;
    
    try {
      const saveData = this.emulator.getSaveData();
      if (!saveData) {
        alert('No save data available for this game.');
        return;
      }
      
      const db = await this.openDB();
      const tx = db.transaction('saves', 'readwrite');
      const store = tx.objectStore('saves');
      store.put(saveData, this.romName);
      
      await new Promise((resolve, reject) => {
        tx.oncomplete = resolve;
        tx.onerror = () => reject(tx.error);
      });
      
      alert('Game saved successfully!');
    } catch (error) {
      alert(`Failed to save: ${error.message}`);
    }
  }

  async loadFromStorage() {
    if (!this.romName) return;
    
    try {
      const db = await this.openDB();
      const tx = db.transaction('saves', 'readonly');
      const store = tx.objectStore('saves');
      const request = store.get(this.romName);
      
      await new Promise((resolve, reject) => {
        request.onsuccess = resolve;
        request.onerror = () => reject(request.error);
      });
      
      if (request.result) {
        if (this.emulator.loadSaveData(request.result)) {
          console.log('Save data loaded from storage');
        }
      }
    } catch (error) {
      console.log('No saved data found for this game');
    }
  }

  exportSave() {
    const saveData = this.emulator.getSaveData();
    if (!saveData) {
      alert('No save data available for this game.');
      return;
    }
    
    const blob = new Blob([saveData], { type: 'application/octet-stream' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `${this.romName}.sav`;
    a.click();
    URL.revokeObjectURL(url);
  }

  async importSave(event) {
    const file = event.target.files[0];
    if (!file) return;
    
    try {
      const arrayBuffer = await file.arrayBuffer();
      const data = new Uint8Array(arrayBuffer);
      
      if (this.emulator.loadSaveData(data)) {
        alert('Save data imported successfully!');
      } else {
        alert('Failed to import save data. Make sure the file matches the current ROM.');
      }
    } catch (error) {
      alert(`Failed to import save: ${error.message}`);
    }
    
    event.target.value = '';
  }

  updateLoadingText(text) {
    document.getElementById('loading-text').textContent = text;
  }

  hideLoading() {
    document.getElementById('loading-overlay').classList.add('hidden');
  }
}

// Initialize when DOM is ready
document.addEventListener('DOMContentLoaded', () => {
  window.nbaEmulator = new NBAEmulator();
});
