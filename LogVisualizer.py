import matplotlib.pyplot as plt
import numpy as np
import glob

# data is a sequence of (start, duration) tuples
# cpu_1 = [(0, 3), (3.5, 1), (5, 5)]
# cpu_2 = np.column_stack([np.linspace(0, 9, 10), np.full(10, 0.5)])
# cpu_3 = np.column_stack([10*np.random.random(61), np.full(61, 0.05)])
# cpu_4 = [(2, 1.7), (7, 1.2)]
# disk = [(1, 1.5)]
# network = np.column_stack([10*np.random.random(10), np.full(10, 0.05)])

# fig, ax = plt.subplots()
# # broken_barh(xranges, (ymin, height))
# ax.broken_barh(cpu_1, (-0.2, 0.4))
# ax.broken_barh(cpu_2, (0.8, 0.4))
# ax.broken_barh(cpu_3, (1.8, 0.4))
# ax.broken_barh(cpu_4, (2.8, 0.4))
# ax.broken_barh(disk, (3.8, 0.4), color="tab:orange")
# ax.broken_barh(network, (4.8, 0.4), color="tab:green")
# ax.set_xlim(0, 10)
# ax.set_yticks(range(6),
#               labels=["CPU 1", "CPU 2", "CPU 3", "CPU 4", "disk", "network"])
# ax.invert_yaxis()
# ax.set_title("Resource usage")

# plt.show()

def parse_file_line(fp) -> tuple:
    pass

def parse_file(fp) -> list[tuple]:
    r = []
    with open(fp, "r") as f:
        lines = f.readlines()
        for l in lines:
            pass
    return r

def get_segments(files) -> list[tuple]:
    r = []
    for f in files:
        r += parse_file(f)
    return r

def main():
    files = glob.glob("../worker_thread_*.txt")
    

    print(files)

    # (thread_id, start, duration, label, color)
    segments = [
        (0, 0, 5,  "A", "tab:blue"),
        (0, 6, 3,  "B", "tab:orange"),
        (0, 10, 7, "C", "tab:green"),

        (1, 1, 4,  "D", "tab:red"),
        (1, 6, 5,  "E", "tab:purple"),
        (1, 12, 6, "F", "tab:brown"),
    ]

    fig, ax = plt.subplots()

    row_height = 8
    gap = 4

    for thread_id, start, duration, label, color in segments:
        y_base = thread_id * (row_height + gap)

        # draw bar
        ax.broken_barh(
            [(start, duration)],
            (y_base, row_height),
            facecolors=color,
            edgecolors="black"
        )

        # label inside each bar
        ax.text(
            start + 0.2,
            y_base + row_height / 2,
            label,
            ha="left",
            va="center",
            fontsize=9,
            color="white"
        )

    # y-axis labeling (threads)
    ax.set_yticks([
        row_height / 2,
        (row_height + gap) + row_height / 2
    ])
    ax.set_yticklabels(["Thread 0", "Thread 1"])

    ax.set_xlabel("Time")
    ax.set_ylim(0, 2 * (row_height + gap))
    ax.set_xlim(0, 20)

    plt.show()
    pass

main()
