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
from typing import Optional, Tuple, List, Dict
import statistics
import csv
import re

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
    # Run WITHOUT -q to capture progress output
    cmd = [boltdm_path, url, "-o", output_file]

    start = time.perf_counter()
    result = subprocess.run(cmd, capture_output=True, text=True)
    end = time.perf_counter()

    duration = end - start

    # Parse output for peak speed - scan ALL lines for speed values
    peak_speed = 0.0
    all_output = result.stdout + result.stderr

    # Find all speed values like "@ 43.2 MB/s" or "43.2 MB/s"
    speed_pattern = r'(\d+\.?\d*)\s*MB/s'
    matches = re.findall(speed_pattern, all_output)
    for match in matches:
        try:
            speed = float(match)
            if speed > peak_speed:
                peak_speed = speed
        except:
            pass

    # Calculate avg speed from file size
    if os.path.exists(output_file):
        file_size = os.path.getsize(output_file)
        avg_speed = (file_size / (1024 * 1024)) / duration if duration > 0 else 0
    else:
        avg_speed = 0

    # If peak is still 0, use avg_speed as peak
    if peak_speed == 0:
        peak_speed = avg_speed

    return duration, avg_speed, peak_speed

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

def run_benchmark(url: str, runs: int, boltdm_path: str, idm_time: Optional[float],
                  idm_speed: Optional[float], idm_peak: Optional[float],
                  file_size_mb: float) -> dict:
    """Run full benchmark suite"""
    results: Dict[str, Dict[str, List]] = {
        'boltdm': {'times': [], 'avg_speeds': [], 'peak_speeds': []},
        'idm': {'times': [], 'avg_speeds': [], 'peak_speeds': []},
        'browser': {'times': [], 'avg_speeds': []}
    }

    # Add manual IDM results if provided
    if idm_time is not None:
        results['idm']['times'].append(idm_time)
        # Calculate speed from time if not provided
        if idm_speed is None:
            idm_speed = file_size_mb / idm_time if idm_time > 0 else 0
        results['idm']['avg_speeds'].append(idm_speed)
        if idm_peak is not None:
            results['idm']['peak_speeds'].append(idm_peak)

    # Get expected file size
    print(f"Fetching file info for {url}...")
    expected_size = get_content_length(url)
    if expected_size:
        print(f"Content-Length: {expected_size / (1024*1024):.2f} MB")
    filename = url.split('/')[-1] or 'download'
    if '?' in filename:
        filename = filename.split('?')[0]

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
            print(f"{duration:.2f}s, {avg_speed:.2f} MB/s avg, {peak_speed:.2f} MB/s peak")

            # Clean up
            if os.path.exists(output_file):
                os.remove(output_file)

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

    # Print IDM manual results
    if idm_time is not None:
        print(f"\nIDM (manual): {idm_time:.2f}s, {results['idm']['avg_speeds'][0]:.2f} MB/s" +
              (f", {idm_peak:.2f} MB/s peak" if idm_peak else ""))

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

    # Calculate peak as highest avg_speed across all runs if peak_speed is 0
    if medians.get('boltdm', {}).get('peak_speed', 0) == 0:
        all_speeds = results.get('boltdm', {}).get('avg_speeds', [])
        if all_speeds:
            medians['boltdm']['peak_speed'] = max(all_speeds)

    return medians

def print_results(medians: dict, file_size_mb: float, has_manual_idm: bool):
    """Print results table"""
    print("\n" + "=" * 80)
    print(f"BENCHMARK RESULTS - {file_size_mb:.2f} MB File")
    if has_manual_idm:
        print("* IDM measured manually under identical conditions")
    print("=" * 80)
    print(f"{'Tool':<15} {'Time (s)':<12} {'Avg Speed':<18} {'Peak Speed':<15}")
    print("-" * 80)

    for tool, data in medians.items():
        if data['time'] > 0:
            peak_str = f"{data['peak_speed']:.2f} MB/s" if data['peak_speed'] > 0 else "N/A"
            tool_name = tool.upper()
            if tool == 'idm' and has_manual_idm:
                tool_name += "*"
            print(f"{tool_name:<15} {data['time']:<12.2f} {data['avg_speed']:<18.2f} {peak_str:<15}")

    print("=" * 80)

