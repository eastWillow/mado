#!/bin/bash
# Script to flash and run pico tests on a Raspberry Pi Pico
# and capture the Segger RTT output.

TYPE="unit"
INTERFACE="cmsis-dap"
HEADLESS=0

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --headless)
            HEADLESS=1
            shift
            ;;
        hw|unit)
            TYPE="$1"
            shift
            ;;
        cmsis-dap|picoprobe|jlink|stlink)
            INTERFACE="$1"
            shift
            ;;
        *)
            # Assume any other argument is INTERFACE if it doesn't match known types
            INTERFACE="$1"
            shift
            ;;
    esac
done

if [ "$TYPE" == "hw" ]; then
    ELF_FILE="build/pico-hw-tests.elf"
else
    ELF_FILE="build/pico-unit-tests.elf"
fi

# Detect OpenOCD
OPENOCD="/usr/local/bin/openocd"
PICO_SDK_OPENOCD="/home/eastwillow/.pico-sdk/openocd/0.12.0+dev/openocd.exe"
if [ -x "$PICO_SDK_OPENOCD" ]; then
    OPENOCD="$PICO_SDK_OPENOCD"
    SCRIPTS_DIR="/home/eastwillow/.pico-sdk/openocd/0.12.0+dev/scripts"
fi

# Search paths for OpenOCD scripts
SEARCH_PATHS=""
if [ -n "$SCRIPTS_DIR" ]; then
    SEARCH_PATHS="-s $SCRIPTS_DIR"
fi

if [ ! -f "$ELF_FILE" ]; then
    echo "Error: $ELF_FILE not found."
    exit 1
fi

# Check for required tools
for tool in nc; do
    if ! command -v $tool &> /dev/null; then
        echo "Error: Required tool '$tool' not found."
        exit 1
    fi
done

# Cleanup existing OpenOCD processes on this port
if command -v lsof &> /dev/null; then
    EXISTING_PID=$(lsof -t -i :19021)
    if [ -n "$EXISTING_PID" ]; then
        [ $HEADLESS -eq 0 ] && echo "Killing existing OpenOCD process (PID $EXISTING_PID) listening on port 19021..."
        kill $EXISTING_PID 2>/dev/null
        sleep 1
    fi
fi

if [ $HEADLESS -eq 0 ]; then
    echo "========================================================="
    echo " Flashing and running $ELF_FILE"
    echo " Using OpenOCD: $OPENOCD"
    echo "========================================================="
fi

# Start OpenOCD in the background.
$OPENOCD $SEARCH_PATHS \
        -f "interface/$INTERFACE.cfg" \
        -f "target/rp2350.cfg" \
        -c "adapter speed 5000" \
        -c "program $ELF_FILE verify" \
        -c "init" \
        -c "rtt setup 0x20000000 0x82000 \"SEGGER RTT\"" \
        -c "rtt server start 19021 0" \
        -c "rtt start" \
        -c "reset" > openocd.log 2>&1 &

OPENOCD_PID=$!

# Wait for OpenOCD to initialize and start the RTT server
[ $HEADLESS -eq 0 ] && echo "Waiting for RTT server..."
MAX_RETRIES=15
RETRY_COUNT=0
while ! nc -z localhost 19021 > /dev/null 2>&1; do
    sleep 1
    RETRY_COUNT=$((RETRY_COUNT + 1))
    if [ $RETRY_COUNT -ge $MAX_RETRIES ]; then
        echo "Error: Timeout waiting for RTT server. Check openocd.log."
        kill $OPENOCD_PID 2>/dev/null
        exit 1
    fi
    if ! kill -0 $OPENOCD_PID 2>/dev/null; then
        echo "Error: OpenOCD exited prematurely. Check openocd.log."
        exit 1
    fi
    [ $HEADLESS -eq 0 ] && printf "."
done
[ $HEADLESS -eq 0 ] && echo " Ready!"

[ $HEADLESS -eq 0 ] && echo "--- RTT OUTPUT START ---"
RTT_LOG="rtt_output.log"
rm -f "$RTT_LOG"
touch "$RTT_LOG"

# Start the RTT listener in the background
if command -v socat &> /dev/null; then
    socat -u TCP:localhost:19021,connect-timeout=5 - > "$RTT_LOG" 2>&1 &
else
    nc -N localhost 19021 > "$RTT_LOG" 2>&1 &
fi
LISTENER_PID=$!

# Monitor the log for the completion string
MAX_WAIT=60
ELAPSED=0
while [ $ELAPSED -lt $MAX_WAIT ]; do
    if grep -q "TEST SUMMARY" "$RTT_LOG"; then
        break
    fi
    sleep 1
    ELAPSED=$((ELAPSED + 1))
done

if [ $ELAPSED -ge $MAX_WAIT ]; then
    echo "Warning: Timeout waiting for test completion."
fi

[ $HEADLESS -eq 0 ] && echo "--- RTT OUTPUT END ---"

# In headless mode, output the log to stdout
if [ $HEADLESS -eq 1 ]; then
    cat "$RTT_LOG"
    if grep -q "Memory:" "$RTT_LOG"; then
        echo "--- MEMORY STATS ---"
        grep "Memory:" "$RTT_LOG"
    fi
fi

# Cleanup everything
kill $LISTENER_PID 2>/dev/null
kill $OPENOCD_PID 2>/dev/null
wait $OPENOCD_PID 2>/dev/null

# Exit with success if TEST SUMMARY found and no failures
if grep -q "TEST SUMMARY:.*0 passed" "$RTT_LOG"; then
    [ $HEADLESS -eq 0 ] && echo "Done (FAILED)."
    exit 1
elif grep -q "TEST SUMMARY" "$RTT_LOG"; then
    [ $HEADLESS -eq 0 ] && echo "Done (PASSED)."
    exit 0
else
    [ $HEADLESS -eq 0 ] && echo "Done (TIMEOUT/UNKNOWN)."
    exit 1
fi

