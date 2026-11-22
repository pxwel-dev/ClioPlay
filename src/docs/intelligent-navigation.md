# Intelligent navigation

ClioPlay can learn the driver's own journeys and typical road travel times.
This is an optional, entirely offline feature. It is not a live traffic service,
does not send GPS history anywhere, and does not require an active navigation
route to learn. Use the normal navigation engine for actual guidance.

## Enable and use

Open **MENU → Settings → Driving history**.

| Setting | Effect |
| --- | --- |
| Learn driving history | Off initially; turn on to record and learn on the SD card. Turning it off stops collection and hides both intelligent features. |
| Destination suggestions | Offers a learned destination in the driving-map status pill when no itinerary is active. Independent of diversion display. |
| Intelligent diversions | Allows historical-traffic warnings, adjusted ETAs and a diversion choice when available. |
| Clear driving history | Confirm to delete the recorded history and learned model; Favourites, Recents, preferences and itineraries are not deleted. |

Use Left/Right or OK to toggle the first three settings. OK on Clear driving
history opens a confirmation. To stop learning after clearing, also turn off
Learn driving history.

### Suggested trips

With sufficient evidence and no active itinerary, the normal driving-map
status pill displays **Suggested: …** in the existing success green, followed
by the standard blue **OK to review** instruction. In BT CTRL the hint is
**BACK to review**: Back exits playback control, and OK then opens the same
confirmation. The invitation is a persistent single row, not a timer or an
automatic route. It is independent of the destination-entry pages. The empty
Itinerary retains its original prompt and Coordinates action.

The confirmation displays the destination and **Based on driving history**,
with Yes initially selected. Yes starts the normal route (waiting for a GPS
fix if necessary); No suppresses that destination for the remainder of the
recording session. Back closes the confirmation without rejecting. A later
independently supported destination can still be offered. Ignoring the pill
requires no action: it remains only while the evidence supports it. A changed
or expired suggestion cannot be accepted or rejected through an old popup.
Starting another trip, disabling suggestions or learning, or entering Explore
hides the invitation. The normal driving zoom and lower HUD are unchanged.

A named Favourite within 100 m is preferred to a named Recent. Otherwise,
the model retains the actual learned coordinates and checks for a named mapped
**Fuel**, **Parking** or **Hospital** POI at the destination, then tries the
nearest street in a small destination-centred map neighbourhood, then a nearby
named settlement. These names describe the vicinity, not a verified address;
the last fallback is latitude and longitude. Explicitly accepted suggestions use
normal route and Recent storage. No new training record is created by either
confirmation choice: only the usual completed power-off session trains the
model.

### Frequent-place favourites

When destination suggestions and driving-history learning are enabled, ClioPlay
can ask **Add to favourites?** / **You come here often** on a subsequent startup.
It requires five completed journeys ending within 100 m of the most recent
completed destination, on at least three distinct arrival dates within the
last 90 days. A fresh live GPS fix must confirm that location, with at least
three seconds of stationary samples before the prompt opens. It never opens
while moving, during Bluetooth control, Explore, an active itinerary, or an
existing menu or dialog. The startup opportunity expires after three minutes;
if driving begins or the position becomes invalid, an open prompt closes.

No is selected by default because saving a favourite modifies personal data.
The prompt never displays an inferred name. Yes always opens the existing
Favourite naming keyboard, populated with the best nearby named Recent, named
mapped **Fuel** / **Parking** / **Hospital** POI, or offline street/settlement
name when available. The driver can edit it or
enter a name from scratch. Only Save stores the actual learned endpoint
under the reviewed name and returns to the driving map. Back cancels; if
the vehicle moves or its position becomes invalid, naming closes without
saving. No or leaving the prompt unanswered also saves nothing. Already saved nearby favourites and a full
favourites list are excluded. A checksummed local record prevents the same
place from prompting again on a later startup (including after declining or
ignoring it); clearing driving history resets the record. The prompt never
starts a journey or adds evidence to the prediction model. Accepting it makes
future suggestions easier to recognise through a named Favourite; it does not
artificially increase their statistical confidence.

### Route diversions and unguided traffic warnings

