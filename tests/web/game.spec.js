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
  await expect(page.locator('#game-status')).toContainText('BRACKEN RIDGE. Unlocked');
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
  await expect(page.locator('#game-status')).toContainText('OPTIONS');
  await activate(page.getByRole('button', { name: 'Select option', exact: true }));
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
  await press(page, 'Enter');
  await press(page, 'ArrowDown');
  await expect(page.locator('#game-status')).toContainText('PACE NOTES OFF');
});
