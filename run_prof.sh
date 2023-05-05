#! /bin/bash

# ./time_build /home/odin/dumps/generated/ 300

IN_DIR=/home/odin/dumps/generated/
OUT_DIR=prof/

./indexer ${IN_DIR}

sleep 1

gprof -p indexer > ${OUT_DIR}/flat.output
gprof -q indexer > ${OUT_DIR}/graph.output

