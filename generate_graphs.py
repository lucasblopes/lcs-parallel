import os
import re
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
from pathlib import Path
import json
from matplotlib.table import Table


def plot_parallel_results(results, sizes, filename, title):
    """Gera gráfico para resultados paralelos."""
    plt.figure(figsize=(12, 7))

    for i, size in enumerate(sizes):
        x_values = []
        y_values = []
        std_values = []

        for thread in num_threads:
            mean = results["parallel"][size][thread]["mean"]
            std = results["parallel"][size][thread]["std"]
            if mean is not None:
                x_values.append(thread)
                y_values.append(mean)
                std_values.append(std)

        plt.plot(
            x_values,
            y_values,
            marker="o",
            linewidth=2,
            markersize=8,
            label=f"Tamanho {size}",
            color=colors[i % len(colors)],
        )
        plt.fill_between(
            x_values,
            [y - s for y, s in zip(y_values, std_values)],
            [y + s for y, s in zip(y_values, std_values)],
            alpha=0.2,
            color=colors[i % len(colors)],
        )

    plt.xscale("log", base=2)
    plt.xlabel("Número de Processadores")
    plt.ylabel("Tempo (s)")
    plt.title(title)
    plt.grid(True)
    plt.legend(loc="best")
    plt.tight_layout()
    plt.savefig(filename, dpi=300)
    plt.close()


def format_cell(mean, std):
    """Formata a célula com média e desvio padrão."""
    if mean is None or std is None:
        return "N/A"
    return f"{mean:.6f} (±{std:.6f})"


# Configurações
string_sizes = [100, 1000, 10000, 100000, 110000, 120000, 130000]
num_threads = [2, 4, 8, 12, 16, 32]
num_executions = 20

# Configurações de estilo para os gráficos
# plt.style.use("seaborn-v0_8-darkgrid")
sns.set_context("paper", font_scale=1.2)
colors = sns.color_palette("viridis", len(string_sizes))


def extract_time_from_log(log_content):
    """Extrai o tempo de execução de um arquivo de log."""
    # Procura por padrões como 'SEQUENTIAL: 0.000011s' ou 'PARALLEL: 0.002520s'
    pattern = r"(?:SEQUENTIAL|PARALLEL): (\d+\.\d+)s"
    match = re.search(pattern, log_content)
    if match:
        return float(match.group(1))
    return None


def read_logs_and_calculate_stats():
    """Lê todos os logs e calcula estatísticas."""
    results = {
        "sequential": {size: {"times": []} for size in string_sizes},
        "parallel": {
            size: {thread: {"times": []} for thread in num_threads}
            for size in string_sizes
        },
    }

    # Base directory
    base_dir = "./logscpu1"

    # Processa logs sequenciais
    for size in string_sizes:
        size_dir = os.path.join(base_dir, str(size))

        # Para logs sequenciais (estão em todos os diretórios de thread, mas são os mesmos)
        # Vamos usar o diretório do primeiro thread apenas
        thread_dir = os.path.join(size_dir, str(num_threads[0]))
        if os.path.exists(thread_dir):
            for execution in range(1, num_executions + 1):
                log_file = os.path.join(thread_dir, f"sequential_{execution}.log")
                if os.path.exists(log_file):
                    with open(log_file, "r") as f:
                        content = f.read()
                        time = extract_time_from_log(content)
                        if time is not None:
                            results["sequential"][size]["times"].append(time)

    # Processa logs paralelos
    for size in string_sizes:
        size_dir = os.path.join(base_dir, str(size))
        for thread in num_threads:
            thread_dir = os.path.join(size_dir, str(thread))
            if os.path.exists(thread_dir):
                for execution in range(1, num_executions + 1):
                    log_file = os.path.join(thread_dir, f"parallel_{execution}.log")
                    if os.path.exists(log_file):
                        with open(log_file, "r") as f:
                            content = f.read()
                            time = extract_time_from_log(content)
                            if time is not None:
                                results["parallel"][size][thread]["times"].append(time)

    # Calcula médias e desvios padrão
    for size in string_sizes:
        times = results["sequential"][size]["times"]
        if times:
            results["sequential"][size]["mean"] = np.mean(times)
            results["sequential"][size]["std"] = np.std(times)
        else:
            results["sequential"][size]["mean"] = None
            results["sequential"][size]["std"] = None

        for thread in num_threads:
            times = results["parallel"][size][thread]["times"]
            if times:
                results["parallel"][size][thread]["mean"] = np.mean(times)
                results["parallel"][size][thread]["std"] = np.std(times)
            else:
                results["parallel"][size][thread]["mean"] = None
                results["parallel"][size][thread]["std"] = None

    # Salva resultados estatísticos nos diretórios correspondentes
    for size in string_sizes:
        for thread in num_threads:
            stats_dir = os.path.join(base_dir, str(size), str(thread))
            if os.path.exists(stats_dir):
                # Estatísticas sequenciais
                seq_stats = {
                    "mean": results["sequential"][size]["mean"],
                    "std": results["sequential"][size]["std"],
                }

                # Estatísticas paralelas
                par_stats = {
                    "mean": results["parallel"][size][thread]["mean"],
                    "std": results["parallel"][size][thread]["std"],
                }

                # Salva em arquivos JSON
                with open(os.path.join(stats_dir, "sequential_stats.json"), "w") as f:
                    json.dump(seq_stats, f, indent=2)

                with open(os.path.join(stats_dir, "parallel_stats.json"), "w") as f:
                    json.dump(par_stats, f, indent=2)

    return results


