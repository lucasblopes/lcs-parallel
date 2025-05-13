import os
import re
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
from pathlib import Path
import math

# Configurações
string_sizes = [100, 1000, 10000, 100000, 110000, 120000, 130000]
num_threads = [1, 2, 4, 8, 12, 16, 32]
num_runs = 20
logs_dir = "./logs/"


# Função para extrair dados de um arquivo de log
def extract_data_from_log(filepath):
    try:
        with open(filepath, "r") as file:
            content = file.read()

            # Usando expressões regulares para extrair os tempos
            total_time_match = re.search(r"Total time: (\d+\.\d+)s", content)
            parallel_time_match = re.search(r"Parallel time: (\d+\.\d+)s", content)
            sequential_time_match = re.search(r"Sequential time: (\d+\.\d+)s", content)
            score_match = re.search(r"Score: (\d+)", content)

            if (
                total_time_match
                and parallel_time_match
                and sequential_time_match
                and score_match
            ):
                total_time = float(total_time_match.group(1))
                parallel_time = float(parallel_time_match.group(1))
                sequential_time = float(sequential_time_match.group(1))
                score = int(score_match.group(1))

                return {
                    "total_time": total_time,
                    "parallel_time": parallel_time,
                    "sequential_time": sequential_time,
                    "score": score,
                }
            else:
                print(f"Warning: Could not extract all data from {filepath}")
                return None
    except Exception as e:
        print(f"Error reading file {filepath}: {e}")
        return None


# Função para calcular estatísticas e salvar resultados
def calculate_stats_and_save(string_size, n_threads):
    base_dir = f"{logs_dir}/{string_size}/{n_threads}"

    if not os.path.exists(base_dir):
        print(f"Directory {base_dir} does not exist")
        return None

    # Coletar todos os dados dos logs
    data_list = []
    for run in range(1, num_runs + 1):
        log_path = f"{base_dir}/{run}.log"
        data = extract_data_from_log(log_path)
        if data:
            data_list.append(data)

    if not data_list:
        print(f"No data found for string_size={string_size}, n_threads={n_threads}")
        return None

    # Calcular estatísticas
    total_times = [data["total_time"] for data in data_list]
    parallel_times = [data["parallel_time"] for data in data_list]
    sequential_times = [data["sequential_time"] for data in data_list]
    scores = [data["score"] for data in data_list]

    stats = {
        "total_time_mean": np.mean(total_times),
        "total_time_std": np.std(total_times),
        "parallel_time_mean": np.mean(parallel_times),
        "parallel_time_std": np.std(parallel_times),
        "sequential_time_mean": np.mean(sequential_times),
        "sequential_time_std": np.std(sequential_times),
        "score_mean": np.mean(scores),
        "score_std": np.std(scores),
    }

    # Salvar estatísticas em um arquivo
    stats_path = f"{base_dir}/stats.txt"
    with open(stats_path, "w") as stats_file:
        stats_file.write(
            f"Statistics for string_size={string_size}, n_threads={n_threads}\n"
        )
        stats_file.write(
            f"Total time: {stats['total_time_mean']:.6f} ± {stats['total_time_std']:.6f}s\n"
        )
        stats_file.write(
            f"Parallel time: {stats['parallel_time_mean']:.6f} ± {stats['parallel_time_std']:.6f}s\n"
        )
        stats_file.write(
            f"Sequential time: {stats['sequential_time_mean']:.6f} ± {stats['sequential_time_std']:.6f}s\n"
        )
        stats_file.write(
            f"Score: {stats['score_mean']:.2f} ± {stats['score_std']:.2f}\n"
        )

    print(f"Saved statistics to {stats_path}")
    return stats


# Coletar todos os dados
all_data = {}
for size in string_sizes:
    all_data[size] = {}
    for threads in num_threads:
        stats = calculate_stats_and_save(size, threads)
        if stats:
            all_data[size][threads] = stats

# Verificar se temos dados para continuar
if not all_data:
    print("No data was collected. Check the log directories.")
    exit(1)

# Criar diretório para salvar as tabelas e gráficos
output_dir = "results"
os.makedirs(output_dir, exist_ok=True)


