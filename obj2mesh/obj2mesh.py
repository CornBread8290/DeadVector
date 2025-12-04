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
    pos = []
    uvs = []
    nrm = []
    faces = []

    with open(path, 'r') as f:
        for line in f:
            if line.startswith("v "):
                _, x, y, z = line.split()
                pos.append((float(x), float(y), float(z)))
            elif line.startswith("vt "):
                parts = line.split()
                uvs.append((float(parts[1]), float(parts[2])))
            elif line.startswith("vn "):
                _, x, y, z = line.split()
                nrm.append((float(x), float(y), float(z)))
            elif line.startswith("f "):
                verts = [parse_idx(v) for v in line.split()[1:]]
                for i in range(1, len(verts)-1):
                    faces.append([verts[0], verts[i], verts[i+1]])

    key_to_index = {}
    out_vertices = []
    out_indices = []

    for tri in faces:
        for (vi, ti, ni) in tri:
            key = (vi, ti, ni)
            if key not in key_to_index:
                px, py, pz = pos[vi]
                nx, ny, nz = nrm[ni] if ni >= 0 else (0,0,1)
                tu, tv = uvs[ti] if ti >= 0 else (0,0)
                key_to_index[key] = len(out_vertices)
                out_vertices.append((px,py,pz, nx,ny,nz, tu,tv))
            out_indices.append(key_to_index[key])

    verts_str = ", ".join(
        "{ {%.6ff,%.6ff,%.6ff}, {%.6ff,%.6ff,%.6ff}, {1,1,1,1}, {%.6ff,%.6ff} }" %
        v for v in out_vertices
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