def create_table_image(df, filename, title):
    """Cria uma imagem PNG de uma tabela."""
    # Configurar o tamanho da figura com base no número de linhas e colunas
    fig_width = 2 + df.shape[1] * 2  # Ajustando a largura com base no número de colunas
    fig_height = (
        1 + df.shape[0] * 0.5
    )  # Ajustando a altura com base no número de linhas

    # Criar a figura
    fig, ax = plt.subplots(figsize=(fig_width, fig_height))
    ax.axis("off")  # Desativar os eixos

    # Criar a tabela
    tbl = ax.table(
        cellText=df.values,
        colLabels=df.columns,
        cellLoc="center",
        loc="center",
        bbox=[0, 0, 1, 1],  # Ocupar todo o espaço disponível
    )

    # Definir estilo da tabela
    tbl.auto_set_font_size(False)
    tbl.set_fontsize(10)
    tbl.scale(1, 1.5)  # Ajustar escala

    # Definir estilo para a linha de cabeçalho
    for k, cell in tbl.get_celld().items():
        if k[0] == 0:  # Cabeçalho
            cell.set_text_props(weight="bold", color="white")
            cell.set_facecolor("#4472C4")
        else:  # Corpo da tabela
            if k[0] % 2 == 0:  # Linhas pares
                cell.set_facecolor("#D9E1F2")
            else:  # Linhas ímpares
                cell.set_facecolor("#E9EDF4")

    # Adicionar título
    plt.title(title, fontsize=14, fontweight="bold", pad=20)
    plt.tight_layout()

    # Salvar a figura
    plt.savefig(filename, dpi=300, bbox_inches="tight")
    plt.close()


def create_sequential_table(results):
    """Cria a tabela com resultados sequenciais."""
    data = []
    for size in string_sizes:
        mean = results["sequential"][size]["mean"]
        std = results["sequential"][size]["std"]
        if mean is not None and std is not None:
            data.append([size, format_cell(mean, std)])

    df = pd.DataFrame(data, columns=["Tamanho da Entrada", "Tempo (s)"])
    return df


def create_parallel_table(results, sizes):
    """Cria a tabela com resultados paralelos para tamanhos específicos."""
    data = []
    for thread in num_threads:
        row = [thread]
        for size in sizes:
            mean = results["parallel"][size][thread]["mean"]
            std = results["parallel"][size][thread]["std"]
            row.append(format_cell(mean, std))
        data.append(row)

    columns = ["Nº de Processadores"] + [f"{size}" for size in sizes]
    df = pd.DataFrame(data, columns=columns)
    return df


def plot_sequential_results(results):
    """Gera gráfico para resultados sequenciais."""
    plt.figure(figsize=(10, 6))

    x_values = []
    y_values = []

    for size in string_sizes:
        mean = results["sequential"][size]["mean"]
        if mean is not None:
            x_values.append(size)
            y_values.append(mean)

    plt.plot(x_values, y_values, marker="o", linewidth=2, markersize=8)
    plt.xscale("log")
    plt.xlabel("Tamanho da Entrada")
    plt.ylabel("Tempo (s)")
    plt.title("Desempenho do Algoritmo Sequencial LCS")
    plt.grid(True)
    plt.tight_layout()
    plt.savefig("sequential_performance.png", dpi=300)
    plt.close()


