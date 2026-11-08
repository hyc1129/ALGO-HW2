#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
plot.py -- Plotting script for Algorithm Analysis PA2 (BST, AVL, Treap, Skip List)

Reads `raw_data.csv` and generates:
  - fig1.png : Experiment 1 (4 structures) -- X: n (log2 scale, 2^0..2^20), Y: log scale (ms)
  - fig2.png : Experiment 2 (4 structures) -- X: n (log2 scale, 2^0..2^20), Y: linear scale (ns)
  - fig3.png : Experiment 3 (7 structures) -- X: n (log2 scale, 2^0..2^20), Y: linear scale (levels / 層)
"""

import argparse
import os
import sys

import matplotlib
matplotlib.use("Agg")  # Headless-safe backend for all environments
import matplotlib.pyplot as plt
from matplotlib import font_manager
import pandas as pd


def configure_fonts() -> bool:
    """
    Configures Matplotlib to support Traditional Chinese (繁體中文) if available
    on the system, returning True if a CJK-capable font was found.
    """
    cjk_candidates = [
        "Microsoft JhengHei",
        "Microsoft YaHei",
        "PingFang TC",
        "Heiti TC",
        "Noto Sans CJK TC",
        "Noto Sans TC",
        "Source Han Sans TW",
        "Arial Unicode MS",
        "SimHei",
    ]
    available_fonts = {f.name for f in font_manager.fontManager.ttflist}
    for candidate in cjk_candidates:
        if candidate in available_fonts:
            plt.rcParams["font.sans-serif"] = [candidate, "DejaVu Sans"]
            plt.rcParams["axes.unicode_minus"] = False
            return True

    plt.rcParams["font.sans-serif"] = ["DejaVu Sans"]
    plt.rcParams["axes.unicode_minus"] = False
    return False


# Visual styles and explicit legend labels (including Delta and p) for each structure
STYLE_MAP = {
    "BST": {
        "label": "BST",
        "color": "#1f77b4",
        "marker": "o",
        "linestyle": "-",
    },
    "AVL(delta=1)": {
        "label": r"AVL tree ($\Delta = 1$)",
        "color": "#d62728",
        "marker": "s",
        "linestyle": "-",
    },
    "AVL(delta=5)": {
        "label": r"AVL tree ($\Delta = 5$)",
        "color": "#ff7f0e",
        "marker": "D",
        "linestyle": "--",
    },
    "Treap": {
        "label": "Treap (Min-Heap)",
        "color": "#2ca02c",
        "marker": "^",
        "linestyle": "--",
    },
    "SkipList(p=1/2)": {
        "label": r"Skip list ($p = 1/2$)",
        "color": "#9467bd",
        "marker": "v",
        "linestyle": "-",
    },
    "SkipList(p=3/4)": {
        "label": r"Skip list ($p = 3/4$)",
        "color": "#8c564b",
        "marker": "P",
        "linestyle": "-.",
    },
    "SkipList(p=1/4)": {
        "label": r"Skip list ($p = 1/4$)",
        "color": "#17becf",
        "marker": "X",
        "linestyle": ":",
    },
}

EXP1_ORDER = ["BST", "AVL(delta=1)", "Treap", "SkipList(p=1/2)"]
EXP2_ORDER = ["BST", "AVL(delta=1)", "Treap", "SkipList(p=1/2)"]
EXP3_ORDER = [
    "BST",
    "AVL(delta=1)",
    "AVL(delta=5)",
    "Treap",
    "SkipList(p=1/2)",
    "SkipList(p=3/4)",
    "SkipList(p=1/4)",
]


def apply_log2_x_axis(ax, has_cjk: bool) -> None:
    """
    Sets the X-axis to base-2 logarithmic scale with explicit tick marks and
    labels 2^0, 2^1, ..., 2^20.
    """
    ax.set_xscale("log", base=2)
    xticks = [2 ** k for k in range(21)]
    xtick_labels = [f"$2^{{{k}}}$" for k in range(21)]
    ax.set_xticks(xticks)
    ax.set_xticklabels(xtick_labels, fontsize=9)
    ax.set_xlim(2 ** -0.4, 2 ** 20.4)
    if has_cjk:
        ax.set_xlabel(r"插入資料筆數 $n$（$\log_2$ 刻度） / Number of Insertions $n$ ($\log_2$ scale)", fontsize=11)
    else:
        ax.set_xlabel(r"Number of Insertions $n$ ($\log_2$ scale)", fontsize=11)


def plot_curves(ax, df_exp: pd.DataFrame, structure_order: list) -> None:
    """
    Plots each structure's curve in the specified order with distinct markers and styles.
    """
    for struct_name in structure_order:
        sub = df_exp[df_exp["structure"] == struct_name].sort_values("n")
        if sub.empty:
            continue
        style = STYLE_MAP.get(
            struct_name,
            {"label": struct_name, "color": None, "marker": "o", "linestyle": "-"},
        )
        ax.plot(
            sub["n"],
            sub["mean_value"],
            label=style["label"],
            color=style["color"],
            marker=style["marker"],
            linestyle=style["linestyle"],
            linewidth=1.8,
            markersize=5.5,
            alpha=0.9,
        )


def generate_fig1(df: pd.DataFrame, out_path: str, has_cjk: bool) -> None:
    """
    Figure 1 (Experiment 1): Total Insertion Time vs. n
      - 4 curves: BST, AVL(delta=1), Treap, SkipList(p=1/2)
      - X-axis: n on log2 scale (2^0 .. 2^20)
      - Y-axis: log scale, unit = ms
    """
    df_exp1 = df[df["exp"] == 1]
    if df_exp1.empty:
        print("[Warning] No Experiment 1 rows (exp == 1) found in CSV; skipping fig1.png.")
        return

    fig, ax = plt.subplots(figsize=(10.5, 6.2), dpi=200)
    plot_curves(ax, df_exp1, EXP1_ORDER)
    apply_log2_x_axis(ax, has_cjk)

    ax.set_yscale("log")
    if has_cjk:
        ax.set_ylabel("平均總插入時間 (ms，對數刻度) / Avg Total Insert Time (ms, log scale)", fontsize=11)
        ax.set_title("圖 1（實驗一）：插入 n 筆資料的平均總時間 / Fig 1: Average Total Insertion Time vs. n", fontsize=13, pad=12)
    else:
        ax.set_ylabel("Average Total Insertion Time (ms, log scale)", fontsize=11)
        ax.set_title("Figure 1 (Experiment 1): Average Total Insertion Time vs. n", fontsize=13, pad=12)

    ax.grid(True, which="both", linestyle="--", linewidth=0.5, alpha=0.6)
    ax.legend(loc="upper left", fontsize=10, frameon=True)
    fig.tight_layout()
    fig.savefig(out_path, dpi=200)
    plt.close(fig)
    print(f"[Saved] {out_path}")


def generate_fig2(df: pd.DataFrame, out_path: str, has_cjk: bool) -> None:
    """
    Figure 2 (Experiment 2): Average Single SearchAVG Time vs. n
      - 4 curves: BST, AVL(delta=1), Treap, SkipList(p=1/2)
      - X-axis: n on log2 scale (2^0 .. 2^20)
      - Y-axis: linear scale, unit = ns
    """
    df_exp2 = df[df["exp"] == 2]
    if df_exp2.empty:
        print("[Warning] No Experiment 2 rows (exp == 2) found in CSV; skipping fig2.png.")
        return

    fig, ax = plt.subplots(figsize=(10.5, 6.2), dpi=200)
    plot_curves(ax, df_exp2, EXP2_ORDER)
    apply_log2_x_axis(ax, has_cjk)

    ax.set_yscale("linear")
    if has_cjk:
        ax.set_ylabel("單次搜尋平均時間 (ns，線性刻度) / Avg SearchAVG Time (ns, linear scale)", fontsize=11)
        ax.set_title("圖 2（實驗二）：一次 SearchAVG 的平均時間 (Q = 100,000) / Fig 2: Average SearchAVG Time vs. n", fontsize=13, pad=12)
    else:
        ax.set_ylabel("Average SearchAVG Time (ns, linear scale)", fontsize=11)
        ax.set_title("Figure 2 (Experiment 2): Average SearchAVG Time vs. n (Q = 100,000)", fontsize=13, pad=12)

    ax.grid(True, which="major", linestyle="--", linewidth=0.5, alpha=0.6)
    ax.legend(loc="upper left", fontsize=10, frameon=True)
    fig.tight_layout()
    fig.savefig(out_path, dpi=200)
    plt.close(fig)
    print(f"[Saved] {out_path}")


def generate_fig3(df: pd.DataFrame, out_path: str, has_cjk: bool) -> None:
    """
    Figure 3 (Experiment 3): Average Height of 7 Data Structures vs. n
      - 7 curves: BST, AVL(1), AVL(5), Treap, SkipList(1/2, 3/4, 1/4)
      - X-axis: n on log2 scale (2^0 .. 2^20)
      - Y-axis: linear scale, unit = levels (層)
    """
    df_exp3 = df[df["exp"] == 3]
    if df_exp3.empty:
        print("[Warning] No Experiment 3 rows (exp == 3) found in CSV; skipping fig3.png.")
        return

    fig, ax = plt.subplots(figsize=(10.5, 6.2), dpi=200)
    plot_curves(ax, df_exp3, EXP3_ORDER)
    apply_log2_x_axis(ax, has_cjk)

    ax.set_yscale("linear")
    if has_cjk:
        ax.set_ylabel("平均高度（層，線性刻度） / Average Height (levels, linear scale)", fontsize=11)
        ax.set_title("圖 3（實驗三）：七個資料結構的平均高度 / Fig 3: Average Height of 7 Data Structures vs. n", fontsize=13, pad=12)
    else:
        ax.set_ylabel("Average Height (levels, linear scale)", fontsize=11)
        ax.set_title("Figure 3 (Experiment 3): Average Height of 7 Data Structures vs. n", fontsize=13, pad=12)

    ax.grid(True, which="major", linestyle="--", linewidth=0.5, alpha=0.6)
    ax.legend(loc="upper left", fontsize=10, frameon=True)
    fig.tight_layout()
    fig.savefig(out_path, dpi=200)
    plt.close(fig)
    print(f"[Saved] {out_path}")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Generate fig1.png, fig2.png, and fig3.png from raw_data.csv."
    )
    parser.add_argument(
        "--input",
        default="raw_data.csv",
        help="Path to the unified raw CSV file (default: raw_data.csv).",
    )
    parser.add_argument(
        "--outdir",
        default=".",
        help="Directory to write fig1.png, fig2.png, and fig3.png (default: current directory).",
    )
    args = parser.parse_args()

    if not os.path.isfile(args.input):
        sys.stderr.write(
            f"[Error] Input CSV '{args.input}' not found.\n"
            "Please run `./treebench --exp all --trials 10 --seed 0` first.\n"
        )
        sys.exit(1)

    os.makedirs(args.outdir, exist_ok=True)
    has_cjk = configure_fonts()

    df = pd.read_csv(args.input)
    required_cols = {"exp", "n", "structure", "mean_value"}
    if not required_cols.issubset(df.columns):
        sys.stderr.write(
            f"[Error] CSV '{args.input}' is missing required columns {required_cols}.\n"
        )
        sys.exit(1)

    generate_fig1(df, os.path.join(args.outdir, "fig1.png"), has_cjk)
    generate_fig2(df, os.path.join(args.outdir, "fig2.png"), has_cjk)
    generate_fig3(df, os.path.join(args.outdir, "fig3.png"), has_cjk)


if __name__ == "__main__":
    main()
