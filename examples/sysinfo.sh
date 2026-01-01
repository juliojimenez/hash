#!/usr/local/bin/hash-shell
#
# sysinfo.sh - System Information Script
# Displays basic system information
#

echo "========================================"
echo "        System Information Report       "
echo "========================================"
echo ""

# Date and Time
echo "Date/Time"
echo "---------"
date
echo ""

# System Information
echo "System"
echo "------"
if [ -f "/etc/os-release" ]; then
    . /etc/os-release
    echo "OS: $PRETTY_NAME"
fi
echo "Hostname: $(hostname)"
echo "Kernel: $(uname -r)"
echo "Architecture: $(uname -m)"
echo ""

# User Information
echo "User"
echo "----"
echo "Username: $USER"
echo "Home: $HOME"
echo "Shell: $SHELL"
echo ""

# Uptime
echo "Uptime"
echo "------"
uptime
echo ""

# Memory
echo "Memory"
echo "------"
if [ -x "$(command -v free)" ]; then
    free -h | head -2
else
    echo "free command not available"
fi
echo ""

# Disk Usage
echo "Disk Usage"
echo "----------"
df -h / | tail -1 | awk '{print "Root: " $3 " used / " $2 " total (" $5 " used)"}'
echo ""

# Network
echo "Network"
echo "-------"
if [ -x "$(command -v ip)" ]; then
    ip addr show | grep "inet " | grep -v "127.0.0.1" | awk '{print $2}'
elif [ -x "$(command -v hostname)" ]; then
    hostname -I 2>/dev/null || echo "Could not determine IP"
fi
echo ""

# CPU Info
echo "CPU"
echo "---"
if [ -f "/proc/cpuinfo" ]; then
    grep "model name" /proc/cpuinfo | head -1 | cut -d: -f2 | sed 's/^[ \t]*//'
    CPU_COUNT=$(grep -c "processor" /proc/cpuinfo)
    echo "CPU Count: $CPU_COUNT"
fi
echo ""

# Load Average
echo "Load Average"
echo "------------"
cat /proc/loadavg | awk '{print "1 min: " $1 ", 5 min: " $2 ", 15 min: " $3}'
echo ""

# Running Processes
echo "Processes"
echo "---------"
PROC_COUNT=$(ps aux | wc -l)
echo "Total running: $((PROC_COUNT - 1))"
echo ""

echo "========================================"
echo "           Report Complete              "
echo "========================================"
