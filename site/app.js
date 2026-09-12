const findElement = (elementIdentifier) => document.getElementById(elementIdentifier);
const canvas = findElement('game'),
  renderingContext = canvas.getContext('2d', {
    alpha: false,
  }),
  framebufferImage = renderingContext.createImageData(120, 120);
const storageKey = 'gravelbyte.records.v4';
let gameEngine,
  playing = false,
  muted = false,
  activeInput = matchMedia('(pointer:coarse)').matches ? 'touch' : 'keyboard';
let keyboard = 0,
  touchBits = 0,
  pulses = 0,
  lastTime = 0,
  lastMode = -1,
  lastMenuState = '',
  lastSeedUrl = '',
  previousGamepadInputs = 0,
  saveFailed = false;
let engineAudioContext, oscillator, gain;
const menuCommands = [];
let menuRelease = false;
const keyBits = {
  ArrowLeft: 1,
  ArrowRight: 2,
  ArrowUp: 4 | 512,
  KeyZ: 4 | 32,
  ArrowDown: 8 | 1024,
  KeyX: 8,
  Space: 16,
  Enter: 32,
  KeyP: 64,
  Escape: 128,
};
const pressedKeys = new Set(),
  pointers = new Map();
function useInput(input) {
  activeInput = input;
  findElement('input-label').textContent = input;
  findElement('touch').hidden = input !== 'touch' || !playing;
  const hints = {
    keyboard:
      '← → steer · ↑ / Z gas · ↓ / X brake · Space drift · Enter confirm · Esc back · P pause · M sound · Space records after finishing',
    gamepad:
      'D-pad / left stick steer · RT / A gas · LT / B brake · X drift · A confirm · B back · Start pause · X assist in car menu / records at finish; sound in Options',
    touch:
      'Hold arrows to steer; Gas, Brake and Drift to drive. Go confirms; Back returns; Pause stops. Records shows splits after finishing.',
  };
  findElement('controls').replaceChildren();
  if (input === 'keyboard') {
    const bindings = [
      [['←', '→'], 'steer / select'],
      [['↑', 'Z'], 'gas / menu ↑'],
      [['↓', 'X'], 'brake / menu ↓'],
      [['␣'], 'drift / assist / records'],
      [['↵'], 'confirm / retry'],
      [['Esc'], 'back'],
      [['P'], 'pause'],
      [['M'], 'sound'],
      [['F'], 'fullscreen'],
    ];
    for (const [keys, action] of bindings) {
      const item = document.createElement('span');
      item.className = 'binding';
      const keyGroup = document.createElement('span');
      keyGroup.className = 'key-group';
      for (const key of keys) {
        const keyElement = document.createElement('kbd');
        keyElement.textContent = key;
        keyElement.setAttribute(
          'aria-label',
          {
            '←': 'Left arrow',
            '→': 'Right arrow',
            '↑': 'Up arrow',
            '↓': 'Down arrow',
            '␣': 'Space',
            '↵': 'Enter',
          }[key] ?? key,
        );
        keyGroup.append(keyElement);
      }
      item.append(keyGroup, document.createTextNode(action));
      findElement('controls').append(item);
    }
  } else findElement('controls').textContent = hints[input];
}
function startAudio() {
  try {
    if (!engineAudioContext) {
      engineAudioContext = new window.AudioContext();
      oscillator = engineAudioContext.createOscillator();
      gain = engineAudioContext.createGain();
      oscillator.type = 'sawtooth';
      gain.gain.value = 0;
      oscillator.connect(gain).connect(engineAudioContext.destination);
      oscillator.start();
    }
    engineAudioContext.resume().catch(() => {});
  } catch {
    /* Silent play remains available. */
  }
}
function resetInputs() {
  keyboard = touchBits = pulses = previousGamepadInputs = 0;
  pressedKeys.clear();
  menuCommands.length = 0;
  menuRelease = false;
  pointers.clear();
  document
    .querySelectorAll('#touch .pressed')
    .forEach((button) => button.classList.remove('pressed'));
}
function suspend() {
  resetInputs();
  lastTime = 0;
  if (gameEngine) gameEngine._GravelbyteSuspend();
  if (gain) gain.gain.value = 0;
}
window.addEventListener('blur', suspend);
document.addEventListener('visibilitychange', () => {
  if (document.hidden) suspend();
});
canvas.addEventListener('blur', suspend);
canvas.addEventListener('pointerdown', (event) => {
  useInput(event.pointerType === 'touch' ? 'touch' : 'keyboard');
  canvas.focus();
  startAudio();
});
canvas.addEventListener('keydown', (event) => {
  if (!(event.code in keyBits) && !['KeyM', 'KeyF'].includes(event.code)) return;
  event.preventDefault();
  useInput('keyboard');
  startAudio();
  if (event.code === 'KeyF') {
    if (!event.repeat) fullscreen();
    return;
  }
  if (event.code === 'KeyM') {
    if (!event.repeat) findElement('mute').click();
    return;
  }
  if (!event.repeat) pulses |= keyBits[event.code];
  pressedKeys.add(event.code);
  keyboard = [...pressedKeys].reduce((inputValue, keyCode) => inputValue | keyBits[keyCode], 0);
});
window.addEventListener('keyup', (event) => {
  pressedKeys.delete(event.code);
  keyboard = [...pressedKeys].reduce((inputValue, keyCode) => inputValue | keyBits[keyCode], 0);
});
for (const button of document.querySelectorAll('[data-bit]')) {
  button.addEventListener('pointerdown', (event) => {
    event.preventDefault();
    useInput('touch');
    canvas.focus();
    startAudio();
    button.setPointerCapture(event.pointerId);
    const buttonBit = navigationBit(Number(button.dataset.bit));
    pulses |= buttonBit;
    pointers.set(event.pointerId, buttonBit);
    button.classList.add('pressed');
    touchBits = [...pointers.values()].reduce(
      (accumulatedInputs, inputValue) => accumulatedInputs | inputValue,
      0,
    );
  });
  const release = (event) => {
    pointers.delete(event.pointerId);
    touchBits = [...pointers.values()].reduce(
      (accumulatedInputs, inputValue) => accumulatedInputs | inputValue,
      0,
    );
    button.classList.remove('pressed');
  };
  button.addEventListener('pointerup', release);
  button.addEventListener('pointercancel', release);
  button.addEventListener('lostpointercapture', release);
}
findElement('play').addEventListener('click', (event) => {
  useInput(event.pointerType === 'touch' ? 'touch' : activeInput);
  playing = true;
  canvas.focus();
  startAudio();
  findElement('start-overlay').hidden = true;
  useInput(activeInput);
  lastTime = 0;
});
findElement('mute').addEventListener('pointerdown', (event) => {
  if (playing) event.preventDefault();
});
function updateMute() {
  findElement('mute').setAttribute('aria-label', muted ? 'Unmute' : 'Mute');
  findElement('mute').setAttribute('title', muted ? 'Unmute' : 'Mute');
  findElement('mute').setAttribute('aria-pressed', String(muted));
}
findElement('mute').addEventListener('click', () => {
  if (!gameEngine) return;
  gameEngine._GravelbyteToggleAudio();
  muted = !!gameEngine._GravelbyteMuted();
  save();
  updateMute();
  if (playing) {
    canvas.focus();
    startAudio();
  }
});
async function fullscreen() {
  try {
    if (document.fullscreenElement) await document.exitFullscreen();
    else await findElement('stage').requestFullscreen();
    if (playing) canvas.focus();
  } catch {
    findElement('save-status').textContent =
      'Fullscreen is unavailable in this browser. You can still play here.';
  }
}
function gamepad() {
  let inputBits = 0;
  for (const gamepadState of navigator.getGamepads?.() ?? []) {
    if (!gamepadState || gamepadState.mapping !== 'standard') continue;
    const held = (index) => !!gamepadState.buttons[index]?.pressed;
    if (held(12) || gamepadState.axes[1] < -0.25) inputBits |= 512;
    if (held(13) || gamepadState.axes[1] > 0.25) inputBits |= 1024;
    if (held(14) || gamepadState.axes[0] < -0.25) inputBits |= 1;
    if (held(15) || gamepadState.axes[0] > 0.25) inputBits |= 2;
    if (held(0) || held(7)) inputBits |= 4;
    if (held(1) || held(6)) inputBits |= 8;
    if (held(2)) inputBits |= 16;
    if (held(0)) inputBits |= 32;
    if (held(9)) inputBits |= 64;
    if (held(1)) inputBits |= 128;
  }
  if (inputBits && inputBits !== previousGamepadInputs) {
    useInput('gamepad');
  }
  previousGamepadInputs = inputBits;
  return document.activeElement === canvas ? inputBits : 0;
}
function render() {
  const bufferOffset = gameEngine._GravelbyteFrame() >>> 1;
  const pixelMemory = gameEngine.HEAPU16;
  for (let index = 0; index < 14400; index++) {
    let packedColor = pixelMemory[bufferOffset + index];
    framebufferImage.data[index * 4] = (packedColor >>> 12) * 17;
    framebufferImage.data[index * 4 + 1] = ((packedColor >>> 8) & 15) * 17;
    framebufferImage.data[index * 4 + 2] = ((packedColor >>> 4) & 15) * 17;
    framebufferImage.data[index * 4 + 3] = 255;
  }
  renderingContext.putImageData(framebufferImage, 0, 0);
}
let ghostDatabase;
let loadedGhostGeneration = -1;
function openGhostDatabase() {
  if (!ghostDatabase)
    ghostDatabase = new Promise((resolve, reject) => {
      const request = indexedDB.open('gravelbyte-replays-v4', 1);
      request.onupgradeneeded = () => request.result.createObjectStore('ghosts');
      request.onsuccess = () => resolve(request.result);
      request.onerror = () => reject(request.error);
      request.onblocked = () => reject(new Error('Replay database blocked'));
    });
  return ghostDatabase;
}
async function storeGhost(record, bytes) {
  const database = await openGhostDatabase();
  await new Promise((resolve, reject) => {
    const transaction = database.transaction('ghosts', 'readwrite');
    transaction.objectStore('ghosts').put(bytes, record);
    transaction.oncomplete = resolve;
    transaction.onerror = () => reject(transaction.error);
    transaction.onabort = () => reject(transaction.error);
  });
}
async function loadGhost(record, generation) {
  const database = await openGhostDatabase();
  const bytes = await new Promise((resolve, reject) => {
    const request = database.transaction('ghosts').objectStore('ghosts').get(record);
    request.onsuccess = () => resolve(request.result);
    request.onerror = () => reject(request.error);
  });
  if (
    gameEngine._GravelbyteGhostGeneration() !== generation ||
    !(bytes instanceof Uint8Array) ||
    bytes.length !== gameEngine._GravelbyteGhostBytes()
  )
    return;
  gameEngine.HEAPU8.set(bytes, gameEngine._GravelbyteGhostBuffer());
  gameEngine._GravelbyteImportGhost(generation);
}
function updateGhostStorage() {
  const failure = () => {
    findElement('save-status').textContent = 'Replay storage is unavailable. You can keep racing.';
  };
  if (gameEngine._GravelbyteGhostPending()) {
    const record = gameEngine._GravelbyteRecordIndex();
    const offset = gameEngine._GravelbyteExportGhost();
    // Copy before awaiting IndexedDB: the live replay is reused on restart.
    const bytes = gameEngine.HEAPU8.slice(offset, offset + gameEngine._GravelbyteGhostBytes());
    gameEngine._GravelbyteGhostMarkSaved();
    storeGhost(record, bytes).catch(failure);
    loadedGhostGeneration = gameEngine._GravelbyteGhostGeneration();
  }
  const generation = gameEngine._GravelbyteGhostGeneration();
  if (generation !== loadedGhostGeneration) {
    loadedGhostGeneration = generation;
    loadGhost(gameEngine._GravelbyteRecordIndex(), generation).catch(failure);
  }
}
function save() {
  if (!gameEngine._GravelbyteSavePending()) return;
  try {
    const bufferOffset = gameEngine._GravelbyteSave(),
      size = gameEngine._GravelbyteSaveSize();
    localStorage.setItem(
      storageKey,
      JSON.stringify(Array.from(gameEngine.HEAPU8.subarray(bufferOffset, bufferOffset + size))),
    );
    gameEngine._GravelbyteMarkSaved();
    if (saveFailed) findElement('save-status').textContent = '';
    saveFailed = false;
  } catch {
    gameEngine._GravelbyteMarkSaved();
    saveFailed = true;
    findElement('save-status').textContent =
      'Storage is unavailable. Records last for this session only.';
  }
}
function tick(timestampMilliseconds) {
  if (playing && !document.hidden) {
    const deltaTimeSeconds = lastTime ? (timestampMilliseconds - lastTime) / 1000 : 0;
    lastTime = timestampMilliseconds;
    if (deltaTimeSeconds > 0.25) {
      suspend();
    } else {
      const gamepadState = gamepad();
      let menuCommand = 0;
      if (menuRelease) menuRelease = false;
      else if (menuCommands.length) {
        menuCommand = menuCommands.shift();
        menuRelease = true;
      }
      gameEngine._GravelbyteUpdate(
        Math.max(0.0001, deltaTimeSeconds),
        keyboard | touchBits | pulses | gamepadState | menuCommand,
        {
          keyboard: 1,
          gamepad: 2,
          touch: 3,
        }[activeInput],
      );
      if (gameEngine._GravelbyteRandomRequested()) {
        let seed = crypto.getRandomValues(new Uint32Array(1))[0];
        if (seed === gameEngine._GravelbyteSeed() >>> 0) seed = (seed ^ 0x9e3779b9) >>> 0;
        gameEngine._GravelbyteSetSeed(seed);
      }
      pulses = 0;
    }
    const mode = gameEngine._GravelbyteMode();
    muted = !!gameEngine._GravelbyteMuted();
    updateMute();
    canvas.dataset.mode = String(mode);
    canvas.dataset.car = String(gameEngine._GravelbyteCar());
    canvas.dataset.track = String(gameEngine._GravelbyteTrack());
    const menuState = [
      mode,
      gameEngine._GravelbyteChallenge(),
      gameEngine._GravelbyteSeedEditor(),
      gameEngine._GravelbyteVariantsUnlocked(),
    ].join(':');
    if (menuState !== lastMenuState) {
      lastMenuState = menuState;
      lastMode = mode;
      const race = mode === 3 || mode === 4;
      document
        .querySelectorAll('.race-control')
        .forEach((inputValue) => (inputValue.hidden = !race));
      const pauseTouch = document.querySelector('.touch-pause');
      pauseTouch.hidden = !race && mode !== 5;
      pauseTouch.textContent = mode === 5 ? 'Resume' : 'Pause';
      document
        .querySelectorAll('.menu-control')
        .forEach((inputValue) => (inputValue.hidden = race));
      findElement('records').hidden =
        mode !== 6 &&
        mode !== 1 &&
        !(mode === 2 && gameEngine._GravelbyteChallenge() && !gameEngine._GravelbyteSeedEditor());
      findElement('records').textContent = mode === 1 ? 'Assist' : mode === 2 ? 'New' : 'Records';
      const variants =
        mode === 2 &&
        (gameEngine._GravelbyteChallenge() || gameEngine._GravelbyteVariantsUnlocked());
      findElement('variant-left').hidden = findElement('variant-right').hidden = !variants;
      const variantLabel = gameEngine._GravelbyteSeedEditor()
        ? 'digit'
        : gameEngine._GravelbyteChallenge()
          ? 'seed editor'
          : 'variant';
      findElement('variant-left').setAttribute('aria-label', `Previous ${variantLabel}`);
      findElement('variant-right').setAttribute('aria-label', `Next ${variantLabel}`);
      for (const [bit, label] of [
        [1, mode === 2 || mode === 5 ? '▲' : '◀'],
        [2, mode === 2 || mode === 5 ? '▼' : '▶'],
      ]) {
        const button = document.querySelector(`#touch [data-bit="${bit}"]`);
        button.textContent = label;
        button.setAttribute(
          'aria-label',
          mode === 2 || mode === 5
            ? bit === 1
              ? 'Previous choice'
              : 'Next choice'
            : bit === 1
              ? 'Steer left'
              : 'Steer right',
        );
      }
      const menuHadFocus = findElement('accessible-menu').contains(document.activeElement);
      updateAccessibleMenu(mode);
      if (mode === 3 && menuHadFocus) canvas.focus();
      else if (menuHadFocus && document.activeElement === document.body)
        findElement('menu-confirm').focus();
    }
    const status = gameEngine.UTF8ToString(gameEngine._GravelbyteStatus());
    if (findElement('game-status').textContent !== status)
      findElement('game-status').textContent = status;
    if (mode === 2)
      findElement('menu-confirm').setAttribute(
        'aria-disabled',
        String(!gameEngine._GravelbyteTrackUnlocked()),
      );
    if (gain) {
      gain.gain.setTargetAtTime(
        !muted && (mode === 0 || mode === 4 || mode === 6) ? 0.035 : 0,
        engineAudioContext.currentTime,
        0.03,
      );
      oscillator.frequency.setTargetAtTime(
        gameEngine._GravelbyteEngineFrequency(),
        engineAudioContext.currentTime,
        0.03,
      );
    }
    const seedUrl = gameEngine._GravelbyteChallenge()
      ? (gameEngine._GravelbyteSeed() >>> 0).toString(16).padStart(8, '0')
      : '';
    if (seedUrl !== lastSeedUrl) {
      lastSeedUrl = seedUrl;
      const url = new URL(location.href);
      if (seedUrl) url.searchParams.set('seed', seedUrl);
      else url.searchParams.delete('seed');
      history.replaceState(null, '', url);
    }
    updateGhostStorage();
    save();
    render();
  }
  requestAnimationFrame(tick);
}
function navigationBit(bit) {
  if (bit === 2048) return 1;
  if (bit === 4096) return 2;
  return [2, 5].includes(lastMode) && bit === 1
    ? 512
    : [2, 5].includes(lastMode) && bit === 2
      ? 1024
      : bit;
}
function menuInput(bit) {
  if (gameEngine && playing) menuCommands.push(navigationBit(bit));
}
for (const [elementIdentifier, bit] of [
  ['menu-previous', 1],
  ['menu-next', 2],
  ['menu-confirm', 32],
  ['menu-back', 128],
  ['menu-left', 2048],
  ['menu-right', 4096],
])
  findElement(elementIdentifier).addEventListener('click', () => menuInput(bit));
