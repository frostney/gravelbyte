import { test, expect } from '@playwright/test';
async function play(page) {
  await page.goto('/');
  await page.getByRole('button', { name: 'Play Gravelbyte', exact: true }).click();
}
async function press(page, key) {
  await page.locator('#game').press(key);
  await page.waitForTimeout(50);
}
test('keyboard menus, pause, records selections and reload', async ({ page }) => {
  const errors = [];
  page.on('pageerror', (e) => errors.push(e.message));
  await play(page);
  const game = page.locator('#game');
  await press(page, 'Enter');
  await expect(game).toHaveAttribute('data-mode', '1');
  await press(page, 'ArrowRight');
  await expect(game).toHaveAttribute('data-car', '2');
  await press(page, 'Enter');
  await expect(game).toHaveAttribute('data-mode', '2');
  await press(page, 'ArrowRight');
  await press(page, 'ArrowRight');
  await expect(game).toHaveAttribute('data-track', '2');
  await press(page, 'Enter');
  await expect(game).toHaveAttribute('data-mode', '2');
  await press(page, 'ArrowRight');
  await expect(game).toHaveAttribute('data-track', '0');
  await press(page, 'Enter');
  await expect(game).toHaveAttribute('data-mode', '3');
  await expect(game).toHaveAttribute('data-mode', '4', { timeout: 6000 });
  await page.keyboard.down('ArrowUp');
  await page.waitForTimeout(250);
  await page.keyboard.up('ArrowUp');
  await page.getByRole('button', { name: 'Pause', exact: true }).click();
  await expect(game).toHaveAttribute('data-mode', '5');
  await page.getByRole('button', { name: 'Resume', exact: true }).click();
  await expect(game).toHaveAttribute('data-mode', '4');
  await page.getByRole('button', { name: 'Mute', exact: true }).click();
  await expect(game).toHaveAttribute('data-mode', '4');
  await page.reload();
  await page.getByRole('button', { name: 'Play Gravelbyte', exact: true }).click();
  await press(page, 'Enter');
  await expect(game).toHaveAttribute('data-car', '2');
  await expect(game).toHaveAttribute('data-track', '0');
  await expect(page.getByRole('button', { name: 'Unmute', exact: true })).toBeVisible();
  await expect(page.locator('#touch')).toBeHidden();
  await expect(page.locator('#controls')).toContainText('Enter confirm');
  expect(errors).toEqual([]);
});
test('touch input, simultaneous steering/gas, cancellation and keyboard switching', async ({
  browser,
}) => {
  const context = await browser.newContext({
    viewport: { width: 390, height: 844 },
    hasTouch: true,
    isMobile: true,
  });
  const page = await context.newPage();
  await play(page);
  const game = page.locator('#game');
  await expect(page.locator('#touch')).toBeVisible();
  const go = page.getByRole('button', { name: 'Go', exact: true });
  await go.tap();
  await expect(game).toHaveAttribute('data-mode', '1');
  await page.getByRole('button', { name: 'Steer right' }).tap();
  await expect(game).toHaveAttribute('data-car', '2');
  await go.tap();
  await expect(game).toHaveAttribute('data-mode', '2');
  await go.tap();
  await expect(game).toHaveAttribute('data-mode', '4', { timeout: 6000 });
  const gas = await page.getByRole('button', { name: 'Gas', exact: true }).boundingBox();
  const left = await page.getByRole('button', { name: 'Steer left' }).boundingBox();
  const cdp = await context.newCDPSession(page);
  await cdp.send('Input.dispatchTouchEvent', {
    type: 'touchStart',
    touchPoints: [
      { x: gas.x + gas.width / 2, y: gas.y + gas.height / 2, id: 10 },
      { x: left.x + left.width / 2, y: left.y + left.height / 2, id: 11 },
    ],
  });
  await expect(page.locator('#touch .pressed')).toHaveCount(2);
  await cdp.send('Input.dispatchTouchEvent', { type: 'touchCancel', touchPoints: [] });
  await expect(page.locator('#touch .pressed')).toHaveCount(0);
  await press(page, 'p');
  await expect(page.locator('#touch')).toBeHidden();
  await expect(page.locator('#input-label')).toHaveText('keyboard');
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth)).toBe(true);
  await context.close();
});
test('gamepad activation and storage denied fallback', async ({ page }) => {
  await page.addInitScript(() => {
    window.testPad = {
      mapping: 'standard',
      axes: [0, 0],
      buttons: Array.from({ length: 16 }, () => ({ pressed: false, value: 0 })),
    };
    navigator.getGamepads = () => [window.testPad];
    Storage.prototype.setItem = () => {
      throw new Error('Storage denied');
    };
  });
  await play(page);
  const game = page.locator('#game');
  const button = async (n) => {
    await page.evaluate((n) => {
      window.testPad.buttons[n].pressed = true;
    }, n);
    await page.waitForTimeout(80);
    await page.evaluate((n) => {
      window.testPad.buttons[n].pressed = false;
    }, n);
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
  await button(1);
  await expect(game).toHaveAttribute('data-mode', '1');
});

for (const [width, height, touch] of [
  [1366, 768, false],
  [1280, 720, false],
  [390, 844, true],
  [375, 667, true],
  [844, 390, true],
]) {
  test(`game and instructions fit without scrolling at ${width}x${height}`, async ({ browser }) => {
    const context = await browser.newContext({
      viewport: { width, height },
      hasTouch: touch,
      isMobile: touch,
    });
    const page = await context.newPage();
    await play(page);
    for (const selector of ['#game', '.toolbar', '#controls', '#save-status', '.install']) {
      await expect(page.locator(selector)).toBeInViewport({ ratio: 1 });
    }
    expect(
      await page.evaluate(() => ({
        horizontal: document.documentElement.scrollWidth > innerWidth,
        vertical: document.documentElement.scrollHeight > innerHeight,
      })),
    ).toEqual({ horizontal: false, vertical: false });
    const game = await page.locator('#game').boundingBox();
    expect(game.width).toBeGreaterThanOrEqual(280);
    expect(game.height).toBeCloseTo(game.width);
    await expect(page.getByRole('link', { name: 'GitHub' })).toHaveAttribute(
      'href',
      'https://github.com/frostney/gravelbyte',
    );
    await expect(page.getByRole('link', { name: 'Download for PicoSystem' })).toHaveAttribute(
      'href',
      'gravelbyte.uf2',
    );
    await context.close();
  });
}
