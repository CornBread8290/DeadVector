import os
import sys

# Force working directory to where this .py file is located
os.chdir(os.path.dirname(os.path.abspath(__file__)))

def parse_idx(x):
    parts = x.split('/')
    vi = int(parts[0]) - 1
    ti = int(parts[1]) - 1 if len(parts) > 1 and parts[1] else -1
    ni = int(parts[2]) - 1 if len(parts) > 2 and parts[2] else -1
    return vi, ti, ni

def convert_obj(path):
    pos, uvs, faces = [], [], []

    with open(path, 'r') as f:
        for line in f:
            if line.startswith("v "):
                _, x, y, z = line.split()
                pos.append((float(x), float(y), float(z)))
            elif line.startswith("vt "):
                parts = line.split()
                uvs.append((float(parts[1]), float(parts[2])))
            elif line.startswith("f "):
                verts = [parse_idx(v) for v in line.split()[1:]]
                for i in range(1, len(verts)-1):
                    faces.append([verts[0], verts[i], verts[i+1]])

    def r6(x):  # weld epsilon; tweak digits if needed
        return round(x, 6)

    key_to_index = {}
    out_pos = []
    out_uv  = []
    out_nrm = []
    out_indices = []

    # Build vertices (weld by position; keep UV seams if present)
    for tri in faces:
        tri_idx = []
        for (vi, ti, ni) in tri:
            px, py, pz = pos[vi]
            if ti >= 0:
                tu, tv = uvs[ti]
                key = (r6(px), r6(py), r6(pz), r6(tu), r6(tv))
            else:
                key = (r6(px), r6(py), r6(pz))

            idx = key_to_index.get(key)
            if idx is None:
                idx = len(out_pos)
                key_to_index[key] = idx
                out_pos.append((px, py, pz))
                out_uv.append((uvs[ti] if ti >= 0 else (0.0, 0.0)))
                out_nrm.append([0.0, 0.0, 0.0])  # accumulate
            tri_idx.append(idx)
        out_indices += tri_idx

        # Accumulate face normal
        i0, i1, i2 = tri_idx
        ax, ay, az = out_pos[i0]
        bx, by, bz = out_pos[i1]
        cx, cy, cz = out_pos[i2]
        abx, aby, abz = bx-ax, by-ay, bz-az
        acx, acy, acz = cx-ax, cy-ay, cz-az
        nx = aby*acz - abz*acy
        ny = abz*acx - abx*acz
        nz = abx*acy - aby*acx
        for i in (i0, i1, i2):
            out_nrm[i][0] += nx
            out_nrm[i][1] += ny
            out_nrm[i][2] += nz

    # Normalize normals
    out_vertices = []
    for (p, n, t) in zip(out_pos, out_nrm, out_uv):
        nx, ny, nz = n
        l = (nx*nx + ny*ny + nz*nz) ** 0.5
        if l > 1e-12:
            nx, ny, nz = nx/l, ny/l, nz/l
        else:
            nx, ny, nz = 0.0, 0.0, 1.0
        px, py, pz = p
        tu, tv = t
        out_vertices.append((px,py,pz, nx,ny,nz, tu,tv))

    verts_str = ", ".join(
        "{ {%.6ff,%.6ff,%.6ff}, {%.6ff,%.6ff,%.6ff}, {1,1,1,1}, {%.6ff,%.6ff} }" % v
        for v in out_vertices
    )
    inds_str = ", ".join(str(i) for i in out_indices)

    return (
            "{ "
            ".vertices = (VertexFormat[]){ %s }, "
            ".indices = (unsigned[]){ %s }, "
            ".vertex_count = %d, "
            ".index_count = %d "
            "}"
            % (verts_str, inds_str, len(out_vertices), len(out_indices))
    )

def main():
    objs = [f for f in os.listdir('.') if f.lower().endswith('.obj')]

    if not objs:
        print("No OBJ files found in this folder.")
        input("Press Enter to exit...")
        return

    for obj in objs:
        name = os.path.splitext(obj)[0]
        out_path = name + ".txt"
        print(f"Converting {obj} → {out_path}")
        try:
            data = convert_obj(obj)
            with open(out_path, 'w') as out:
                out.write(data)
        except Exception as e:
            print(f"Error converting {obj}: {e}")

    print("Done.")
    input("Press Enter to exit...")

if __name__ == "__main__":
    main()
