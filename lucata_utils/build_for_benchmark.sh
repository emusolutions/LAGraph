#!/usr/bin/env bash

if [ "$#" -ne 1 ]; then 
    echo "[ERROR]:\n"
    echo "Usage: ./scripts/build_for_benchmark.sh <path_to_lgb_installation>"
    exit -1
fi

GRAPHBLAS_ROOT=$1

set -eux -o pipefail

HASH=$(git rev-parse --short HEAD)
echo "Current short hash: ${HASH}"

BUILD_DIR="build_${HASH}"
CMAKE_CONFIG="cmake -S . -B ${BUILD_DIR} -DCMAKE_C_COMPILER=/tools/lucata/bin/emu-cc.sh -DCMAKE_CXX_COMPILER=/tools/lucata/bin/emu-cc.sh -DCMAKE_BUILD_TYPE=Release -DGRAPHBLAS_ROOT=${GRAPHBLAS_ROOT}"

${CMAKE_CONFIG}
cmake --build ${BUILD_DIR} --parallel 12

# Dump info to TOML file in build dir
BENCH_INFO_FILE=${BUILD_DIR}/bench_info.toml
rm -f ${BENCH_INFO_FILE}

touch ${BENCH_INFO_FILE}
echo -e "[application-software]\n[application-software.lagraph]\n" >> ${BENCH_INFO_FILE}
echo -e "hash = \"$(git rev-parse HEAD)\"" >> ${BENCH_INFO_FILE}
echo -e "cmake_args = \"${CMAKE_CONFIG}\"" >> ${BENCH_INFO_FILE}