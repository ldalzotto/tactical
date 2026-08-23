#!/usr/bin/env node

const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');

const DIST_DIR = 'dist';
const BUILD_DIR = 'build';

function log(msg) {
  console.log(`[build-pages] ${msg}`);
}

function ensureDir(dir) {
  if (!fs.existsSync(dir)) {
    fs.mkdirSync(dir, { recursive: true });
  }
}

function copyFile(src, dest) {
  ensureDir(path.dirname(dest));
  fs.copyFileSync(src, dest);
  log(`Copied ${src} → ${dest}`);
}

try {
  log('Starting GitHub Pages build...');

  // Step 1: Run CMake release build
  log('Running CMake release build...');
  execSync('npm run build:release', { stdio: 'inherit' });

  // Step 2: Clean and create dist directory
  log('Preparing output directory...');
  if (fs.existsSync(DIST_DIR)) {
    fs.rmSync(DIST_DIR, { recursive: true });
  }
  ensureDir(DIST_DIR);

  // Step 3: Copy and process web artifacts
  log('Copying web artifacts...');
  copyFile('web/index.html', path.join(DIST_DIR, 'index.html'));

  // Copy main.js and fix paths for GitHub Pages (absolute → relative)
  const mainJsSrc = fs.readFileSync('web/main.js', 'utf8');
  const mainJsFixed = mainJsSrc.replace(/fetch\('\/build\/app\.wasm'\)/g, "fetch('./build/app.wasm')");
  fs.writeFileSync(path.join(DIST_DIR, 'main.js'), mainJsFixed);
  log(`Copied web/main.js → ${path.join(DIST_DIR, 'main.js')} (paths adjusted for GitHub Pages)`);

  // Note: dev-reload.js is intentionally NOT copied (dev-only)

  // Step 4: Copy build artifacts
  log('Copying build artifacts...');
  copyFile(
    path.join(BUILD_DIR, 'app.wasm'),
    path.join(DIST_DIR, BUILD_DIR, 'app.wasm')
  );

  log('GitHub Pages build completed successfully!');
  log(`Output directory: ${path.resolve(DIST_DIR)}`);
  process.exit(0);
} catch (error) {
  log(`Error: ${error.message}`);
  process.exit(1);
}
