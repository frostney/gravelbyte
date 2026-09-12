import { test, expect } from '@playwright/test';
async function play(page) {
  await page.goto('/');
  await page
    .getByRole('button', {
      name: 'Play Gravelbyte',
      exact: true,
    })
    .click();
}
async function press(page, key) {
  await page.locator('#game').press(key);
  await page.waitForTimeout(50);
}
test('keyboard menus, pause, records selections and reload', async ({ page: page }) => {
  const errors = [];
  page.on('pageerror', (event) => errors.push(event.message));
  await play(page);
  const game = page.locator('#game');
  await press(page, 'Enter');
  await expect(game).toHaveAttribute('data-mode', '1');
  await press(page, 'ArrowRight');
  await expect(game).toHaveAttribute('data-car', '2');
  await press(page, 'Enter');
  await expect(game).toHaveAttribute('data-mode', '2');
  await press(page, 'ArrowDown');
  await press(page, 'ArrowDown');
  await expect(game).toHaveAttribute('data-track', '2');
  await press(page, 'Enter');
  await expect(game).toHaveAttribute('data-mode', '2');
  await press(page, 'ArrowDown');
  await expect(page.locator('#game-status')).toContainText('Random challenge');
  await press(page, 'ArrowDown');
  await expect(game).toHaveAttribute('data-track', '0');
  await press(page, 'Enter');
  await expect(game).toHaveAttribute('data-mode', '3');
  await expect(game).toHaveAttribute('data-mode', '4', {
    timeout: 6000,
  });
  await page.keyboard.down('ArrowUp');
  await page.waitForTimeout(250);
  await page.keyboard.up('ArrowUp');
  await press(page, 'p');
  await expect(game).toHaveAttribute('data-mode', '5');
  await press(page, 'p');
  await expect(game).toHaveAttribute('data-mode', '4');
  await page
    .getByRole('button', {
      name: 'Mute',
      exact: true,
    })
    .click();
  await expect(game).toHaveAttribute('data-mode', '4');
  await page.reload();
  await page
    .getByRole('button', {
      name: 'Play Gravelbyte',
      exact: true,
    })
    .click();
  await press(page, 'Enter');
  await expect(game).toHaveAttribute('data-car', '2');
  await expect(game).toHaveAttribute('data-track', '0');
  await expect(
    page.getByRole('button', {
      name: 'Unmute',
      exact: true,
    }),
  ).toBeVisible();
  await expect(page.locator('#touch')).toBeHidden();
  await expect(page.locator('kbd[aria-label="Enter"]')).toBeVisible();
  expect(errors).toEqual([]);
});
test('touch input, simultaneous steering/gas, cancellation and keyboard switching', async ({
  browser: browser,
}) => {
  const context = await browser.newContext({
    viewport: {
      width: 390,
      height: 844,
    },
    hasTouch: true,
    isMobile: true,
  });
  const page = await context.newPage();
  await play(page);
  const game = page.locator('#game');
  await expect(page.locator('#touch')).toBeVisible();
  const go = page.getByRole('button', {
    name: 'Go',
    exact: true,
  });
  await go.tap();
  await expect(game).toHaveAttribute('data-mode', '1');
  await page
    .getByRole('button', {
      name: 'Steer right',
    })
    .tap();
  await expect(game).toHaveAttribute('data-car', '2');
  await go.tap();
  await expect(game).toHaveAttribute('data-mode', '2');
  await go.tap();
  await expect(game).toHaveAttribute('data-mode', '4', {
    timeout: 6000,
  });
  const gas = await page
    .getByRole('button', {
      name: 'Gas',
      exact: true,
    })
    .boundingBox();
  const left = await page
    .getByRole('button', {
      name: 'Steer left',
    })
    .boundingBox();
  const cdp = await context.newCDPSession(page);
  await cdp.send('Input.dispatchTouchEvent', {
    type: 'touchStart',
    touchPoints: [
      {
        x: gas.x + gas.width / 2,
        y: gas.y + gas.height / 2,
        id: 10,
      },
      {
        x: left.x + left.width / 2,
        y: left.y + left.height / 2,
        id: 11,
      },
    ],
  });
  await expect(page.locator('#touch .pressed')).toHaveCount(2);
  await cdp.send('Input.dispatchTouchEvent', {
    type: 'touchCancel',
    touchPoints: [],
  });
  await expect(page.locator('#touch .pressed')).toHaveCount(0);
  await press(page, 'p');
  await expect(page.locator('#touch')).toBeHidden();
  await expect(page.locator('#input-label')).toHaveText('keyboard');
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth)).toBe(true);
  await context.close();
});
test('gamepad activation and storage denied fallback', async ({ page: page }) => {
  await page.addInitScript(() => {
    window.testGamepad = {
      mapping: 'standard',
      axes: [0, 0],
      buttons: Array.from(
        {
          length: 16,
        },
        () => ({
          pressed: false,
          value: 0,
        }),
      ),
    };
    navigator.getGamepads = () => [window.testGamepad];
    Storage.prototype.setItem = () => {
      throw new Error('Storage denied');
    };
  });
  await play(page);
  const game = page.locator('#game');
  const button = async (buttonIndex) => {
    await page.evaluate((buttonIndex) => {
      window.testGamepad.buttons[buttonIndex].pressed = true;
    }, buttonIndex);
    await page.waitForTimeout(80);
    await page.evaluate((buttonIndex) => {
      window.testGamepad.buttons[buttonIndex].pressed = false;
    }, buttonIndex);
    await page.waitForTimeout(80);
  };
  await button(0);
  await expect(game).toHaveAttribute('data-mode', '1');
  await expect(page.locator('#input-label')).toHaveText('gamepad');
  await expect(page.locator('#controls')).toContainText('RT / A gas');
  await button(0);
  await button(0);
  await expect(page.locator('#save-status')).toContainText('session only');
  await button(9);
  await expect(game).toHaveAttribute('data-mode', '5');
  await button(13);
  await button(13);
  await button(13);
  await button(13);
  await expect(page.locator('#game-status')).toContainText('COURSES');
  await button(0);
  await expect(game).toHaveAttribute('data-mode', '2');
});
for (const [width, height, touch] of [
  [1366, 768, false],
  [1280, 720, false],
  [390, 844, true],
  [375, 667, true],
  [844, 390, true],
]) {
  test(`game and instructions fit without scrolling at ${width}x${height}`, async ({
    browser: browser,
  }) => {
    const context = await browser.newContext({
      viewport: {
        width: width,
        height: height,
      },
      hasTouch: touch,
      isMobile: touch,
    });
    const page = await context.newPage();
    await play(page);
    for (const selector of ['#game', '.links', '#controls', '.install']) {
      await expect(page.locator(selector)).toBeInViewport({
        ratio: 1,
      });
    }
    expect(
      await page.evaluate(() => ({
        horizontal: document.documentElement.scrollWidth > innerWidth,
        vertical: document.documentElement.scrollHeight > innerHeight,
      })),
    ).toEqual({
      horizontal: false,
      vertical: false,
    });
    const game = await page.locator('#game').boundingBox();
    expect(game.width).toBeGreaterThanOrEqual(280);
    expect(game.height).toBeCloseTo(game.width);
    await expect(
      page.getByRole('link', {
        name: 'GitHub',
      }),
    ).toHaveAttribute('href', 'https://github.com/frostney/gravelbyte');
    await expect(
      page.getByRole('link', {
        name: 'Download for PicoSystem',
      }),
    ).toHaveAttribute('href', /assets\/[0-9a-f]{40}\/gravelbyte\.uf2$/);
    await context.close();
  });
}
for (const asset of ['gravelbyte.js', 'gravelbyte.wasm', 'app.js']) {
  test(`missing ${asset} reports a load error instead of hanging`, async ({ page: page }) => {
    await page.route(`**/${asset}`, (route) =>
      route.fulfill({
        status: 404,
        body: 'missing',
      }),
    );
    await page.goto('/');
    await expect(page.locator('#load-status')).toContainText('could not load');
    await expect(page.locator('#play')).toBeDisabled();
  });
}
test('mixed release executable is rejected before play', async ({ page: page }) => {
  await page.route('**/', async (route) => {
    const response = await route.fetch();
    const html = (await response.text()).replace(
      /(name="gravelbyte-build" content=")[0-9a-f]{40}/,
      '$1' + '0'.repeat(40),
    );
    await route.fulfill({
      response: response,
      body: html,
    });
  });
  await page.goto('/');
  await expect(page.locator('#load-status')).toContainText('different builds');
  await expect(page.locator('#play')).toBeDisabled();
});
test('corrupt and unavailable storage preserve playable controls', async ({ page: page }) => {
  await page.addInitScript(() => localStorage.setItem('gravelbyte.records.v4', '[1,2,3]'));
  await play(page);
  await expect(page.locator('#save-status')).toContainText('could not be restored');
  await press(page, 'Enter');
  await expect(page.locator('#game-status')).toContainText('Choose car');
});
test('accessible menus announce choices, refuse locked stages and start an unlocked race', async ({
  page: page,
}) => {
  await play(page);
  const chooseCar = page.getByRole('button', {
    name: 'Choose car',
    exact: true,
  });
  await chooseCar.focus();
  await chooseCar.press('Enter');
  await expect(page.locator('#game-status')).toContainText('KESTREL GT, STANDARD');
  const next = page.getByRole('button', {
    name: 'Next car',
    exact: true,
  });
  await next.focus();
  await next.press('Enter');
  await expect(page.locator('#game-status')).toContainText('GOSHAWK TURBO, EXPERT');
  const choose = page.getByRole('button', {
    name: 'Choose track',
    exact: true,
  });
  await choose.focus();
  await choose.press('Enter');
  await expect(page.locator('#game-status')).toContainText('BRACKEN RIDGE, Original. Unlocked');
  const nextTrack = page.getByRole('button', {
    name: 'Next track',
    exact: true,
  });
  await nextTrack.focus();
  await nextTrack.press('Enter');
  await expect(page.locator('#game-status')).toContainText(
    'SUNMEADOW RUN. Locked. Beat BRACKEN RIDGE',
  );
  const start = page.getByRole('button', {
    name: 'Start race',
    exact: true,
  });
  await expect(start).toHaveAttribute('aria-disabled', 'true');
  await start.focus();
  await start.press('Enter');
  await expect(page.locator('#game')).toHaveAttribute('data-mode', '2');
  const previous = page.getByRole('button', {
    name: 'Previous track',
    exact: true,
  });
  await previous.focus();
  await previous.press('Enter');
  await start.focus();
  await start.press('Enter');
  await expect(page.locator('#game')).toHaveAttribute('data-mode', '3');
  await expect(page.locator('#game')).toBeFocused();
});
test('leaving the game pauses and clears held throttle', async ({ page: page }) => {
  await play(page);
  await press(page, 'Enter');
  await press(page, 'Enter');
  await press(page, 'Enter');
  await expect(page.locator('#game')).toHaveAttribute('data-mode', '4', {
    timeout: 6000,
  });
  await page.keyboard.down('ArrowUp');
  await page
    .getByRole('link', {
      name: 'GitHub',
      exact: true,
    })
    .focus();
  await expect(page.locator('#game')).toHaveAttribute('data-mode', '5');
  await page.keyboard.up('ArrowUp');
  await press(page, 'p');
  await expect(page.locator('#game')).toHaveAttribute('data-mode', '4');
});
test('static discovery metadata and download survive disabled JavaScript', async ({
  browser: browser,
}) => {
  const context = await browser.newContext({
    javaScriptEnabled: false,
  });
  const page = await context.newPage();
  await page.goto('/');
  await expect(page).toHaveTitle(/Gravelbyte.*PicoSystem/);
  await expect(page.locator('link[rel="canonical"]')).toHaveAttribute(
    'href',
    'https://frostney.github.io/gravelbyte/',
  );
  await expect(page.locator('.no-script')).toContainText('Enable JavaScript');
  await expect(
    page.getByRole('link', {
      name: 'Download for PicoSystem',
    }),
  ).toBeVisible();
  await expect(
    page.getByRole('heading', {
      name: 'Gravelbyte',
      exact: true,
      level: 1,
    }),
  ).toHaveCount(1);
  await context.close();
});
test('audio initializes and survives mute toggles', async ({ page: page }) => {
  await page.addInitScript(() => {
    const nativeAudioContext = window.AudioContext;
    window.gravelbyteAudioProbe = {
      contexts: 0,
      oscillators: 0,
    };
    window.AudioContext = class extends nativeAudioContext {
      constructor(...callArguments) {
        super(...callArguments);
        window.gravelbyteAudioProbe.contexts++;
      }
      createOscillator() {
        window.gravelbyteAudioProbe.oscillators++;
        return super.createOscillator();
      }
    };
  });
  await play(page);
  await expect
    .poll(() => page.evaluate(() => window.gravelbyteAudioProbe))
    .toEqual({
      contexts: 1,
      oscillators: 1,
    });
  await page
    .getByRole('button', {
      name: 'Mute',
      exact: true,
    })
    .click();
  await page
    .getByRole('button', {
      name: 'Unmute',
      exact: true,
    })
    .click();
  await expect(
    page.getByRole('button', {
      name: 'Mute',
      exact: true,
    }),
  ).toBeVisible();
  expect(await page.evaluate(() => window.gravelbyteAudioProbe)).toEqual({
    contexts: 1,
    oscillators: 1,
  });
});

test('assist and pace notes are accessible and survive reload', async ({ page }) => {
  await play(page);
  const activate = async (button) => {
    await button.focus();
    await button.press('Enter');
  };
  await activate(page.getByRole('button', { name: 'Choose car', exact: true }));
  await expect(page.locator('#game-status')).toContainText('Steering assist off');
  await activate(page.getByRole('button', { name: 'Toggle steering assist' }));
  await expect(page.locator('#game-status')).toContainText('Steering assist on');
  await activate(page.getByRole('button', { name: 'Choose track', exact: true }));
  await activate(page.getByRole('button', { name: 'Start race', exact: true }));
  await expect(page.locator('#game')).toHaveAttribute('data-mode', '3');
  await press(page, 'p');
  const next = page.getByRole('button', { name: 'Next choice', exact: true });
  await activate(next);
  await activate(next);
  await activate(next);
  await expect(page.locator('#game-status')).toContainText('OPTIONS');
  await activate(page.getByRole('button', { name: 'Select option', exact: true }));
  await activate(next);
  await activate(next);
  await expect(page.locator('#game-status')).toContainText('PACE NOTES ON');
  await activate(page.getByRole('button', { name: 'Select option', exact: true }));
  await expect(page.locator('#game-status')).toContainText('PACE NOTES OFF');
  await page.reload();
  await activate(page.getByRole('button', { name: 'Play Gravelbyte', exact: true }));
  await press(page, 'Enter');
  await expect(page.locator('#game-status')).toContainText('Steering assist on');
  await press(page, 'Enter');
  await press(page, 'Enter');
  await press(page, 'p');
  await press(page, 'ArrowDown');
  await press(page, 'ArrowDown');
  await press(page, 'ArrowDown');
  await press(page, 'Enter');
  await press(page, 'ArrowDown');
  await press(page, 'ArrowDown');
  await expect(page.locator('#game-status')).toContainText('PACE NOTES OFF');
});

test('shared challenge seed can be edited, randomized and reused after car changes', async ({
  page,
}) => {
  await page.goto('/?seed=00000000');
  await page.getByRole('button', { name: 'Play Gravelbyte', exact: true }).click();
  await expect(page.locator('#game-status')).toContainText('Seed 00000000');
  await press(page, 'ArrowRight');
  await expect(page.locator('#game-status')).toContainText('Edit seed 00000000. Digit 1');
  await press(page, 'ArrowUp');
  await expect(page.locator('#game-status')).toContainText('Edit seed 10000000');
  await press(page, 'Enter');
  await expect(page).toHaveURL(/seed=10000000/);
  await press(page, 'Escape');
  await press(page, 'ArrowRight');
  await press(page, 'Enter');
  await expect(page.locator('#game-status')).toContainText('Seed 10000000');
  await press(page, 'Space');
  await expect(page).not.toHaveURL(/seed=10000000/);
  const sharedUrl = page.url();
  await page.reload();
  await page.getByRole('button', { name: 'Play Gravelbyte', exact: true }).click();
  expect(page.url()).toBe(sharedUrl);
  await expect(page.locator('#game-status')).toContainText('Random challenge');
  await press(page, 'Enter');
  await expect(page.locator('#game')).toHaveAttribute('data-mode', '3');
});
test('invalid seed and unavailable replay database leave racing playable', async ({ page }) => {
  await page.addInitScript(() => {
    indexedDB.open = () => {
      throw new Error('Storage denied');
    };
  });
  await page.goto('/?seed=not-a-seed');
  await expect(page.locator('#save-status')).toContainText('Invalid seed');
  await page.getByRole('button', { name: 'Play Gravelbyte', exact: true }).click();
  await press(page, 'Enter');
  await press(page, 'Enter');
  await press(page, 'Enter');
  await expect(page.locator('#game')).toHaveAttribute('data-mode', '3');
  await expect(page.locator('#save-status')).toContainText('Replay storage is unavailable');
});
test('practice restart is explicitly marked and full restart restores eligibility', async ({
  page,
}) => {
  await play(page);
  await press(page, 'Enter');
  await press(page, 'Enter');
  await press(page, 'Enter');
  await press(page, 'p');
  await press(page, 'ArrowDown');
  await expect(page.locator('#game-status')).toContainText('RETRY LAST SPLIT');
  await press(page, 'Enter');
  await expect(page.locator('#game-status')).toContainText('Practice. No records or medals.');
  await press(page, 'p');
  await press(page, 'ArrowDown');
  await press(page, 'ArrowDown');
  await expect(page.locator('#game-status')).toContainText('RESTART');
  await press(page, 'Enter');
  await expect(page.locator('#game')).toHaveAttribute('data-mode', '3');
  await expect(page.locator('#game-status')).not.toContainText('Practice');
});

test('persisted personal-best ghost reloads and rejects a corrupt replacement', async ({
  page,
}) => {
  const { readFileSync } = await import('node:fs');
  const saveBytes = [...readFileSync('tests/fixtures/best-records-v4.bin')];
  const ghostBytes = [...readFileSync('tests/fixtures/best-ghost-v1.bin')];
  await page.goto('/');
  await page.evaluate(
    async ({ saveBytes, ghostBytes }) => {
      localStorage.setItem('gravelbyte.records.v4', JSON.stringify(saveBytes));
      await new Promise((resolve, reject) => {
        const request = indexedDB.open('gravelbyte-replays-v4', 1);
        request.onupgradeneeded = () => request.result.createObjectStore('ghosts');
        request.onerror = () => reject(request.error);
        request.onsuccess = () => {
          const database = request.result;
          const transaction = database.transaction('ghosts', 'readwrite');
          transaction.objectStore('ghosts').put(new Uint8Array(ghostBytes), 0);
          transaction.oncomplete = () => {
            database.close();
            resolve();
          };
          transaction.onerror = () => reject(transaction.error);
        };
      });
    },
    { saveBytes, ghostBytes },
  );
  await page.reload();
  await page.getByRole('button', { name: 'Play Gravelbyte', exact: true }).click();
  await press(page, 'Enter');
  await press(page, 'Enter');
  await expect(page.locator('#game-status')).toContainText('Personal best ghost available');
  await page.reload();
  await page.getByRole('button', { name: 'Play Gravelbyte', exact: true }).click();
  await press(page, 'Enter');
  await press(page, 'Enter');
  await expect(page.locator('#game-status')).toContainText('Personal best ghost available');
  await page.evaluate(async () => {
    await new Promise((resolve, reject) => {
      const request = indexedDB.open('gravelbyte-replays-v4', 1);
      request.onsuccess = () => {
        const database = request.result;
        const transaction = database.transaction('ghosts', 'readwrite');
        const store = transaction.objectStore('ghosts');
        const stored = store.get(0);
        stored.onsuccess = () => {
          const bytes = stored.result;
          bytes[300] ^= 1;
          store.put(bytes, 0);
        };
        transaction.oncomplete = () => {
          database.close();
          resolve();
        };
        transaction.onerror = () => reject(transaction.error);
      };
    });
  });
  await page.reload();
  await page.getByRole('button', { name: 'Play Gravelbyte', exact: true }).click();
  await press(page, 'Enter');
  await press(page, 'Enter');
  await expect(page.locator('#game-status')).toContainText('Personal best ghost unavailable');
  await press(page, 'Enter');
  await expect(page.locator('#game')).toHaveAttribute('data-mode', '4', { timeout: 6000 });
});

for (const seed of ['80000000', 'ffffffff']) {
  test(`unsigned seed ${seed} survives URL and reload`, async ({ page }) => {
    await page.goto(`/?seed=${seed}`);
    await page.getByRole('button', { name: 'Play Gravelbyte', exact: true }).click();
    await expect(page.locator('#game-status')).toContainText(`Seed ${seed.toUpperCase()}`);
    await expect(page).toHaveURL(new RegExp(`seed=${seed}$`));
    await page.reload();
    await page.getByRole('button', { name: 'Play Gravelbyte', exact: true }).click();
    await expect(page.locator('#game-status')).toContainText(`Seed ${seed.toUpperCase()}`);
  });
}

test('touch challenge seed editor fits and uses shared controls', async ({ browser }) => {
  const context = await browser.newContext({
    viewport: { width: 390, height: 844 },
    hasTouch: true,
    isMobile: true,
  });
  const page = await context.newPage();
  await page.goto('/?seed=00000000');
  await page.getByRole('button', { name: 'Play Gravelbyte', exact: true }).tap();
  await page.locator('#variant-right').tap();
  await expect(page.locator('#game-status')).toContainText('Edit seed');
  await page.locator('#touch [data-bit="1"]').tap();
  await expect(page.locator('#game-status')).toContainText('Edit seed 10000000');
  for (const selector of ['#variant-left', '#variant-right', '#touch [data-bit="32"]']) {
    const bounds = await page.locator(selector).boundingBox();
    expect(bounds.y).toBeGreaterThanOrEqual(0);
    expect(bounds.y + bounds.height).toBeLessThanOrEqual(844);
  }
  await page.locator('#touch [data-bit="32"]').tap();
  await expect(page).toHaveURL(/seed=10000000/);
  await page.locator('#touch [data-bit="32"]').tap();
  await expect(page.locator('#game')).toHaveAttribute('data-mode', '4', { timeout: 6000 });
  await context.close();
});

test('a complete driven best saves its ghost through the browser and reloads it', async ({
  page,
}) => {
  test.setTimeout(90000);
  const { readFileSync } = await import('node:fs');
  const inputs = [...readFileSync('tests/fixtures/finch-driving-inputs.bin')];
  // Control animation time and send public keyboard events; no game-state edits.
  await page.addInitScript(() => {
    let queuedFrames = [];
    let timestamp = 1000;
    let previousButtons = 0;
    window.requestAnimationFrame = (callback) => {
      queuedFrames.push(callback);
      return queuedFrames.length;
    };
    window.gravelbyteAdvanceFrame = (buttons = 0, command = '') => {
      const canvas = document.querySelector('#game');
      for (const [bit, code] of [
        [1, 'ArrowLeft'],
        [2, 'ArrowRight'],
        [4, 'ArrowUp'],
        [8, 'ArrowDown'],
      ]) {
        if ((buttons & bit) !== (previousButtons & bit))
          canvas.dispatchEvent(
            new KeyboardEvent(buttons & bit ? 'keydown' : 'keyup', {
              code,
              key: code,
              bubbles: true,
            }),
          );
      }
      previousButtons = buttons;
      if (command)
        canvas.dispatchEvent(
          new KeyboardEvent('keydown', { code: command, key: command, bubbles: true }),
        );
      const callbacks = queuedFrames;
      queuedFrames = [];
      timestamp += 20;
      for (const callback of callbacks) callback(timestamp);
      if (command)
        canvas.dispatchEvent(
          new KeyboardEvent('keyup', { code: command, key: command, bubbles: true }),
        );
    };
  });
  await play(page);
  const advance = async (command = '') =>
    page.evaluate((command) => window.gravelbyteAdvanceFrame(0, command), command);
  await advance();
  await advance('Enter');
  await advance('ArrowLeft');
  await advance();
  await advance('Enter');
  await advance();
  await advance('Enter');
  await page.evaluate(() => {
    for (
      let frame = 0;
      frame < 200 && document.querySelector('#game').dataset.mode === '3';
      frame++
    )
      window.gravelbyteAdvanceFrame();
  });
  await expect(page.locator('#game')).toHaveAttribute('data-mode', '4');
  for (let offset = 0; offset < inputs.length; offset += 200)
    await page.evaluate(
      (chunk) => {
        for (const buttons of chunk) window.gravelbyteAdvanceFrame(buttons);
      },
      inputs.slice(offset, offset + 200),
    );
  await page.evaluate(() => {
    for (
      let frame = 0;
      frame < 100 && document.querySelector('#game').dataset.mode === '4';
      frame++
    )
      window.gravelbyteAdvanceFrame(4);
  });
  await expect(page.locator('#game')).toHaveAttribute('data-mode', '6');
  await expect(page.locator('#game-status')).toContainText('Target beaten');
  await expect
    .poll(() =>
      page.evaluate(async () => {
        return new Promise((resolve, reject) => {
          const request = indexedDB.open('gravelbyte-replays-v4', 1);
          request.onerror = () => reject(request.error);
          request.onsuccess = () => {
            const database = request.result;
            const stored = database.transaction('ghosts').objectStore('ghosts').get(0);
            stored.onsuccess = () => {
              database.close();
              resolve(stored.result instanceof Uint8Array ? stored.result.length : 0);
            };
            stored.onerror = () => reject(stored.error);
          };
        });
      }),
    )
    .toBe(12556);
  await page.reload();
  await page.getByRole('button', { name: 'Play Gravelbyte', exact: true }).click();
  await advance();
  await advance('Enter');
  await advance();
  await advance('Enter');
  await expect
    .poll(async () => {
      await advance();
      return page.locator('#game-status').textContent();
    })
    .toContain('Personal best ghost available');
});
