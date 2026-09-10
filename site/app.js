const $ = (id) => document.getElementById(id);
const canvas = $('game'),
  ctx = canvas.getContext('2d', { alpha: false }),
  frame = ctx.createImageData(120, 120);
const storageKey = 'gravelbyte.records.v1';
let engine,
  playing = false,
  muted = false,
  activeInput = matchMedia('(pointer:coarse)').matches ? 'touch' : 'keyboard';
let keyboard = 0,
  touchBits = 0,
  pulses = 0,
  lastTime = 0,
  lastMode = -1,
  lastPad = 0,
  saveFailed = false;
let audioContext, oscillator, gain;
const menuCommands = [];
let menuRelease = false;
const keyBits = {
  ArrowLeft: 1,
  ArrowRight: 2,
  ArrowUp: 4,
  KeyZ: 4 | 32,
  ArrowDown: 8,
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
  $('input-label').textContent = input;
  $('touch').hidden = input !== 'touch' || !playing;
  const hints = {
    keyboard:
      '← → steer · ↑ / Z gas · ↓ / X brake · Space drift · Enter confirm · Esc back · P pause · M sound · Space records after finishing',
    gamepad:
      'D-pad / left stick steer · RT / A gas · LT / B brake · X drift · A confirm · B back · Start pause · X sound in title/pause or records after finishing',
    touch:
      'Hold arrows to steer; Gas, Brake and Drift to drive. Go confirms; Back returns; Pause stops. Records shows splits after finishing.',
  };
  $('controls').replaceChildren();
  if (input === 'keyboard') {
    const bindings = [
      [['←', '→'], 'steer / select'],
      [['↑', 'Z'], 'gas'],
      [['↓', 'X'], 'brake'],
      [['␣'], 'drift / records'],
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
        const kbd = document.createElement('kbd');
        kbd.textContent = key;
        kbd.setAttribute(
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
        keyGroup.append(kbd);
      }
      item.append(keyGroup, document.createTextNode(action));
      $('controls').append(item);
    }
  } else $('controls').textContent = hints[input];
}
function startAudio() {
  try {
    if (!audioContext) {
      audioContext = new AudioContext();
      oscillator = audioContext.createOscillator();
      gain = audioContext.createGain();
      oscillator.type = 'sawtooth';
      gain.gain.value = 0;
      oscillator.connect(gain).connect(audioContext.destination);
      oscillator.start();
    }
    audioContext.resume().catch(() => {});
  } catch {
    /* Silent play remains available. */
  }
}
function resetInputs() {
  keyboard = touchBits = pulses = lastPad = 0;
  pressedKeys.clear();
  menuCommands.length = 0;
  menuRelease = false;
  pointers.clear();
  document.querySelectorAll('#touch .pressed').forEach((b) => b.classList.remove('pressed'));
}
function suspend() {
  resetInputs();
  lastTime = 0;
  if (engine) engine._gb_blur();
  if (gain) gain.gain.value = 0;
}
window.addEventListener('blur', suspend);
document.addEventListener('visibilitychange', () => {
  if (document.hidden) suspend();
});
canvas.addEventListener('blur', suspend);
canvas.addEventListener('pointerdown', (e) => {
  useInput(e.pointerType === 'touch' ? 'touch' : 'keyboard');
  canvas.focus();
  startAudio();
});
canvas.addEventListener('keydown', (e) => {
  if (!(e.code in keyBits) && !['KeyM', 'KeyF'].includes(e.code)) return;
  e.preventDefault();
  useInput('keyboard');
  startAudio();
  if (e.code === 'KeyF') {
    if (!e.repeat) fullscreen();
    return;
  }
  if (e.code === 'KeyM') {
    if (!e.repeat) $('mute').click();
    return;
  }
  if (!e.repeat) pulses |= keyBits[e.code];
  pressedKeys.add(e.code);
  keyboard = [...pressedKeys].reduce((b, k) => b | keyBits[k], 0);
});
window.addEventListener('keyup', (e) => {
  pressedKeys.delete(e.code);
  keyboard = [...pressedKeys].reduce((b, k) => b | keyBits[k], 0);
});
for (const button of document.querySelectorAll('[data-bit]')) {
  button.addEventListener('pointerdown', (e) => {
    e.preventDefault();
    useInput('touch');
    canvas.focus();
    startAudio();
    button.setPointerCapture(e.pointerId);
    pulses |= Number(button.dataset.bit);
    pointers.set(e.pointerId, Number(button.dataset.bit));
    button.classList.add('pressed');
    touchBits = [...pointers.values()].reduce((a, b) => a | b, 0);
  });
  const release = (e) => {
    pointers.delete(e.pointerId);
    touchBits = [...pointers.values()].reduce((a, b) => a | b, 0);
    button.classList.remove('pressed');
  };
  button.addEventListener('pointerup', release);
  button.addEventListener('pointercancel', release);
  button.addEventListener('lostpointercapture', release);
}
$('play').addEventListener('click', (e) => {
  useInput(e.pointerType === 'touch' ? 'touch' : activeInput);
  playing = true;
  canvas.focus();
  startAudio();
  $('start-overlay').hidden = true;
  useInput(activeInput);
  lastTime = 0;
});
$('mute').addEventListener('pointerdown', (e) => {
  if (playing) e.preventDefault();
});
function updateMute() {
  $('mute').setAttribute('aria-label', muted ? 'Unmute' : 'Mute');
  $('mute').setAttribute('title', muted ? 'Unmute' : 'Mute');
  $('mute').setAttribute('aria-pressed', String(muted));
}
$('mute').addEventListener('click', () => {
  if (!engine) return;
  engine._gb_toggle_audio();
  muted = !!engine._gb_muted();
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
    else await $('stage').requestFullscreen();
    if (playing) canvas.focus();
  } catch {
    $('save-status').textContent =
      'Fullscreen is unavailable in this browser. You can still play here.';
  }
}
function gamepad() {
  let bits = 0;
  for (const pad of navigator.getGamepads?.() ?? []) {
    if (!pad || pad.mapping !== 'standard') continue;
    const held = (i) => !!pad.buttons[i]?.pressed;
    if (held(14) || pad.axes[0] < -0.25) bits |= 1;
    if (held(15) || pad.axes[0] > 0.25) bits |= 2;
    if (held(0) || held(7)) bits |= 4;
    if (held(1) || held(6)) bits |= 8;
    if (held(2)) bits |= 16;
    if (held(0)) bits |= 32;
    if (held(9)) bits |= 64;
    if (held(1)) bits |= 128;
  }
  if (bits && bits !== lastPad) {
    useInput('gamepad');
  }
  lastPad = bits;
  return document.activeElement === canvas ? bits : 0;
}
function render() {
  const ptr = engine._gb_frame() >>> 1;
  const raw = engine.HEAPU16;
  for (let i = 0; i < 14400; i++) {
    let c = raw[ptr + i];
    frame.data[i * 4] = (c >>> 12) * 17;
    frame.data[i * 4 + 1] = ((c >>> 8) & 15) * 17;
    frame.data[i * 4 + 2] = ((c >>> 4) & 15) * 17;
    frame.data[i * 4 + 3] = 255;
  }
  ctx.putImageData(frame, 0, 0);
}
function save() {
  if (!engine._gb_dirty()) return;
  try {
    const ptr = engine._gb_save(),
      size = engine._gb_save_size();
    localStorage.setItem(
      storageKey,
      JSON.stringify(Array.from(engine.HEAPU8.subarray(ptr, ptr + size))),
    );
    engine._gb_saved();
    if (saveFailed) $('save-status').textContent = '';
    saveFailed = false;
  } catch {
    engine._gb_saved();
    saveFailed = true;
    $('save-status').textContent = 'Storage is unavailable. Records last for this session only.';
  }
}
function tick(time) {
  if (playing && !document.hidden) {
    const delta = lastTime ? (time - lastTime) / 1000 : 0;
    lastTime = time;
    if (delta > 0.25) {
      suspend();
    } else {
      const pad = gamepad();
      let menuCommand = 0;
      if (menuRelease) menuRelease = false;
      else if (menuCommands.length) {
        menuCommand = menuCommands.shift();
        menuRelease = true;
      }
      engine._gb_step(
        Math.max(0.0001, delta),
        keyboard | touchBits | pulses | pad | menuCommand,
        { keyboard: 1, gamepad: 2, touch: 3 }[activeInput],
      );
      pulses = 0;
    }
    const mode = engine._gb_mode();
    muted = !!engine._gb_muted();
    updateMute();
    canvas.dataset.mode = String(mode);
    canvas.dataset.car = String(engine._gb_car());
    canvas.dataset.track = String(engine._gb_track());
    if (mode !== lastMode) {
      lastMode = mode;
      const race = mode === 3 || mode === 4;
      document.querySelectorAll('.race-control').forEach((b) => (b.hidden = !race));
      const pauseTouch = document.querySelector('.touch-pause');
      pauseTouch.hidden = !race && mode !== 5;
      pauseTouch.textContent = mode === 5 ? 'Resume' : 'Pause';
      document.querySelectorAll('.menu-control').forEach((b) => (b.hidden = race));
      $('records').hidden = mode !== 6;
      const menuHadFocus = $('accessible-menu').contains(document.activeElement);
      updateAccessibleMenu(mode);
      if (mode === 3 && menuHadFocus) canvas.focus();
      else if (menuHadFocus && document.activeElement === document.body) $('menu-confirm').focus();
    }
    const status = engine.UTF8ToString(engine._gb_status());
    if ($('game-status').textContent !== status) $('game-status').textContent = status;
    if (mode === 2) $('menu-confirm').setAttribute('aria-disabled', String(!engine._gb_unlocked()));
    if (gain) {
      gain.gain.setTargetAtTime(
        !muted && (mode === 0 || mode === 4 || mode === 6) ? 0.035 : 0,
        audioContext.currentTime,
        0.03,
      );
      oscillator.frequency.setTargetAtTime(
        65 + engine._gb_speed() * 8,
        audioContext.currentTime,
        0.03,
      );
    }
    save();
    render();
  }
  requestAnimationFrame(tick);
}
function menuInput(bit) {
  if (engine && playing) menuCommands.push(bit);
}
for (const [id, bit] of [
  ['menu-previous', 1],
  ['menu-next', 2],
  ['menu-confirm', 32],
  ['menu-back', 128],
])
  $(id).addEventListener('click', () => menuInput(bit));
