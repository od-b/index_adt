
# Assumptions

## index
* All words and paths given to the index are strings.
* It is the job of the program using the index to free any query_result returned by the index,
  but the path within any given query_result is NOT freed by any external parties.
* Since it is the job of index to free given words / paths, they will not be freed by other sources,
  and so the index does not need to duplicate words it wants to store.
* No two given path strings will be identical when compared by strcmp
