"""Crta graf najboljeg fitnessa (makespana) po generaciji za jedan run."""

import argparse

import matplotlib.pyplot as plt
import pandas as pd


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv", nargs="?", default="best_fitness_per_generation.csv",
                        help="CSV s fitnessom po generaciji (zadano: best_fitness_per_generation.csv)")
    parser.add_argument("-o", "--izlaz", help="spremi sliku u datoteku umjesto prikaza (npr. fitness.png)")
    args = parser.parse_args()

    df = pd.read_csv(args.csv, header=None, names=["Generacija", "Fitness"])

    fig, ax = plt.subplots(figsize=(10, 6))
    ax.plot(df["Generacija"].to_numpy(), df["Fitness"].to_numpy())
    ax.set_title("Najbolji fitness po generaciji")
    ax.set_xlabel("Generacija")
    ax.set_ylabel("Fitness (makespan)")
    ax.grid(True)

    if args.izlaz:
        fig.savefig(args.izlaz, dpi=150, bbox_inches="tight")
    else:
        plt.show()


if __name__ == "__main__":
    main()
