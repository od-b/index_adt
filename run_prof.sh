#! /bin/bash

./time_index /home/odin/dumps/generated/ 4 queries.csv 40

sleep 1

gprof -p time_build > prof/flat.output

sleep 1

gprof -q time_build > prof/graph.output

