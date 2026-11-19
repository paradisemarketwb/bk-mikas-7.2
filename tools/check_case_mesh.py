#!/usr/bin/env python3
"""Check exported ASCII STL files for disconnected shells and open mesh edges."""
from collections import Counter
from pathlib import Path

folder = Path(__file__).resolve().parents[1] / 'hardware/case/r2/stl'
required = {'gauge-r2-body.stl','gauge-r2-rear.stl','gauge-r2-spacer.stl','adapter-r2-base.stl','adapter-r2-lid.stl','adapter-power-stack-r2-base.stl','adapter-power-stack-r2-lid.stl'}
assert required <= {p.name for p in folder.glob('*.stl')}, 'Missing required case exports'
for path in sorted(folder.glob('*.stl')):
    points = [tuple(map(float, line.split()[1:])) for line in path.read_text().splitlines()
              if line.strip().startswith('vertex')]
    assert points and len(points) % 3 == 0, path
    parent = {point: point for point in points}
    edges = Counter()

    def root(vertex):
        while parent[vertex] != vertex:
            parent[vertex] = parent[parent[vertex]]
            vertex = parent[vertex]
        return vertex

    for i in range(0, len(points), 3):
        a, b, c = points[i:i + 3]
        for v, w in ((a, b), (b, c), (c, a)):
            parent[root(v)] = root(w)
            edges[tuple(sorted((v, w)))] += 1
    assert len({root(v) for v in parent}) == 1, f'{path}: disconnected shells'
    assert all(count == 2 for count in edges.values()), f'{path}: open/nonmanifold edges'
    print(f'PASS {path.name}: one closed shell, {len(points) // 3} triangles')
