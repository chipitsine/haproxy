const { execSync } = require('child_process');
const fs = require('fs');
const path = require('path');

function run(command, options = {}) {
  const { ignoreError = false, cwd = process.cwd() } = options;

  try {
    execSync(command, {
      cwd,
      stdio: 'inherit',
      shell: '/bin/sh'
    });
  } catch (error) {
    if (!ignoreError) {
      throw error;
    }
  }
}

function setupCoreDumps() {
  if (process.platform !== 'linux') {
    return;
  }

  run('sudo mkdir -p /tmp/core');
  run('sudo sysctl fs.suid_dumpable=1');
  run('sudo sysctl kernel.core_pattern=/tmp/core/core.%h.%e.%t');
}

function setupUlimit() {
  if (process.platform === 'win32') {
    return;
  }

  run('ulimit -n 65536; ulimit -c unlimited');
}

function installVtest(workspace) {
  const vtestBin = path.join(workspace, 'vtest', 'vtest');
  if (fs.existsSync(vtestBin)) {
    return;
  }

  run(`DESTDIR="${path.join(workspace, 'vtest')}" scripts/build-vtest.sh`, { cwd: workspace });
}

function addMatcher() {
  // This allows one to more easily see which tests fail.
  process.stdout.write('::add-matcher::.github/vtest.json\n');
}

function shellQuote(value) {
  return `'${String(value).replace(/'/g, `'"'"'`)}'`;
}

function runRegtests(workspace) {
  const vtestProgram = './vtest/vtest';
  const regtestsTypes = 'default,bug,devel';

  run(
    `make reg-tests VTEST_PROGRAM=${shellQuote(vtestProgram)} REGTESTS_TYPES=${shellQuote(regtestsTypes)}`,
    { cwd: workspace }
  );
}

function main() {
  const workspace = process.env.GITHUB_WORKSPACE || process.cwd();

  setupCoreDumps();
  setupUlimit();
  installVtest(workspace);
  addMatcher();
  runRegtests(workspace);
}

try {
  main();
} catch (error) {
  const message = error && error.message ? error.message : String(error);
  process.stdout.write(`::error::vtest action failed: ${message}\n`);
  process.exit(1);
}
