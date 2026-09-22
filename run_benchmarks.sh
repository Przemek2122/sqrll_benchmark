#!/usr/bin/env bash
# Script for running benchmarks in more deterministic environment (Release build, -O3, DNDEBUG, HT off, Turbo off, performance governor)
# Probably still not perfect and will work only on My machine. (Intel CPU, Linux, root privileges)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build-release"

# --- Root check for hardware settings ---
if [[ $EUID -ne 0 ]]; then
    echo "[ERROR] This script requires administrator privileges (root/sudo) to modify HT state, CPU governors, and clock frequency." >&2
    echo "Run: sudo $0" >&2
    exit 1
fi

echo "=========================================================================="
echo "          BENCHMARK ENVIRONMENT INITIALIZATION (RELEASE)                  "
echo "=========================================================================="

# --- Helper to check if a CPU core is currently online ---
is_cpu_online() {
    local cpu_dir="$1"
    if [[ -f "${cpu_dir}/online" ]]; then
        local online_val
        online_val=$(cat "${cpu_dir}/online" 2>/dev/null || echo "0")
        [[ "$online_val" == "1" ]]
    else
        # cpu0 is always online and usually has no 'online' sysfs entry
        true
    fi
}

# --- Save initial system state (for restoration in trap) ---
ORIG_SMT_CONTROL=""
if [[ -f /sys/devices/system/cpu/smt/control ]]; then
    ORIG_SMT_CONTROL=$(cat /sys/devices/system/cpu/smt/control)
fi

ORIG_INTEL_NO_TURBO=""
if [[ -f /sys/devices/system/cpu/intel_pstate/no_turbo ]]; then
    ORIG_INTEL_NO_TURBO=$(cat /sys/devices/system/cpu/intel_pstate/no_turbo)
fi

declare -A ORIG_GOVERNORS
declare -A ORIG_EPP
declare -A ORIG_MIN_FREQ
declare -A ORIG_MAX_FREQ

for cpu_dir in /sys/devices/system/cpu/cpu[0-9]*; do
    if ! is_cpu_online "$cpu_dir"; then
        continue
    fi
    cpu_name=$(basename "$cpu_dir")
    
    if [[ -f "${cpu_dir}/cpufreq/scaling_governor" ]]; then
        ORIG_GOVERNORS["$cpu_name"]=$(cat "${cpu_dir}/cpufreq/scaling_governor" 2>/dev/null || true)
    fi
    if [[ -f "${cpu_dir}/cpufreq/energy_performance_preference" ]]; then
        ORIG_EPP["$cpu_name"]=$(cat "${cpu_dir}/cpufreq/energy_performance_preference" 2>/dev/null || true)
    fi
    if [[ -f "${cpu_dir}/cpufreq/scaling_min_freq" ]]; then
        ORIG_MIN_FREQ["$cpu_name"]=$(cat "${cpu_dir}/cpufreq/scaling_min_freq" 2>/dev/null || true)
    fi
    if [[ -f "${cpu_dir}/cpufreq/scaling_max_freq" ]]; then
        ORIG_MAX_FREQ["$cpu_name"]=$(cat "${cpu_dir}/cpufreq/scaling_max_freq" 2>/dev/null || true)
    fi
done

