const { execSync } = require('child_process');

function run(command) {
  execSync(command, {
    stdio: 'inherit',
    shell: '/bin/sh'
  });
}

function showVtestLogs() {
  const command = [
    'for folder in ${TMPDIR:-/tmp}/haregtests-*/vtc.*; do',
    '  if [ ! -f "$folder/INFO" ] && [ ! -f "$folder/LOG" ]; then',
    '    continue',
    '  fi',
    '  printf "::group::"',
    '  [ -f "$folder/INFO" ] && cat "$folder/INFO"',
    '  [ -f "$folder/LOG" ] && cat "$folder/LOG"',
    '  echo "::endgroup::"',
    'done'
  ].join('\n');

  run(command);
}

function showCoreDumps() {
  const command = [
    'HAPROXY_BIN=./haproxy',
    'for file in /tmp/core/core.* /tmp/core.*; do',
    '  [ -f "$file" ] || continue',
    '  printf "::group::"',
    '  gdb -ex "thread apply all bt full" "$HAPROXY_BIN" "$file" || true',
    '  echo "::endgroup::"',
    'done'
  ].join('\n');

  run(command);
}

function main() {
  if (process.platform === 'win32') {
    return;
  }

  showVtestLogs();
  showCoreDumps();
}

try {
  main();
} catch (error) {
  const message = error && error.message ? error.message : String(error);
  process.stdout.write(`::warning::vtest post step could not show VTest logs: ${message}\n`);
}
