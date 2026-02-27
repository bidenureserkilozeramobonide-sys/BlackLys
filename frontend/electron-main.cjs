const { app, BrowserWindow, Menu, shell } = require('electron');
const { spawn } = require('child_process');
const path = require('path');
const http = require('http');

// ── Config ──────────────────────────────────────────
const BACKEND_PORT = 8001;
const VITE_DEV_PORT = 5174;

// Detect dev mode: check if dist/index.html exists
const fs = require('fs');
const distIndex = path.join(__dirname, 'dist', 'index.html');
const hasDistBuild = fs.existsSync(distIndex);
// Dev mode = when Vite dev server is running (no dist build used)
// For now, always load from dist since we build before launching
const isDev = process.argv.includes('--dev');

let mainWindow = null;
let backendProcess = null;
let backendExternallyManaged = false;

// ── Backend Management ──────────────────────────────
function checkBackendAlreadyRunning() {
    return new Promise((resolve) => {
        const req = http.get(`http://127.0.0.1:${BACKEND_PORT}/health`, (res) => {
            res.resume(); // Consume response
            resolve(res.statusCode === 200);
        });
        req.on('error', () => resolve(false));
        req.setTimeout(1000, () => { req.destroy(); resolve(false); });
    });
}

function startBackend() {
    const backendDir = path.join(__dirname, '..', 'backend');
    const venvPython = path.join(backendDir, '.venv', 'Scripts', 'python.exe');

    console.log('[Electron] Starting backend from:', backendDir);

    backendProcess = spawn(
        venvPython,
        ['-m', 'uvicorn', 'app.main:app', '--host', '127.0.0.1', '--port', String(BACKEND_PORT)],
        {
            cwd: backendDir,
            stdio: ['ignore', 'pipe', 'pipe'],
            windowsHide: true,
        }
    );

    backendProcess.stdout.on('data', (data) => {
        console.log(`[Backend] ${data.toString().trim()}`);
    });

    backendProcess.stderr.on('data', (data) => {
        console.log(`[Backend] ${data.toString().trim()}`);
    });

    backendProcess.on('error', (err) => {
        console.error('[Backend] Failed to start:', err.message);
    });

    backendProcess.on('exit', (code) => {
        console.log(`[Backend] Exited with code ${code}`);
        backendProcess = null;
    });
}

function stopBackend() {
    if (backendProcess && !backendExternallyManaged) {
        console.log('[Electron] Stopping backend...');
        backendProcess.kill('SIGTERM');
        // Force kill after 3s if still running
        setTimeout(() => {
            if (backendProcess) {
                backendProcess.kill('SIGKILL');
                backendProcess = null;
            }
        }, 3000);
    }
}

function waitForBackend(maxRetries = 30) {
    return new Promise((resolve) => {
        let retries = 0;
        let resolved = false;
        const done = () => {
            if (!resolved) {
                resolved = true;
                resolve();
            }
        };
        const check = () => {
            const req = http.get(`http://127.0.0.1:${BACKEND_PORT}/health`, (res) => {
                res.resume(); // Consume response data
                if (res.statusCode === 200) {
                    console.log('[Electron] Backend is ready!');
                    done();
                } else {
                    retry();
                }
            });
            req.on('error', () => retry());
            req.setTimeout(1000, () => { req.destroy(); retry(); });
        };
        const retry = () => {
            if (resolved) return;
            retries++;
            if (retries >= maxRetries) {
                console.warn('[Electron] Backend not responding after', maxRetries, 'attempts. Loading anyway...');
                done();
            } else {
                setTimeout(check, 500);
            }
        };
        check();
    });
}

// ── Window Creation ─────────────────────────────────
function createWindow() {
    mainWindow = new BrowserWindow({
        width: 1400,
        height: 900,
        minWidth: 1024,
        minHeight: 680,
        title: 'BlackLys',
        backgroundColor: '#0a0a0f',
        icon: path.join(__dirname, 'public', 'favicon.ico'),
        webPreferences: {
            preload: path.join(__dirname, 'electron-preload.cjs'),
            contextIsolation: true,
            nodeIntegration: false,
        },
        // Frameless with custom titlebar feel — but keep system controls
        titleBarStyle: 'hidden',
        titleBarOverlay: {
            color: '#0a0a0f',
            symbolColor: '#a1a1aa',
            height: 36,
        },
        show: false, // Show after ready-to-show
    });

    // Show when ready — prevents white flash
    mainWindow.once('ready-to-show', () => {
        mainWindow.show();
    });

    // Load frontend
    if (isDev) {
        console.log('[Electron] Dev mode — loading Vite dev server');
        mainWindow.loadURL(`http://localhost:${VITE_DEV_PORT}`);
        mainWindow.webContents.openDevTools();
    } else {
        console.log('[Electron] Production mode — loading dist/index.html');
        mainWindow.loadFile(path.join(__dirname, 'dist', 'index.html'));
    }

    // Open external links in default browser
    mainWindow.webContents.setWindowOpenHandler(({ url }) => {
        shell.openExternal(url);
        return { action: 'deny' };
    });

    mainWindow.on('closed', () => {
        mainWindow = null;
    });
}

// ── Native Menu ─────────────────────────────────────
function buildMenu() {
    const template = [
        {
            label: 'Fichier',
            submenu: [
                { label: 'Nouvelle Mission', accelerator: 'CmdOrCtrl+N', click: () => mainWindow?.webContents.send('menu:new-mission') },
                { type: 'separator' },
                { label: 'Quitter', accelerator: 'CmdOrCtrl+Q', click: () => app.quit() },
            ],
        },
        {
            label: 'Édition',
            submenu: [
                { role: 'undo', label: 'Annuler' },
                { role: 'redo', label: 'Rétablir' },
                { type: 'separator' },
                { role: 'cut', label: 'Couper' },
                { role: 'copy', label: 'Copier' },
                { role: 'paste', label: 'Coller' },
            ],
        },
        {
            label: 'Affichage',
            submenu: [
                { role: 'reload', label: 'Recharger' },
                { role: 'forceReload', label: 'Forcer le rechargement' },
                { role: 'toggleDevTools', label: 'Outils de développement' },
                { type: 'separator' },
                { role: 'resetZoom', label: 'Zoom 100%' },
                { role: 'zoomIn', label: 'Zoom +' },
                { role: 'zoomOut', label: 'Zoom -' },
                { type: 'separator' },
                { role: 'togglefullscreen', label: 'Plein écran' },
            ],
        },
    ];

    Menu.setApplicationMenu(Menu.buildFromTemplate(template));
}

// ── App Lifecycle ───────────────────────────────────
app.whenReady().then(async () => {
    buildMenu();

    // Check if backend is already running (e.g., user started it manually)
    const alreadyRunning = await checkBackendAlreadyRunning();
    if (alreadyRunning) {
        console.log('[Electron] Backend already running on port', BACKEND_PORT);
        backendExternallyManaged = true;
    } else {
        console.log('[Electron] Starting backend...');
        startBackend();
        await waitForBackend();
    }

    createWindow();

    app.on('activate', () => {
        if (BrowserWindow.getAllWindows().length === 0) createWindow();
    });
});

app.on('window-all-closed', () => {
    stopBackend();
    app.quit();
});

app.on('before-quit', () => {
    stopBackend();
});
