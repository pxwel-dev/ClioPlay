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
   For Auto theme, configure Day starts and Night starts in local time.
   Check transitions at the exact saved times, restart on both sides of each
   boundary, and compare the splash with the first UI palette before GPS lock.
   Confirm manual Day/Night overrides and automatic timezone/DST changes.
2. Parked remote: Menu → Trip/Bluetooth/Settings; scrolling settings; Auto/Metric/
   Imperial, Clock Auto/manual and all available languages; saved favourites/trip;
   smooth map browsing entry/exit. Check long/native names and the frosted speed-limit inset, including estimated values
   with `?`. Compare mapped and estimated limits against signs across urban/rural
   transitions and local zones; an estimate is not a verified posted limit.
3. Confirm physical AUX still initializes. Cycle FM/MW/LW/CD-Bluetooth/AUX and
   check the temporary source and volume notices, radio text, mute and playback
   control highlights. Record any source or focus delay.
   Check CD → BT; AUX stays AUX both before and after connecting a phone, with
   BT output/controls disabled. Boot without a readable radio source, both with
   empty first-boot values and a cached radio/volume record: BT output should be
   enabled, unavailable readings hidden, and metadata, playback progress and BT
   CTRL available with a connected phone. Repeated unknown-source events must
   not replay/extend track notices. Test audio sent specifically to Carminat BT,
   independently of the factory Bluetooth receiver. Check the radio knob and
   steering-wheel volume/mute controls during playback, including with FM and
   factory Bluetooth selected, and check calls, pause/resume and reconnects.
   A live CD event must keep BT available even while volume remains unknown.
   When a valid FM/MW/LW source appears, verify radio labels return and BT PCM
   output stops; losing source telemetry should restore the BT fallback once.
   Check phone-audio camera alerts on CD and independent BT. Host UI/gate checks
   do not establish physical playback, volume control or coexistence on another radio.
4. Pair and reconnect a real phone, restart car/phone, pause/resume/skip and send
   a named destination with accents/non-Latin text. Confirm the remote stays
   connected. Boot with the remote asleep, allow the phone to reconnect, then
   wake the remote with a button and confirm control and audio coexist. Repeat
   with a remote button pressed during phone startup. Phone setup has no fixed
   three-second remote wait. Factory Bluetooth/hands-free coexistence is outside this build.
   Compare the same pill/Explore animation while paused and immediately after
   starting Bluetooth music. Keep Debug off for the normal comparison, then
   collect `FRAME 5S` from `state.log` alongside `AUDIO 5S` from `bt.log` with
   Debug on. Audio summaries report received packets/decoded frames, maintenance
   passes (normally about 50 per five seconds), the longest packet processing
   time and PCM underrun/drop deltas. Packet time includes scheduling delays;
   it is not a CPU-utilisation or temperature reading. These summaries use the
   existing Debug-only log and stop when Debug is disabled.
   Verify unchanged sound, camera-tone duck/release, pause/resume, progress,
   track updates and remembered-phone names. Reconnect and pairing timeouts
   must follow elapsed time regardless of packet traffic. The receiver now
   uses exact reciprocal SBC division, bounded byte reads, direct PCM packing
   when no chime is active, and elapsed-time maintenance/metadata polling.
   Stereo rate, bitpool capability, secondary-ring capacity and audio priority
   are unchanged. Synthesis now folds exact matrix symmetries; no approximate
   coefficients or quality reduction are used. PCM copies use aligned words with
   a byte fallback. Check transitions between BT and FM/AUX while the phone keeps
   streaming: decoded-frame growth should stop off-source and resume cleanly.
   Confirm phone destinations/metadata still arrive while output is disabled.
   Pause without disconnecting, then resume, including an interrupted packet
   stream without transport suspend. No old audio should loop; queued audio and
   a paused-playback camera cue must finish before idle closure. Resume should
   prefill normally. Where lower output rates are actually required, confirm
   stable playback at the logged rate: primary chunk samples now scale with
   rate to retain their duration, including at 11.025 kHz.
   Physical frame-rate gains and thermal behaviour remain unverified.
5. On a recorded passenger-observed drive: motorway/urban roads, an active route,
   roundabout exits, recalculation, bridge-over-water drawing, day/night changes.
   Measure frame interval and route calculation time; log memory/underrun symptoms.
   With Debug on, collect the five-second `FRAME 5S` lines from `state.log`.
   Outside calculation, fades, resizing, new content, warning pulses and Explore
   transitions target about 100 HUD presentations per five seconds. Steady speed,
   distance, time, volume and BT playback-progress updates target 50, matching
   the 50 scheduled map frames. Repeated value updates must not keep the faster
   clock active; a new track/notice should promptly promote it again.
   During calculation/rerouting,
   both HUD and map target 25 per five seconds, including menus and Explore.
   Confirm that finishing, cancelling or failing restores the normal rates. `CAMERA` counts
   additional Explore drawing passes, separate from scheduled `MAP` frames.
   `LATE` counts presentations over 8 ms late; `MAXLATE_MS` is the worst delay.
   `MAXHUD_MS` includes framebuffer output; `MAXBG_WALL_MS` includes overlay
   yields while building a background, and `ROUTE_MS` is total route-slice time.
   Compare quiet driving, loading, speeding pulse and Explore with an active
   volume/zoom notice. Check new map frames continue during transitions, menus
   retain notice timers, and no partial roads or old glyph rectangles appear.
   Repeat BT CTRL expansion, volume/track changes and Explore entry/exit while
   streaming. Glass interiors, framebuffer copies and repeated camera math have
   scalar/pixel parity checks; their instruction savings are not measured unit
   FPS. Audio priority remains unchanged. Confirm clarity, uninterrupted audio
   and the same antialiased/frosted appearance during the transitions.
   Record route calculation time with Debug off too. These counters establish
   device scheduling/output timing, not physical LCD scanout or a 20 FPS guarantee.
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

- Enter a named destination through Trip → Coordinates with the real remote.
  Check negative longitude, decimal input, inline range errors, delete/clear and
  BACK at every step. Review, confirm, restart and reuse the named Recent; save it
  as a favourite. Repeat as Add stop and Change destination. Cancel without
  changing the itinerary/Recents, including during a simultaneous phone send.
  Check keyboard languages and day/night glass rendering, active audio notices
  and the time/temperature row on numeric, name and review pages. Check a target
  outside installed routing data reports the usual routing failure.

- In Explore, choose a point and confirm a new route, then repeat as Add stop.
  Confirm it returns directly to driving with the reverse rotation/tilt animation,
  including a quick confirmation before entry finishes. The time/temperature
  pill must stay visible without a second entrance, with time-only and valid
  temperature configurations. Declining leaves Explore unchanged. Route
  calculation must retain its existing five-FPS cap.
