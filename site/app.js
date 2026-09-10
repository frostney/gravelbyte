const FindElement = (ElementIdentifier) => document.getElementById(ElementIdentifier);
const Canvas = FindElement('game'),
  RenderingContext = Canvas.getContext('2d', {
    alpha: false,
  }),
  FramebufferImage = RenderingContext.createImageData(120, 120);
const StorageKey = 'gravelbyte.records.v1';
let GameEngine,
  Playing = false,
  Muted = false,
  ActiveInput = matchMedia('(pointer:coarse)').matches ? 'touch' : 'keyboard';
let Keyboard = 0,
  TouchBits = 0,
  Pulses = 0,
  LastTime = 0,
  LastMode = -1,
  PreviousGamepadInputs = 0,
  SaveFailed = false;
let EngineAudioContext, Oscillator, Gain;
const MenuCommands = [];
let MenuRelease = false;
const KeyBits = {
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
const PressedKeys = new Set(),
  Pointers = new Map();
function UseInput(Input) {
  ActiveInput = Input;
  FindElement('input-label').textContent = Input;
  FindElement('touch').hidden = Input !== 'touch' || !Playing;
  const Hints = {
    keyboard:
      '← → steer · ↑ / Z gas · ↓ / X brake · Space drift · Enter confirm · Esc back · P pause · M sound · Space records after finishing',
    gamepad:
      'D-pad / left stick steer · RT / A gas · LT / B brake · X drift · A confirm · B back · Start pause · X sound in title/pause or records after finishing',
    touch:
      'Hold arrows to steer; Gas, Brake and Drift to drive. Go confirms; Back returns; Pause stops. Records shows splits after finishing.',
  };
  FindElement('controls').replaceChildren();
  if (Input === 'keyboard') {
    const Bindings = [
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
    for (const [Keys, Action] of Bindings) {
      const Item = document.createElement('span');
      Item.className = 'binding';
      const KeyGroup = document.createElement('span');
      KeyGroup.className = 'key-group';
      for (const Key of Keys) {
        const KeyElement = document.createElement('kbd');
        KeyElement.textContent = Key;
        KeyElement.setAttribute(
          'aria-label',
          {
            '←': 'Left arrow',
            '→': 'Right arrow',
            '↑': 'Up arrow',
            '↓': 'Down arrow',
            '␣': 'Space',
            '↵': 'Enter',
          }[Key] ?? Key,
        );
        KeyGroup.append(KeyElement);
      }
      Item.append(KeyGroup, document.createTextNode(Action));
      FindElement('controls').append(Item);
    }
  } else FindElement('controls').textContent = Hints[Input];
}
function StartAudio() {
  try {
    if (!EngineAudioContext) {
      EngineAudioContext = new window.AudioContext();
      Oscillator = EngineAudioContext.createOscillator();
      Gain = EngineAudioContext.createGain();
      Oscillator.type = 'sawtooth';
      Gain.gain.value = 0;
      Oscillator.connect(Gain).connect(EngineAudioContext.destination);
      Oscillator.start();
    }
    EngineAudioContext.resume().catch(() => {});
  } catch {
    /* Silent play remains available. */
  }
}
function ResetInputs() {
  Keyboard = TouchBits = Pulses = PreviousGamepadInputs = 0;
  PressedKeys.clear();
  MenuCommands.length = 0;
  MenuRelease = false;
  Pointers.clear();
  document
    .querySelectorAll('#touch .pressed')
    .forEach((Button) => Button.classList.remove('pressed'));
}
function Suspend() {
  ResetInputs();
  LastTime = 0;
  if (GameEngine) GameEngine._GravelbyteSuspend();
  if (Gain) Gain.gain.value = 0;
}
window.addEventListener('blur', Suspend);
document.addEventListener('visibilitychange', () => {
  if (document.hidden) Suspend();
});
Canvas.addEventListener('blur', Suspend);
Canvas.addEventListener('pointerdown', (Event) => {
  UseInput(Event.pointerType === 'touch' ? 'touch' : 'keyboard');
  Canvas.focus();
  StartAudio();
});
Canvas.addEventListener('keydown', (Event) => {
  if (!(Event.code in KeyBits) && !['KeyM', 'KeyF'].includes(Event.code)) return;
  Event.preventDefault();
  UseInput('keyboard');
  StartAudio();
  if (Event.code === 'KeyF') {
    if (!Event.repeat) Fullscreen();
    return;
  }
  if (Event.code === 'KeyM') {
    if (!Event.repeat) FindElement('mute').click();
    return;
  }
  if (!Event.repeat) Pulses |= KeyBits[Event.code];
  PressedKeys.add(Event.code);
  Keyboard = [...PressedKeys].reduce((InputValue, KeyCode) => InputValue | KeyBits[KeyCode], 0);
});
window.addEventListener('keyup', (Event) => {
  PressedKeys.delete(Event.code);
  Keyboard = [...PressedKeys].reduce((InputValue, KeyCode) => InputValue | KeyBits[KeyCode], 0);
});
for (const Button of document.querySelectorAll('[data-bit]')) {
  Button.addEventListener('pointerdown', (Event) => {
    Event.preventDefault();
    UseInput('touch');
    Canvas.focus();
    StartAudio();
    Button.setPointerCapture(Event.pointerId);
    Pulses |= Number(Button.dataset.bit);
    Pointers.set(Event.pointerId, Number(Button.dataset.bit));
    Button.classList.add('pressed');
    TouchBits = [...Pointers.values()].reduce(
      (AccumulatedInputs, InputValue) => AccumulatedInputs | InputValue,
      0,
    );
  });
  const Release = (Event) => {
    Pointers.delete(Event.pointerId);
    TouchBits = [...Pointers.values()].reduce(
      (AccumulatedInputs, InputValue) => AccumulatedInputs | InputValue,
      0,
    );
    Button.classList.remove('pressed');
  };
  Button.addEventListener('pointerup', Release);
  Button.addEventListener('pointercancel', Release);
  Button.addEventListener('lostpointercapture', Release);
}
FindElement('play').addEventListener('click', (Event) => {
  UseInput(Event.pointerType === 'touch' ? 'touch' : ActiveInput);
  Playing = true;
  Canvas.focus();
  StartAudio();
  FindElement('start-overlay').hidden = true;
  UseInput(ActiveInput);
  LastTime = 0;
});
FindElement('mute').addEventListener('pointerdown', (Event) => {
  if (Playing) Event.preventDefault();
});
function UpdateMute() {
  FindElement('mute').setAttribute('aria-label', Muted ? 'Unmute' : 'Mute');
  FindElement('mute').setAttribute('title', Muted ? 'Unmute' : 'Mute');
  FindElement('mute').setAttribute('aria-pressed', String(Muted));
}
FindElement('mute').addEventListener('click', () => {
  if (!GameEngine) return;
  GameEngine._GravelbyteToggleAudio();
  Muted = !!GameEngine._GravelbyteMuted();
  Save();
  UpdateMute();
  if (Playing) {
    Canvas.focus();
    StartAudio();
  }
});
async function Fullscreen() {
  try {
    if (document.fullscreenElement) await document.exitFullscreen();
    else await FindElement('stage').requestFullscreen();
    if (Playing) Canvas.focus();
  } catch {
    FindElement('save-status').textContent =
      'Fullscreen is unavailable in this browser. You can still play here.';
  }
}
function Gamepad() {
  let InputBits = 0;
  for (const GamepadState of navigator.getGamepads?.() ?? []) {
    if (!GamepadState || GamepadState.mapping !== 'standard') continue;
    const Held = (Index) => !!GamepadState.buttons[Index]?.pressed;
    if (Held(14) || GamepadState.axes[0] < -0.25) InputBits |= 1;
    if (Held(15) || GamepadState.axes[0] > 0.25) InputBits |= 2;
    if (Held(0) || Held(7)) InputBits |= 4;
    if (Held(1) || Held(6)) InputBits |= 8;
    if (Held(2)) InputBits |= 16;
    if (Held(0)) InputBits |= 32;
    if (Held(9)) InputBits |= 64;
    if (Held(1)) InputBits |= 128;
  }
  if (InputBits && InputBits !== PreviousGamepadInputs) {
    UseInput('gamepad');
  }
  PreviousGamepadInputs = InputBits;
  return document.activeElement === Canvas ? InputBits : 0;
}
function Render() {
  const BufferOffset = GameEngine._GravelbyteFrame() >>> 1;
  const PixelMemory = GameEngine.HEAPU16;
  for (let Index = 0; Index < 14400; Index++) {
    let PackedColor = PixelMemory[BufferOffset + Index];
    FramebufferImage.data[Index * 4] = (PackedColor >>> 12) * 17;
    FramebufferImage.data[Index * 4 + 1] = ((PackedColor >>> 8) & 15) * 17;
    FramebufferImage.data[Index * 4 + 2] = ((PackedColor >>> 4) & 15) * 17;
    FramebufferImage.data[Index * 4 + 3] = 255;
  }
  RenderingContext.putImageData(FramebufferImage, 0, 0);
}
function Save() {
  if (!GameEngine._GravelbyteSavePending()) return;
  try {
    const BufferOffset = GameEngine._GravelbyteSave(),
      Size = GameEngine._GravelbyteSaveSize();
    localStorage.setItem(
      StorageKey,
      JSON.stringify(Array.from(GameEngine.HEAPU8.subarray(BufferOffset, BufferOffset + Size))),
    );
    GameEngine._GravelbyteMarkSaved();
    if (SaveFailed) FindElement('save-status').textContent = '';
    SaveFailed = false;
  } catch {
    GameEngine._GravelbyteMarkSaved();
    SaveFailed = true;
    FindElement('save-status').textContent =
      'Storage is unavailable. Records last for this session only.';
  }
}
function Tick(TimestampMilliseconds) {
  if (Playing && !document.hidden) {
    const DeltaTimeSeconds = LastTime ? (TimestampMilliseconds - LastTime) / 1000 : 0;
    LastTime = TimestampMilliseconds;
    if (DeltaTimeSeconds > 0.25) {
      Suspend();
    } else {
      const GamepadState = Gamepad();
      let MenuCommand = 0;
      if (MenuRelease) MenuRelease = false;
      else if (MenuCommands.length) {
        MenuCommand = MenuCommands.shift();
        MenuRelease = true;
      }
      GameEngine._GravelbyteUpdate(
        Math.max(0.0001, DeltaTimeSeconds),
        Keyboard | TouchBits | Pulses | GamepadState | MenuCommand,
        {
          keyboard: 1,
          gamepad: 2,
          touch: 3,
        }[ActiveInput],
      );
      Pulses = 0;
    }
    const Mode = GameEngine._GravelbyteMode();
    Muted = !!GameEngine._GravelbyteMuted();
    UpdateMute();
    Canvas.dataset.mode = String(Mode);
    Canvas.dataset.car = String(GameEngine._GravelbyteCar());
    Canvas.dataset.track = String(GameEngine._GravelbyteTrack());
    if (Mode !== LastMode) {
      LastMode = Mode;
      const Race = Mode === 3 || Mode === 4;
      document
        .querySelectorAll('.race-control')
        .forEach((InputValue) => (InputValue.hidden = !Race));
      const PauseTouch = document.querySelector('.touch-pause');
      PauseTouch.hidden = !Race && Mode !== 5;
      PauseTouch.textContent = Mode === 5 ? 'Resume' : 'Pause';
      document
        .querySelectorAll('.menu-control')
        .forEach((InputValue) => (InputValue.hidden = Race));
      FindElement('records').hidden = Mode !== 6;
      const MenuHadFocus = FindElement('accessible-menu').contains(document.activeElement);
      UpdateAccessibleMenu(Mode);
      if (Mode === 3 && MenuHadFocus) Canvas.focus();
      else if (MenuHadFocus && document.activeElement === document.body)
        FindElement('menu-confirm').focus();
    }
    const Status = GameEngine.UTF8ToString(GameEngine._GravelbyteStatus());
    if (FindElement('game-status').textContent !== Status)
      FindElement('game-status').textContent = Status;
    if (Mode === 2)
      FindElement('menu-confirm').setAttribute(
        'aria-disabled',
        String(!GameEngine._GravelbyteTrackUnlocked()),
      );
    if (Gain) {
      Gain.gain.setTargetAtTime(
        !Muted && (Mode === 0 || Mode === 4 || Mode === 6) ? 0.035 : 0,
        EngineAudioContext.currentTime,
        0.03,
      );
      Oscillator.frequency.setTargetAtTime(
        65 + GameEngine._GravelbyteSpeed() * 8,
        EngineAudioContext.currentTime,
        0.03,
      );
    }
    Save();
    Render();
  }
  requestAnimationFrame(Tick);
}
function MenuInput(Bit) {
  if (GameEngine && Playing) MenuCommands.push(Bit);
}
for (const [ElementIdentifier, Bit] of [
  ['menu-previous', 1],
  ['menu-next', 2],
  ['menu-confirm', 32],
  ['menu-back', 128],
])
  FindElement(ElementIdentifier).addEventListener('click', () => MenuInput(Bit));
function UpdateAccessibleMenu(Mode) {
  FindElement('accessible-menu').hidden = ![0, 1, 2, 5, 6].includes(Mode);
  const Select = Mode === 1 || Mode === 2;
  for (const ElementIdentifier of ['menu-previous', 'menu-next'])
    FindElement(ElementIdentifier).hidden = !Select;
  FindElement('menu-previous').textContent = Mode === 1 ? 'Previous car' : 'Previous track';
  FindElement('menu-next').textContent = Mode === 1 ? 'Next car' : 'Next track';
  FindElement('menu-confirm').textContent =
    Mode === 0 ? 'Choose car' : Mode === 1 ? 'Choose track' : Mode === 2 ? 'Start race' : 'Retry';
  FindElement('menu-confirm').setAttribute('aria-disabled', 'false');
  FindElement('menu-back').hidden = Mode === 0;
  FindElement('menu-back').textContent =
    Mode === 1 ? 'Back to title' : Mode === 6 ? 'Choose track' : 'Choose car';
  FindElement('menu-aux').hidden = Mode !== 6 && Mode !== 5;
  FindElement('menu-aux').textContent = Mode === 5 ? 'Resume' : 'Toggle checkpoint records';
}
FindElement('menu-aux').addEventListener('click', () => MenuInput(LastMode === 5 ? 64 : 256));
UseInput(ActiveInput);
try {
  const { default: CreateGravelbyte } = await import('./gravelbyte.js');
  GameEngine = await CreateGravelbyte();
  const Expected = document.querySelector('meta[name="gravelbyte-build"]').content;
  if (
    typeof GameEngine._GravelbyteBuildIdentifier !== 'function' ||
    GameEngine.UTF8ToString(GameEngine._GravelbyteBuildIdentifier()) !== Expected
  ) {
    const BuildError = new Error('Game build mismatch');
    BuildError.code = 'BUILD_MISMATCH';
    throw BuildError;
  }
  try {
    const Saved = localStorage.getItem(StorageKey);
    if (Saved) {
      const Bytes = JSON.parse(Saved);
      if (
        !Array.isArray(Bytes) ||
        Bytes.length !== GameEngine._GravelbyteSaveSize() ||
        !Bytes.every(
          (ByteValue) => Number.isInteger(ByteValue) && ByteValue >= 0 && ByteValue <= 255,
        )
      )
        throw Error('Invalid record');
      GameEngine.HEAPU8.set(Bytes, GameEngine._GravelbyteSave());
      if (!GameEngine._GravelbyteLoad()) throw Error('Invalid record');
    }
  } catch {
    FindElement('save-status').textContent =
      'Previous records could not be restored. A fresh session is ready.';
  }
  Render();
  FindElement('play').disabled = false;
  FindElement('play').textContent = 'Play Gravelbyte';
  FindElement('load-status').textContent = 'Ready to play.';
  Muted = !!GameEngine._GravelbyteMuted();
  UpdateMute();
  requestAnimationFrame(Tick);
} catch (Error) {
  FindElement('load-status').textContent =
    Error.code === 'BUILD_MISMATCH'
      ? 'The game files are from different builds. Reload the page for a matching release.'
      : 'The game could not load. Please reload the page or download the PicoSystem version.';
  console.error(Error);
}
