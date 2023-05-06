#! /bin/bash

./time_index /home/odin/dumps/generated/ 800 queries.csv 100

sleep 1

gprof -p time_build > prof/flat.output

sleep 1

gprof -q time_build > prof/graph.output

