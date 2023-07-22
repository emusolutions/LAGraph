#!/usr/bin/env bash

set -eu -o pipefail

EXE=$1
INPUT=$2
OUTPUT=$3
VERSION_INFO=$4

RUN_CMD="emu_multinode_exec 0 -- ${EXE} ${INPUT}"

LGB_HASH="\"$5\""
LGB_CMAKE="\"$6\""

LAGraph_HASH="\"$7\""
LAGraph_CMAKE="\"$8\""

SC_VER="\"$(grep -m1 'sc-driver' ${VERSION_INFO} | cut -d ' ' -f5)\""
SC_TESTS="\"$(grep -m1 'sc-driver-tests' ${VERSION_INFO} | cut -d ' ' -f5)\""
SC_NCDIMM="\"$(grep -m1 'ncdimm' ${VERSION_INFO} | cut -d ' ' -f5)\""

N_NODES=$(cat /etc/emutechnology/LogicalTotalNodes)

# Extract problem size info
INPUT_NROWS=$(head -n3 ${INPUT} | tail -n 1 | cut -d " " -f1)
INPUT_NCOLS=$(head -n3 ${INPUT} | tail -n 1 | cut -d " " -f2)
INPUT_NNZ=$(head -n3 ${INPUT} | tail -n 1 | cut -d " " -f3)


rm -f ${OUTPUT}
touch ${OUTPUT}

echo -e "date = \"$(date)\"\n" >> ${OUTPUT}
echo -e "[application-software]" >> ${OUTPUT}
echo -e "[application-software.lgb]\nhash = ${LGB_HASH}\ncmake_args = ${LGB_CMAKE}\n" >> ${OUTPUT}
echo -e "[application-software.lagraph]\nhash = ${LAGraph_HASH}\ncmake_args = ${LAGraph_CMAKE}\n" >> ${OUTPUT}
echo -e "[system-software]" >> ${OUTPUT}
echo -e "[system-software.sc-driver]" >> ${OUTPUT} 
echo -e "ver = ${SC_VER}\ntests = ${SC_TESTS}\nncdimm = ${SC_NCDIMM}\n" >> ${OUTPUT}


echo -e "[problem]" >> ${OUTPUT}
echo -e "executable = \"$(basename ${EXE})\"" >> ${OUTPUT}
echo -e "" >> ${OUTPUT}
echo -e "[problem.input]" >> ${OUTPUT}
echo -e "name = \"$(basename ${INPUT})\"" >> ${OUTPUT}
echo -e "nrows = ${INPUT_NROWS}" >> ${OUTPUT}
echo -e "ncols = ${INPUT_NCOLS}" >> ${OUTPUT}
echo -e "nnz = ${INPUT_NNZ}" >> ${OUTPUT}
echo -e "" >> ${OUTPUT}

echo -e "[execution]\nnodes = [\"$(hostname)\"]" >> ${OUTPUT}
echo -e "n_nodes = ${N_NODES}" >> ${OUTPUT}
echo -e "working_dir = \"$(pwd)\"\ncommand = \"${RUN_CMD}\"" >> ${OUTPUT}
echo -e "stdout = \"\"\"\n" >> ${OUTPUT}

/usr/bin/time -v -o time.tmp ${RUN_CMD} 2>&1 | tee -a ${OUTPUT}
cat time.tmp >> ${OUTPUT}
rm time.tmp

echo -e "\"\"\"" >> ${OUTPUT}