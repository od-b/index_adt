#! /bin/bash

#! /bin/bash
DATA_SRC=/home/odin/dumps/wiki/en/
# ./indexer ${DATA_SRC} && python3 run_queries.py
./indexer ${DATA_SRC}

sleep 1

gprof -p indexer > prof_words/test.output

# while [ $min_words -le $stop ] do
#   i=0
#   while [ $i -le $n_loops ] do
#     ./spamfilter_list benchmarks/$outputFile'_list_'$min_words.csv $n_mail $min_words $max_words $i $n_loops
#     ./spamfilter_tree benchmarks/$outputFile'_tree_'$min_words.csv $n_mail $min_words $max_words $i $n_loops
#     ((i++))
#   done
#   python3 benchmarks/add_benchmark.py benchmarks/$outputFile'_list_'$min_words.csv benchmarks/$outputFile'_tree_'$min_words.csv $n_mail $min_words $max_words $n_loops
#   ((n_mail+=20))
#   ((min_words+=increment))
#   ((max_words+=increment))
#   ((increment+=10))
#   echo -e n_mail: $n_mail
# done