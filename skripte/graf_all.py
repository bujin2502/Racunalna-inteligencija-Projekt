"""Uspoređuje više runova: crta najbolji, najgori i medijanski run te prosjek svih runova."""

import argparse
import re
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd


def broj_runa(putanja: Path) -> int:
    m = re.search(r"_run_(\d+)\.csv$", putanja.name)
    return int(m.group(1)) if m else -1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mapa", nargs="?", default="rezultati/izvorna_verzija/runovi",
                        help="mapa s datotekama best_fitness_per_generation_run_*.csv")
    parser.add_argument("-o", "--izlaz", help="spremi sliku u datoteku umjesto prikaza")
    args = parser.parse_args()

    datoteke = sorted(Path(args.mapa).glob("best_fitness_per_generation_run_*.csv"), key=broj_runa)
    if not datoteke:
        parser.error(f"u mapi '{args.mapa}' nema datoteka best_fitness_per_generation_run_*.csv")

    runovi = [pd.read_csv(f, header=None, names=["Generacija", "Fitness"]) for f in datoteke]
    zadnji = np.array([df["Fitness"].iloc[-1] for df in runovi])
    poredak = np.argsort(zadnji)

    print(f"Broj runova: {len(runovi)}")
    print(f"Najbolji: {zadnji.min()}, najgori: {zadnji.max()}, "
          f"prosjek: {zadnji.mean():.1f}, medijan: {np.median(zadnji):.1f}, "
          f"std: {zadnji.std(ddof=1) if len(zadnji) > 1 else 0:.1f}")

    fig, ax = plt.subplots(figsize=(10, 6))
    for idx, oznaka in [(poredak[0], "Najbolji"), (poredak[-1], "Najgori"), (poredak[len(poredak) // 2], "Medijan")]:
        df = runovi[idx]
        ax.plot(df["Generacija"].to_numpy(), df["Fitness"].to_numpy(),
                label=f"{oznaka} (run {broj_runa(datoteke[idx])})")

    duljina = min(len(df) for df in runovi)
    prosjek = np.mean([df["Fitness"].to_numpy()[:duljina] for df in runovi], axis=0)
    ax.plot(runovi[0]["Generacija"].to_numpy()[:duljina], prosjek, linestyle="--", label="Prosjek")

    ax.set_title(f"Fitness po generaciji ({len(runovi)} runova)")
    ax.set_xlabel("Generacija")
    ax.set_ylabel("Fitness (makespan)")
    ax.grid(True)
    ax.legend()

    if args.izlaz:
        fig.savefig(args.izlaz, dpi=150, bbox_inches="tight")
    else:
        plt.show()


if __name__ == "__main__":
    main()
