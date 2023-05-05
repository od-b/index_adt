#! /bin/bash


./time_build /home/odin/dumps/generated/ 300

sleep 1

gprof -p time_build > prof/flat.output

sleep 1

gprof -q time_build > prof/graph.output

