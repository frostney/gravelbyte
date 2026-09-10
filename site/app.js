import createGravelbyte from './gravelbyte.js';
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
      '← → steer · ↑ / Z gas · ↓ / X brake · Space drift · Enter confirm · Esc back · P pause',
    gamepad:
      'D-pad / left stick steer · RT / A gas · LT / B brake · X drift · A confirm · B back · Start pause',
    touch:
      'Hold arrows to steer. Hold Gas, Brake or Drift to drive. Tap Go to confirm, Back to choose again, and Pause to stop.',
  };
  $('controls').textContent = hints[input];
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
  if (!(e.code in keyBits) && e.code !== 'KeyM') return;
  e.preventDefault();
  useInput('keyboard');
  startAudio();
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
  $('pause').disabled = false;
  useInput(activeInput);
  lastTime = 0;
});
for (const b of document.querySelectorAll('.toolbar button'))
  b.addEventListener('pointerdown', (e) => {
    if (playing) e.preventDefault();
  });
$('pause').addEventListener('click', () => {
  canvas.focus();
  pulses |= 64;
});
$('mute').addEventListener('click', () => {
  muted = !muted;
  $('mute').textContent = muted ? 'Unmute' : 'Mute';
  $('mute').setAttribute('aria-pressed', String(muted));
  if (playing) {
    canvas.focus();
    startAudio();
  }
});
$('fullscreen').addEventListener('click', async () => {
  try {
    if (document.fullscreenElement) await document.exitFullscreen();
    else await $('stage').requestFullscreen();
    if (playing) canvas.focus();
  } catch {
    $('save-status').textContent =
      'Fullscreen is unavailable in this browser. You can still play here.';
  }
});
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
    if (saveFailed) $('save-status').textContent = 'Records stay in this browser.';
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
      engine._gb_step(
        Math.max(0.0001, delta),
        keyboard | touchBits | pulses | pad,
        { keyboard: 1, gamepad: 2, touch: 3 }[activeInput],
      );
      pulses = 0;
    }
    const mode = engine._gb_mode();
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
      $('pause').textContent = mode === 5 ? 'Resume' : 'Pause';
    }
    if (gain) {
      gain.gain.setTargetAtTime(!muted && mode === 4 ? 0.035 : 0, audioContext.currentTime, 0.03);
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
useInput(activeInput);
try {
  engine = await createGravelbyte();
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
  $('load-status').textContent = 'Three cars. Three roads. All yours.';
  requestAnimationFrame(tick);
} catch (error) {
  $('load-status').textContent =
    'The game could not load. Please reload the page or download the PicoSystem version.';
  console.error(error);
}
fetch('version.json')
  .then((r) => (r.ok ? r.json() : null))
  .then((v) => {
    if (v) $('version').textContent = `Build ${v.commit.slice(0, 7)} · Browser + PicoSystem`;
  })
  .catch(() => {});
