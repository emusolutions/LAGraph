import os
import click
import json

try:
    import tomllib
except ImportError:
    try:
        import tomli as tomllib
    except ImportError:
        print("You need Python 3.11 or to install tomli with `pip install tomli`")

import pandas as pd


def summarize_hooks_data(data: pd.DataFrame) -> pd.Series:
    regions = data["region_name"].unique()
    print("[PARSER]: Found the following hook regions: ", regions)
    summary_data = pd.Series()
    for r in regions:
        mean_sec = data.loc[data["region_name"] == r]["time_ms"].mean() / 1000
        # std_sec = data.loc[data["region_name"] == r]["time_ms"].std() / 1000
        # max_sec = data.loc[data["region_name"] == r]["time_ms"].max() / 1000
        # min_sec = data.loc[data["region_name"] == r]["time_ms"].min() / 1000
        summary_data[f"{r} mean"] = [mean_sec]
        # summary_data[f"{r} std. dev."] = [std_sec]
        # summary_data[f"{r} max"] = [max_sec]
        # summary_data[f"{r} min"] = [min_sec]

    print("[PARSER]: Summary data:", summary_data)
    return summary_data


def check_for_avg_iter(line: str, d_sum: dict) -> bool:
    # BFS
    if "Avg: BFS pushpull parent only" in line:
        if len(line.split()) != 10:
            raise AssertionError("Something went wrong parsing the avg bfs time")
        d_sum["avg. iter (s)"].append(float(line.split()[7]))
        return True

    # SSSP
    elif "Avg: SSSP" in line:
        d_sum["avg. iter (s)"].append(float(line.split()[9]))
        return True

    # BC
    elif "Ave BC" in line:
        d_sum["avg. iter (s)"].append(float(line.split()[3]))
        return True

    # Pagerank
    elif "STD:   1: avg time:" in line:
        d_sum["avg. iter (s)"].append(float(line.split()[4]))
        return True

    return False


