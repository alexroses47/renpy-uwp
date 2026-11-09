# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

"""Milestone 3 acceptance test (run by the host as media_test.main(), on the SDL thread).

With Ren'Py 8.5.3's pygame_sdl2 (renpy.pygame) and audio/video module (renpy.audio.renpysound):
  1. opens a window, draws python/test/test.png (SDL2_image + libpng) for a few seconds;
  2. plays python/test/test.ogg (Ogg Vorbis, FFmpeg + SDL audio) and logs the position;
  3. decodes a few seconds of VP9 (8-bit and 10-bit) + Opus in Matroska, the formats KS:RE uses,
     shows the frames and logs how many were decoded;
  4. measures VP9 1080p decode speed (all frames as fast as possible, FFmpeg threads=auto as in
     Ren'Py), with and without conversion to RGBA. Criterion: 10-bit 1080p > 30 fps.
Log: LocalState\\media-test.log. The test media are synthetic (generated in CI), not game files.
"""

import os
import sys
import time
import traceback

TEST_DIR = os.path.join("python", "test")


def main():
    localstate = sys._renpy_uwp_localstate
    log = open(os.path.join(localstate, "media-test.log"), "w", encoding="utf-8", buffering=1)
    sys.stdout = sys.stderr = log
    failures = 0
    pygame = None
    try:
        import _renpy_uwp
        print(f"main thread stack reserve: {_renpy_uwp.thread_stack_size() // 1024} KB (8192 KB expected)")
        import renpy
        print("renpy package:", renpy.version, "| windows:", renpy.windows)
        import renpy.pygame as pygame
        print("pygame_sdl2 (renpy.pygame) platform:", pygame.get_platform(), "| SDL", pygame.get_sdl_version())

        # pygame_sdl2 creates an OpenGL window unless told otherwise (display.pyx); on WinRT that
        # needs ANGLE's libEGL.dll, which is packaged with the GL renderer in milestone 4. This test
        # only blits 2D surfaces, so use SDL's non-GL window framebuffer.
        pygame.display.hint("renpy.pygame_AVOID_GL", "1")
        pygame.display.init()
        screen = pygame.display.set_mode((1920, 1080))
        print("display: window", screen.get_size(), "| driver:", getattr(pygame.display, "get_driver", lambda: "?")())
        failures += show_png(pygame, screen)
        failures += play_ogg(pygame, screen)
        for name in ("test-vp9.mkv", "test-vp9-10bit.mkv"):
            failures += decode_video(pygame, screen, name)
        failures += benchmark()
    except BaseException:
        traceback.print_exc()
        failures += 1
    finally:
        try:
            if pygame is not None:
                pygame.display.quit()  # gives the window back to the host (no SDL_Quit)
        except BaseException:
            traceback.print_exc()
        print(f"MEDIA TEST DONE: {failures} failure(s)")
        log.flush()
        sys.stdout, sys.stderr = sys.__stdout__, sys.__stderr__
        log.close()
    return failures


def pump_for(pygame, seconds, each=None):
    end = time.time() + seconds
    while time.time() < end:
        for ev in pygame.event.get():
            if ev.type == pygame.QUIT:
                print("event: QUIT")
        if each:
            each()
        time.sleep(0.005)


def show_png(pygame, screen):
    try:
        img = pygame.image.load(os.path.join(TEST_DIR, "test.png"))
        print("PNG: loaded", img.get_size(), "bitsize", img.get_bitsize())
        screen.fill((10, 10, 40))
        sw, sh = screen.get_size()
        iw, ih = img.get_size()
        screen.blit(img, ((sw - iw) // 2, (sh - ih) // 2))
        pygame.display.flip()
        pump_for(pygame, 4.0)
        print("PNG: OK, shown for 4 s")
        return 0
    except BaseException:
        traceback.print_exc()
        return 1


def play_ogg(pygame, screen):
    import renpy.audio.renpysound as rs
    try:
        rs.init(48000, True, 2048, True, False, False)
        rs.set_channel_count(4)
        print("audio: init OK, sample rate", rs.get_sample_rate())
        f = open(os.path.join(TEST_DIR, "test.ogg"), "rb")
        rs.play(0, f, "test.ogg")
        positions = []

        def tick():
            rs.periodic()
            if not positions or time.time() - positions[-1][0] >= 1.0:
                positions.append((time.time(), rs.get_pos(0), rs.busy(0)))

        pump_for(pygame, 4.5, tick)
        for _, pos, busy in positions:
            print(f"audio: pos {pos:.2f} s busy={busy}")
        print("audio: duration", rs.get_duration(0))
        rs.stop(0)
        advanced = any(pos > 0.5 for _, pos, _ in positions)
        print("OGG:", "OK, playback advanced" if advanced else "FAIL, position did not advance")
        return 0 if advanced else 1
    except BaseException:
        traceback.print_exc()
        return 1


def benchmark():
    import _renpy_uwp
    failures = 0
    for name in ("test-vp9.mkv", "test-vp9-10bit.mkv"):
        path = os.path.join(TEST_DIR, name)
        for convert in (False, True):
            try:
                r = _renpy_uwp.decode_benchmark(path, convert=convert)
                print(f"BENCH {name} {'decode+RGBA' if convert else 'decode only'}: {r['fps']:.1f} fps "
                      f"({r['frames']} frames in {r['seconds']:.2f} s), {r['codec']} {r['width']}x{r['height']} "
                      f"{r['pix_fmt']}, threads={r['threads']} ({r['thread_type']})")
                if "10bit" in name and r["fps"] <= 30.0:
                    print(f"BENCH FAIL: {name} below 30 fps")
                    failures += 1
            except BaseException:
                traceback.print_exc()
                failures += 1
    return failures


def decode_video(pygame, screen, name):
    import renpy.audio.renpysound as rs
    try:
        rgb = pygame.Surface((16, 16), 0, 32)
        rgba = pygame.Surface((16, 16), pygame.SRCALPHA, 32)
        rs.sample_surfaces(rgb, rgba)
        rs.set_video(1, rs.NODROP_VIDEO)
        rs.play(1, open(os.path.join(TEST_DIR, name), "rb"), name)
        frames = 0
        first = None
        size = None
        t0 = time.time()

        def tick():
            nonlocal frames, first, size
            rs.periodic()
            if rs.video_ready(1):
                surf = rs.read_video(1)
                if surf is not None:
                    frames += 1
                    if first is None:
                        first = time.time() - t0
                        size = surf.get_size()
                    if frames % 2 == 0:
                        screen.blit(pygame.transform.scale(surf, screen.get_size()), (0, 0))
                        pygame.display.flip()

        pump_for(pygame, 5.0, tick)
        elapsed = time.time() - t0
        rs.stop(1)
        rs.set_video(1, rs.NO_VIDEO)
        print(f"VIDEO {name}: {frames} frames decoded in {elapsed:.1f} s ({frames / elapsed:.1f} fps), "
              f"frame size {size}, first frame after {first if first is None else round(first, 2)} s")
        return 0 if frames >= 60 else 1
    except BaseException:
        traceback.print_exc()
        return 1
