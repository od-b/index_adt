#! /bin/bash

#./time_index /home/odin/dumps/generated/ 800 queries.csv 100



gprof -p time_index > prof/flat.output

sleep 1

gprof -q time_index > prof/graph.output