@click.command()
@click.option("--data_dir", default=".", help="The data directory you want to process")
@click.option("--output", default="", help="The filename to save data, must be csv.")
def main(data_dir: str, output: str):
    if not output.endswith("csv"):
        raise ValueError("Output must be a CSV!")

    data = []
    for f in os.listdir(data_dir):
        if f.endswith(".toml"):
            print(f"Reading data from {f}")
            try:
                with open(f"{data_dir}/{f}", "rb") as myfile:
                    data.append(tomllib.load(myfile))
                # print(data[-1])
            except tomllib.TOMLDecodeError:
                print(f"Looks like the job never finished. Skipping {data_dir}/{f}")

    # Fields we want to use in our final table
    d_sum = {
        "date": [],
        "image": [],
        "cluster": [],
        "nodes": [],
        "# nodes": [],
        "working dir": [],
        "toolchain version": [],
        "LGB commit": [],
        # "LGB cmake args": [],
        "LAGraph commit": [],
        # "LAGraph cmake args": [],
        "executable": [],
        "input matrix": [],
        "input rows": [],
        "input cols": [],
        "input edges": [],
        "wall time (s)": [],
        "read time (s)": [],
        "avg. iter (s)": [],
    }

    for d in data:
        d_sum["date"].append(d["date"])
        d_sum["image"].append(d["system-software"]["sc-driver"]["ncdimm"])
        d_sum["cluster"].append("condor6")
        d_sum["working dir"].append(d["execution"]["working_dir"])
        d_sum["nodes"].append(d["execution"]["nodes"][0])
        if "n_nodes" in d["execution"].keys():
            d_sum["# nodes"].append(d["execution"]["n_nodes"])
        else:
            d_sum["# nodes"].append(8)
        d_sum["toolchain version"].append("23.04")
        d_sum["LGB commit"].append(d["application-software"]["lgb"]["hash"][:7])
        # d_sum["LGB cmake args"].append(d["application-software"]["lgb"]["cmake_args"])
        d_sum["LAGraph commit"].append(d["application-software"]["lagraph"]["hash"][:7])
        # d_sum["LAGraph cmake args"].append(
        #     d["application-software"]["lagraph"]["cmake_args"]
        # )
        d_sum["executable"].append(d["problem"]["executable"].split("/")[-1])
        d_sum["input matrix"].append(d["problem"]["input"]["name"].split("/")[-1])
        d_sum["input rows"].append(d["problem"]["input"]["nrows"])
        d_sum["input cols"].append(d["problem"]["input"]["ncols"])
        d_sum["input edges"].append(d["problem"]["input"]["nnz"])

        #
        # Parsing the STDOUT
        #
        df_hooks = pd.DataFrame()
        df_hooks_i = pd.DataFrame()
        found_avg = False
        done_with_warmup = False
        for line in reversed(d["execution"]["stdout"].splitlines()):
            if "Elapsed (wall clock) time" in line:
                ti = line.split()[-1].split(":")
                print(ti)
                ti_s = 0.0
                # TODO THIS IS VERY FRAGILE
                if len(ti) == 3:
                    ti_s = float(ti[0]) * 3600 + float(ti[1]) * 60 + float(ti[2])
                elif len(ti) == 2:
                    ti_s = float(ti[0]) * 60 + float(ti[1])
                d_sum["wall time (s)"].append(ti_s)

            if "read time:" in line:
                d_sum["read time (s)"].append(float(line.split()[2]))

            if not found_avg:
                found_avg = check_for_avg_iter(line, d_sum)

            # TODO incredibly fragile, this is a patch to work with old data
            # generated without tracking the end of each iteration (i.e. a
            # single bfs call)
            if '"region_name"' in line:
                if "LAGr_BFS" in line and not df_hooks_i.empty and done_with_warmup:
                    df_hooks_i = df_hooks_i.groupby(["region_name"]).sum()
                    # print(df_hooks_i)
                    df_hooks = pd.concat(
                        [df_hooks, df_hooks_i],
                        # ignore_index=True,
                    )
                    # print(df_hooks)
                    df_hooks_i = pd.DataFrame()
                    df_hooks_i = pd.concat(
                        [df_hooks_i, pd.DataFrame([json.loads(line)])],
                        ignore_index=True,
                    )

                elif "LAGr_BFS" in line:
                    df_hooks_i = pd.concat(
                        [df_hooks_i, pd.DataFrame([json.loads(line)])],
                        ignore_index=True,
                    )
                    done_with_warmup = True
                elif done_with_warmup:
                    df_hooks_i = pd.concat(
                        [df_hooks_i, pd.DataFrame([json.loads(line)])],
                        ignore_index=True,
                    )

                # df_hooks = pd.concat(
                #     [df_hooks, pd.DataFrame([json.loads(line)])],
                #     ignore_index=True,
                # )

        if not df_hooks_i.empty and done_with_warmup:
            df_hooks_i = df_hooks_i.groupby(["region_name"]).mean()
            df_hooks = pd.concat(
                [df_hooks, df_hooks_i],
            )

        if not df_hooks.empty:
            df_hooks = df_hooks.reset_index()
            # print(df_hooks)
            df_hooks_summary = summarize_hooks_data(df_hooks).sort_index()
            for k, v in df_hooks_summary.items():
                if f"{k} (s)" in d_sum.keys():
                    d_sum[f"{k} (s)"].append(v[0])
                else:
                    d_sum[f"{k} (s)"] = v

        # print(d_sum)
        for k, v in d_sum.items():
            print(k, v, len(v))

    df = pd.DataFrame(d_sum).sort_values(by=["executable", "date"])

    print("\n" + "=" * 120)
    print(
        "Copy and past into the appropriate tab of https://lucata.box.com/s/hqrv7ska5878s4bs43jiu58uxymxhcls"
    )
    print("=" * 120)
    print(df.to_csv(index=False))

    print("\n" + "=" * 120)
    print(f"Save summary to {output}")
    print("=" * 120)
    df.to_csv(output, index=False)


if __name__ == "__main__":
    main()
