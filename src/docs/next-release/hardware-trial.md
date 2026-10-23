# Global release-candidate vehicle trial

No head unit is attached to the development environment. Every item below is
**unverified on hardware**, even where host tests have passed.

Prepare a spare supported card with the installer, retaining its generated
backup. Keep the known working card available for comparison/rollback. Record
results against the executable SHA-256 values and navigation `build.json`.

1. Cold boot in daylight and darkness: theme-aware splash, remote discovery,
   first frame, GPS acquisition, no blank/reboot loop. Measure seconds to each.
2. Parked remote: menu → Trip/Phone connection/Settings; scrolling settings; Auto/Metric/
   Imperial, Clock Auto/manual and all available languages; saved favourites/trip;
   smooth freecam entry/exit. Check long/native names and the speed-limit circle, including estimated values
   with `?`. Compare mapped and estimated limits against signs across urban/rural
   transitions and local zones; an estimate is not a verified posted limit.
3. Confirm physical AUX still initializes. Cycle FM/MW/LW/CD-Bluetooth/AUX:
   correct current source, radio text and controls, volume/mute, no stale music
   metadata covering radio. Record any source or focus delay.
4. Pair and reconnect a real phone, restart car/phone, pause/resume/skip and send
   a named destination with accents/non-Latin text. Confirm the remote stays
   connected. Factory Bluetooth/hands-free coexistence is outside this build.
5. On a recorded passenger-observed drive: motorway/urban roads, an active route,
   roundabout exits, recalculation, bridge-over-water drawing, day/night changes.
   Measure frame interval and route calculation time; log memory/underrun symptoms.
6. Use a journey crossing two installed atlas cores: continuous tiles, no false
   arrival at a gateway, correct final destination and remaining distance. Include
   leaving/rejoining the planned route near a pack boundary. Check turn cues
   close to a boundary and handover with highway-speed/sparse GPS updates.
7. At a known permitted camera in a reviewed enabled country: approach direction and road identity,
   visual strip on radio, Phone audio during playing/paused Bluetooth, no repeat
   flood or clipping. For an average section, verify observed entry/exit and GPS
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