# Função auxiliar para formatar valor com desvio padrão
def format_value_with_std(mean, std):
    return f"{mean:.3f} (±{std:.3f})"


# 1. Tabela e Gráfico - Resultados Sequencial
sequential_data = []
for size in string_sizes:
    # Vamos usar os dados do caso com 1 thread para o sequencial
    if size in all_data and 1 in all_data[size]:
        stats = all_data[size][1]
        sequential_data.append(
            {
                "Tamanho da entrada": size,
                "Tempo (s)": stats["sequential_time_mean"],
                "Std": stats["sequential_time_std"],
            }
        )

sequential_df = pd.DataFrame(sequential_data)
if not sequential_df.empty:
    # Salvar tabela
    sequential_table = sequential_df.copy()
    sequential_table["Tempo (s)"] = sequential_table.apply(
        lambda row: format_value_with_std(row["Tempo (s)"], row["Std"]), axis=1
    )
    sequential_table = sequential_table.drop(columns=["Std"])
    sequential_table.to_csv(f"{output_dir}/tabela_sequencial.csv", index=False)

    # Criar gráfico
    plt.figure(figsize=(10, 6))
    plt.errorbar(
        sequential_df["Tamanho da entrada"],
        sequential_df["Tempo (s)"],
        yerr=sequential_df["Std"],
        fmt="o-",
        capsize=5,
    )
    plt.xlabel("Tamanho da entrada")
    plt.ylabel("Tempo (s)")
    plt.title("Tempo de execução do algoritmo sequencial")
    plt.grid(True, linestyle="--", alpha=0.7)
    plt.tight_layout()
    plt.savefig(f"{output_dir}/grafico_sequencial.png")
    plt.close()

# 2. Tabela - Resultados Paralelo (para todos os tamanhos)
parallel_data = {}
for threads in num_threads:
    parallel_data[threads] = {}
    for size in string_sizes:
        if size in all_data and threads in all_data[size]:
            stats = all_data[size][threads]
            parallel_data[threads][size] = {
                "mean": stats["parallel_time_mean"],
                "std": stats["parallel_time_std"],
            }

# Criar DataFrame para a tabela
parallel_df = pd.DataFrame(index=num_threads, columns=string_sizes)
for threads in num_threads:
    for size in string_sizes:
        if threads in parallel_data and size in parallel_data[threads]:
            mean = parallel_data[threads][size]["mean"]
            std = parallel_data[threads][size]["std"]
            parallel_df.at[threads, size] = format_value_with_std(mean, std)

# Salvar tabela
if not parallel_df.empty:
    parallel_df.index.name = "Nº de processadores"
    parallel_df.columns.name = "Tamanho da entrada"
    parallel_df.to_csv(f"{output_dir}/tabela_paralelo_todos.csv")

# 3. Tabela e Gráfico - Resultados Paralelo para entradas menores (100-10000)
small_sizes = [size for size in string_sizes if size <= 10000]
parallel_small_df = pd.DataFrame(
    index=num_threads, columns=[f"{size}" for size in small_sizes]
)
parallel_small_pct_df = pd.DataFrame(
    index=num_threads, columns=[f"{size}_pct" for size in small_sizes]
)

for threads in num_threads:
    for size in small_sizes:
        if size in all_data and threads in all_data[size] and 1 in all_data[size]:
            p_mean = all_data[size][threads]["parallel_time_mean"]
            p_std = all_data[size][threads]["parallel_time_std"]
            s_mean = all_data[size][1][
                "sequential_time_mean"
            ]  # Tempo sequencial (1 thread)

            # Calcular porcentagem do tempo sequencial
            pct = (p_mean / s_mean) * 100 if s_mean > 0 else 0

            parallel_small_df.at[threads, f"{size}"] = format_value_with_std(
                p_mean, p_std
            )
            parallel_small_pct_df.at[threads, f"{size}_pct"] = f"{pct:.2f}%"

