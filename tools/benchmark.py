#!/usr/bin/env python3
"""
Bolt Download Manager Benchmark Tool
Compares BoltDM vs IDM vs Browser download speeds
"""

import argparse
import os
import subprocess
import tempfile
import time
import requests
from pathlib import Path
from typing import Optional, Tuple, List
import statistics
import csv

def get_content_length(url: str) -> Optional[int]:
    """Get Content-Length from HEAD request"""
    try:
        resp = requests.head(url, timeout=10, allow_redirects=True)
        resp.raise_for_status()
        length = resp.headers.get('Content-Length')
        return int(length) if length else None
    except Exception as e:
        print(f"HEAD request failed: {e}")
        return None

def benchmark_boltdm(boltdm_path: str, url: str, output_file: str) -> Tuple[float, float, float]:
    """
    Benchmark BoltDM download.
    Returns: (duration_seconds, avg_speed_mbps, peak_speed_mbps)
    """
    cmd = [boltdm_path, url, "-o", output_file, "-q"]

    start = time.perf_counter()
    result = subprocess.run(cmd, capture_output=True, text=True)
    end = time.perf_counter()

    duration = end - start

    # Parse output for speed info (if available)
    avg_speed = 0.0
    peak_speed = 0.0

    if result.stdout:
        for line in result.stdout.split('\n'):
            if 'MB/s' in line:
                try:
                    # Extract speed from output like "@ 50.5 MB/s"
                    parts = line.split('@')
                    if len(parts) > 1:
                        speed_str = parts[-1].strip().split()[0]
                        speed = float(speed_str)
                        peak_speed = max(peak_speed, speed)
                except:
                    pass

    # Calculate avg speed from file size
    if os.path.exists(output_file):
        file_size = os.path.getsize(output_file)
        avg_speed = (file_size / (1024 * 1024)) / duration if duration > 0 else 0

    return duration, avg_speed, peak_speed

def benchmark_idm(idm_path: str, url: str, output_dir: str, expected_size: int, filename: str) -> Tuple[float, float]:
    """
    Benchmark IDM download (silent mode).
    Returns: (duration_seconds, avg_speed_mbps)
    """
    output_file = os.path.join(output_dir, filename)

    # Remove existing file
    if os.path.exists(output_file):
        os.remove(output_file)

    cmd = [
        idm_path,
        "/n", "/q",  # Silent mode
        "/d", url,
        "/p", output_dir,
        "/f", filename
    ]

    start = time.perf_counter()
    subprocess.run(cmd, capture_output=True)

    # Poll-wait for file to be complete
    timeout = 300  # 5 minutes max
    poll_interval = 0.1
    elapsed = 0

    while elapsed < timeout:
        if os.path.exists(output_file):
            size = os.path.getsize(output_file)
            # Check if file is complete (size matches expected or hasn't changed)
            if size >= expected_size:
                break
            # Wait a bit and check again to ensure file is fully written
            time.sleep(poll_interval)
            new_size = os.path.getsize(output_file)
            if size == new_size and size > 0:
                # Size hasn't changed, likely complete
                break
        time.sleep(poll_interval)
        elapsed += poll_interval

    end = time.perf_counter()
    duration = end - start

    # Calculate avg speed
    if os.path.exists(output_file):
        file_size = os.path.getsize(output_file)
        avg_speed = (file_size / (1024 * 1024)) / duration if duration > 0 else 0
    else:
        avg_speed = 0

    return duration, avg_speed

def benchmark_browser(url: str, output_file: str) -> Tuple[float, float]:
    """
    Benchmark browser download using PowerShell Invoke-WebRequest.
    Returns: (duration_seconds, avg_speed_mbps)
    """
    cmd = [
        "powershell", "-Command",
        f"Invoke-WebRequest -Uri '{url}' -OutFile '{output_file}'"
    ]

    start = time.perf_counter()
    subprocess.run(cmd, capture_output=True)
    end = time.perf_counter()

    duration = end - start

    # Calculate avg speed
    if os.path.exists(output_file):
        file_size = os.path.getsize(output_file)
        avg_speed = (file_size / (1024 * 1024)) / duration if duration > 0 else 0
    else:
        avg_speed = 0

    return duration, avg_speed

