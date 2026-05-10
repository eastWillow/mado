# RPI PICO 2 Support Reorganization Design

**Topic:** Restructure Raspberry Pi Pico 2 (RP2350) support into 5 logical, verified phases.
**Date:** 2026-05-10
**Author:** Gemini CLI

## Overview
The current implementation of Pico 2 support in the `mado` project is spread across 22 granular commits, making the history difficult to follow. This design outlines a plan to reorganize this work into 5 clean, functional phases on a new branch.

## Phases

### Phase 1: Add RPI PICO 2 Compiler Environment
**Goal:** Establish the CMake build system and basic environment for RP2350.
- **Files:**
  - `CMakeLists.txt` (Root updates for Pico SDK)
  - `pico_sdk_import.cmake`
  - `backend/pico/config/DEV_Config.h`
  - `backend/pico/config/DEV_Config.c`
- **Verification:**
  - `mkdir build && cd build && cmake .. -DPICO_BOARD=pico2`
  - Verify `pico-sdk` is found and project configures successfully.

### Phase 2: Add RPI PICO 2 Unit test to Verify the HW
**Goal:** Integrate hardware drivers and a standalone test to verify the LCD and Touch functionality.
- **Files:**
  - `backend/pico/lcd/*`
  - `backend/pico/font/*`
  - `tests/pico_hw_tests.c`
- **Verification:**
  - Build `pico-hw-tests` target.
  - Flash and verify LCD initialization (e.g., color fills, text display).

### Phase 3: Add RPI PICO 2 Unit test to Verify the SW
**Goal:** Add automated software unit testing using Segger RTT for logging.
- **Files:**
  - `tests/pico_unit_tests.c`
  - `scripts/run-pico-tests.sh`
- **Verification:**
  - Run `./scripts/run-pico-tests.sh unit --headless`.
  - Verify all unit tests pass and RTT output is captured.

### Phase 4: Add RPI PICO 2 Backend for Mado
**Goal:** Implement the `twin` screen interface for Pico 2, connecting drivers to the core library.
- **Files:**
  - `backend/pico_backend.c`
  - `CMakeLists.txt` (Update `twin` library target to include Pico sources)
- **Verification:**
  - Build `twin` library for Pico.
  - No functional verification yet (pending Phase 5).

### Phase 5: Verify the RPI PICO 2 Application Demo Spline
**Goal:** Finalize the main application and verify the Spline demo on hardware.
- **Files:**
  - `apps/main.c` (Pico-specific initialization and demo selection)
  - `CMakeLists.txt` (Update `demo-pico` target)
- **Verification:**
  - Build `demo-pico` target.
  - Flash and verify the Spline demo runs at the correct resolution (480x320) without OOM.

## Implementation Strategy
1. Create a new branch `pico2-reorg` from commit `3a1417ee`.
2. For each phase:
   - Identify the files and changes from the current `main` branch.
   - Apply changes surgically.
   - Verify the state (compile/test).
   - Commit with a descriptive "Phase X: ..." message.
3. Once all 5 phases are complete, verify the final state against the original `main` branch.
