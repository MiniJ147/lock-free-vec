import re
import sys
import matplotlib.pyplot as plt
import math
import os

def parse_and_plot_tests(input_file):
    with open(input_file, 'r') as f:
        lines = [line.strip() for line in f]

    thread_re = re.compile(r"Threads:\s*(\d+)\s+Total Time:\s*(\d+)ms")
    section_names = ["STL-MTX", "LF-P-1", "LF-P-T", "LF-LEAKS"]

    tests_data = []
    i = 0

    while i < len(lines):
        if lines[i] == "START_TEST":
            # The line *after* START_TEST is the "locked tests | seed: 42 | pools: 1 | 15+ / 5- / 10w / 70r"
            title_line = None
            if i + 1 < len(lines):
                title_line = lines[i + 1]
            i += 2

            # Extract relevant title info: seed and last 4 parts
            # Example: locked tests | seed: 42 | pools: 1 | 15+ / 5- / 10w / 70r
            # We want: (seed: 42 | 15+ / 5- / 10w / 70r)
            seed_match = re.search(r"seed:\s*\d+", title_line) if title_line else None
            last_parts_match = re.search(r"(\d+\+ / \d+- / \d+w / \d+r)", title_line) if title_line else None
            if seed_match and last_parts_match:
                graph_title = f"({seed_match.group(0)} | {last_parts_match.group(1)})"
            else:
                graph_title = title_line if title_line else "Test"

            section_data = {name: {} for name in section_names}
            threads_set = set()
            max_time = 0.0

            while i < len(lines) and lines[i] != "END_TEST":
                if lines[i] == "START_PART":
                    name = lines[i + 1].strip()
                    i += 2
                    while i < len(lines) and lines[i] != "END_PART":
                        match = thread_re.match(lines[i])
                        if match:
                            threads = int(match.group(1))
                            time_sec = int(match.group(2)) / 1000.0
                            max_time = max(max_time, time_sec)
                            threads_set.add(threads)
                            if name in section_data:
                                section_data[name][threads] = time_sec
                        i += 1
                i += 1

            tests_data.append({
                "title": graph_title,
                "sections": section_data,
                "threads": sorted(threads_set),
                "max_time": max_time
            })
        else:
            i += 1

    # Plot all tests vertically stacked in one figure (mega page)
    num_tests = len(tests_data)
    fig, axs = plt.subplots(num_tests, 1, figsize=(10, 5 * num_tests), squeeze=False)
    axs = axs.flatten()

    for idx, test in enumerate(tests_data):
        ax = axs[idx]
        section_data = test["sections"]
        sorted_threads = test["threads"]
        max_time = test["max_time"]
        y_max = 10 ** math.ceil(math.log10(max(max_time, 0.01)))

        for name in section_names:
            x_vals = []
            y_vals = []
            for t in sorted_threads:
                val = section_data[name].get(t)
                if val is not None:
                    x_vals.append(t)
                    y_vals.append(val)
            if x_vals:
                ax.plot(x_vals, y_vals, marker='o', label=name)

        ax.set_xscale("linear")
        ax.set_yscale("log")
        ax.set_ylim(0.01, y_max)
        ax.set_xticks(sorted_threads)
        ax.set_xlabel("Threads")
        ax.set_ylabel("Time (s)")
        ax.set_title(test["title"])
        ax.grid(True, which="both", ls="--", linewidth=0.5)
        ax.legend()

    plt.tight_layout()
    
    base_name = os.path.splitext(os.path.basename(input_file))[0]
    output_file = f"{base_name}_graph.svg"
    plt.savefig(output_file)
    plt.close()
    print(f"Saved mega page graph as {output_file}")

# Example usage
parse_and_plot_tests(sys.argv[1])