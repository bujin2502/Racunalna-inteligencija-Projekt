"""Crta Ganttov dijagram najboljeg rasporeda iz CSV-a koji generira GA_JSSP."""

import argparse

import matplotlib.patches as mpatches
import matplotlib.pyplot as plt
import pandas as pd


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv", nargs="?", default="best_schedule.csv",
                        help="CSV s rasporedom (zadano: best_schedule.csv)")
    parser.add_argument("-o", "--izlaz", help="spremi sliku u datoteku umjesto prikaza (npr. gantt.png)")
    args = parser.parse_args()

    df = pd.read_csv(args.csv)
    boje = plt.cm.tab20.colors
    poslovi = sorted(df["Job ID"].unique())
    strojevi = sorted(df["Machine ID"].unique())

    fig, ax = plt.subplots(figsize=(14, 8))
    fig.subplots_adjust(right=0.86)

    for i, posao in enumerate(poslovi):
        for _, op in df[df["Job ID"] == posao].iterrows():
            stroj = op["Machine ID"]
            ax.broken_barh([(op["Start Time"], op["End Time"] - op["Start Time"])], (i + 0.55, 0.9),
                           facecolors=boje[stroj % len(boje)])
            ax.text((op["Start Time"] + op["End Time"]) / 2, i + 1, f"S{stroj}",
                    ha="center", va="center", color="white", fontsize=6)

    ax.set_xlim(0, df["End Time"].max() + 10)
    ax.set_ylim(0, len(poslovi) + 1)
    ax.set_yticks(range(1, len(poslovi) + 1))
    ax.set_yticklabels([f"Posao {p + 1}" for p in poslovi])
    ax.set_xlabel("Vrijeme")
    ax.set_ylabel("Posao")
    ax.set_title(f"Ganttov dijagram (makespan = {df['End Time'].max()})")

    legenda = [mpatches.Patch(color=boje[s % len(boje)], label=f"Stroj {s}") for s in strojevi]
    ax.legend(handles=legenda, bbox_to_anchor=(1.01, 1), loc="upper left")

    if args.izlaz:
        fig.savefig(args.izlaz, dpi=150, bbox_inches="tight")
    else:
        plt.show()


if __name__ == "__main__":
    main()
