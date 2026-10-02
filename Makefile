CC ?= gcc
CFLAGS ?= -Wall -Wextra -pthread -std=c11 -Iinclude
SRCS = src/pm_sim.c src/process_manager.c
TARGET = pm_sim

.PHONY: all clean run test tree stress help

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET)

test: $(TARGET)
	./$(TARGET) tests/thread1.txt tests/thread2.txt

tree: $(TARGET)
	./$(TARGET) tests/tree_t0.txt tests/tree_t1.txt tests/tree_t2.txt

stress: $(TARGET)
	./$(TARGET) tests/stress_t0.txt tests/stress_t1.txt tests/stress_t2.txt

run: $(TARGET)
	./$(TARGET) tests/thread1.txt tests/thread2.txt

ifeq ($(OS),Windows_NT)
  CLEAN_CMD = cmd /c del /q /f $(TARGET) $(TARGET).exe snapshots.txt 2>nul || exit 0
else
  CLEAN_CMD = rm -f $(TARGET) $(TARGET).exe snapshots.txt
endif

clean:
	@$(CLEAN_CMD)
	@echo Clean complete.

help:
	@echo Available make targets:
	@echo   all    - Compile the pm_sim executable (default)
	@echo   test   - Execute Standard Benchmark (tests/thread1.txt tests/thread2.txt)
	@echo   tree   - Execute Deep Hierarchy Tree (3 threads, multi-level processes)
	@echo   stress - Execute High Concurrency Stress Test (dual waits, multiple exits)
	@echo   run    - Execute default test suite
	@echo   clean  - Remove compiled executable and snapshots.txt
	@echo   help   - Display this help message
