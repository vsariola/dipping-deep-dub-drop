# dipping deep dub drop

A windows 4k intro by chlumpie / rebels & pestis / brainlez Coders!, released at
Revision 2025.

Source: https://github.com/vsariola/dipping-deep-dub-drop

Capture: https://youtu.be/tAPOfutb-0c

## Prerequisites for building

Rocket is included as a submodule in the repo, so you should clone it
with e.g.
`git clone --depth=1 --recursive https://github.com/vsariola/dipping-deep-dub-drop`

Following tools should be in path:

1. [nasm](https://www.nasm.us/)
2. [Python 3](https://www.python.org/)
3. [Shader-minifier](https://github.com/laurentlb/Shader_Minifier)
4. [Crinkler](https://github.com/runestubbe/Crinkler) Note: As crinkler.exe, not link.exe
5. Optionally: [glslangValidator](https://github.com/KhronosGroup/glslang)

So far, building has been tested with Visual Studio 2022. Make sure you also
installed [CMake](https://cmake.org/) with it as the build was automated with
CMake.

## Build

1. Open the repository folder using Visual Studio
2. Choose the configuration (heavy-1080 is the compo version).
   Light/medium/heavy refers to compression level, 720/1080/1440/2160 the
   resolution. Debug versions are for development.
3. Build & run.

CMake should copy the exes into the dist/ folder.

## How to sync

Choose one of the sync-XXX configurations, which has SYNC macro defined. Build
it and then:

1. Run this rocket server: https://github.com/emoon/rocket
2. Then run the sync build intro. Note that if you try to sync.exe
   before running the server, it just closes. So the server needs to be
   ran first.
3. With the server, open the data/syncs.rocket and start syncing. Then
   save your changes back to the XML.

If you need more sync tracks, just manually add a new empty track to the
syncs.rocket XML before building the `sync` target
(`<track name="mygroup#mytrackname"/>`). When building intro, the file
will get processed and the executable will become aware of the sync
variable. The new track will appear as a const variable in the shader.
For a track named `mygroup#mytrackname`, a constant
`MYGROUP_MYTRACKNAME` will be available to the shader, and you can
access the variable with `syncs[MYGROUP_MYTRACKNAME]`. The `#` grouping
character is replaced with `_` and the string is made uppercase.

Notice that when building the final intro, the sync key values are stored as
signed 8.8 fixed point. Thus, never use values outside the range -128 <= x <
128.

## How does it actually work

No, we don't actually zoom into any julia or other fractal that deep. I think it
would not be impossible, but would still be actually challenging to make it this
fast & fit it all in 4k. Rather, we cheat.

In 2D, we calculate the "ring" a point is on by computing floor(log(length(p))).
Each ring is then actually the exact  same julia or some other fractal, but just
slightly rotated. In this log space, it is easy to zoom with addition with
something like floor(log(length(p))+zoom). Now, to hide that it's just actually
repeating rings of the same fractal, we linearly blend from current ring to the
next.

In the very beginning of the intro, you see that there is something wrong with
the julia fractal: julia fractal is supposed to be point symmetric but the
fractal is not.

## What was learned this time

- Be careful with /HASHSIZE:1000 in Crinkler! And always make a Crinkled debug
  version that checks every Windows function returns correct HRESULT (e.g.
  S_OK), e.g. by MessageBoxing the expected & got value and exiting. We had a
  case of DirectSoundCreate already failing due to out-of-memory error in the
  Crinkled version, but only on Gargaj's machine. Not checking the value causes
  several hours of wasted time. The fix was to use smaller hash (e.g.
  /HASHSIZE:100), which was not issue at all becuase we were not even close to
  being out of bytes. Interestingly, the rest of the intro takes only ~ 300 megs
  of memory, so in theory, /HASHSIZE:1000 should not be too big, even with the ~
  2-4 gig limits of 32-bit modes. But there still was an issue.
- How to make DOF (I know, this is pretty basic): there's two passes, with the
  first writing color and depth info in alpha channel (actually, the amount of
  blur needed). Then in the second pass it does the blur.

## License

[MIT](LICENSE)