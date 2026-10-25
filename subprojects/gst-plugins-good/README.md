GStreamer 1.24.x stable series

Local Armbian V4L2 integration
-----------------------------

This fork carries two fixes to the GStreamer 1.24.2 V4L2 stateful decoder
tested with the Meson VDEC driver on an Amlogic S905W Armbian TV box.

The startup fix is in `gst_v4l2_video_dec_wait_for_src_ch()`. Before the
CAPTURE queue has streaming buffers, it temporarily polls only priority events
and restores normal buffer-read polling afterward. The Meson driver can report
`POLLERR` for readable-buffer polling during this valid startup gap; treating
that as a fatal decoder failure prevents playback. This fix does not sleep,
retry, suppress errors, or change the kernel.

The rotation fix is in `gst_v4l2_video_dec_set_format()`. In upstream 1.24.2,
`V4L2_FMT_FLAG_DYN_RESOLUTION` bypasses the usual incompatible-caps teardown
and skips setting the V4L2 OUTPUT format. With the Meson decoder, an iPhone
orientation change updates the H.264 SPS and emits `SOURCE_CHANGE`, but leaving
the old OUTPUT/capture configuration in place stalls decoded output. The fix
runs the normal caps-compatibility check, drains/stops the queues when caps
change, and applies the new OUTPUT format even for a dynamic-resolution
format. Framerate-only caps changes remain compatible.

`uxplay-kms-launch.c` is the board-specific display helper. It discovers the
active HDMI CRTC, primary and NV12 overlay planes, disables the opaque console
primary plane, and execs UxPlay with a DRM master descriptor inherited by
`kmssink`. This gives V4L2 decoder DMA-BUFs a direct NV12 scanout path. It is
not a generic GStreamer component.

## Rebuild and install

Tested versions: GStreamer 1.24.2 (source revision
`2d8273151571fcab887cc81de48e87aeb61b5c06`), UxPlay 1.73.7, and kernel
`6.12.44-ophub`. The board needs the matching GStreamer core/video development
files, Meson, Ninja, libdrm, and libgudev development files.

Clone this fork and build only `gst-plugins-good` with the isolated prefix:

```sh
git clone https://github.com/pngocthach/gstreamer.git
cd gstreamer
git switch armbian-v4l2-dyn-resolution
cd subprojects/gst-plugins-good
meson setup build \
  --prefix=/opt/uxplay-gst-v4l2 \
  --buildtype=release \
  --auto-features=disabled \
  -Dv4l2=enabled \
  -Dv4l2-gudev=enabled \
  -Dv4l2-probe=true
ninja -C build -j2
ninja -C build install
```

Build the KMS helper from the same checkout:

```sh
cc -std=c11 -O2 -Wall -Wextra -Werror -I/usr/include/libdrm \
  uxplay-kms-launch.c -ldrm -o /usr/local/bin/uxplay-kms-launch
```

The user service must set `GST_PLUGIN_PATH_1_0` to
`/opt/uxplay-gst-v4l2/lib/gstreamer-1.0` and use a private
`GST_REGISTRY_1_0` cache. Verify that `gst-inspect-1.0 v4l2h264dec` reports
the plugin filename under that prefix. Keep the distro plugin untouched.

The tested UxPlay video configuration is:

```sh
/usr/local/bin/uxplay-kms-launch /usr/local/bin/uxplay \
  -n Armbian-TV-UxPlay -srgb no -bt709 -vd v4l2h264dec \
  -vc "video/x-raw,format=NV12"
```

The launcher supplies `kmssink` and its DRM descriptor. This selects V4L2
hardware decode with no software fallback and NV12 direct scanout. Leave
UxPlay's default timestamp synchronization enabled; `-vsync no` was tried but
caused audible/video desynchronization. The direct caps path is simpler than
`videoconvert ! video/x-raw,format=NV12`; short live samples measured about
22.9 scanouts/s with the converter at 500x1080 and 22.7/s direct at 1920x888.
Since those resolutions differ, they do not establish a speedup or slowdown.
The direct caps path remains the selected NV12-native configuration.

## Verification and observed results

