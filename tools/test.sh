#!/bin/bash
# SmartHub Test Runner
#
# Usage:
#   ./tools/test.sh           - Run all tests
#   ./tools/test.sh coverage  - Run tests with coverage
#   ./tools/test.sh verbose   - Run tests with verbose output
#   ./tools/test.sh <filter>  - Run specific tests matching filter

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
APP_DIR="$PROJECT_ROOT/app"
BUILD_DIR="$APP_DIR/build"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Parse arguments
COVERAGE=0
VERBOSE=0
FILTER=""

for arg in "$@"; do
    case $arg in
        coverage)
            COVERAGE=1
            BUILD_DIR="$APP_DIR/build-coverage"
            ;;
        verbose)
            VERBOSE=1
            ;;
        *)
            FILTER="$arg"
            ;;
    esac
done

# Configure
log_info "Configuring build..."
cd "$APP_DIR"

CMAKE_ARGS="-B $BUILD_DIR -DSMARTHUB_BUILD_TESTS=ON -DSMARTHUB_NATIVE_BUILD=ON"

if [ $COVERAGE -eq 1 ]; then
    CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_BUILD_TYPE=Debug -DSMARTHUB_COVERAGE=ON"
else
    CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_BUILD_TYPE=Debug"
fi

cmake $CMAKE_ARGS

# Build
log_info "Building..."
cmake --build "$BUILD_DIR" --parallel

# Run tests
log_info "Running tests..."
cd "$BUILD_DIR"

CTEST_ARGS="--output-on-failure"

if [ $VERBOSE -eq 1 ]; then
    CTEST_ARGS="$CTEST_ARGS --verbose"
fi

if [ -n "$FILTER" ]; then
    CTEST_ARGS="$CTEST_ARGS -R $FILTER"
fi

if ctest $CTEST_ARGS; then
    log_info "All tests passed!"
else
    log_error "Some tests failed!"
    exit 1
fi

# Generate coverage report
if [ $COVERAGE -eq 1 ]; then
    log_info "Generating coverage report..."
    if command -v lcov &> /dev/null; then
        lcov --capture --directory . --output-file coverage.info --ignore-errors mismatch 2>/dev/null || true
        lcov --remove coverage.info '/usr/*' '*/third_party/*' '*/tests/*' --output-file coverage.info --ignore-errors unused 2>/dev/null || true

        if command -v genhtml &> /dev/null; then
            genhtml coverage.info --output-directory coverage_html
            log_info "Coverage report generated in $BUILD_DIR/coverage_html/index.html"
        else
            log_warn "genhtml not found, skipping HTML report"
        fi

        # Print summary
        lcov --list coverage.info 2>/dev/null || true
    else
        log_warn "lcov not found, skipping coverage report"
    fi
fi

log_info "Done!"