During guidance, **Likely traffic ahead** means historical evidence suggests
a delay, not that a real-time traffic incident was detected. When **Route
diversions** and **Learn driving history** are enabled, unguided drives also
check the current snapped corridor and a `~220 m` forward heading probe against
the current two-hour time bucket, surfacing a one-shot amber **Likely traffic
ahead** notice (at most once per congested road per trip, when confirmed
historical delay is `≥ 60 s`) without proposing a diversion. Once a beneficial
alternative is found during active guidance, the animated glass status pill reads **Traffic diversion
available** and shows the next available control (distinguishing traffic-based
bypasses unambiguously from learned **Usual route available** recommendations).
The shared geometry stays solid magenta; only the bypassed original geometry is
gold/yellow and only the distinct proposed geometry is dashed aqua/teal. Small
junction markers identify the divergence and rejoin points when visible. A
rounded, anti-aliased frosted-glass badge with centred approximate savings text
sits beside the decision point when space permits; the confirmation always
includes the saving. The invitation is a single line: the theme-aware success
green announcement followed by the standard blue review instruction, in both
driving and BT CTRL contexts. It remains actionable until acceptance, rejection
or expiry at the matched decision point.

On the driving map, press **OK to review**, then OK on the initially selected
Yes to accept. In **BT CTRL**, the invitation instead reads **BACK to review**:
Back closes Bluetooth controls, then the hint changes to OK to review. OK
still controls playback while BT CTRL is active; track, volume, Explore,
console Info and existing modal controls keep their ordinary behaviour.
Trip → Itinerary → **Diversion** remains another way to open the same
confirmation. The confirmation opens **Take traffic diversion?** (showing the
approximate saving and **Likely traffic ahead**) with **Yes** selected
alongside **No** and a **View diversion** button below them (use **Up / Down**
or the console knob to focus **View diversion**). Selecting **View diversion** (or **View route** on a usual-route confirmation)
closes the popup and opens north-up Explore tightly framed and vertically
centred around the proposed split, bypass and rejoin points across the full
`480 × 272` viewport (up to Z9) so you can pan and zoom to review it;
pressing **MAP** returns to the driving view and automatically re-opens the
same **Take traffic diversion?** confirmation if the offer is still valid (or
shows **Diversion unavailable** if it expired). No automatic zoom, countdown,
sound or automatic route acceptance is used. The confirmation leaves the bottom
HUD visible. Back closes a map-opened confirmation without rejecting it; No
rejects only that bypassed section, and returns to the map (or Itinerary when
opened there). The original route, warning and adjusted ETA remain; independent
alternatives can still be offered. Yes emits **Diversion selected** and makes
the new active route solid magenta.

The candidate is trimmed at matching legal turn states before presentation,
so the badge and offer expiry refer to the actual departure point from the
current route, not a shared search prefix. A stale confirmation cannot accept
or reject a newer offer. Accepted sections are settled until passed; a full
route recalculation clears route-local decisions. Normal off-route rerouting
remains available. Verify road signs and conditions: historical travel times
cannot predict incidents, closures, weather or current traffic.

## Extended local learning (Version 2.2.4)

When **Learn driving history** is enabled, ClioPlay also maintains a bounded,
integer-only companion model (`clio843/mobility-ml.dat`, format `MML1`, ~26 KiB,
FNV-1a checksummed atomic saves) alongside `mobility.dat`. Turning off **Learn
driving history** suspends all of these behaviours, and **Clear driving history**
deletes `mobility-ml.dat` completely. No data ever leaves the SD card.

### Personal road-class and cold-weather ETA calibration

For roads with no road-specific history yet, ClioPlay learns personal
travel-time multipliers across the eight CPR2 road classes (roundabouts,
motorways, trunk, primary, secondary, tertiary, residential and service roads)
using Bayesian shrinkage toward the legal static estimate once at least four
sessions have been observed. Whenever a valid Focus or VehicleStatus exterior
temperature reading is `<= 3 °C`, a separate cold-weather multiplier is trained
and blended (never predicting faster than the mild baseline) so winter ETAs stay
realistic.

### Dwell-time trip chains and recurring intermediate stops