function updateAccessibleMenu(mode) {
  $('accessible-menu').hidden = ![0, 1, 2, 5, 6].includes(mode);
  const select = mode === 1 || mode === 2;
  for (const id of ['menu-previous', 'menu-next']) $(id).hidden = !select;
  $('menu-previous').textContent = mode === 1 ? 'Previous car' : 'Previous track';
  $('menu-next').textContent = mode === 1 ? 'Next car' : 'Next track';
  $('menu-confirm').textContent =
    mode === 0 ? 'Choose car' : mode === 1 ? 'Choose track' : mode === 2 ? 'Start race' : 'Retry';
  $('menu-confirm').setAttribute('aria-disabled', 'false');
  $('menu-back').hidden = mode === 0;
  $('menu-back').textContent =
    mode === 1 ? 'Back to title' : mode === 6 ? 'Choose track' : 'Choose car';
  $('menu-aux').hidden = mode !== 6 && mode !== 5;
  $('menu-aux').textContent = mode === 5 ? 'Resume' : 'Toggle checkpoint records';
}
$('menu-aux').addEventListener('click', () => menuInput(lastMode === 5 ? 64 : 256));
useInput(activeInput);
try {
  const { default: createGravelbyte } = await import('./gravelbyte.js');
  engine = await createGravelbyte();
  const expected = document.querySelector('meta[name="gravelbyte-build"]').content;
  if (
    typeof engine._gb_build_id !== 'function' ||
    engine.UTF8ToString(engine._gb_build_id()) !== expected
  ) {
    const error = new Error('Game build mismatch');
    error.code = 'BUILD_MISMATCH';
    throw error;
  }
  try {
    const saved = localStorage.getItem(storageKey);
    if (saved) {
      const bytes = JSON.parse(saved);
      if (
        !Array.isArray(bytes) ||
        bytes.length !== engine._gb_save_size() ||
        !bytes.every((n) => Number.isInteger(n) && n >= 0 && n <= 255)
      )
        throw Error('Invalid record');
      engine.HEAPU8.set(bytes, engine._gb_save());
      if (!engine._gb_load()) throw Error('Invalid record');
    }
  } catch {
    $('save-status').textContent =
      'Previous records could not be restored. A fresh session is ready.';
  }
  render();
  $('play').disabled = false;
  $('play').textContent = 'Play Gravelbyte';
  $('load-status').textContent = 'Ready to play.';
  muted = !!engine._gb_muted();
  updateMute();
  requestAnimationFrame(tick);
} catch (error) {
  $('load-status').textContent =
    error.code === 'BUILD_MISMATCH'
      ? 'The game files are from different builds. Reload the page for a matching release.'
      : 'The game could not load. Please reload the page or download the PicoSystem version.';
  console.error(error);
}
