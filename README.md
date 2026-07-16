# Simple Limiter

A no-frills true-peak mastering limiter. One knob (Gain), fixed -1 dBTP ceiling,
dual-mono (independent L/R) lookahead limiting, live LUFS Integrated and
LUFS Short-Term readout.

## How to get a working VST3 (no local compiling needed)

1. Go to https://github.com/new and create a new **empty** repository
   (any name, e.g. `simple-limiter`). Don't initialize it with a README.
2. On the new repo's page, click **"uploading an existing file"**.
3. Drag this entire `SimpleLimiter` folder's contents into the upload box
   (make sure `CMakeLists.txt`, the `Source` folder, and the `.github` folder
   all end up at the top level of the repo — GitHub's drag-and-drop upload
   preserves folder structure, so dragging the whole folder in should work).
4. Click **Commit changes**.
5. Click the **Actions** tab at the top of your repo. You should see a
   workflow run start automatically ("Build VST3"). It takes about 5-10
   minutes.
6. Once it finishes (green checkmark), click into the run, scroll to
   **Artifacts** at the bottom, and download:
   - `SimpleLimiter-Windows-VST3` for Windows
   - `SimpleLimiter-macOS-VST3` for Mac
7. Unzip. Drop the `.vst3` file/folder into your VST3 folder:
   - Windows: `C:\Program Files\Common Files\VST3\`
   - Mac: `/Library/Audio/Plug-Ins/VST3/`
8. Rescan plugins in your DAW.

## If the Actions run fails (red X)

Click into the failed job and open the "Build" step to see the error log.
Bring that log back here and I'll fix the code — this is a much smaller
debugging surface than a local build, since it's not tangled up with your
machine's environment.

## About the limiter

- **Gain knob**: drives the signal into the limiter (this is your "make it
  louder" control).
- **Ceiling**: fixed at -1.0 dBTP (true peak, oversampled 4x internally so
  inter-sample peaks are caught, not just sample peaks).
- **L/R processing**: fully independent (dual mono) per your spec, not
  stereo-linked.
- **Release**: program-dependent — blends a fast (50ms) and slow (400ms)
  time constant based on how deep the current gain reduction is, so short
  transients recover quickly and sustained loud sections stay smoother.

Honest note: this is a solid, real lookahead true-peak limiter, but matching
the transparency of FabFilter Pro-L2 / Sonible Limitless at extreme gain
reduction (6dB+) took those teams years of iteration on their gain-computer
algorithms. Once you've got this loaded and can A/B it against your mixes,
tell me what you're hearing (pumping, distortion character, LUFS meter
behavior, etc.) and I'll tune the DSP from there.
