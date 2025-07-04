import os
import re
import glob
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.figure import Figure
from matplotlib.backends.backend_agg import FigureCanvasAgg as FigureCanvas
from tabulate import tabulate
import seaborn as sns
from pathlib import Path

DIR = "logsmpi"


class LCSLogProcessor:
    def __init__(self):
        # Configurações que podem ser facilmente modificadas
        self.string_sizes = [
            "10k_10k",
            "20k_20k",
            "30k_30k",
            "40k_40k",
        ]
        self.thread_counts = [1, 2, 4, 6]
        self.num_runs = 50
        self.base_dir = f"./{DIR}/"
        self.results = {}

    def extract_data_from_log(self, log_file):
        """Extrai os dados de tempo e score de um arquivo de log com formato novo."""
        try:
            with open(log_file, "r", encoding="utf-8", errors="ignore") as f:
                content = f.read()

            # Detecta se o tempo é PARALLEL ou SEQUENTIAL
            time_match = re.search(r"(PARALLEL|SEQUENTIAL): (\d+\.\d+)s", content)
            score_match = re.search(r"Score: (\d+)", content)

            if not (time_match and score_match):
                raise ValueError("Formato inesperado no log.")

            mode = time_match.group(1)
            time_value = float(time_match.group(2))
            score = int(score_match.group(1))

            if mode == "PARALLEL":
                return {
                    "total_time": time_value,
                    "parallel_time": time_value,
                    "sequential_time": 0.0,
                    "score": score,
                }
            else:  # SEQUENTIAL
                return {
                    "total_time": time_value,
                    "parallel_time": 0.0,
                    "sequential_time": time_value,
                    "score": score,
                }

        except Exception as e:
            print(f"Erro ao processar arquivo {log_file}: {e}")
            return None

    def process_logs(self):
        """Processa todos os arquivos de log e calcula estatísticas."""
        for size in self.string_sizes:
            size_dir = os.path.join(self.base_dir, size)
            self.results[size] = {}

            for threads in self.thread_counts:
                thread_dir = os.path.join(size_dir, str(threads))
                thread_results = []

                for run in range(1, self.num_runs + 1):
                    log_file = os.path.join(thread_dir, f"{run}.log")
                    if os.path.exists(log_file):
                        data = self.extract_data_from_log(log_file)
                        if data:
                            thread_results.append(data)

                if thread_results:
                    total_times = [result["total_time"] for result in thread_results]
                    parallel_times = [
                        result["parallel_time"] for result in thread_results
                    ]
                    sequential_times = [
                        result["sequential_time"] for result in thread_results
                    ]
                    scores = [result["score"] for result in thread_results]

                    # Calcular estatísticas
                    stats = {
                        "total_time": {
                            "mean": np.mean(total_times),
                            "std": np.std(total_times, ddof=1)
                            if len(total_times) > 1
                            else 0,
                        },
                        "parallel_time": {
                            "mean": np.mean(parallel_times),
                            "std": np.std(parallel_times, ddof=1)
                            if len(parallel_times) > 1
                            else 0,
                        },
                        "sequential_time": {
                            "mean": np.mean(sequential_times),
                            "std": np.std(sequential_times, ddof=1)
                            if len(sequential_times) > 1
                            else 0,
                        },
                        "score": {
                            "mean": np.mean(scores),
                            "std": np.std(scores, ddof=1) if len(scores) > 1 else 0,
                        },
                        "raw_data": thread_results,
                    }

                    # Calcular fração sequencial
                    if stats["total_time"]["mean"] > 0:
                        stats["sequential_fraction"] = (
                            stats["sequential_time"]["mean"]
                            / stats["total_time"]["mean"]
                        )
                    else:
                        stats["sequential_fraction"] = 0

                    self.results[size][threads] = stats

                    # Salvar estatísticas em arquivo
                    stats_file = os.path.join(thread_dir, "statistics.txt")
                    with open(stats_file, "w") as f:
                        f.write(
                            f"Total Time: {stats['total_time']['mean']:.6f} ± {stats['total_time']['std']:.6f}s\n"
                        )
                        f.write(
                            f"Parallel Time: {stats['parallel_time']['mean']:.6f} ± {stats['parallel_time']['std']:.6f}s\n"
                        )
                        f.write(
                            f"Sequential Time: {stats['sequential_time']['mean']:.6f} ± {stats['sequential_time']['std']:.6f}s\n"
                        )
                        f.write(
                            f"Sequential Fraction: {stats['sequential_fraction']:.6f}\n"
                        )
                        f.write(
                            f"Score: {stats['score']['mean']:.1f} ± {stats['score']['std']:.1f}\n"
                        )

    def calculate_metrics(self):
        """Calcula métricas de speedup e eficiência."""
        # Para cada tamanho, calcular speedup e eficiência
        for size in self.string_sizes:
            # Obter o tempo sequencial (threads=1) como referência
            if 1 in self.results[size]:
                seq_time = self.results[size][1]["total_time"]["mean"]

                for threads in self.thread_counts:
                    if threads in self.results[size]:
                        # Calcular speedup
                        par_time = self.results[size][threads]["total_time"]["mean"]
                        speedup = seq_time / par_time if par_time > 0 else 0
                        self.results[size][threads]["speedup"] = speedup

                        # Calcular eficiência
                        efficiency = speedup / threads if threads > 0 else 0
                        self.results[size][threads]["efficiency"] = efficiency

    def calculate_theoretical_speedup(self):
        """Calcula o speedup teórico baseado na Lei de Amdahl para cada número de threads."""
        self.theoretical_speedups = {}
        self.seq_fractions_by_thread = {}

        for threads in self.thread_counts:
            if threads == 1:
                self.theoretical_speedups[threads] = 1.0
                self.seq_fractions_by_thread[threads] = 1.0
                continue

            fractions = []
            for size in self.string_sizes:
                if threads in self.results[size]:
                    fractions.append(self.results[size][threads]["sequential_fraction"])

            if fractions:
                mean_fraction = np.mean(fractions)
            else:
                mean_fraction = 1.0  # fallback caso não haja dados

            self.seq_fractions_by_thread[threads] = mean_fraction

            # Speedup teórico pela Lei de Amdahl
            self.theoretical_speedups[threads] = 1 / (
                mean_fraction + (1 - mean_fraction) / threads
            )

        # Speedup teórico para infinito de processadores
        all_fractions = list(self.seq_fractions_by_thread.values())[
            1:
        ]  # ignora threads=1
        overall_seq_fraction = np.mean(all_fractions) if all_fractions else 1.0
        self.theoretical_speedups["inf"] = (
            1 / overall_seq_fraction if overall_seq_fraction > 0 else float("inf")
        )
        self.seq_fractions_by_thread["inf"] = overall_seq_fraction

    def create_general_results_table(self):
        """Cria a tabela de resultados gerais."""
        # Preparar os dados
        data = []
        for threads in self.thread_counts:
            row = [f"{threads} CPU"]
            for size in self.string_sizes:
                if threads in self.results[size]:
                    mean = self.results[size][threads]["total_time"]["mean"]
                    std = self.results[size][threads]["total_time"]["std"]
                    row.append(f"{mean:.3f} (±{std:.3f})")
                else:
                    row.append("N/A")
            data.append(row)

        # Criar o DataFrame
        columns = ["CPU"] + self.string_sizes
        df = pd.DataFrame(data, columns=columns)

        return df

    def create_theoretical_speedup_table(self):
        """Cria a tabela de speedup teórico baseado na Lei de Amdahl."""
        data = []
        for threads in self.thread_counts + ["inf"]:
            row = [
                f"{threads} CPU" if threads != "inf" else "∞ CPU",
                f"{self.seq_fractions_by_thread[threads]:.6f}",
                f"{self.theoretical_speedups[threads]:.3f}",
            ]
            data.append(row)

        columns = ["CPU", "Fração Sequencial Média", "Speedup Teórico"]
        df = pd.DataFrame(data, columns=columns)
        return df

    def create_observed_speedup_table(self):
        """Cria a tabela de speedup observado."""
        # Preparar os dados
        data = []
        for size in self.string_sizes:
            row = [size]
            for threads in self.thread_counts:
                if (
                    threads in self.results[size]
                    and "speedup" in self.results[size][threads]
                ):
                    row.append(f"{self.results[size][threads]['speedup']:.3f}")
                else:
                    row.append("N/A")
            data.append(row)

        # Criar o DataFrame
        columns = ["Tamanho"] + [f"{t} CPU" for t in self.thread_counts]
        df = pd.DataFrame(data, columns=columns)

        return df

    def create_efficiency_table(self):
        """Cria a tabela de eficiência."""
        # Preparar os dados
        data = []
        for size in self.string_sizes:
            row = [size]
            for threads in self.thread_counts:
                if (
                    threads in self.results[size]
                    and "efficiency" in self.results[size][threads]
                ):
                    row.append(f"{self.results[size][threads]['efficiency']:.3f}")
                else:
                    row.append("N/A")
            data.append(row)

        # Criar o DataFrame
        columns = ["Tamanho"] + [f"{t} CPU" for t in self.thread_counts]
        df = pd.DataFrame(data, columns=columns)

        return df

    def save_table_as_image(self, df, title, filename):
        """Salva uma tabela como imagem PNG estilizada."""
        # Configurar o estilo
        sns.set(font_scale=1.2)
        plt.figure(figsize=(12, len(df) * 0.7 + 1.5))

        # Criar tabela
        ax = plt.subplot(111, frame_on=False)
        ax.xaxis.set_visible(False)
        ax.yaxis.set_visible(False)

        # Mostrar tabela estilizada
        table = pd.plotting.table(
            ax,
            df,
            loc="center",
            cellLoc="center",
            colWidths=[0.2] + [0.8 / (len(df.columns) - 1)] * (len(df.columns) - 1),
        )

        # Estilizar tabela
        table.auto_set_font_size(False)
        table.set_fontsize(12)
        table.scale(1.2, 1.7)

        # Colorir cabeçalhos
        for k, cell in table.get_celld().items():
            if k[0] == 0:
                cell.set_facecolor("#4472C4")
                cell.set_text_props(color="white", fontweight="bold")
            else:
                if k[1] == 0:
                    cell.set_facecolor("#D9E1F2")
                    cell.set_text_props(fontweight="bold")
                else:
                    cell.set_facecolor("#E9EFF6")

        # Adicionar título
        plt.title(title, fontsize=16, fontweight="bold", pad=20)
        plt.tight_layout()

        # Salvar como PNG
        plt.savefig(filename, dpi=300, bbox_inches="tight")
        plt.close()

    def save_table_as_csv(self, df, filename):
        """Salva uma tabela como CSV."""
        df.to_csv(filename, index=False)

    def generate_tables(self):
        """Gera todas as tabelas solicitadas."""
        # Criar diretório para os resultados se não existir
        results_dir = f"./results_{DIR}"
        os.makedirs(results_dir, exist_ok=True)

        # Tabela 1: Resultados Gerais
        general_df = self.create_general_results_table()
        self.save_table_as_image(
            general_df,
            "Resultados Gerais - Tempo Total (segundos)",
            f"{results_dir}/general_results.png",
        )
        self.save_table_as_csv(general_df, f"{results_dir}/general_results.csv")

        # Tabela 2: Lei de Amdahl (Speedup Teórico)
        theoretical_df = self.create_theoretical_speedup_table()
        self.save_table_as_image(
            theoretical_df,
            "Lei de Amdahl - Speedup Teórico",
            f"{results_dir}/theoretical_speedup.png",
        )
        self.save_table_as_csv(theoretical_df, f"{results_dir}/theoretical_speedup.csv")

        # Tabela 3: Speedup Observado
        speedup_df = self.create_observed_speedup_table()
        self.save_table_as_image(
            speedup_df, "Speedup Observado", f"{results_dir}/observed_speedup.png"
        )
        self.save_table_as_csv(speedup_df, f"{results_dir}/observed_speedup.csv")

        # Tabela 4: Eficiência
        efficiency_df = self.create_efficiency_table()
        self.save_table_as_image(
            efficiency_df, "Eficiência", f"{results_dir}/efficiency.png"
        )
        self.save_table_as_csv(efficiency_df, f"{results_dir}/efficiency.csv")


def main():
    processor = LCSLogProcessor()
    print("Processando logs...")
    processor.process_logs()
    print("Calculando métricas...")
    processor.calculate_metrics()
    processor.calculate_theoretical_speedup()
    print("Gerando tabelas...")
    processor.generate_tables()
    print("Processamento concluído! Resultados salvos no diretório 'results'.")


if __name__ == "__main__":
    main()
