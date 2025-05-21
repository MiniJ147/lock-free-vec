import re
import sys
import matplotlib.pyplot as plt
import math

def parse_and_plot_tests(input_file):
    with open(input_file, 'r') as f:
        lines = [line.strip() for line in f]

    thread_re = re.compile(r"Threads:\s*(\d+)\s+Total Time:\s*(\d+)ms")
    section_names = ["STL-MTX", "LF-P-1", "LF-P-T", "LF-LEAKS"]

    tests_data = []
    i = 0

    while i < len(lines):
        if lines[i] == "START_TEST":
            i += 1
            if i >= len(lines):
                break
            test_title_raw = lines[i].strip()
            i += 1

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
                "title": test_title_raw,
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
    plt.savefig("mega_page.svg")
    plt.close()

# Run the function on your input file
parse_and_plot_tests(sys.argv[1])