# Makefile for Hoowachy C++ project linting and checking
.PHONY: help check quick-check format pio-check install-tools clean reports

# Colors for output
RED := \033[0;31m
GREEN := \033[0;32m
YELLOW := \033[1;33m
BLUE := \033[0;34m
NC := \033[0m # No Color

# Default target
help: ## Show this help message
	@printf "$(GREEN)Hoowachy Code Quality Tools$(NC)\n"
	@printf "$(BLUE)Available targets:$(NC)\n"
	@awk 'BEGIN {FS = ":.*##"} /^[a-zA-Z_-]+:.*##/ { printf "  $(YELLOW)%-15s$(NC) %s\n", $$1, $$2 }' $(MAKEFILE_LIST)

quick-check: ## Quick check of source code with cppcheck (fast)
	@printf "$(GREEN)Running quick code check...$(NC)\n"
	@mkdir -p reports
	@printf "$(YELLOW)Checking src/ directory...$(NC)\n"
	@if command -v cppcheck >/dev/null 2>&1; then \
		cppcheck --enable=warning,style,performance,portability \
		         --std=c++17 \
		         --suppress=missingIncludeSystem \
		         --suppress=uninitMemberVar \
		         --suppress=operatorEqRetRefThis \
		         --suppress=returnStdMoveLocal \
		         --suppress=constParameter* \
		         --suppress=unreadVariable \
		         --suppress=cstyleCast \
		         --suppress=accessMoved \
		         --suppress=unsignedLessThanZero \
		         --suppress=uselessAssignmentArg \
		         --suppress=returnTempReference \
		         --suppress=missingReturn \
		         --quiet \
		         --template='{file}:{line}: {severity}: {message}' \
		         src/ 2>/dev/null | tee reports/quick_check.txt; \
		if [ $$? -eq 0 ] && [ ! -s reports/quick_check.txt ]; then \
			printf "$(GREEN)✓ No issues found in your source code$(NC)\n"; \
		elif [ -s reports/quick_check.txt ]; then \
			printf "$(YELLOW)Issues found - check reports/quick_check.txt$(NC)\n"; \
		fi; \
	else \
		printf "$(RED)✗ cppcheck not found$(NC)\n"; \
		echo "Please install: brew install cppcheck"; \
		exit 1; \
	fi

check: ## Full code analysis with all tools
	@echo -e "$(GREEN)Starting comprehensive code analysis...$(NC)"
	@mkdir -p reports
	@echo -e "$(YELLOW)1. Running cppcheck...$(NC)"
	@if command -v cppcheck >/dev/null 2>&1; then \
		cppcheck --enable=all \
		         --std=c++17 \
		         --suppress=missingIncludeSystem \
		         --suppress=toomanyconfigs \
		         --suppress=unmatchedSuppression \
		         --xml \
		         --xml-version=2 \
		         --output-file=reports/cppcheck.xml \
		         src/ 2>&1; \
		echo -e "$(GREEN)✓ cppcheck completed$(NC)"; \
	else \
		echo -e "$(YELLOW)! cppcheck not found, skipping$(NC)"; \
	fi
	@echo -e "$(YELLOW)2. Running clang-tidy...$(NC)"
	@if command -v clang-tidy >/dev/null 2>&1; then \
		find src -name "*.cpp" | head -3 | \
		xargs clang-tidy --config-file=.clang-tidy > reports/clang_tidy.txt 2>&1; \
		echo -e "$(GREEN)✓ clang-tidy completed (sample files)$(NC)"; \
	else \
		echo -e "$(YELLOW)! clang-tidy not found, skipping$(NC)"; \
	fi
	@echo -e "$(YELLOW)3. Checking code formatting...$(NC)"
	@if command -v clang-format >/dev/null 2>&1; then \
		find src -name "*.cpp" -o -name "*.h" | \
		xargs clang-format --dry-run --Werror > reports/format_check.txt 2>&1; \
		if [ $$? -eq 0 ]; then \
			echo -e "$(GREEN)✓ Code formatting is correct$(NC)"; \
		else \
			echo -e "$(YELLOW)! Code formatting issues found$(NC)"; \
			echo "Run 'make format' to fix formatting"; \
		fi; \
	else \
		echo -e "$(YELLOW)! clang-format not found, skipping$(NC)"; \
	fi
	@echo -e "$(GREEN)Comprehensive check complete! Reports in reports/$(NC)"

format: ## Format all C++ source files
	@printf "$(GREEN)Formatting C++ code...$(NC)\n"
	@if command -v clang-format >/dev/null 2>&1; then \
		printf "$(YELLOW)Formatting .cpp files...$(NC)\n"; \
		find src -name "*.cpp" -exec clang-format -i {} \;; \
		printf "$(YELLOW)Formatting .h files...$(NC)\n"; \
		find src -name "*.h" -exec clang-format -i {} \;; \
		printf "$(GREEN)✓ Code formatting complete!$(NC)\n"; \
	else \
		printf "$(RED)✗ clang-format not found$(NC)\n"; \
		echo "Please install: brew install clang-format"; \
		exit 1; \
	fi

pio-check: ## Run PlatformIO code check
	@echo -e "$(GREEN)Running PlatformIO code check...$(NC)"
	@mkdir -p reports
	@echo -e "$(YELLOW)This may take a moment...$(NC)"
	@pio check --environment esp32s3-n16r8 \
	           --severity=high \
	           --json-output > reports/pio_check.json 2>&1; \
	if [ $$? -eq 0 ]; then \
		echo -e "$(GREEN)✓ PlatformIO check completed$(NC)"; \
		if grep -q '"defects": \[\]' reports/pio_check.json; then \
			echo -e "$(GREEN)✓ No high-severity issues found$(NC)"; \
		else \
			echo -e "$(YELLOW)Issues found - check reports/pio_check.json$(NC)"; \
		fi; \
	else \
		echo -e "$(YELLOW)PlatformIO check encountered issues$(NC)"; \
		echo "Check reports/pio_check.json for details"; \
	fi

build: ## Build the project
	@echo -e "$(GREEN)Building project...$(NC)"
	@pio run --environment esp32s3-n16r8

upload: ## Upload firmware to device
	@echo -e "$(GREEN)Uploading firmware...$(NC)"
	@pio run --environment esp32s3-n16r8 --target upload

monitor: ## Open serial monitor
	@echo -e "$(GREEN)Opening serial monitor...$(NC)"
	@pio device monitor

install-tools: ## Install required tools (macOS with Homebrew)
	@echo -e "$(GREEN)Installing code quality tools...$(NC)"
	@if command -v brew >/dev/null 2>&1; then \
		echo -e "$(YELLOW)Installing cppcheck...$(NC)"; \
		brew install cppcheck; \
		echo -e "$(YELLOW)Installing clang-format...$(NC)"; \
		brew install clang-format; \
		echo -e "$(GREEN)✓ Tools installed!$(NC)"; \
	else \
		echo -e "$(RED)Homebrew not found!$(NC)"; \
		echo "Please install Homebrew first: https://brew.sh"; \
		exit 1; \
	fi

clean: ## Clean build files and reports
	@echo -e "$(GREEN)Cleaning project...$(NC)"
	@pio run --target clean
	@rm -rf reports/
	@echo -e "$(GREEN)✓ Cleaned!$(NC)"

reports: ## Show summary of all reports
	@printf "$(GREEN)Code Quality Reports Summary$(NC)\n"
	@printf "$(BLUE)================================$(NC)\n"
	@if [ -d reports ]; then \
		for file in reports/*; do \
			if [ -f "$$file" ]; then \
				printf "$(YELLOW)📄 $$(basename $$file)$(NC)\n"; \
				if [ -s "$$file" ]; then \
					printf "   Size: $$(wc -l < "$$file") lines\n"; \
				else \
					printf "   $(GREEN)Empty (no issues)$(NC)\n"; \
				fi; \
			fi; \
		done; \
	else \
		printf "$(YELLOW)No reports directory found. Run 'make check' first.$(NC)\n"; \
	fi

# Development workflow targets
dev-check: quick-check ## Alias for quick-check (development workflow)

pre-commit: format quick-check ## Run before committing code
	@echo -e "$(GREEN)Pre-commit checks complete!$(NC)"

clang-tidy-full: ## Run clang-tidy on all files (slow)
	@printf "$(GREEN)Running full clang-tidy analysis...$(NC)\n"
	@if command -v clang-tidy >/dev/null 2>&1; then \
		printf "$(YELLOW)This may take several minutes...$(NC)\n"; \
		find src -name "*.cpp" | \
		xargs clang-tidy --config-file=.clang-tidy > reports/clang_tidy_full.txt 2>&1; \
		printf "$(GREEN)✓ Full clang-tidy completed$(NC)\n"; \
		printf "Report saved to reports/clang_tidy_full.txt\n"; \
	else \
		printf "$(RED)✗ clang-tidy not found$(NC)\n"; \
		exit 1; \
	fi

ci: check build ## Continuous integration workflow
	@echo -e "$(GREEN)CI pipeline complete!$(NC)" 