def generate_chart(medians: dict, output_path: str, file_size_mb: float, has_manual_idm: bool):
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
                peaks.append(medians[key]['peak_speed'])

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
        display_peaks = [p if p > 0 else 0.1 for p in peaks]
        bars3 = ax3.bar(available_tools, display_peaks, color=available_colors)
        ax3.set_ylabel('Speed (MB/s)')
        ax3.set_title('Peak Speed')
        for bar, peak in zip(bars3, peaks):
            label = f'{peak:.1f}' if peak > 0 else 'N/A'
            ax3.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 1,
                    label, ha='center', va='bottom', fontweight='bold')

        for ax in axes:
            ax.set_facecolor('white')
            ax.spines['top'].set_visible(False)
            ax.spines['right'].set_visible(False)

        # Add footnote
        if has_manual_idm:
            fig.text(0.5, 0.02, '* IDM measured manually under identical conditions',
                    ha='center', fontsize=9, style='italic')

        fig.patch.set_facecolor('white')
        plt.tight_layout(rect=[0, 0.05, 1, 1])

        # Create docs directory if needed
        os.makedirs(os.path.dirname(output_path), exist_ok=True)
        plt.savefig(output_path, dpi=150, bbox_inches='tight', facecolor='white')
        print(f"\nChart saved to {output_path}")

    except ImportError:
        print("matplotlib not installed, skipping chart generation")

def generate_csv(medians: dict, output_path: str, has_manual_idm: bool):
    """Generate CSV with raw data"""
    os.makedirs(os.path.dirname(output_path), exist_ok=True)

    with open(output_path, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['Tool', 'Time (s)', 'Avg Speed (MB/s)', 'Peak Speed (MB/s)', 'Notes'])
        for tool, data in medians.items():
            if data['time'] > 0:
                peak_str = f"{data['peak_speed']:.2f}" if data['peak_speed'] > 0 else "N/A"
                note = "manual measurement" if tool == 'idm' and has_manual_idm else ""
                writer.writerow([tool, f"{data['time']:.2f}", f"{data['avg_speed']:.2f}", peak_str, note])

    print(f"CSV saved to {output_path}")

def print_markdown(medians: dict, file_size_mb: float, has_manual_idm: bool):
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
            tool_name = tool.upper()
            suffix = "*" if tool == 'idm' and has_manual_idm else ""
            print(f"| {tool_name}{suffix} | {data['time']:.1f}s | {data['avg_speed']:.1f} MB/s | {peak_str} |")
    if has_manual_idm:
        print()
    print("![Benchmark Results](docs/benchmark.png)")
    if has_manual_idm:
        print()
    print("```")

def main():
    parser = argparse.ArgumentParser(description='Benchmark BoltDM vs IDM vs Browser')
    parser.add_argument('--url', default='https://testfileorg.netwet.net/500MB-CZIPtestfile.org.zip',
                        help='URL to download for benchmarking')
    parser.add_argument('--runs', type=int, default=1, help='Number of runs per tool')
    parser.add_argument('--boltdm', default='build/bin/boltdm.exe', help='Path to boltdm.exe')
    parser.add_argument('--idm-time', type=float, help='Manual IDM time in seconds')
    parser.add_argument('--idm-speed', type=float, help='Manual IDM avg speed in MB/s (calculated from time if omitted)')
    parser.add_argument('--idm-peak', type=float, help='Manual IDM peak speed in MB/s')
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
    if args.idm_time:
        print(f"IDM (manual): {args.idm_time}s")

    # Run benchmark
    results = run_benchmark(
        url=args.url,
        runs=args.runs,
        boltdm_path=str(boltdm_path),
        idm_time=args.idm_time,
        idm_speed=args.idm_speed,
        idm_peak=args.idm_peak,
        file_size_mb=file_size_mb
    )

    # Calculate medians
    medians = calculate_median(results)

    # Print results
    has_manual_idm = args.idm_time is not None
    print_results(medians, file_size_mb, has_manual_idm)

    # Generate outputs
    output_dir = script_dir / args.output_dir
    generate_chart(medians, str(output_dir / 'benchmark.png'), file_size_mb, has_manual_idm)
    generate_csv(medians, str(output_dir / 'benchmark.csv'), has_manual_idm)
    print_markdown(medians, file_size_mb, has_manual_idm)

if __name__ == '__main__':
    main()
