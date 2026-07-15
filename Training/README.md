# Training data 

Output of the neuroevolution runs that underlie the manuscript. One subtree per
experiment (`e1`, `e2`, `e3`), one folder per selection method / hyperparameter
configuration, and within each, per-seed files for 30 independent GA seeds.

## Layout

```
Training/
  e1/  e2/  e3/                     # one per experiment
    Elitism/ Truncation/ SUS/       # selection methods
    SUS_H2*/ SUS_H3*/ SUS_H4*/      # hyperparameter sweeps (reduced mutation / genome / population)
      best_genomes_<exp>_seed_<NN>.txt   # winning genome per seed (topology + weights)
      train_<exp>_<method>_seed_<NN>.csv # per-generation fitness / diversity log
```

- `best_genomes_*.txt` — one evolved network per seed, in the genome encoding
  described in the manuscript (Methods, "Genome encoding"): each connection is
  `(sourceType, targetType, sourceId, targetId, weight)`.
- `train_*.csv` — per-generation record written by
  `ASimulationManager::WriteTrainingGenerationRow` (avg/max fitness, genetic
  diversity), consumed by `Plotting/01_import_training_data.R`.

## Final configurations used in the manuscript

| Experiment | Winning configuration | Folder |
|---|---|---|
| E1 | SUS, baseline | `e1/SUS` |
| E2 | SUS, reduced population | `e2/SUS_H4b` |
| E3 | SUS, reduced genome | `e3/SUS_H3` |

## Data 

The per-generation logs (`train_*.csv`) and evolved genomes (`best_genomes_*.txt`) described above are **archived on Zenodo** (10.5281/zenodo.21381268). 
Download the archive and restore its `training/` folder into this `Training/` directory to run the training-side analysis.
