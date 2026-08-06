# Coral Larvae NE-ABM

A neuroevolution-driven **agent-based model of coral larval settlement**, built in
Unreal Engine 5.8 (C++). Each larva is an agent whose neural-network "brain" is evolved by
a genetic algorithm to settle in response to environmental cues: crustose coralline algae
(CCA), biofilm, depth, sound, and water current. The model replicates three published
laboratory experiments (E1/E2/E3) and is under revision for **PLOS ONE**.

## Repository layout

| Path | What |
|---|---|
| `coral-larvae-abm/` | Unreal Engine 5 C++ project |
| `Plotting/` | Analysis pipeline and scripts |
| `Training/` | Training output per experiment/method |

## The three experiments

- **E1** — CCA / patch-size settlement [Sampayo et al. 2020](https://link.springer.com/article/10.1007/s00338-020-01901-1)
- **E2** — vertical distribution in a tall tube [Tay et al. 2011](https://www.sciencedirect.com/science/article/abs/pii/S0022098111004230)
- **E3** — phonotaxis toward reef sound [Vermeij et al. 2010](https://journals.plos.org/plosone/article?id=10.1371/journal.pone.0010660)

Details, exact comparison numbers, and validation scenarios: link coming soon.

## Getting started

The model runs inside the Unreal editor; analysis runs in R + Python. 

- **Build** the C++ in an IDE (full rebuild for header changes; close the editor first).
- **Train** via the automated batch-seed driver on the `SimulationManager` (one Play runs a
  whole seed sweep, writing genomes + CSVs into `Training/`).
- **Analyze** with the `Plotting/` scripts (needs `numpy`, `openpyxl`).

## Data availability
This repository contains the **code** and the small result artifacts (figures in `Plotting/output/plots/`, summary tables in `Plotting/output/tables/`). All larger **data** — per-agent validation positions, controller trajectory logs, and the full neuroevolution training output (per-generation logs and evolved genomes) — is archived, with a citable DOI, at 10.5281/zenodo.21381268. 
To reproduce figures that need the raw data, download the Zenodo archive and restore: `positions/*` and `extracts/*` → `Plotting/output/data/`, `trajectories/*` → `coral-larvae-abm/Content/Evolution/`, `training/*` → `Training/`.

## License

Released under the [MIT License](LICENSE).

## Acknowledgements

The genome-encoded neural-controller representation is adapted from David R. Miller's [biosim4](https://github.com/davidrmiller/biosim4) (MIT License).
