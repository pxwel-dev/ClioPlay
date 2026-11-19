# Intelligent navigation (2.2.0)

ClioPlay can learn the driver's own journeys and typical road travel times.
This is an optional, entirely offline feature. It is not a live traffic service,
does not send GPS history anywhere, and does not require an active navigation
route to learn. Use the normal navigation engine for actual guidance.

## Enable and use

Open **MENU → Settings** and scroll to the new rows.

| Setting | Effect |
| --- | --- |
| Learn driving history | Off initially; turn on to record and learn on the SD card. Turning it off stops collection and hides both intelligent features. |
| Destination suggestions | Allows a learned destination in the empty Itinerary sheet. Independent of diversion display. |
| Intelligent diversions | Allows historical-traffic warnings, adjusted ETAs and a diversion choice when available. |
| Clear driving history | Confirm to delete the recorded history and learned model; Favourites, Recents, preferences and itineraries are not deleted. |

Use Left/Right or OK to toggle the first three settings. OK on Clear driving
history opens a confirmation. To stop learning after clearing, also turn off
Learn driving history.

The empty **Trip → Itinerary** page retains its original destination prompt
and shows a second row, **Suggested: …**, when
the model has sufficient evidence. Highlight it and press OK to start the normal
route to that place. A saved Favourite within 100 m is preferred to a nearby
Recent; otherwise the predicted latitude and longitude are displayed.
Suggestions do not automatically start a route, add an itinerary stop or mark
an arrival. They are hidden while an itinerary is active.

During guidance, **Likely traffic ahead** means historical evidence suggests
a delay, not that a real-time traffic incident was detected. Once a beneficial
alternative is found, the animated glass status pill reads **Faster route
available**, gives the estimated saving and remaining distance to the actual
decision point, and shows the next available control. The shared geometry
stays solid magenta; only the bypassed original geometry is gold/yellow and
only the distinct proposed geometry is dashed aqua/teal. Small junction
markers identify the divergence and rejoin points when visible. Estimated
savings are shown only in the status pill and confirmation, leaving the
map clear of redundant text. The full invitation is shown for five seconds,
then collapses into one line with the saving and the current review control.
It expands once more for five seconds when the driver is approximately
20 seconds from the decision point at the current GPS speed (minimum 200 m,
maximum 700 m). An offer already inside that range during its first expansion
needs no second reminder. There are no repeated reminders; the compact
invitation remains actionable until acceptance, rejection or expiry at the
matched decision point. A new independent offer starts its own timing.

On the driving map, press **OK to review**, then OK on the initially selected
Yes to accept. In **BT CTRL**, the invitation instead reads **BACK to review**:
Back closes Bluetooth controls, then the hint changes to OK to review. OK
still controls playback while BT CTRL is active; track, volume, Explore,
console Info and existing modal controls keep their ordinary behaviour.
Trip → Itinerary → **Diversion** remains another way to open the same
confirmation. No temporary zoom, map overview, countdown, sound or automatic
route acceptance is used. The confirmation leaves the bottom HUD visible.
Back closes a map-opened confirmation without rejecting it; No rejects only
that bypassed section, and returns to the map (or Itinerary when opened there).
The original route, warning and adjusted ETA remain; independent alternatives
can still be offered. Yes makes the new active route solid magenta.

The candidate is trimmed at matching legal turn states before presentation,
so the distance and expiry refer to the actual departure point from the
current route, not a shared search prefix. A stale confirmation cannot accept
or reject a newer offer. Accepted sections are settled until passed; a full
route recalculation clears route-local decisions. Normal off-route rerouting
remains available. Verify road signs and conditions: historical travel times
cannot predict incidents, closures, weather or current traffic.

## Recording and successful sessions

The recorder runs independently of map availability and of active guidance.
Enable it before driving. The last saved GPS point in `gpslast.dat` is the
fixed starting point of the new session, where available. Otherwise the first
fresh valid fix establishes that point. No origin reconciliation is performed.
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
No arrival prompt, dwell-time arrival detector or GPS-loss heuristic is used.
A device shutdown not observed by the application, a crash or an incomplete
session does not count as a successful journey. Intermediate stops with
Carminat still powered are part of the same session.

Raw date/time is assumed consistent, even if the actual calendar is wrong.
The seven day indices are independent categories, not hardcoded weekdays and
weekends. The display time adjustment, timezone and automatic theme continue
to work exactly as before. They do not change the history clock. Traversal
times use the monotonic clock. If time is unavailable, destination prediction
uses spatial/frequency evidence until RMC time is received; road-speed learning
and traffic estimation require valid raw time.

## Model and routing

The contextual destination model stores up to 512 completed sessions. A
bounded weighted scan compares origins within 300 m, cyclic time-of-day
distance (up to four hours), seven day indices, session recency and the
first 32 approximate 75 m-spaced trajectory points. Similar endpoints within
100 m are combined. Three matching visits, sufficient absolute evidence
and at least 65% of the relative candidate weight are required for a
suggestion. These are heuristic weights, not calibrated probabilities.
While driving, the model refreshes approximately every five seconds and
uses the observed early trajectory to refine suggestions.

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
state IDs. Compatible unchanged geometry can retain learned statistics after
a map replacement; changed geometry needs new observations. The current
implementation does not remap old raw traces to replacement geometry.

For development, see the [build guide](../BUILDING.md) and
[release workflow](release-workflow.md). The standard host regression suite
includes `mobility.c`, which checks time/day handling, complete-session
learning, direction-specific profiles, exact-state candidate search and
route splicing against a small directed fixture. A physical Carminat trial is
needed to verify actual shutdown timing, GPS matching quality and performance.

The optional local tool `tools/inspect_history.py` reports counts and durations
of complete sessions from raw history. It does not modify the logs or upload
anything. Run it from `src/` with the file paths on a mounted SD card:

```sh
python3 tools/inspect_history.py /path/to/card/clio843/history.prev.dat /path/to/card/clio843/history.dat
```

The [preview catalogue](demo/README.md) contains controlled 2.2.0 recordings;
these previews do not demonstrate real predictive accuracy or observed traffic.