ClioPlay records the previous completed journey's `(origin, destination, arrival
time)` across power cycles. When a new trip departs from near the previous
destination, a bounded `(previous origin, current origin, dwell bucket) ->
destination` chain table boosts matching multi-leg routines (e.g., short
`<= 15 min` errand vs full work day `> 5 h`).

When the driver repeatedly stops at the same intermediate point on a familiar
origin-to-destination trip (either via short `<= 45 min` unguided chained legs
— automatically labelled from matching saved **Favourites**, mapped
Fuel/Parking/Hospital POIs or **Recents** — or by adding the stop explicitly via
**Add stop** across at least three trips within a 3-hour time-of-day window),
starting that destination offers **Usual stop available · OK to review** in the
status pill. Pressing **OK** opens **Add usual stop?** (`Yes` inserts the stop
into the itinerary; `No` suppresses it for the session).

### Preferred-route ("Usual route") recommendations

On each completed session (guided or unguided), traversed directed arcs
accumulate positive preference scores (`+10` per drive) while arcs voluntarily
bypassed during forward off-route forks on a guided route (ignoring U-turns)
accumulate negative avoidance scores. Off-route detours taken while a historical
traffic warning is active — or bypassing arcs already known to be congested in
the current two-hour time bucket — do not stage permanent avoidance scores,
keeping temporary traffic detours separate from permanent road preferences.

If no historical-traffic diversion is active, the bounded turn-state splice
search looks for a time-competitive usual alternative in two cases:
- an upcoming route arc has been consistently avoided (`score <= -30`), or
- a rejoin junction ahead has an alternate incoming road driven at least four
  times unguided (`score >= +40`) while the routed arrival road is unfamiliar
  (`score <= 0`), requiring the reconstructed shortcut to be strictly more
  familiar than the routed stretch (`alt_pref >= old_pref + 40`).

To avoid any ambiguity with traffic diversions, preferred-route recommendations
use dedicated wording across every surface:

| Surface | Traffic diversion | Usual route recommendation |
| --- | --- | --- |
| Map styling | Bypassed congested section in **yellow**, alternative in **dashed teal** + `Save ~X min` badge | Active route stays **normal route colour** (no yellow), usual road in **dashed teal** |
| Status pill | **Traffic diversion available · OK to review** | **Usual route available · OK to review** |
| Itinerary footer | **Diversion** | **Usual route** |
| Confirmation title | **Take traffic diversion?** | **Use usual route?** |
| Confirmation details | `Save approximately X min` / **Likely traffic ahead** | **Based on your usual route** / **Based on driving history** (or `Save approximately X min` / **Based on your usual route** when saving ≥ 1 min) |
| Preview button | **View diversion** | **View route** |
| Accepted / expired notice | **Diversion selected** / **Diversion unavailable** | **Usual route selected** / **Route unavailable** |

### ETA reliability and uncertainty bands

Alongside mean road travel times, ClioPlay tracks online integer Mean Absolute
Deviation (MAD) per directed road and two-hour bucket. When the sub-additive
spread across the remaining route is at least 3 minutes, both the **OK** HUD
journey card and **Trip → Itinerary** display a realistic range (for example,
`~9-14 min`) instead of a single optimistic number. Congestion diversion ranking
also factors in reliability so volatile bottlenecks lose tie-breaks to steadier
alternatives.

### Adaptive Auto-zoom and Day/Night brightness

- **Auto-zoom:** manual zoom overrides while driving in **Auto zoom** are
  tracked across three speed regimes aligned with Auto zoom’s hysteresis tiers
  (`< 25 mph` / `Z9–Z7`, `25–45 mph` / `Z6`, `≥ 45 mph` / `Z5–Z4`), inheriting
  the base `23/27` and `42/48 mph` hysteresis so boundary fluctuations never
  jitter zoom. An override must be held for at least **20 seconds** before it is
  counted (at most once per band per drive), so accidental taps that you undo,
  direction changes before settling, or brief peeks cancelled via **MAP** /
  **Explore** never bias learning. After four consistent sessions in the same
  direction, Auto zoom gently shifts by `±1` zoom level in that band (always
  clamped to Z4–Z9); a single sustained override in the opposite direction
  cancels the shift back to neutral.
- **Brightness:** manual brightness changes in **Settings → Display** are
  remembered separately for Day and Night palettes. Once at least two
  adjustments are learned for a theme, **Auto** Day↔Night switches automatically
  apply your preferred brightness level (rounded to 10% steps).

### Learned Nearby-POI affinity and Usual Parking

- **Preferred Nearby POIs:** learns both when you route to or add a **Fuel**,
  **Parking** or **Hospital** POI from **Trip → Nearby** *and* automatically
  when an unguided drive (or short chained stop) finishes right at a mapped
  **Fuel** (`≤ 55 m`), **Parking** (`≤ 75 m`) or **Hospital** (`≤ 100 m`) POI
  (deduplicated per trip so explicit picks never double-count). Once visited or
  chosen at least twice, that station or car park receives a gentle 600 m
  effective ranking boost in **Trip → Nearby** and displays a crisp cyan star
  indicator beside its highlighted distance.
- **Usual parking:** learns in three ways:
  1. when guided trips to a destination repeatedly finish `100–600 m` away across
     at least three sessions (including unmapped on-street parking),
  2. automatically when unguided drives finish at a mapped **Parking** POI
     (`≤ 75 m`) located `100–600 m` from a unique saved **Favourite** or
     **Recent** (skipping any location `< 100 m` from another saved place or
     ambiguous between multiple Favourites), and
  3. automatically for *any* mapped **Parking** POI you park at across at least
     three unguided drives — even with no Favourite or Recent nearby — so
     routing later to any new destination `100–600 m` from that habitual car
     park offers it as a fallback (skipped if the destination is itself a car
     park, a place you already regularly park directly at `< 100 m`, or if you
     are currently departing from that car park).
  When matched, starting a route (or accepting a destination prediction)
  displays **Usual parking available · OK to review**. Pressing **OK** opens
  **Route to usual parking?** (`Yes` updates the destination to the learned
  parking spot; `No` declines it for the session).

### Repeat hard-braking and unmapped slowdown alerts

On every snapped 1 Hz sample while driving at `>= 20 mph` (guided or unguided),
sharp deceleration events (`>= 9.0 mph/s`) are staged per directed arc and
committed on clean shutdown (capped at `6` warm / `5` cold sessions so fixed
roadworks can recover). When a learned hotspot ahead (`40–350 m`) has
triggered across at least three sessions (or two sessions in `<= 3 °C` cold
weather), ClioPlay surfaces a non-intrusive amber **Frequent slowdown ahead**
(or **Cold-weather slowdown ahead**) status cue at most once per arc per trip —
both along an active route and during unguided driving (verified against the
directed road arc’s heading and a tight forward bearing cone to ignore opposite
carriageways and cross streets) — and applies a virtual penalty against
repeat-hazard rat-runs during diversion ranking.

Hotspots also self-correct dynamically rather than lingering forever: traversing
a learned hazard road smoothly (with no sharp braking on that trip) steps its
counter down by `1` whenever at least four journeys have elapsed since its last
update (preventing single-pass flickering while clearing fixed roadworks or
consistently gentle passes over time). Cold-weather counts only decay on cold
(`<= 3 °C`) smooth passes so summer driving never erases winter icy-road alerts.
Unvisited hotspots also expire automatically after `256` journeys (warm) or
`512` journeys (cold), and slots are reclaimed once both counters reach zero.

## Recording and successful sessions

The recorder runs independently of map availability and of active guidance.
Enable it before driving. The last saved GPS point in `gpslast.dat` is a provisional starting point
where available. The first fresh valid fix establishes the origin if missing
and replaces a saved point more than 100 m away, invalidating the old
prediction. A fresh raw RMC time is required for a provisional startup offer,
so the unit can suggest a destination even before position lock without
relying on the display clock.
The GPS heading stored by the previous session is not used as a departure
direction.

Fresh RMC observations are sampled approximately once a second, including while
stationary. Raw RMC date/time is used as soon as it becomes available, even
before the receiver acquires a position fix. The initial system clock is not
used in training. Before motion is confirmed, the provisional departure time
follows the GPS clock and predictions refresh at the first usable time and
subsequently when the minute changes. A departure requires at least 50 m
displacement and five seconds of sustained speed of about 5 km/h. It is
backdated to the first movement in the confirmed sequence.
An unknown departure time can be inferred from a subsequent valid RMC time
and the monotonic elapsed interval.

The only completion signal is the existing `/proc/barcelona/onoff` power
request. It is handled before expensive route and rendering work. A session
must have travelled at least 150 m and lasted 60 seconds, with an endpoint
from a valid fix no older than two minutes, before it can train the model.
A clean power-off adds a checksummed END record and commits the model.
No arrival-time learning prompt, dwell-time arrival detector or GPS-loss completion heuristic is used. The optional frequent-place prompt occurs on a later stationary startup, not at the end of a journey.
A device shutdown not observed by the application, a crash or an incomplete
session does not count as a successful journey. Intermediate stops with
Carminat still powered are part of the same session.

Raw date/time is assumed consistent, even if the actual calendar is wrong.
Road-speed profiles remain independent across all seven day indices, while
destination prediction distinguishes exact days and weekday/weekend regimes
(Monday–Friday vs Saturday–Sunday). The display time adjustment, timezone and
automatic theme continue to work exactly as before. They do not change the
history clock. Traversal times use the monotonic clock. If time is unavailable
after a valid live position, prediction uses a more conservative
spatial/frequency model. A previously confirmed position must remain fresh
(within 15 seconds) to support an invitation; road-speed learning and traffic
estimation still require valid raw time.

## Model and routing

The contextual destination model stores up to 512 completed sessions in the
existing model format. A bounded weighted scan compares origins within 300 m,
cyclic time-of-day distance (up to four hours), learned day-specific patterns
and both session and calendar recency. A quadratic time kernel favours usual
departure times. Exact-day observations receive full weight (`100%`); different
days in the same weekday/weekend regime (Monday–Friday vs Saturday–Sunday)
receive `75%` weight while exact-day history is sparse (and `35%` once three or
more exact-day observations exist so distinct weekday schedules still separate),
while cross weekday↔weekend trips are damped (`20–25%`) so weekday commutes do
not overwhelm weekend routines.

During movement, a forward-only segment match compares the first 32
approximately 75 m-spaced trajectory samples against a bounded window of
historical segments. Contradictory trajectories lose support. The model also
withdraws a destination already reached or when the distance travelled plus
the remaining direct distance is substantially beyond a historically plausible
trip. The stored traces cover only the early route, not an entire journey.
Predictions refresh at usable startup clock/position changes, then roughly
every five seconds while moving.

Similar endpoints within 100 m are combined, visiting the history ring newest
first so a real recent endpoint anchors each cluster. New invitations require
at least three independently weighted visits, sufficient absolute evidence and
70% of the relative weight. Without a reliable departure time this becomes
five visits and 80%. A still-leading displayed candidate can retain its
identity at 55% (70% without time), reducing flicker; a new candidate must
cross the higher entry threshold. Rejected locations are applied **after**
ranking, so rejection does not falsely increase a weaker alternative's score.
These are fixed-capacity, integer-only heuristics, not calibrated probabilities
or a claim of measured real-world forecasting accuracy. A genuinely ambiguous
context shows no suggestion.

Road learning uses CPR2 v5 geometry when available. Samples are matched to
nearby directed base arcs using position, heading, continuity and ambiguity
checks. A traversal must cover most of the arc from near its entrance to near
its exit. Elapsed time includes ordinary queues but rejects GPS discontinuities,
incomplete traversals and prolonged stationary periods. One successful
session contributes at most one independent observation per directed road
and time bucket; repeated passes in that session are averaged. The sparse
profile table uses two-hour bins over seven day indices; when fewer than five
same-day observations exist, it can pool the corresponding hour across all
days. Older observations gradually lose influence; unknown bins use the legal
static travel-time cost. Learned history may add delay to, but never reduce,
the static cost. Separate direction-specific geometry fingerprints prevent
one direction's congestion from affecting the other.

The existing CH/QAD7 shortcuts are not edited or reweighted. The traffic
adapter evaluates up to 30 minutes / 1024 geometry vertices ahead on the
current regional leg, estimates arrival times at successive directed arcs,
and adds supported delays to its ETA. For a sufficiently slow arc with
a divergence and rejoin ahead, it uses a cooperative, bounded reverse search
of the **legal CPR2 base-graph turn states**, following the same exact-state
splice invariant as ordinary rerouting. It excludes the selected slow
directed arc, reconstructs a candidate path, scores it forward using historical
time estimates, and retains the original route prefix and suffix. A candidate
must save at least two minutes and 10% of the estimated remaining regional
leg; the search is bounded to 768 states, 1024 candidate points, five seconds
and a local search area. Only one alternative is offered. This is a local
diversion search, not a globally optimal time-dependent route planner.
It does not change CPM5, CPR2 v5, QAD7, or atlas manifests; cross-region
optimisation is not performed.

## Privacy, storage and recovery

The following files belong only on a user's SD card. Do not share them
with a bug report or include them in Git/publication packages.

| File | Purpose |
| --- | --- |
| `clio843/history.dat` | Append-only GPS records with BEGIN/SAMPLE/END markers. |
| `clio843/history.prev.dat` | Previous rotated history segment. |
| `clio843/mobility.dat` | Checksummed aggregate model committed on successful shutdown. |
| `clio843/mobility.tmp` | Temporary replacement model during a save. |
| `clio843/mobility-places.dat` | Private, checksummed frequent-place prompt locations. |
| `clio843/mobility-places.tmp` | Temporary replacement of the prompt record. |
| `clio843/mobility-ml.dat` | Checksummed companion ML model (`MML1`: road classes, chains, stops, preferred arcs, variance, POI affinity, parking, hazards, zoom/brightness). |
| `clio843/mobility-ml.tmp` | Temporary replacement companion model during a save. |

The raw format is 28 bytes per little-endian record (approximately 101 kB
per recorded hour, excluding markers); samples are buffered in blocks of
30. The current log rotates at the next recording session after about 32 MiB,
with an absolute 64 MiB cap per file (at most two files). The learned model and
working buffers have fixed capacities; old journey records and road profiles
are replaced as needed. The model is saved with an integrity checksum and
temporary-file rename. Unexpected power loss can leave a partial or
uncommitted raw session; it will not be used for training. A failed save or
corrupt model is not automatically replayed on the head unit. The history
inspector below checks only complete sessions.

The history and destination model do not reference map IDs. Road profiles are
indexed by directed geometry and OSM way fingerprints, not mutable graph
state IDs. Compatible unchanged geometry retains learned statistics across a map
replacement. When rebuilding maps with fresh OSM extracts, the desktop replay
tool below can re-snap raw `history.prev.dat` / `history.dat` traces onto the
new map geometry offline.

For development, see the [build guide](../BUILDING.md) and
[release workflow](release-workflow.md). The standard host regression suite
includes `mobility.c` and `test_replay_history.py`, which check time/day
handling, complete-session learning, contextual prediction and confidence
hysteresis, session dismissal, startup GPS reconciliation, frequent-place
prompting and persistence, direction-specific profiles, exact-state candidate
search and route splicing, all extended `MML1` local ML models, and byte-exact
Python↔C binary parity. A physical Carminat trial is needed to verify actual
shutdown timing, GPS matching quality and performance.

### Local history tools

- **Inspect complete sessions:** `tools/inspect_history.py` reports counts and
  durations of complete sessions from raw history without modifying logs or
  uploading anything:

```sh
python3 tools/inspect_history.py /path/to/card/clio843/history.prev.dat /path/to/card/clio843/history.dat
```

- **Replay history & migrate across map rebuilds:** `tools/replay_history.py`
  (Python stdlib only, strictly local, never prints raw coordinates) reads
  `history.prev.dat` and `history.dat`, runs pure-Python DBSCAN endpoint
  denoising, snaps 1 Hz traces onto a single CPR2 v5 `route.cpr` or multi-region
  `atlas.cpa` maps folder, preserves existing UI preferences (Auto-zoom,
  Day/Night brightness, POI affinity and usual parking) from `mobility-ml.dat`,
  and writes verified `mobility.dat` (`MOB1`) and `mobility-ml.dat` (`MML1`):

```sh
python3 tools/replay_history.py --card /path/to/card --verify
# Or preview aggregate counts without writing:
python3 tools/replay_history.py --card /path/to/card --dry-run
```

The [preview catalogue](demo/README.md) contains recordings made with controlled fixtures;
these previews do not demonstrate real predictive accuracy or observed traffic.
