import re
import matplotlib.pyplot as plt
import numpy as np

def parse_tests(file_path):
    tests = []
    with open(file_path) as f:
        lines = f.readlines()

    current_test = []
    inside_part = False
    current_part_name = None
    data_for_test = {}
    seed_and_params = None

    for i, line in enumerate(lines):
        line = line.strip()
        if line.startswith("START_TEST"):
            current_test = []
            data_for_test = {}
            seed_and_params = None
        elif line.startswith("locked tests") or line.startswith("lock_free tests"):
            seed_and_params = line  # capture seed and params line
        elif line == "START_PART":
            inside_part = True
            # next line should be the part name
            current_part_name = lines[i+1].strip()
            data_for_test[current_part_name] = []
        elif line == "END_PART":
            inside_part = False
            current_part_name = None
        elif line == "END_TEST":
            # save current test
            if seed_and_params:
                tests.append((seed_and_params, data_for_test))
        elif inside_part and current_part_name:
            # parse thread/time line e.g. Threads: 1	Total Time: 40ms
            m = re.match(r"Threads:\s*(\d+)\s*Total Time:\s*(\d+)ms", line)
            if m:
                thread_num = int(m.group(1))
                time_sec = int(m.group(2)) / 1000.0
                data_for_test[current_part_name].append((thread_num, time_sec))
    return tests

def plot_tests_separately_and_mega(tests, filename_base):
    categories = ['STL-MTX', 'LF-P-1', 'LF-P-T', 'LF-LEAKS']

    # Create mega page figure with one subplot per test stacked vertically
    mega_fig_height = len(tests) * 4  # 4 inches height per subplot
    mega_fig, mega_axes = plt.subplots(len(tests), 1, figsize=(12, mega_fig_height), squeeze=False)

    for idx, (title_line, data) in enumerate(tests, start=1):
        ax = mega_axes[idx-1][0]

        # Extract seed and params for title, keep negative sign etc.
        seed_match = re.search(r"seed:\s*(\d+)", title_line)
        seed_str = seed_match.group(0) if seed_match else "seed: ?"
        pools_params_match = re.search(r"\|\s*(.+)$", title_line)
        pools_params = pools_params_match.group(1).strip() if pools_params_match else ""
        graph_title = f"({seed_str} | {pools_params})"

        ax.set_title(graph_title, fontsize=12)
        ax.set_xlabel("Threads")
        ax.set_ylabel("Total Time (s)")
        ax.set_yscale('log')

        # Determine all thread values that appear in this test (union of all categories)
        all_threads = set()
        for cat in categories:
            if cat in data:
                all_threads.update(t[0] for t in data[cat])
        all_threads = sorted(all_threads)
        ax.set_xticks(all_threads)
        ax.set_xlim(min(all_threads), max(all_threads))

        # Find max time to set y-limit
        max_time = 0
        for cat in categories:
            if cat in data:
                max_cat_time = max(t for _, t in data[cat])
                if max_cat_time > max_time:
                    max_time = max_cat_time
        y_max = 10 ** np.ceil(np.log10(max_time))
        ax.set_ylim(0.01, y_max)

        # Plot all except STL-MTX first
        for cat in categories:
            if cat != "STL-MTX" and cat in data:
                points = sorted(data[cat], key=lambda x: x[0])
                threads = [p[0] for p in points]
                times = [p[1] for p in points]
                ax.plot(threads, times, label=cat, marker='o')

        # Plot STL-MTX last to overlap
        if "STL-MTX" in data:
            points = sorted(data["STL-MTX"], key=lambda x: x[0])
            threads = [p[0] for p in points]
            times = [p[1] for p in points]
            ax.plot(threads, times, label="STL-MTX", marker='o')

        ax.legend()
        ax.grid(True, which="both", ls="--", alpha=0.5)

    plt.tight_layout()
    mega_save_name = f"{filename_base}_mega_page.svg"
    mega_fig.savefig(mega_save_name)
    print(f"Saved mega page graph as {mega_save_name}")
    plt.close(mega_fig)

    # Now also save individual test graphs as before
    for idx, (title_line, data) in enumerate(tests, start=1):
        plt.figure(figsize=(12, 6))
        seed_match = re.search(r"seed:\s*(\d+)", title_line)
        seed_str = seed_match.group(0) if seed_match else "seed: ?"
        pools_params_match = re.search(r"\|\s*(.+)$", title_line)
        pools_params = pools_params_match.group(1).strip() if pools_params_match else ""
        graph_title = f"({seed_str} | {pools_params})"

        plt.title(graph_title, fontsize=14)
        plt.xlabel("Threads")
        plt.ylabel("Total Time (s)")
        plt.yscale('log')

        all_threads = set()
        for cat in categories:
            if cat in data:
                all_threads.update(t[0] for t in data[cat])
        all_threads = sorted(all_threads)
        plt.xticks(all_threads)
        plt.xlim(min(all_threads), max(all_threads))

        max_time = 0
        for cat in categories:
            if cat in data:
                max_cat_time = max(t for _, t in data[cat])
                if max_cat_time > max_time:
                    max_time = max_cat_time
        y_max = 10 ** np.ceil(np.log10(max_time))
        plt.ylim(0.01, y_max)

        for cat in categories:
            if cat != "STL-MTX" and cat in data:
                points = sorted(data[cat], key=lambda x: x[0])
                threads = [p[0] for p in points]
                times = [p[1] for p in points]
                plt.plot(threads, times, label=cat, marker='o')

        if "STL-MTX" in data:
            points = sorted(data["STL-MTX"], key=lambda x: x[0])
            threads = [p[0] for p in points]
            times = [p[1] for p in points]
            plt.plot(threads, times, label="STL-MTX", marker='o')

        plt.legend()
        plt.grid(True, which="both", ls="--", alpha=0.5)
        plt.tight_layout()

        save_name = f"{filename_base}_test{idx}.svg"
        plt.savefig(save_name)
        plt.close()
        print(f"Saved graph for test {idx} as {save_name}")

if __name__ == "__main__":
    import sys
    if len(sys.argv) < 2:
        print("Usage: python script.py <input_file>")
        exit(1)

    input_file = sys.argv[1]
    filename_base = input_file.rsplit('.', 1)[0]
    tests = parse_tests(input_file)
    plot_tests_separately_and_mega(tests, filename_base)