def run_benchmark(url: str, runs: int, boltdm_path: str, idm_path: Optional[str], skip_idm: bool) -> dict:
    """Run full benchmark suite"""
    results = {
        'boltdm': {'times': [], 'avg_speeds': [], 'peak_speeds': []},
        'idm': {'times': [], 'avg_speeds': []},
        'browser': {'times': [], 'avg_speeds': []}
    }

    # Get expected file size
    print(f"Fetching file info for {url}...")
    expected_size = get_content_length(url)
    if expected_size:
        print(f"Content-Length: {expected_size / (1024*1024):.2f} MB")
    filename = url.split('/')[-1] or 'download'

    with tempfile.TemporaryDirectory() as tmpdir:
        for run in range(runs):
            print(f"\n=== Run {run + 1}/{runs} ===")

            # Benchmark BoltDM
            print("Testing BoltDM...", end=" ", flush=True)
            output_file = os.path.join(tmpdir, f"boltdm_{run}.bin")
            duration, avg_speed, peak_speed = benchmark_boltdm(boltdm_path, url, output_file)
            results['boltdm']['times'].append(duration)
            results['boltdm']['avg_speeds'].append(avg_speed)
            results['boltdm']['peak_speeds'].append(peak_speed)
            print(f"{duration:.2f}s, {avg_speed:.2f} MB/s, peak {peak_speed:.2f} MB/s")

            # Clean up
            if os.path.exists(output_file):
                os.remove(output_file)

            # Benchmark IDM
            if not skip_idm and idm_path and os.path.exists(idm_path):
                print("Testing IDM...", end=" ", flush=True)
                duration, avg_speed = benchmark_idm(idm_path, url, tmpdir, expected_size or 0, f"idm_{run}.bin")
                results['idm']['times'].append(duration)
                results['idm']['avg_speeds'].append(avg_speed)
                print(f"{duration:.2f}s, {avg_speed:.2f} MB/s")

                # Clean up
                idm_file = os.path.join(tmpdir, f"idm_{run}.bin")
                if os.path.exists(idm_file):
                    os.remove(idm_file)
            elif not skip_idm:
                print("IDM not found, skipping...")

            # Benchmark Browser
            print("Testing Browser (PowerShell)...", end=" ", flush=True)
            output_file = os.path.join(tmpdir, f"browser_{run}.bin")
            duration, avg_speed = benchmark_browser(url, output_file)
            results['browser']['times'].append(duration)
            results['browser']['avg_speeds'].append(avg_speed)
            print(f"{duration:.2f}s, {avg_speed:.2f} MB/s")

            # Clean up
            if os.path.exists(output_file):
                os.remove(output_file)

    return results

def calculate_median(results: dict) -> dict:
    """Calculate median values for each tool"""
    medians = {}
    for tool, data in results.items():
        medians[tool] = {
            'time': statistics.median(data['times']) if data['times'] else 0,
            'avg_speed': statistics.median(data['avg_speeds']) if data['avg_speeds'] else 0,
            'peak_speed': statistics.median(data['peak_speeds']) if data.get('peak_speeds') else 0
        }
    return medians

def print_results(medians: dict, file_size_mb: float):
    """Print results table"""
    print("\n" + "=" * 70)
    print(f"BENCHMARK RESULTS - {file_size_mb:.2f} MB File")
    print("=" * 70)
    print(f"{'Tool':<15} {'Time (s)':<12} {'Avg Speed':<15} {'Peak Speed':<15}")
    print("-" * 70)

    for tool, data in medians.items():
        if data['time'] > 0:
            peak_str = f"{data['peak_speed']:.2f} MB/s" if data['peak_speed'] > 0 else "N/A"
            print(f"{tool.upper():<15} {data['time']:<12.2f} {data['avg_speed']:<15.2f} {peak_str:<15}")

    print("=" * 70)

def generate_chart(medians: dict, output_path: str, file_size_mb: float):
    """Generate comparison chart"""
    try:
        import matplotlib.pyplot as plt
        import numpy as np

        tools = ['BoltDM', 'IDM', 'Browser']
        colors = ['#2196F3', '#4CAF50', '#9E9E9E']

        # Filter tools that have results
        available_tools = []
        available_colors = []
        times = []
        speeds = []
        peaks = []

        for tool, color in zip(tools, colors):
            key = tool.lower()
            if key in medians and medians[key]['time'] > 0:
                available_tools.append(tool)
                available_colors.append(color)
                times.append(medians[key]['time'])
                speeds.append(medians[key]['avg_speed'])
                peaks.append(medians[key]['peak_speed'] if medians[key]['peak_speed'] > 0 else medians[key]['avg_speed'])

        fig, axes = plt.subplots(1, 3, figsize=(15, 5))
        fig.suptitle(f'BoltDM vs IDM vs Browser - {file_size_mb:.0f}MB File', fontsize=14, fontweight='bold')

        # Avg Speed
        ax1 = axes[0]
        bars1 = ax1.bar(available_tools, speeds, color=available_colors)
        ax1.set_ylabel('Speed (MB/s)')
        ax1.set_title('Average Speed')
        for bar, speed in zip(bars1, speeds):
            ax1.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 1,
                    f'{speed:.1f}', ha='center', va='bottom', fontweight='bold')

        # Time
        ax2 = axes[1]
        bars2 = ax2.bar(available_tools, times, color=available_colors)
        ax2.set_ylabel('Time (seconds)')
        ax2.set_title('Download Time')
        for bar, t in zip(bars2, times):
            ax2.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 0.1,
                    f'{t:.1f}s', ha='center', va='bottom', fontweight='bold')

        # Peak Speed
        ax3 = axes[2]
        bars3 = ax3.bar(available_tools, peaks, color=available_colors)
        ax3.set_ylabel('Speed (MB/s)')
        ax3.set_title('Peak Speed')
        for bar, peak in zip(bars3, peaks):
            ax3.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 1,
                    f'{peak:.1f}', ha='center', va='bottom', fontweight='bold')

        for ax in axes:
            ax.set_facecolor('white')
            ax.spines['top'].set_visible(False)
            ax.spines['right'].set_visible(False)

        fig.patch.set_facecolor('white')
        plt.tight_layout()

        # Create docs directory if needed
        os.makedirs(os.path.dirname(output_path), exist_ok=True)
        plt.savefig(output_path, dpi=150, bbox_inches='tight', facecolor='white')
        print(f"\nChart saved to {output_path}")

    except ImportError:
        print("matplotlib not installed, skipping chart generation")