def create_sequential_vs_parallel_table(results, thread_count):
    """Cria uma tabela comparando resultados sequenciais e paralelos para um número específico de threads."""
    data = []
    for size in string_sizes:
        # Dados sequenciais
        seq_mean = results["sequential"][size]["mean"]
        seq_std = results["sequential"][size]["std"]

        # Dados paralelos para o número específico de threads
        par_mean = results["parallel"][size][thread_count]["mean"]
        par_std = results["parallel"][size][thread_count]["std"]

        # Calculando speedup
        speedup = None
        if seq_mean is not None and par_mean is not None and par_mean > 0:
            speedup = seq_mean / par_mean

        # Formatar células
        seq_cell = format_cell(seq_mean, seq_std)
        par_cell = format_cell(par_mean, par_std)
        speedup_cell = f"{speedup:.2f}x" if speedup is not None else "N/A"

        data.append([size, seq_cell, par_cell, speedup_cell])

    df = pd.DataFrame(
        data,
        columns=[
            "Tamanho da Entrada",
            "Tempo Sequencial (s)",
            f"Tempo Paralelo ({thread_count} threads) (s)",
            "Speedup",
        ],
    )
    return df


def plot_sequential_vs_parallel(results, thread_count):
    """Gera gráfico comparando resultados sequenciais e paralelos para um número específico de threads."""
    plt.figure(figsize=(12, 7))

    x_values = []
    seq_values = []
    par_values = []
    speedup_values = []

    for size in string_sizes:
        seq_mean = results["sequential"][size]["mean"]
        par_mean = results["parallel"][size][thread_count]["mean"]

        if seq_mean is not None and par_mean is not None:
            x_values.append(size)
            seq_values.append(seq_mean)
            par_values.append(par_mean)
            speedup_values.append(seq_mean / par_mean)

    # Criar dois eixos Y (um para tempos, outro para speedup)
    fig, ax1 = plt.subplots(figsize=(12, 7))
    ax2 = ax1.twinx()

    # Plotar tempos de execução no eixo Y primário
    line1 = ax1.plot(
        x_values,
        seq_values,
        "o-",
        linewidth=2,
        markersize=8,
        color="blue",
        label="Sequencial",
    )
    line2 = ax1.plot(
        x_values,
        par_values,
        "s-",
        linewidth=2,
        markersize=8,
        color="red",
        label=f"Paralelo ({thread_count} threads)",
    )

    # Plotar speedup no eixo Y secundário
    line3 = ax2.plot(
        x_values,
        speedup_values,
        "^-",
        linewidth=2,
        markersize=8,
        color="green",
        label="Speedup",
    )

    # Configuração do eixo X
    ax1.set_xscale("log")
    ax1.set_xlabel("Tamanho da Entrada")

    # Configuração do eixo Y primário (tempos)
    ax1.set_ylabel("Tempo de Execução (s)")
    ax1.tick_params(axis="y", labelcolor="black")

    # Configuração do eixo Y secundário (speedup)
    ax2.set_ylabel("Speedup (x vezes)")
    ax2.tick_params(axis="y", labelcolor="green")

    # Combinando as legendas
    lines = line1 + line2 + line3
    labels = [l.get_label() for l in lines]
    ax1.legend(lines, labels, loc="upper left")

    plt.title(f"Comparação: LCS Sequencial vs Paralelo ({thread_count} threads)")
    plt.grid(True)
    plt.tight_layout()
    plt.savefig(f"sequential_vs_parallel_{thread_count}threads.png", dpi=300)
    plt.close()