# Combinar os dois DataFrames
if not parallel_small_df.empty and not parallel_small_pct_df.empty:
    combined_small_df = pd.DataFrame(index=num_threads)
    for size in small_sizes:
        combined_small_df[f"{size}"] = parallel_small_df[f"{size}"]
        combined_small_df[f"{size}_pct"] = parallel_small_pct_df[f"{size}_pct"]

    # Reordenar as colunas
    cols = []
    for size in small_sizes:
        cols.append(f"{size}")
        cols.append(f"{size}_pct")
    combined_small_df = combined_small_df[cols]

    combined_small_df.index.name = "Nº de processadores"
    combined_small_df.to_csv(f"{output_dir}/tabela_paralelo_pequenas.csv")

# Criar gráfico para entradas menores
plt.figure(figsize=(12, 8))
for size in small_sizes:
    times = []
    for threads in num_threads:
        if size in all_data and threads in all_data[size]:
            times.append(all_data[size][threads]["parallel_time_mean"])
        else:
            times.append(np.nan)
    plt.plot(num_threads, times, "o-", label=f"N={size}")

plt.xlabel("Número de processadores")
plt.ylabel("Tempo (s)")
plt.title("Tempo de execução do algoritmo paralelo para entradas menores")
plt.legend()
plt.grid(True, linestyle="--", alpha=0.7)
plt.tight_layout()
plt.savefig(f"{output_dir}/grafico_paralelo_pequenas.png")
plt.close()

# 4. Tabela e Gráfico - Resultados Paralelo para entradas maiores (100000-130000)
large_sizes = [size for size in string_sizes if size >= 100000]
parallel_large_df = pd.DataFrame(
    index=num_threads, columns=[f"{size}" for size in large_sizes]
)
parallel_large_pct_df = pd.DataFrame(
    index=num_threads, columns=[f"{size}_pct" for size in large_sizes]
)

for threads in num_threads:
    for size in large_sizes:
        if size in all_data and threads in all_data[size] and 1 in all_data[size]:
            p_mean = all_data[size][threads]["parallel_time_mean"]
            p_std = all_data[size][threads]["parallel_time_std"]
            s_mean = all_data[size][1][
                "sequential_time_mean"
            ]  # Tempo sequencial (1 thread)

            # Calcular porcentagem do tempo sequencial
            pct = (p_mean / s_mean) * 100 if s_mean > 0 else 0

            parallel_large_df.at[threads, f"{size}"] = format_value_with_std(
                p_mean, p_std
            )
            parallel_large_pct_df.at[threads, f"{size}_pct"] = f"{pct:.2f}%"

# Combinar os dois DataFrames
if not parallel_large_df.empty and not parallel_large_pct_df.empty:
    combined_large_df = pd.DataFrame(index=num_threads)
    for size in large_sizes:
        combined_large_df[f"{size}"] = parallel_large_df[f"{size}"]
        combined_large_df[f"{size}_pct"] = parallel_large_pct_df[f"{size}_pct"]

    # Reordenar as colunas
    cols = []
    for size in large_sizes:
        cols.append(f"{size}")
        cols.append(f"{size}_pct")
    combined_large_df = combined_large_df[cols]

    combined_large_df.index.name = "Nº de processadores"
    combined_large_df.to_csv(f"{output_dir}/tabela_paralelo_grandes.csv")

# Criar gráfico para entradas maiores
plt.figure(figsize=(12, 8))
for size in large_sizes:
    times = []
    for threads in num_threads:
        if size in all_data and threads in all_data[size]:
            times.append(all_data[size][threads]["parallel_time_mean"])
        else:
            times.append(np.nan)
    plt.plot(num_threads, times, "o-", label=f"N={size}")

plt.xlabel("Número de processadores")
plt.ylabel("Tempo (s)")
plt.title("Tempo de execução do algoritmo paralelo para entradas maiores")
plt.legend()
plt.grid(True, linestyle="--", alpha=0.7)
plt.tight_layout()
plt.savefig(f"{output_dir}/grafico_paralelo_grandes.png")
plt.close()