def generate_csv(medians: dict, output_path: str):
    """Generate CSV with raw data"""
    os.makedirs(os.path.dirname(output_path), exist_ok=True)

    with open(output_path, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['Tool', 'Time (s)', 'Avg Speed (MB/s)', 'Peak Speed (MB/s)'])
        for tool, data in medians.items():
            if data['time'] > 0:
                writer.writerow([tool, f"{data['time']:.2f}", f"{data['avg_speed']:.2f}", f"{data['peak_speed']:.2f}"])

    print(f"CSV saved to {output_path}")

def print_markdown(medians: dict, file_size_mb: float):
    """Print markdown snippet for README"""
    print("\n### Markdown for README:\n")
    print("```markdown")
    print(f"## Performance Comparison ({file_size_mb:.0f}MB File)")
    print()
    print("| Tool | Time | Avg Speed | Peak Speed |")
    print("|------|------|-----------|------------|")
    for tool, data in medians.items():
        if data['time'] > 0:
            peak_str = f"{data['peak_speed']:.1f} MB/s" if data['peak_speed'] > 0 else "-"
            print(f"| {tool.upper()} | {data['time']:.1f}s | {data['avg_speed']:.1f} MB/s | {peak_str} |")
    print()
    print("![Benchmark Results](docs/benchmark.png)")
    print("```")

def main():
    parser = argparse.ArgumentParser(description='Benchmark BoltDM vs IDM vs Browser')
    parser.add_argument('--url', default='https://testfileorg.netwet.net/500MB-CZIPtestfile.org.zip',
                        help='URL to download for benchmarking')
    parser.add_argument('--runs', type=int, default=3, help='Number of runs per tool')
    parser.add_argument('--boltdm', default='build/bin/boltdm.exe', help='Path to boltdm.exe')
    parser.add_argument('--idm-path', default='C:/Program Files (x86)/Internet Download Manager/IDMan.exe',
                        help='Path to IDM')
    parser.add_argument('--no-idm', action='store_true', help='Skip IDM benchmark')
    parser.add_argument('--output-dir', default='docs', help='Output directory for chart and CSV')
    args = parser.parse_args()

    # Resolve paths
    script_dir = Path(__file__).parent.parent
    boltdm_path = (script_dir / args.boltdm).resolve()

    if not boltdm_path.exists():
        print(f"Error: BoltDM not found at {boltdm_path}")
        print("Please build BoltDM first or specify correct path with --boltdm")
        return

    # Get file size
    expected_size = get_content_length(args.url)
    file_size_mb = expected_size / (1024 * 1024) if expected_size else 500

    print(f"BoltDM: {boltdm_path}")
    print(f"URL: {args.url}")
    print(f"File Size: {file_size_mb:.2f} MB")
    print(f"Runs: {args.runs}")

    # Run benchmark
    results = run_benchmark(
        url=args.url,
        runs=args.runs,
        boltdm_path=str(boltdm_path),
        idm_path=args.idm_path,
        skip_idm=args.no_idm
    )

    # Calculate medians
    medians = calculate_median(results)

    # Print results
    print_results(medians, file_size_mb)

    # Generate outputs
    output_dir = script_dir / args.output_dir
    generate_chart(medians, str(output_dir / 'benchmark.png'), file_size_mb)
    generate_csv(medians, str(output_dir / 'benchmark.csv'))
    print_markdown(medians, file_size_mb)

if __name__ == '__main__':
    main()