def main():
    # Garante que os diretórios de logs existam (evita erros ao executar o script)
    base_dir = Path("./logscpu1")
    if not base_dir.exists():
        print(
            f"Aviso: O diretório {base_dir} não existe. Criando estrutura de diretórios para exemplificação."
        )
        for size in string_sizes:
            for thread in num_threads:
                thread_dir = base_dir / str(size) / str(thread)
                thread_dir.mkdir(parents=True, exist_ok=True)

                # Cria arquivos de log simulados para testes
                for execution in range(1, num_executions + 1):
                    # Tempos simulados para testes
                    seq_time = 0.000011 * (size / 100) * (1 + np.random.normal(0, 0.1))
                    par_time = (
                        0.002520
                        * (size / 100)
                        / (thread / 2)
                        * (1 + np.random.normal(0, 0.1))
                    )

                    with open(thread_dir / f"sequential_{execution}.log", "w") as f:
                        f.write(f"SEQUENTIAL: {seq_time:.6f}s\nScore: 50\n")

                    with open(thread_dir / f"parallel_{execution}.log", "w") as f:
                        f.write(f"PARALLEL: {par_time:.6f}s\nScore: 50\n")

    print("Lendo logs e calculando estatísticas...")
    results = read_logs_and_calculate_stats()

    print("Criando tabelas...")
    # Tabela 1: Resultados Sequenciais
    sequential_table = create_sequential_table(results)
    print("\n1. Resultados - Sequencial:")
    print(sequential_table.to_string(index=False))
    sequential_table.to_csv("sequential_results.csv", index=False)
    create_table_image(
        sequential_table, "sequential_results.png", "Resultados - Sequencial"
    )

    # Tabela 2: Resultados Paralelos (todos os tamanhos)
    parallel_table = create_parallel_table(results, string_sizes)
    print("\n2. Resultados - Paralelo:")
    print(parallel_table.to_string(index=False))
    parallel_table.to_csv("parallel_results_all.csv", index=False)
    create_table_image(
        parallel_table,
        "parallel_results_all.png",
        "Resultados - Paralelo (Todos os Tamanhos)",
    )

    # Tabela 3: Resultados Paralelos (entradas menores)
    small_entries = [size for size in string_sizes if size <= 10000]
    parallel_small_table = create_parallel_table(results, small_entries)
    print("\n3. Resultados - Paralelo para entradas menores:")
    print(parallel_small_table.to_string(index=False))
    parallel_small_table.to_csv("parallel_results_small.csv", index=False)
    create_table_image(
        parallel_small_table,
        "parallel_results_small.png",
        "Resultados - Paralelo (Entradas Menores)",
    )

    # Tabela 4: Resultados Paralelos (entradas maiores)
    large_entries = [size for size in string_sizes if size >= 100000]
    parallel_large_table = create_parallel_table(results, large_entries)
    print("\n4. Resultados - Paralelo para entradas maiores:")
    print(parallel_large_table.to_string(index=False))
    parallel_large_table.to_csv("parallel_results_large.csv", index=False)
    create_table_image(
        parallel_large_table,
        "parallel_results_large.png",
        "Resultados - Paralelo (Entradas Maiores)",
    )

    print("\nGerando gráficos...")
    # Gráfico para resultados sequenciais
    plot_sequential_results(results)

    # Gráfico para resultados paralelos (entradas menores)
    plot_parallel_results(
        results,
        small_entries,
        "parallel_performance_small.png",
        "Desempenho do Algoritmo Paralelo LCS - Entradas Menores",
    )

    # Gráfico para resultados paralelos (entradas maiores)
    plot_parallel_results(
        results,
        large_entries,
        "parallel_performance_large.png",
        "Desempenho do Algoritmo Paralelo LCS - Entradas Maiores",
    )

    # Tabela e gráfico comparando sequencial vs paralelo (12 threads)
    best_thread = 12  # O thread que mostrou ter mais ganho
    seq_vs_par_table = create_sequential_vs_parallel_table(results, best_thread)
    print(f"\n5. Comparação: Sequencial vs Paralelo ({best_thread} threads):")
    print(seq_vs_par_table.to_string(index=False))
    seq_vs_par_table.to_csv(
        f"sequential_vs_parallel_{best_thread}threads.csv", index=False
    )
    create_table_image(
        seq_vs_par_table,
        f"sequential_vs_parallel_{best_thread}threads_table.png",
        f"Comparação: LCS Sequencial vs Paralelo ({best_thread} threads)",
    )

    # Gráfico comparando sequencial vs paralelo
    plot_sequential_vs_parallel(results, best_thread)

    print("\nProcessamento concluído! Arquivos salvos no diretório atual.")
    print("- Tabelas (CSV): sequential_results.csv, parallel_results_all.csv, etc.")
    print("- Tabelas (PNG): sequential_results.png, parallel_results_all.png, etc.")
    print(
        "- Gráficos (PNG): sequential_performance.png, parallel_performance_small.png, etc."
    )
    print(
        f"- Comparação Sequencial vs Paralelo: sequential_vs_parallel_{best_thread}threads.csv, sequential_vs_parallel_{best_thread}threads_table.png"
    )
    print(
        "- Estatísticas (JSON): Salvas em cada diretório de log como sequential_stats.json e parallel_stats.json"
    )


if __name__ == "__main__":
    main()
