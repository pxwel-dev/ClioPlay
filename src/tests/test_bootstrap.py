"""Exercise the real shell startup ordering with isolated fake device services."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def executable(path, body):
    path.write_text('#!/bin/sh\n' + body)
    path.chmod(0o755)


class BootstrapTest(unittest.TestCase):
    def startup(self, focus_ready):
        with tempfile.TemporaryDirectory(prefix='clioplay-boot-test-') as folder:
            work = Path(folder)
            card = work/'clio843'
            card.mkdir()
            # Execute the actual Focus/vehicle/UI section. Never execute the
            # embedded ARM AUX payload or signal any host service.
            source = (ROOT/'card-template/ttn').read_text()
            section = source.split('# 3) Replace the Focus endpoint,', 1)[1]
            section = '# 3) Replace the Focus endpoint,' + section
            section = section.replace('/tmp/cfready', str(work/'ready'))
            section = section.replace('/bin/vehicledaemon', str(work/'vehicle'))
            executable(work/'killall', 'exit 0\n')
            executable(work/'sleep', 'exec /bin/sleep 0.02\n')
            if focus_ready:
                executable(card/'focusd', '''
while [ ! -e "$TEST_WORK/ui" ]; do /bin/sleep 0.005; done
echo FOCUS_READY >> "$TEST_WORK/events"
: > "$TEST_WORK/ready"
''')
            else:
                executable(card/'focusd', 'exit 1\n')
            executable(work/'vehicle', 'echo VEHICLE >> "$TEST_WORK/events"\n: > "$TEST_WORK/vehicle-up"\n')
            executable(card/'clio040', '''
echo UI >> "$TEST_WORK/events"
: > "$TEST_WORK/ui"
for n in $(seq 1 200); do
  [ -e "$TEST_WORK/vehicle-up" ] && exit 0
  /bin/sleep 0.005
done
exit 1
''')
            script = work/'ttn'
            script.write_text('SD="$TEST_WORK"\nlog(){ echo "$1" >> "$TEST_WORK/events"; }\n'+section)
            subprocess.run(['/bin/sh', str(script)], check=True, timeout=5,
                           env={**os.environ, 'TEST_WORK':str(work),
                                'PATH':str(work)+os.pathsep+os.environ['PATH']})
            return (work/'events').read_text().splitlines()

    def test_ui_overlaps_focus_but_vehicle_waits_for_ready(self):
        events = self.startup(True)
        self.assertLess(events.index('UI'), events.index('FOCUS_READY'))
        self.assertLess(events.index('FOCUS_READY'), events.index('VEHICLE'))
        self.assertNotIn('FOCUS READY TIMEOUT', events)

    def test_failed_focus_keeps_bounded_fallback_without_holding_ui(self):
        events = self.startup(False)
        self.assertLess(events.index('UI'), events.index('FOCUS READY TIMEOUT'))
        self.assertLess(events.index('FOCUS READY TIMEOUT'), events.index('VEHICLE'))

    def test_timestamps_are_debug_only_for_both_scripts(self):
        for relative, destination in [('ttn','STATE'), ('clio843/btstart.sh','LOG')]:
            source = (ROOT/'card-template'/relative).read_text()
            log = 'log(){'+source.split('log(){',1)[1].split('\n}',1)[0]+'\n}'
            with tempfile.TemporaryDirectory(prefix='clioplay-boot-log-') as folder:
                work = Path(folder)
                (work/'uptime').write_text('123.45 67.89\n')
                log = log.replace('/proc/uptime', str(work/'uptime'))
                script = f'{destination}="{work}/debug.log"\ndebug_now(){{ [ "$DEBUG" = 1 ]; }}\n'+log+'\nlog STAGE\n'
                for enabled in ('0','1'):
                    subprocess.run(['/bin/sh','-c',script], check=True,
                                   env={**os.environ,'DEBUG':enabled})
                    self.assertEqual((work/'debug.log').exists(), enabled=='1')
                self.assertEqual((work/'debug.log').read_text(), '[uptime 123.45s] STAGE\n')

    def test_phone_setup_proceeds_with_sleeping_or_existing_remote_listener(self):
        source = (ROOT/'card-template/clio843/btstart.sh').read_text()
        section = source.split('# This is the critical remote path',1)[1].split('\n',1)[1]
        section = section.split('# Stock rc.bluetooth normally',1)[0]
        for existing in ('0','1'):
            with self.subTest(existing_listener=existing), tempfile.TemporaryDirectory(prefix='clioplay-bt-start-') as folder:
                work = Path(folder)
                # Simulate a remote that stays asleep until phone setup starts.
                executable(work/'hidd', '''
while [ ! -e "$TEST_WORK/wake" ]; do /bin/sleep 0.005; done
echo REMOTE_CONNECTED >> "$TEST_WORK/events"
: > "$TEST_WORK/remote-up"
''')
                prefix = '''
set -e
SD="$TEST_WORK"
OUT=/dev/null
log(){ echo "$1" >> "$TEST_WORK/events"; }
pidof(){ [ "$EXISTING_LISTENER" = 1 ]; }
sleep(){ echo UNEXPECTED_WAIT >> "$TEST_WORK/events"; }
killall(){ echo UNEXPECTED_RESET >> "$TEST_WORK/events"; }
hciconfig(){ echo PHONE_SETUP >> "$TEST_WORK/events"; }
'''
                suffix = '''
if [ "$EXISTING_LISTENER" = 0 ]; then
  : > "$TEST_WORK/wake"
  for n in $(seq 1 200); do
    [ -e "$TEST_WORK/remote-up" ] && exit 0
    /bin/sleep 0.005
  done
  exit 1
fi
'''
                subprocess.run(['/bin/sh','-c',prefix+section+suffix], check=True, timeout=5,
                               env={**os.environ,'TEST_WORK':str(work),'EXISTING_LISTENER':existing})
                events = (work/'events').read_text().splitlines()
                self.assertNotIn('UNEXPECTED_WAIT', events)
                self.assertNotIn('UNEXPECTED_RESET', events)
                self.assertIn('PHONE_SETUP', events)
                self.assertIn('HIDD ALREADY' if existing=='1' else 'HIDD STARTED', events)
                if existing=='0':
                    self.assertLess(events.index('PHONE_SETUP'), events.index('REMOTE_CONNECTED'))


if __name__ == '__main__':
    unittest.main()