The original startup failure was `poll error` with the stock plugin. With the
startup polling fix, 30 independent VPU starts decoded 1,800 frames across
1280x720 and 500x1080 fixtures; first-frame luma hashes matched the software
reference. Direct NV12 HDMI scanout completed 60/60 frames, with decoder
DMA-BUF imports visible in DRM state.

For the rotation regression, capture a private H.264 sample from the iPhone,
including portrait and landscape SPS changes. Replay through `h264parse !`
`v4l2h264dec` while observing negotiated caps and output buffers. The old 1.24.2
path stopped after its first resolution change; the patched decoder continued
through repeated 500x1080 / 1920x888 changes on the VPU. The concatenated
capture loop has boundary warnings, so use it as a dynamic-caps regression
check, not as a fidelity or end-to-end frame-rate benchmark. On the actual
receiver, the user confirmed that repeated iPhone rotations no longer froze
the picture.

Live DRM framebuffer sampling measured roughly 22–23 scanouts per second.
Before verbose diagnostics were disabled, decoder logs showed about 30 input
frames/s and recurring QoS late-frame drops. Whole-system samples during
mirroring showed 84–97% CPU idle, about 1.2 GiB RAM available, no I/O wait,
and 56°C, so host-wide CPU, memory, thermal, and storage pressure was not the
cause. Turning off timestamp sync caused audio/video drift and was reverted.

For a fresh hardware check, reconnect the iPhone, play moving content, confirm
`/sys/kernel/debug/dri/0/state` shows the NV12 overlay plane with a changing
framebuffer ID, and rotate between portrait and landscape several times.
Use targeted GStreamer warnings for diagnosis; avoid per-frame `kmssink:6`
logging and `-vdmp` capture in normal operation.

To roll back, restore the saved service override and rebuild the unmodified
1.24.2 plugin into the same isolated prefix. Do not overwrite or remove the
distro GStreamer plugin; the service's private plugin path makes rollback
independent of system packages.

WHAT IT IS
----------

This is GStreamer, a framework for streaming media.

WHERE TO START
--------------

We have a website at

  https://gstreamer.freedesktop.org

Our documentation, including tutorials, API reference and FAQ can be found at

  https://gstreamer.freedesktop.org/documentation/

You can ask questions on the GStreamer Discourse at

  https://discourse.gstreamer.org/

We track bugs, feature requests and merge requests (patches) in GitLab at

  https://gitlab.freedesktop.org/gstreamer/

You can join us on our Matrix room at

  https://matrix.to/#/#gstreamer:gstreamer.org

GStreamer 1.0 series
--------------------

Starring

  GSTREAMER

The core around which all other modules revolve.  Base functionality and
libraries, some essential elements, documentation, and testing.

  BASE

A well-groomed and well-maintained collection of GStreamer plug-ins and
elements, spanning the range of possible types of elements one would want
to write for GStreamer.  

And introducing, for the first time ever, on the development screen ...

  THE GOOD

 --- "Such ingratitude.  After all the times I've saved your life."

A collection of plug-ins you'd want to have right next to you on the
battlefield.  Shooting sharp and making no mistakes, these plug-ins have it
all: good looks, good code, and good licensing.  Documented and dressed up
in tests.  If you're looking for a role model to base your own plug-in on,
here it is.

If you find a plot hole or a badly lip-synced line of code in them,
let us know - it is a matter of honour for us to ensure Blondie doesn't look
like he's been walking 100 miles through the desert without water.

  THE UGLY

  --- "When you have to shoot, shoot.  Don't talk."

There are times when the world needs a color between black and white.
Quality code to match the good's, but two-timing, backstabbing and ready to
sell your freedom down the river.  These plug-ins might have a patent noose
around their neck, or a lock-up license, or any other problem that makes you
think twice about shipping them.

We don't call them ugly because we like them less.  Does a mother love her
son less because he's not as pretty as the other ones ? No  - she commends
him on his great personality.  These plug-ins are the life of the party.
And we'll still step in and set them straight if you report any unacceptable
behaviour - because there are two kinds of people in the world, my friend:
those with a rope around their neck and the people who do the cutting.

  THE BAD

  --- "That an accusation?"