# --- Cleanup function to restore original system state ---
cleanup() {
    echo ""
    echo "=========================================================================="
    echo "          RESTORING ORIGINAL SYSTEM CONFIGURATION                         "
    echo "=========================================================================="

    # Restore SMT (HT)
    if [[ -n "$ORIG_SMT_CONTROL" && -f /sys/devices/system/cpu/smt/control ]]; then
        echo "[CLEANUP] Restoring SMT/HT: $ORIG_SMT_CONTROL"
        echo "$ORIG_SMT_CONTROL" > /sys/devices/system/cpu/smt/control 2>/dev/null || true
    fi

    # Restore Turbo Boost
    if [[ -n "$ORIG_INTEL_NO_TURBO" && -f /sys/devices/system/cpu/intel_pstate/no_turbo ]]; then
        echo "[CLEANUP] Restoring Intel no_turbo: $ORIG_INTEL_NO_TURBO"
        echo "$ORIG_INTEL_NO_TURBO" > /sys/devices/system/cpu/intel_pstate/no_turbo 2>/dev/null || true
    fi

    # Restore frequency governors, EPP, and min/max frequencies
    for cpu_name in "${!ORIG_GOVERNORS[@]}"; do
        cpu_dir="/sys/devices/system/cpu/${cpu_name}"
        
        if [[ -f "${cpu_dir}/cpufreq/scaling_min_freq" && -n "${ORIG_MIN_FREQ[$cpu_name]:-}" ]]; then
            echo "${ORIG_MIN_FREQ[$cpu_name]}" > "${cpu_dir}/cpufreq/scaling_min_freq" 2>/dev/null || true
        fi
        if [[ -f "${cpu_dir}/cpufreq/scaling_max_freq" && -n "${ORIG_MAX_FREQ[$cpu_name]:-}" ]]; then
            echo "${ORIG_MAX_FREQ[$cpu_name]}" > "${cpu_dir}/cpufreq/scaling_max_freq" 2>/dev/null || true
        fi
        if [[ -f "${cpu_dir}/cpufreq/scaling_governor" && -n "${ORIG_GOVERNORS[$cpu_name]:-}" ]]; then
            echo "${ORIG_GOVERNORS[$cpu_name]}" > "${cpu_dir}/cpufreq/scaling_governor" 2>/dev/null || true
        fi
        if [[ -f "${cpu_dir}/cpufreq/energy_performance_preference" && -n "${ORIG_EPP[$cpu_name]:-}" ]]; then
            echo "${ORIG_EPP[$cpu_name]}" > "${cpu_dir}/cpufreq/energy_performance_preference" 2>/dev/null || true
        fi
    done

    echo "[CLEANUP] State restoration completed."
}

trap cleanup EXIT INT TERM

# --- Apply optimal settings for benchmarking ---

# A. Disable Hyper-Threading (SMT)
if [[ -f /sys/devices/system/cpu/smt/control ]]; then
    echo "[SETUP] Disabling Hyper-Threading (SMT)..."
    echo "off" > /sys/devices/system/cpu/smt/control
fi

# B. Disable Turbo Boost (for fixed, deterministic base clock)
if [[ -f /sys/devices/system/cpu/intel_pstate/no_turbo ]]; then
    echo "[SETUP] Disabling Intel Turbo Boost..."
    echo "1" > /sys/devices/system/cpu/intel_pstate/no_turbo
fi

# C. Set CPU frequency governors to 'performance' and fix clock speed
echo "[SETUP] Setting CPU governor to 'performance' and fixing clock frequencies..."
for cpu_dir in /sys/devices/system/cpu/cpu[0-9]*; do
    if ! is_cpu_online "$cpu_dir"; then
        continue
    fi

    if [[ -f "${cpu_dir}/cpufreq/scaling_governor" ]]; then
        echo "performance" > "${cpu_dir}/cpufreq/scaling_governor" 2>/dev/null || true
    fi
    if [[ -f "${cpu_dir}/cpufreq/energy_performance_preference" ]]; then
        echo "performance" > "${cpu_dir}/cpufreq/energy_performance_preference" 2>/dev/null || true
    fi
    
    # If base frequency is available, fix scaling min/max to base frequency
    if [[ -f "${cpu_dir}/cpufreq/base_frequency" ]]; then
        base_f=$(cat "${cpu_dir}/cpufreq/base_frequency" 2>/dev/null || true)
        if [[ -n "$base_f" ]]; then
            echo "$base_f" > "${cpu_dir}/cpufreq/scaling_min_freq" 2>/dev/null || true
            echo "$base_f" > "${cpu_dir}/cpufreq/scaling_max_freq" 2>/dev/null || true
        fi
    fi
done

# --- Build project in Release mode (-O3 / DNDEBUG) ---
echo ""
echo "=========================================================================="
echo "          BUILDING PROJECT IN RELEASE MODE (-O3 / DNDEBUG)                "
echo "=========================================================================="

cmake -B "${BUILD_DIR}" -S "${SCRIPT_DIR}" -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}" --config Release

# --- Run benchmarks ---
echo ""
echo "=========================================================================="
echo "                   RUNNING BENCHMARKS                                     "
echo "=========================================================================="

"${BUILD_DIR}/benchmarks"