function updateAccessibleMenu(mode) {
  findElement('accessible-menu').hidden = ![0, 1, 2, 5, 6].includes(mode);
  const select = [1, 2, 5].includes(mode);
  for (const elementIdentifier of ['menu-previous', 'menu-next'])
    findElement(elementIdentifier).hidden = !select;
  findElement('menu-previous').textContent =
    mode === 1
      ? 'Previous car'
      : mode === 2
        ? gameEngine._GravelbyteSeedEditor()
          ? 'Increase digit'
          : 'Previous track'
        : 'Previous choice';
  findElement('menu-next').textContent =
    mode === 1
      ? 'Next car'
      : mode === 2
        ? gameEngine._GravelbyteSeedEditor()
          ? 'Decrease digit'
          : 'Next track'
        : 'Next choice';
  findElement('menu-confirm').textContent =
    mode === 0
      ? 'Choose car'
      : mode === 1
        ? 'Choose track'
        : mode === 2
          ? gameEngine._GravelbyteSeedEditor()
            ? 'Set seed'
            : 'Start race'
          : mode === 5
            ? 'Select option'
            : 'Retry';
  findElement('menu-confirm').setAttribute('aria-disabled', 'false');
  findElement('menu-back').hidden = mode === 0;
  findElement('menu-back').textContent =
    mode === 1 ? 'Back to title' : mode === 6 ? 'Choose track' : mode === 5 ? 'Back' : 'Choose car';
  findElement('menu-aux').hidden =
    mode !== 6 &&
    mode !== 5 &&
    mode !== 1 &&
    !(mode === 2 && gameEngine._GravelbyteChallenge() && !gameEngine._GravelbyteSeedEditor());
  const variants =
    mode === 2 && (gameEngine._GravelbyteChallenge() || gameEngine._GravelbyteVariantsUnlocked());
  findElement('menu-left').hidden = findElement('menu-right').hidden = !variants;
  const variantLabel = gameEngine._GravelbyteSeedEditor()
    ? 'digit'
    : gameEngine._GravelbyteChallenge()
      ? 'seed editor'
      : 'variant';
  findElement('menu-left').textContent = `Previous ${variantLabel}`;
  findElement('menu-right').textContent = `Next ${variantLabel}`;
  findElement('menu-aux').textContent =
    mode === 5
      ? 'Resume'
      : mode === 1
        ? 'Toggle steering assist'
        : mode === 2
          ? 'New random stage'
          : 'Toggle checkpoint records';
}
findElement('menu-aux').addEventListener('click', () => menuInput(lastMode === 5 ? 64 : 256));
useInput(activeInput);
try {
  const { default: createGravelbyte } = await import('./gravelbyte.js');
  gameEngine = await createGravelbyte();
  gameEngine._GravelbyteInitialSeed(crypto.getRandomValues(new Uint32Array(1))[0]);
  const expected = document.querySelector('meta[name="gravelbyte-build"]').content;
  if (
    typeof gameEngine._GravelbyteBuildIdentifier !== 'function' ||
    gameEngine.UTF8ToString(gameEngine._GravelbyteBuildIdentifier()) !== expected
  ) {
    const buildError = new Error('Game build mismatch');
    buildError.code = 'BUILD_MISMATCH';
    throw buildError;
  }
  try {
    const saved = localStorage.getItem(storageKey);
    if (saved) {
      const bytes = JSON.parse(saved);
      if (
        !Array.isArray(bytes) ||
        bytes.length !== gameEngine._GravelbyteSaveSize() ||
        !bytes.every(
          (byteValue) => Number.isInteger(byteValue) && byteValue >= 0 && byteValue <= 255,
        )
      )
        throw Error('Invalid record');
      gameEngine.HEAPU8.set(bytes, gameEngine._GravelbyteSave());
      if (!gameEngine._GravelbyteLoad()) throw Error('Invalid record');
    }
  } catch {
    findElement('save-status').textContent =
      'Previous records could not be restored. A fresh session is ready.';
  }
  const sharedSeed = new URL(location.href).searchParams.get('seed');
  if (sharedSeed !== null) {
    if (/^(?:0x)?[0-9a-f]{8}$/i.test(sharedSeed))
      gameEngine._GravelbyteSetSeed(Number.parseInt(sharedSeed.replace(/^0x/i, ''), 16));
    else
      findElement('save-status').textContent =
        'Invalid seed: use eight hexadecimal digits. Random challenges remain available.';
  }
  render();
  findElement('play').disabled = false;
  findElement('play').textContent = 'Play Gravelbyte';
  findElement('load-status').textContent = 'Ready to play.';
  muted = !!gameEngine._GravelbyteMuted();
  updateMute();
  requestAnimationFrame(tick);
} catch (error) {
  findElement('load-status').textContent =
    error.code === 'BUILD_MISMATCH'
      ? 'The game files are from different builds. Reload the page for a matching release.'
      : 'The game could not load. Please reload the page or download the PicoSystem version.';
  console.error(error);
}
