import { defineConfig } from '@playwright/test';
export default defineConfig({
  testDir: 'tests/web',
  timeout: 30000,
  workers: 1,
  use: { baseURL: 'http://127.0.0.1:8174', trace: 'retain-on-failure' },
  webServer: {
    command: 'python3 -m http.server 8174 --directory build-web/site',
    url: 'http://127.0.0.1:8174',
    reuseExistingServer: false,
  },
});
