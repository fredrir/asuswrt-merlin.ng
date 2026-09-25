const assert = require('node:assert/strict');
const { chromium } = require('playwright');

const baseURL = process.env.RTBE90U_TEST_URL || 'http://127.0.0.1:18090';
assert.equal(new URL(baseURL).origin, baseURL, 'Use an origin without a path');
assert.equal(new URL(baseURL).hostname, '127.0.0.1', 'Use the isolated localhost fixture');
const paths = [
  'Advanced_VPNDirector.asp', 'Advanced_OpenVPNClient_Content.asp',
  'Advanced_VPN_OpenVPN.asp', 'Advanced_WireguardClient_Content.asp',
  'Advanced_WireguardServer_Content.asp', 'Advanced_VPNStatus.asp'
];

(async () => {
  const browser = await chromium.launch({ headless: true });
  try {
    const context = await browser.newContext({ baseURL });
    context.setDefaultTimeout(10000);
    context.setDefaultNavigationTimeout(30000);
    await context.route('**/*', route => {
      if (new URL(route.request().url()).origin === baseURL) return route.continue();
      return route.abort();
    });
    const marker = await context.request.get('/Main_Login.asp');
    assert((await marker.text()).includes('RTBE90U_HTTPD_FIXTURE'), 'Refusing to modify a non-fixture server');
    const login = await context.request.post('/login.cgi', {
      headers: { Referer: baseURL + '/Main_Login.asp' },
      form: {
        login_authorization: Buffer.from('admin:rtbe90u-fixture').toString('base64'),
        auth_version: '1', next_page: paths[0]
      }
    });
    assert.equal(login.status(), 200);
    assert((await context.cookies()).some(cookie => cookie.name === 'asus_token'), 'Fixture login failed');

    const page = await context.newPage();
    const errors = [];
    let expectedConfirmation = false;
    page.on('pageerror', error => errors.push(error.stack));
    page.on('response', response => {
      if (response.status() >= 400) errors.push(`${response.status()} ${response.url()}`);
    });
    page.on('dialog', async dialog => {
      if (expectedConfirmation && dialog.type() === 'confirm') {
        expectedConfirmation = false;
        await dialog.accept();
      } else {
        errors.push('Unexpected dialog: ' + dialog.message());
        await dialog.dismiss();
      }
    });
    const healthy = () => assert.deepEqual(errors, [], 'Browser errors');
    async function open(path) {
      await page.goto('/' + path);
      await page.waitForFunction(() => document.querySelector('#tabMenu').children.length > 0);
      assert.equal(new URL(page.url()).pathname, '/' + path, 'Unexpected setup/login redirect');
      healthy();
    }
    async function apply() {
      try {
        const [response] = await Promise.all([
          page.waitForResponse(response => response.url().endsWith('/start_apply.htm'), { timeout: 10000 }),
          page.getByRole('button', { name: 'Apply', exact: true }).click()
        ]);
        assert.equal(response.status(), 200);
        await response.finished();
      } finally {
        healthy();
      }
      await page.reload();
      healthy();
    }
    const field = name => page.locator(`form[name="form"] [name="${name}"]`);
    async function selectClient(unit) {
      if (await field('vpn_client_unit').inputValue() === unit) return;
      await Promise.all([
        page.waitForNavigation({ waitUntil: 'load' }),
        field('vpn_client_unit').selectOption(unit)
      ]);
      await value('vpn_client_unit', unit);
      healthy();
    }
    async function value(name, expected) {
      assert.equal(await field(name).inputValue(), expected, name + ' did not survive reload');
    }

    for (const path of paths) {
      await open(path);
      for (const tab of paths) {
        assert(await page.locator(`#tabMenu [title="${tab}"]`).isVisible(), 'Missing VPN tab: ' + tab);
      }
      console.log('PASS render and navigation: ' + path);
    }

    await open(paths[0]);
    while (await page.locator('.remove_btn').count()) {
      expectedConfirmation = true;
      await page.locator('.remove_btn').first().click();
    }
    await page.locator('.add_btn').click();
    await page.locator('#desc_x').fill('fixture-route');
    await page.locator('#localIP_x').fill('192.0.2.0/24');
    await page.locator('#remoteIP_x').fill('198.51.100.0/24');
    await page.locator('#iface_x').selectOption('OVPN1');
    await page.locator('#saveRule').click();
    await apply();
    const row = page.locator('[row_tr_idx]').filter({ hasText: 'fixture-route' });
    assert.equal(await row.count(), 1);
    assert((await row.innerText()).includes('192.0.2.0/24'));
    await row.locator('img[title="Enabled"]').hover();
    await row.locator('img[title="Enabled"]').click();
    await row.locator('img[title="Disabled"]').hover();
    await row.locator('.edit_btn').click();
    await page.locator('#remoteIP_x').fill('203.0.113.0/24');
    await page.locator('#saveRule').click();
    await apply();
    assert.equal(await row.locator('img[title="Disabled"]').count(), 1);
    assert((await row.innerText()).includes('203.0.113.0/24'));
    expectedConfirmation = true;
    await row.locator('.remove_btn').click();
    await apply();
    assert.equal(await page.locator('[row_tr_idx]').count(), 0);
    console.log('PASS VPN Director create, disable, edit, delete and reload');

    const password = 'p'.repeat(128);
    const custom = 'setenv TEST "quoted & value"\n# fixture custom configuration\n';
    await open(paths[1]);
    await selectClient('1');
    await field('vpn_client_addr').fill('vpn.example.test');
    await field('vpn_client_desc').fill('Fixture Client 1');
    await page.locator('[name="vpn_client_userauth"][value="1"]').check();
    await field('vpn_client_username').fill('fixture-user');
    await field('vpn_client_password').fill(password);
    await field('vpn_client_custom3').fill(custom);
    await field('vpn_client_ncp_ciphers').fill('AES-256-GCM:AES-128-GCM');
    await apply();
    await value('vpn_client_addr', 'vpn.example.test');
    await value('vpn_client_desc', 'Fixture Client 1');
    await value('vpn_client_password', password);
    await value('vpn_client_custom3', custom);
    await value('vpn_client_ncp_ciphers', 'AES-256-GCM:AES-128-GCM');
    await selectClient('2');
    await field('vpn_client_desc').fill('Fixture Client 2');
    await field('vpn_client_custom3').fill('# second client\n');
    await apply();
    await value('vpn_client_desc', 'Fixture Client 2');
    await value('vpn_client_custom3', '# second client\n');
    await selectClient('1');
    await value('vpn_client_desc', 'Fixture Client 1');
    await value('vpn_client_password', password);
    await value('vpn_client_custom3', custom);
    await field('vpn_client_custom3').fill('');
    await apply();
    await value('vpn_client_custom3', '');
    console.log('PASS OpenVPN client credentials, ciphers, profile isolation and custom save/clear');

    await open(paths[2]);
    if (!(await page.locator('#selSwitchMode').isVisible())) await page.locator('#radio_VPNServer_enable').click();
    await page.locator('#selSwitchMode').selectOption('2');
    await field('vpn_server_port').fill('1195');
    await field('vpn_server_custom3').fill(custom);
    await apply();
    await value('vpn_server_port', '1195');
    await value('vpn_server_custom3', custom);
    if (!(await page.locator('#selSwitchMode').isVisible())) await page.locator('#radio_VPNServer_enable').click();
    await page.locator('#selSwitchMode').selectOption('2');
    await field('vpn_server_custom3').fill('');
    await apply();
    await value('vpn_server_custom3', '');
    console.log('PASS OpenVPN server port and custom save/clear');

    await open(paths[3]);
    await page.locator('[name="wgc_enable"][value="0"]').check();
    await field('wgc_desc').fill('Fixture WG client');
    await page.locator('[name="wgc_enforce"][value="1"]').check();
    await apply();
    await value('wgc_desc', 'Fixture WG client');
    assert(await page.locator('[name="wgc_enforce"][value="1"]').isChecked());
    console.log('PASS WireGuard client description and enforcement setting');

    await open(paths[4]);
    await field('wgs_port').fill('51821');
    await apply();
    await value('wgs_port', '51821');
    console.log('PASS WireGuard server port');
    healthy();
  } finally {
    await browser.close();
  }
})().catch(error => { console.error(error); process.exitCode = 1; });