No perfectly groomed moustache or any amount of fine clothing is going to
cover up the truth - these plug-ins are Bad with a capital B. 
They look fine on the outside, and might even appear to get the job done, but
at the end of the day they're a black sheep. Without a golden-haired angel
to watch over them, they'll probably land in an unmarked grave at the final
showdown.

Don't bug us about their quality - exercise your Free Software rights,
patch up the offender and send us the patch on the fastest steed you can
steal from the Confederates. Because you see, in this world, there's two
kinds of people, my friend: those with loaded guns and those who dig.
You dig.

The Lowdown
-----------

  --- "I've never seen so many plug-ins wasted so badly."

GStreamer Plug-ins has grown so big that it's hard to separate the wheat from
the chaff.  Also, distributors have brought up issues about the legal status
of some of the plug-ins we ship.  To remedy this, we've divided the previous
set of available plug-ins into four modules:

- gst-plugins-base: a small and fixed set of plug-ins, covering a wide range
  of possible types of elements; these are continuously kept up-to-date
  with any core changes during the development series.

  - We believe distributors can safely ship these plug-ins.
  - People writing elements should base their code on these elements.
  - These elements come with examples, documentation, and regression tests.

- gst-plugins-good: a set of plug-ins that we consider to have good quality
  code, correct functionality, our preferred license (LGPL for the plug-in
  code, LGPL or LGPL-compatible for the supporting library).

  - We believe distributors can safely ship these plug-ins.
  - People writing elements should base their code on these elements.
 
- gst-plugins-ugly: a set of plug-ins that have good quality and correct
  functionality, but distributing them might pose problems.  The license
  on either the plug-ins or the supporting libraries might not be how we'd
  like. The code might be widely known to present patent problems.

  - Distributors should check if they want/can ship these plug-ins.
  - People writing elements should base their code on these elements.

- gst-plugins-bad: a set of plug-ins that aren't up to par compared to the
  rest.  They might be close to being good quality, but they're missing
  something - be it a good code review, some documentation, a set of tests,
  a real live maintainer, or some actual wide use.
  If the blanks are filled in they might be upgraded to become part of
  either gst-plugins-good or gst-plugins-ugly, depending on the other factors.

  - If the plug-ins break, you can't complain - instead, you can fix the
    problem and send us a patch, or bribe someone into fixing them for you.
  - New contributors can start here for things to work on.

PLATFORMS
---------

- Linux is of course fully supported
- FreeBSD is reported to work; other BSDs should work too; same for Solaris
- MacOS works, binary 1.x packages can be built using the cerbero build tool
- Windows works; binary 1.x packages can be built using the cerbero build tool
  - MSys/MinGW builds
  - Microsoft Visual Studio builds are also available and supported
- Android works, binary 1.x packages can be built using the cerbero build tool
- iOS works

INSTALLING FROM PACKAGES
------------------------

You should always prefer installing from packages first.  GStreamer is
well-maintained for a number of distributions, including Fedora, Debian,
Ubuntu, Mandrake, Arch Linux, Gentoo, ...

Only in cases where you:

 - want to hack on GStreamer
 - want to verify that a bug has been fixed
 - do not have a sane distribution

should you choose to build from source tarballs or git.

Find more information about the various packages at

  https://gstreamer.freedesktop.org/download/

For in-depth instructions about building GStreamer visit:
[getting-started](https://gitlab.freedesktop.org/gstreamer/gstreamer/-/blob/main/README.md#getting-started).

PLUG-IN DEPENDENCIES AND LICENSES
---------------------------------

GStreamer is developed under the terms of the LGPL (see COPYING file for
details). Some of our plug-ins however rely on libraries which are available
under other licenses. This means that if you are distributing an application
which has a non-GPL compatible license (for instance a closed-source
application) with GStreamer, you have to make sure not to distribute GPL-linked
plug-ins.

When using GPL-linked plug-ins, GStreamer is for all practical reasons
under the GPL itself.

HISTORY
-------

The fundamental design comes from the video pipeline at Oregon Graduate
Institute, as well as some ideas from DirectMedia.  It's based on plug-ins that
will provide the various codec and other functionality.  The interface
hopefully is generic enough for various companies (ahem, Apple) to release
binary codecs for Linux, until such time as they get a clue and release the
source.