# 5. Tabela - Lei de Amdahl - Speedup máximo teórico
# Calcular a fração serial para cada tamanho de entrada
amdahl_data = {}
for size in string_sizes:
    if size in all_data and 1 in all_data[size]:
        sequential_time = all_data[size][1]["sequential_time_mean"]
        total_time = all_data[size][1]["total_time_mean"]

        # Fração serial (s) é o tempo sequencial sobre o tempo total
        serial_fraction = sequential_time / total_time if total_time > 0 else 0

        # Calcular speedup máximo teórico para diferentes números de processadores
        amdahl_data[size] = {
            "serial_fraction": serial_fraction,
            "speedup_2": 1 / (serial_fraction + (1 - serial_fraction) / 2),
            "speedup_4": 1 / (serial_fraction + (1 - serial_fraction) / 4),
            "speedup_8": 1 / (serial_fraction + (1 - serial_fraction) / 8),
            "speedup_inf": 1 / serial_fraction if serial_fraction > 0 else float("inf"),
        }

# Criar DataFrame para a tabela da Lei de Amdahl
amdahl_df = pd.DataFrame(
    columns=[
        "Tamanho da entrada",
        "Fração serial",
        "2 CPUs",
        "4 CPUs",
        "8 CPUs",
        "∞ CPUs",
    ]
)
for size in string_sizes:
    if size in amdahl_data:
        data = amdahl_data[size]
        amdahl_df = amdahl_df.append(
            {
                "Tamanho da entrada": size,
                "Fração serial": f"{data['serial_fraction']:.4f}",
                "2 CPUs": f"{data['speedup_2']:.2f}",
                "4 CPUs": f"{data['speedup_4']:.2f}",
                "8 CPUs": f"{data['speedup_8']:.2f}",
                "∞ CPUs": f"{data['speedup_inf']:.2f}"
                if data["speedup_inf"] != float("inf")
                else "∞",
            },
            ignore_index=True,
        )

# Salvar tabela da Lei de Amdahl
if not amdahl_df.empty:
    amdahl_df.to_csv(f"{output_dir}/tabela_amdahl.csv", index=False)

# 6. Tabela de Speedup
speedup_df = pd.DataFrame(index=string_sizes, columns=num_threads)
for size in string_sizes:
    if size in all_data and 1 in all_data[size]:
        # Tempo sequencial (usando os dados do caso com 1 thread)
        sequential_time = all_data[size][1]["sequential_time_mean"]

        for threads in num_threads:
            if threads in all_data[size]:
                parallel_time = all_data[size][threads]["parallel_time_mean"]
                # Calcular speedup como tempo sequencial / tempo paralelo
                speedup = sequential_time / parallel_time if parallel_time > 0 else 0
                speedup_df.at[size, threads] = f"{speedup:.2f}"

# Salvar tabela de Speedup
if not speedup_df.empty:
    speedup_df.index.name = "Tamanho da entrada"
    speedup_df.columns = [f"{t} CPUs" for t in num_threads]
    speedup_df.to_csv(f"{output_dir}/tabela_speedup.csv")

# 7. Tabela de Eficiência
efficiency_df = pd.DataFrame(index=string_sizes, columns=num_threads)
for size in string_sizes:
    if size in all_data and 1 in all_data[size]:
        # Tempo sequencial (usando os dados do caso com 1 thread)
        sequential_time = all_data[size][1]["sequential_time_mean"]

        for threads in num_threads:
            if threads in all_data[size]:
                parallel_time = all_data[size][threads]["parallel_time_mean"]
                # Calcular speedup como tempo sequencial / tempo paralelo
                speedup = sequential_time / parallel_time if parallel_time > 0 else 0
                # Eficiência é speedup / número de processadores
                efficiency = speedup / threads
                efficiency_df.at[size, threads] = f"{efficiency:.2f}"

# Salvar tabela de Eficiência
if not efficiency_df.empty:
    efficiency_df.index.name = "Tamanho da entrada"
    efficiency_df.columns = [f"{t} CPUs" for t in num_threads]
    efficiency_df.to_csv(f"{output_dir}/tabela_eficiencia.csv")

print("Análise concluída. Os resultados foram salvos no diretório:", output_dir)
