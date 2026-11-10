# RMSA for Elastic Optical Networks

Routing, Modulation and Spectrum Assignment (RMSA) for static traffic on two reference topologies, with optional dedicated or shared path protection. Every run produces a LaTeX report with performance metrics and plots.

- Spectrum: 320 frequency slot units (FSUs) of 12.5 GHz per link, with one guard-band FSU after each channel.
- Topologies: `G7` (German, 7 nodes, 11 bidirectional links) and `IT10` (Italian, 10 nodes, 15 bidirectional links), with five traffic matrices each.

## Layout

| File | Contents |
| --- | --- |
| `solution.c` | Entry point, scenario definitions, report generation and metrics |
| `routing_solution.c/.h` | Routing, modulation format and slot assignment algorithms |
| `network.c/.h` | Graph representation, Dijkstra, k-shortest paths (with link validators) |
| `problem.c/.h` | Topologies, traffic matrices, node coordinates and modulation formats |
| `graph_drawing.c/.h` | LaTeX/TikZ/pgfplots output: topology graphs, bar plots, spectrum maps |
| `utils.c/.h` | Dynamic arrays |
| `resources/` | Original topology and traffic matrix files (the data is hardcoded in `problem.c`) |
| `reports/` | Generated reports (`.tex` and `.pdf`) for every scenario |

## Algorithms

Requests (one per non-zero entry of the traffic matrix) are sorted by load, in ascending or descending order, and then go through three stages.

**Routing**
- `FIXED_SHORTEST_PATH` (benchmark): shortest path in km.
- `LEAST_USED_PATH_JOINT` (custom): among the 5 shortest paths, pick the one minimizing `max link load + 1000 × hops` (`LENGTH_LOAD_PONDERATION`). Afterwards, requests crossing the most loaded link are rerouted for as long as doing so lowers the peak load.

**Protection**
- `NO_PROTECTION`
- `DEDICATED_PROTECTION`: a node-disjoint backup path per request, with its own spectrum.
- `SHARED_PROTECTION`: node-disjoint backup paths that can reuse spectrum reserved by other backup paths, as long as the working paths they protect cannot fail together.

**Modulation**: the cheapest format whose reach covers the path and whose line rate covers the load. If no single format is enough, the load is split over several transponders on the same path.

| Format | Line rate | Bandwidth | Reach | Cost |
| --- | --- | --- | --- | --- |
| SC-DP-QPSK | 100 Gb/s | 37.5 GHz | 2000 km | 1.5 |
| SC-DP-16QAM | 200 Gb/s | 37.5 GHz | 700 km | 2.0 |
| DP-16QAM | 400 Gb/s | 75 GHz | 500 km | 3.7 |

**Spectrum assignment** (continuous and contiguous)
- `LEAST_USED_SLOT`: among the free blocks along the whole path, pick the one whose slots are least used across the network.
- `FIRST_FIT_SLOT`: the lowest free block.

## Building and running

```sh
gcc -pedantic -Wall -std=c99 -g *.c -lm
./a.out [g7] [it10] [benchmark] [nop] [dedicated] [shared]
```

With no network or scenario given, all of them run (80 reports). For example, `./a.out g7 shared` only writes the 10 G7 shared-protection reports.

| Scenario | Routing | Protection |
| --- | --- | --- |
| `benchmark` | Fixed shortest path | None |
| `nop` | Custom | None |
| `dedicated` | Custom | Dedicated |
| `shared` | Custom | Shared |

All scenarios use least-used slot assignment.

To build, run and compile every report to PDF in one step:

```sh
./run.sh [args]       # sequential latexmk
./runmulti.sh [args]  # 12 latexmk jobs in parallel
```

Both scripts use a local `latexmk` if there is one, and otherwise the `texlive/texlive` Docker image.

## Reports

Files are named `<network>_<matrix>-<scenario>-<order>`, where the scenario is `BENCHMARK`, `NOP`, `DEDP` or `SHP` and the order is `ASC` or `DESC` (request sort order). For example, `IT10_3-SHP-ASC.pdf`.

Each report contains:
- **Topology**: link lengths in km.
- **Performance parameters**: highest link load, highest used FSU, total used FSUs, transponder cost and transponders per modulation format.
- **Link load map**: load per direction, colored from lowest to highest, and the **link load distribution**.
- **Spectrum occupancy**: per-link FSU map of working, protection and guard-band slots.
- **Path length distribution** (and protection path lengths for dedicated protection).
- **Utilization entropy** and **Shannon entropy**: per-link fragmentation shown on the topology, plus their distribution.
