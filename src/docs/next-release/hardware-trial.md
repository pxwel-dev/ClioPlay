# ClioPlay Global vehicle trial

No head unit is attached to the development environment. Every item below is
**unverified on hardware**, even where host tests have passed.

Prepare a spare supported card with the installer, retaining its generated
backup. Keep the known working card available for comparison/rollback. Record
results against the executable SHA-256 values and navigation `build.json`.

1. Cold boot in daylight and darkness: theme-aware splash, remote discovery,
   first frame, GPS acquisition, no blank/reboot loop. Measure seconds to each.
   Include time from power-on to splash, usable map, first accepted remote key
   and phone audio. Repeat with Debug enabled to collect uptime-stamped
   `state.log`/`bt.log`; compare AUX, UI exec, Focus and Bluetooth milestones.
   UI loading now overlaps Focus readiness. Confirm vehicle source/temperature
   still appear and AUX/radio work when Focus becomes ready, including a slow
   startup. Compare with Debug off to separate logging overhead.
   For Auto theme, compare winter dusk with summer evenings. Check splash and
   first UI palette using a saved location before GPS lock, then live GPS;
   confirm manual Day/Night overrides and display clock offsets do not alter
   solar switching. Weather/tunnels have no light-sensor input.
2. Parked remote: Menu → Trip/Bluetooth/Settings; scrolling settings; Auto/Metric/
   Imperial, Clock Auto/manual and all available languages; saved favourites/trip;
   smooth map browsing entry/exit. Check long/native names and the frosted speed-limit inset, including estimated values
   with `?`. Compare mapped and estimated limits against signs across urban/rural
   transitions and local zones; an estimate is not a verified posted limit.
3. Confirm physical AUX still initializes. Cycle FM/MW/LW/CD-Bluetooth/AUX and
   check the temporary source and volume notices, radio text, mute and playback
   control highlights. Record any source or focus delay.
4. Pair and reconnect a real phone, restart car/phone, pause/resume/skip and send
   a named destination with accents/non-Latin text. Confirm the remote stays
   connected. Boot with the remote asleep, allow the phone to reconnect, then
   wake the remote with a button and confirm control and audio coexist. Repeat
   with a remote button pressed during phone startup. Phone setup has no fixed
   three-second remote wait. Factory Bluetooth/hands-free coexistence is outside this build.
5. On a recorded passenger-observed drive: motorway/urban roads, an active route,
   roundabout exits, recalculation, bridge-over-water drawing, day/night changes.
   Measure frame interval and route calculation time; log memory/underrun symptoms.
6. Use a journey crossing two installed atlas cores: continuous tiles, no false
   arrival at a gateway, correct final destination and remaining distance. Include
   leaving/rejoining the planned route near a pack boundary. Check turn cues
   close to a boundary and handover with highway-speed/sparse GPS updates.
7. At a known permitted camera in a reviewed enabled country: approach direction and road identity,
   centred camera pill on radio and phone audio during playing/paused Bluetooth.
   Check the road name fades while the camera warning persists. For an average
   section, verify observed entry/exit and GPS
   loss cancellation. France/DE/CH must have no exact camera warnings/data.
8. Confirm local time against a trusted clock, retain it across restart, then
   check automatic offset on an actual timezone change when practical. Host
   replay verifies DST seconds and 2040; it does not alter real GPS time.

Enable Settings → Debug for a short reproduction, then switch it off afterwards.
While Debug is on, `clio843/routing-status.txt` records the current atlas build
marker, elapsed milliseconds, active QAD mode and actual QAD7/CPR expansion
counts. A slow calculation updates it at most once per five seconds. The file
is not created with Debug off and is removed when Debug is disabled. Capture it
before switching Debug off if investigating routing performance. Coordinates are
stored as integer hundred-thousandths of a degree. These are device measurements;
host verification does not establish head-unit timing.
With the car off and card mounted on the computer, collect evidence:

```sh
python3 tools/collect_trial.py /path/to/card ./trial-evidence
```

Fill in `observations.json`; the collector never converts missing observations
into passed tests. It hashes runtime/navigation manifests and copies diagnostic
logs with MAC addresses redacted. It excludes paired keys, favourites, recents,
trips and phone databases. Framebuffer/timing and audio recordings still need to
be captured from the actual device/test setup.
