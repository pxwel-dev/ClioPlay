# ClioPlay Global showcase

[Watch the video](index.html) or [download the MP4](clioplay-demo.mp4) — 6:53, 1280×720, 30 fps, silent.

A dark-led tour of everyday remote controls, destinations/favourites, itinerary actions, Bluetooth/audio, useful settings, camera alerts and arrivals. The Day and Auto theme options are shown briefly. Characters, setting values, languages, scrolling and boundary cases are not exhaustively repeated.

The remote drawing follows the supplied photograph and highlights the key used. ClioPlay Send is shown as the actual website: paste a business link, review the destination, send a new route or an extra stop, edit native names, and use manual coordinates or transfer options.

The [looping README preview](preview.gif) is a short extract from this movie. GitHub renders it as an animated image, respecting viewer motion settings; the full film opens separately.

These are independent examples from the production framebuffer/handler, a real UK map and a real baseline route. Sender captures use actual browser MediaSession metadata and silent-WAV playback. The native daemon parser validates the packets before they enter the UI through its file-input handler. The Bluetooth radio hop is simulated. Other phone, vehicle, GPS and camera statuses use controlled fixtures; hardware timing, audio and wireless operation are not validated.

The film keeps only short action descriptions and key highlights. Technical provenance stays in the manifest. Explore uses the native 100 ms animation steps. The framebuffer is scaled 2× in the main view and shown at native size beside the Sender; H.264 encoding is lossy. Raw captures and composition frames stay temporary, outside the project.

## Chapters

| Time | Chapter |
|---|---|
| 0:00 | Driving and Explore |
| 0:37 | Destinations and favourites |
| 1:43 | Manage your trip |
| 2:17 | Send from your phone |
| 3:17 | Bluetooth connection |
| 3:47 | Audio controls |
| 4:21 | Display and navigation settings |
| 5:07 | Alerts, clock and language |
| 5:49 | On the road |

## Regenerate

From `src/`:

```sh
python3 tools/render_demo.py --maps navigation-output --sender-captures /path/to/private-browser-captures
```

Requires Pillow, a host C/C++ compiler, FFmpeg (`ffmpeg` on PATH, `imageio-ffmpeg`, or `--ffmpeg /path/to/ffmpeg`). NumPy optionally speeds up RGB565 decoding. Companion browser captures must be supplied using `--sender-captures PATH`; their source and capture tooling live in the separate private project. Map files are read-only. The plan in `tools/render_demo.py` selects the showcase; `tools/render_gallery.py` captures only its chosen scenes in the Night starting theme.
