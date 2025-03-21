import argparse
import os
import glob
import sys
import struct
from xml.dom import minidom


def main():
    """Main entrypoint for the command line program"""

    argparser = argparse.ArgumentParser(
        prog='convert', description=f'Convert .rocket xml file into binary files')
    argparser.add_argument('rocket', help='input rocket file')
    argparser.add_argument('shader', help='input shader frag file')
    argparser.add_argument('-o', '--output', default=os.getcwd(), metavar='str', help='output directory')

    args = argparser.parse_args()

    if not os.path.isdir(args.output):
        sys.exit('Output must be a directory.')

    outinc = os.path.join(args.output, "minirocket.inc")
    outheader = os.path.join(args.output, "minirocket.h")
    outshader = os.path.join(args.output, "shader_sync.frag")
    outtracknames = os.path.join(args.output, "minirocket_tracknames.h")

    file = minidom.parse(args.rocket)

    tracks = file.getElementsByTagName('tracks')[0]

    start_row = int(tracks.attributes['startRow'].value)
    end_row = int(tracks.attributes['endRow'].value)
    track = tracks.getElementsByTagName('track')
    starts = []
    rowtimes = []
    values = []
    types = []
    tracknames = []
    defines = dict()
    curstart = 0
    defines["ROW"] = str(0)
    for i, t in enumerate(track):
        k = [(row, float(k.attributes['value'].value), int(k.attributes['interpolation'].value))
             for k in t.getElementsByTagName('key') if start_row <= (row := int(k.attributes['row'].value)) <= end_row]
        k.sort(key=lambda k: k[0])
        if len(k) == 0:
            k = [(start_row, 0, 0)]
        if k[0][0] > start_row:
            k = [(start_row, k[0][1], 0)] + k
        trackname = str(t.attributes['name'].value)
        tracknames.append(trackname)
        defines[trackname.upper().replace('#', '_')] = str(i+1)
        starts += [curstart]
        curstart += len(k)
        rowtimes += [[(k[i + 1][0] - k[i][0]) if i < len(k) - 1 else 32767 for i, _ in enumerate(k)]]
        def tofixed(x): return min(max(round(x * 128)<<1, -32768), 32767)
        for e in k:
            f = e[1] * 256
            if f < -32768 or f > 32767:
                print(f"Warning: keyframe value {e[1]} as 8.8 fixed point will be clamped to {f}")
        values += [[tofixed(e[1])+e[2] for e in k]]
        types += [[e[2] for e in k]]
        types[-1][-1] = 0  # set the interpolation mode to 0 for the last keyframe point
    values += [[0]]  # for extra safety, add one zero in end so we never read past the array during interpolation

    with open(outinc, 'w') as file:
        file.write(f'numtracks equ {len(track)}\n' +
                   ''.join(r'%define ' + k + ' ' + v + '\n' for k, v in defines.items()) +
                   f'\n' +
                   f'section		.rktracks data	align=1\n' +
                   f'track_data:\n' +
                   f'    dw {",".join(str(v) for v in starts)}\n' +
                   f'\n' +
                   f'row_data:\n' +
                   ''.join('    dw ' + ','.join(str(v) for v in t) + '\n' for t in rowtimes) +
                   f'\n' +
                   f'value_data:\n' +
                   ''.join('    dw ' + ','.join(str(v) for v in t) + '\n' for t in values) +
                   f'\n')

    with open(outheader, 'w') as file:
        file.write('#ifndef MINIROCKET_H_\n' +
                   '#define MINIROCKET_H_\n' +
                   '\n' +
                   '#define RKT_NUMTRACKS ' + str(len(track)) + '\n' +
                   '\n' +
                   '#ifdef __cplusplus\n' +
                   'extern "C" {\n' +
                   '#endif\n' +
                   '\n' +
                   'void __stdcall minirocket_sync(float,void*);\n' +
                   '\n' +
                   '#ifdef __cplusplus\n' +
                   '}\n' +
                   '#endif\n' +
                   '\n' +
                   '#endif\n')

    with open(outshader, 'w') as outfile:
        with open(args.shader, 'r') as file:
            code = file.read().split("\n")
            line = next((i for i, v in enumerate(code) if v.strip().startswith("// SYNCS")), None)
            if line is None:
                print("error: no line starting with // SYNCS found in the shader")
                sys.exit(1)
            code[line] = ''.join('const int ' + k + ' = ' + v + ';\n' for k, v in defines.items()) + \
                "const int RKT_NUMTRACKS = " + str(len(track)) + ";\n"
            strcode = '\n'.join(code)
            outfile.write(strcode)

    with open(outtracknames, 'w') as file:
        file.write('static const char* s_trackNames[] = {\n' +
                   '    ' + ''.join(f'"{t}", ' for t in tracknames) + '\n' +
                   '};\n')
        file.write('#define RKT_NUMTRACKS ' + str(len(track)) + '\n')


if __name__ == '__main__':
    main()
