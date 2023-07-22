#!/usr/bin/env bash

set -eu -o pipefail

DATA_DIR=$1

find ${DATA_DIR} -name "*.toml" -print0 | while read -d $'\0' FILE
do
    echo "FILENAME = ${FILE}"
    cp ${FILE} ${FILE}.txt
done

tar czf ${DATA_DIR}.tgz ${DATA_DIR}