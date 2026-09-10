import { test as Test, expect as Expect } from '@playwright/test';
async function Play(Page) {
  await Page.goto('/');
  await Page.getByRole('button', {
    name: 'Play Gravelbyte',
    exact: true,
  }).click();
}
async function Press(Page, Key) {
  await Page.locator('#game').press(Key);
  await Page.waitForTimeout(50);
}
Test('keyboard menus, pause, records selections and reload', async ({ page: Page }) => {
  const Errors = [];
  Page.on('pageerror', (Event) => Errors.push(Event.message));
  await Play(Page);
  const Game = Page.locator('#game');
  await Press(Page, 'Enter');
  await Expect(Game).toHaveAttribute('data-mode', '1');
  await Press(Page, 'ArrowRight');
  await Expect(Game).toHaveAttribute('data-car', '2');
  await Press(Page, 'Enter');
  await Expect(Game).toHaveAttribute('data-mode', '2');
  await Press(Page, 'ArrowRight');
  await Press(Page, 'ArrowRight');
  await Expect(Game).toHaveAttribute('data-track', '2');
  await Press(Page, 'Enter');
  await Expect(Game).toHaveAttribute('data-mode', '2');
  await Press(Page, 'ArrowRight');
  await Expect(Game).toHaveAttribute('data-track', '0');
  await Press(Page, 'Enter');
  await Expect(Game).toHaveAttribute('data-mode', '3');
  await Expect(Game).toHaveAttribute('data-mode', '4', {
    timeout: 6000,
  });
  await Page.keyboard.down('ArrowUp');
  await Page.waitForTimeout(250);
  await Page.keyboard.up('ArrowUp');
  await Press(Page, 'p');
  await Expect(Game).toHaveAttribute('data-mode', '5');
  await Press(Page, 'p');
  await Expect(Game).toHaveAttribute('data-mode', '4');
  await Page.getByRole('button', {
    name: 'Mute',
    exact: true,
  }).click();
  await Expect(Game).toHaveAttribute('data-mode', '4');
  await Page.reload();
  await Page.getByRole('button', {
    name: 'Play Gravelbyte',
    exact: true,
  }).click();
  await Press(Page, 'Enter');
  await Expect(Game).toHaveAttribute('data-car', '2');
  await Expect(Game).toHaveAttribute('data-track', '0');
  await Expect(
    Page.getByRole('button', {
      name: 'Unmute',
      exact: true,
    }),
  ).toBeVisible();
  await Expect(Page.locator('#touch')).toBeHidden();
  await Expect(Page.locator('kbd[aria-label="Enter"]')).toBeVisible();
  Expect(Errors).toEqual([]);
});
Test(
  'touch input, simultaneous steering/gas, cancellation and keyboard switching',
  async ({ browser: Browser }) => {
    const Context = await Browser.newContext({
      viewport: {
        width: 390,
        height: 844,
      },
      hasTouch: true,
      isMobile: true,
    });
    const Page = await Context.newPage();
    await Play(Page);
    const Game = Page.locator('#game');
    await Expect(Page.locator('#touch')).toBeVisible();
    const Go = Page.getByRole('button', {
      name: 'Go',
      exact: true,
    });
    await Go.tap();
    await Expect(Game).toHaveAttribute('data-mode', '1');
    await Page.getByRole('button', {
      name: 'Steer right',
    }).tap();
    await Expect(Game).toHaveAttribute('data-car', '2');
    await Go.tap();
    await Expect(Game).toHaveAttribute('data-mode', '2');
    await Go.tap();
    await Expect(Game).toHaveAttribute('data-mode', '4', {
      timeout: 6000,
    });
    const Gas = await Page.getByRole('button', {
      name: 'Gas',
      exact: true,
    }).boundingBox();
    const Left = await Page.getByRole('button', {
      name: 'Steer left',
    }).boundingBox();
    const Cdp = await Context.newCDPSession(Page);
    await Cdp.send('Input.dispatchTouchEvent', {
      type: 'touchStart',
      touchPoints: [
        {
          x: Gas.x + Gas.width / 2,
          y: Gas.y + Gas.height / 2,
          id: 10,
        },
        {
          x: Left.x + Left.width / 2,
          y: Left.y + Left.height / 2,
          id: 11,
        },
      ],
    });
    await Expect(Page.locator('#touch .pressed')).toHaveCount(2);
    await Cdp.send('Input.dispatchTouchEvent', {
      type: 'touchCancel',
      touchPoints: [],
    });
    await Expect(Page.locator('#touch .pressed')).toHaveCount(0);
    await Press(Page, 'p');
    await Expect(Page.locator('#touch')).toBeHidden();
    await Expect(Page.locator('#input-label')).toHaveText('keyboard');
    Expect(await Page.evaluate(() => document.documentElement.scrollWidth <= innerWidth)).toBe(
      true,
    );
    await Context.close();
  },
);
Test('gamepad activation and storage denied fallback', async ({ page: Page }) => {
  await Page.addInitScript(() => {
    window.TestGamepad = {
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
    navigator.getGamepads = () => [window.TestGamepad];
    Storage.prototype.setItem = () => {
      throw new Error('Storage denied');
    };
  });
  await Play(Page);
  const Game = Page.locator('#game');
  const Button = async (ButtonIndex) => {
    await Page.evaluate((ButtonIndex) => {
      window.TestGamepad.buttons[ButtonIndex].pressed = true;
    }, ButtonIndex);
    await Page.waitForTimeout(80);
    await Page.evaluate((ButtonIndex) => {
      window.TestGamepad.buttons[ButtonIndex].pressed = false;
    }, ButtonIndex);
    await Page.waitForTimeout(80);
  };
  await Button(0);
  await Expect(Game).toHaveAttribute('data-mode', '1');
  await Expect(Page.locator('#input-label')).toHaveText('gamepad');
  await Expect(Page.locator('#controls')).toContainText('RT / A gas');
  await Button(0);
  await Button(0);
  await Expect(Page.locator('#save-status')).toContainText('session only');
  await Button(9);
  await Expect(Game).toHaveAttribute('data-mode', '5');
  await Button(1);
  await Expect(Game).toHaveAttribute('data-mode', '1');
});
for (const [Width, Height, Touch] of [
  [1366, 768, false],
  [1280, 720, false],
  [390, 844, true],
  [375, 667, true],
  [844, 390, true],
]) {
  Test(
    `game and instructions fit without scrolling at ${Width}x${Height}`,
    async ({ browser: Browser }) => {
      const Context = await Browser.newContext({
        viewport: {
          width: Width,
          height: Height,
        },
        hasTouch: Touch,
        isMobile: Touch,
      });
      const Page = await Context.newPage();
      await Play(Page);
      for (const Selector of ['#game', '.links', '#controls', '.install']) {
        await Expect(Page.locator(Selector)).toBeInViewport({
          ratio: 1,
        });
      }
      Expect(
        await Page.evaluate(() => ({
          horizontal: document.documentElement.scrollWidth > innerWidth,
          vertical: document.documentElement.scrollHeight > innerHeight,
        })),
      ).toEqual({
        horizontal: false,
        vertical: false,
      });
      const Game = await Page.locator('#game').boundingBox();
      Expect(Game.width).toBeGreaterThanOrEqual(280);
      Expect(Game.height).toBeCloseTo(Game.width);
      await Expect(
        Page.getByRole('link', {
          name: 'GitHub',
        }),
      ).toHaveAttribute('href', 'https://github.com/frostney/gravelbyte');
      await Expect(
        Page.getByRole('link', {
          name: 'Download for PicoSystem',
        }),
      ).toHaveAttribute('href', /assets\/[0-9a-f]{40}\/gravelbyte\.uf2$/);
      await Context.close();
    },
  );
}
for (const Asset of ['gravelbyte.js', 'gravelbyte.wasm', 'app.js']) {
  Test(`missing ${Asset} reports a load error instead of hanging`, async ({ page: Page }) => {
    await Page.route(`**/${Asset}`, (Route) =>
      Route.fulfill({
        status: 404,
        body: 'missing',
      }),
    );
    await Page.goto('/');
    await Expect(Page.locator('#load-status')).toContainText('could not load');
    await Expect(Page.locator('#play')).toBeDisabled();
  });
}
Test('mixed release executable is rejected before play', async ({ page: Page }) => {
  await Page.route('**/', async (Route) => {
    const Response = await Route.fetch();
    const Html = (await Response.text()).replace(
      /(name="gravelbyte-build" content=")[0-9a-f]{40}/,
      '$1' + '0'.repeat(40),
    );
    await Route.fulfill({
      response: Response,
      body: Html,
    });
  });
  await Page.goto('/');
  await Expect(Page.locator('#load-status')).toContainText('different builds');
  await Expect(Page.locator('#play')).toBeDisabled();
});
Test('corrupt and unavailable storage preserve playable controls', async ({ page: Page }) => {
  await Page.addInitScript(() => localStorage.setItem('gravelbyte.records.v1', '[1,2,3]'));
  await Play(Page);
  await Expect(Page.locator('#save-status')).toContainText('could not be restored');
  await Press(Page, 'Enter');
  await Expect(Page.locator('#game-status')).toContainText('Choose car');
});
Test(
  'accessible menus announce choices, refuse locked stages and start an unlocked race',
  async ({ page: Page }) => {
    await Play(Page);
    const ChooseCar = Page.getByRole('button', {
      name: 'Choose car',
      exact: true,
    });
    await ChooseCar.focus();
    await ChooseCar.press('Enter');
    await Expect(Page.locator('#game-status')).toContainText('KESTREL GT, STANDARD');
    const Next = Page.getByRole('button', {
      name: 'Next car',
      exact: true,
    });
    await Next.focus();
    await Next.press('Enter');
    await Expect(Page.locator('#game-status')).toContainText('GOSHAWK TURBO, EXPERT');
    const Choose = Page.getByRole('button', {
      name: 'Choose track',
      exact: true,
    });
    await Choose.focus();
    await Choose.press('Enter');
    await Expect(Page.locator('#game-status')).toContainText('BRACKEN RIDGE. Unlocked');
    const NextTrack = Page.getByRole('button', {
      name: 'Next track',
      exact: true,
    });
    await NextTrack.focus();
    await NextTrack.press('Enter');
    await Expect(Page.locator('#game-status')).toContainText(
      'SUNMEADOW RUN. Locked. Beat BRACKEN RIDGE',
    );
    const Start = Page.getByRole('button', {
      name: 'Start race',
      exact: true,
    });
    await Expect(Start).toHaveAttribute('aria-disabled', 'true');
    await Start.focus();
    await Start.press('Enter');
    await Expect(Page.locator('#game')).toHaveAttribute('data-mode', '2');
    const Previous = Page.getByRole('button', {
      name: 'Previous track',
      exact: true,
    });
    await Previous.focus();
    await Previous.press('Enter');
    await Start.focus();
    await Start.press('Enter');
    await Expect(Page.locator('#game')).toHaveAttribute('data-mode', '3');
    await Expect(Page.locator('#game')).toBeFocused();
  },
);
Test('leaving the game pauses and clears held throttle', async ({ page: Page }) => {
  await Play(Page);
  await Press(Page, 'Enter');
  await Press(Page, 'Enter');
  await Press(Page, 'Enter');
  await Expect(Page.locator('#game')).toHaveAttribute('data-mode', '4', {
    timeout: 6000,
  });
  await Page.keyboard.down('ArrowUp');
  await Page.getByRole('link', {
    name: 'GitHub',
    exact: true,
  }).focus();
  await Expect(Page.locator('#game')).toHaveAttribute('data-mode', '5');
  await Page.keyboard.up('ArrowUp');
  await Press(Page, 'p');
  await Expect(Page.locator('#game')).toHaveAttribute('data-mode', '4');
});
Test(
  'static discovery metadata and download survive disabled JavaScript',
  async ({ browser: Browser }) => {
    const Context = await Browser.newContext({
      javaScriptEnabled: false,
    });
    const Page = await Context.newPage();
    await Page.goto('/');
    await Expect(Page).toHaveTitle(/Gravelbyte.*PicoSystem/);
    await Expect(Page.locator('link[rel="canonical"]')).toHaveAttribute(
      'href',
      'https://frostney.github.io/gravelbyte/',
    );
    await Expect(Page.locator('.no-script')).toContainText('Enable JavaScript');
    await Expect(
      Page.getByRole('link', {
        name: 'Download for PicoSystem',
      }),
    ).toBeVisible();
    await Expect(
      Page.getByRole('heading', {
        name: 'Gravelbyte',
        exact: true,
        level: 1,
      }),
    ).toHaveCount(1);
    await Context.close();
  },
);

Test('audio initializes and survives mute toggles', async ({ page: Page }) => {
  await Page.addInitScript(() => {
    const NativeAudioContext = window.AudioContext;
    window.GravelbyteAudioProbe = { Contexts: 0, Oscillators: 0 };
    window.AudioContext = class extends NativeAudioContext {
      constructor(...Arguments) {
        super(...Arguments);
        window.GravelbyteAudioProbe.Contexts++;
      }
      createOscillator() {
        window.GravelbyteAudioProbe.Oscillators++;
        return super.createOscillator();
      }
    };
  });
  await Play(Page);
  await Expect.poll(() => Page.evaluate(() => window.GravelbyteAudioProbe)).toEqual({
    Contexts: 1,
    Oscillators: 1,
  });
  await Page.getByRole('button', { name: 'Mute', exact: true }).click();
  await Page.getByRole('button', { name: 'Unmute', exact: true }).click();
  await Expect(Page.getByRole('button', { name: 'Mute', exact: true })).toBeVisible();
  Expect(await Page.evaluate(() => window.GravelbyteAudioProbe)).toEqual({
    Contexts: 1,
    Oscillators: 1,
  });
});
