#!/bin/bash

P_VALUES=(1 2 3 4 5 6 7 8)
STRATEGY_VALUES=(0 1 2 3)

# Loop over STRATEGY and P values
for STRATEGY in "${STRATEGY_VALUES[@]}"; do
  for P in "${P_VALUES[@]}"; do
    # Check if the current STRATEGY and P combination should be excluded (esempio d'uso)
    # if [[ "$STRATEGY" == 1 && $P -eq 8 ]]; then
    # continue  # Skip this combination
    # fi
    
    # Check if P is 1 or a power of 2 when STRATEGY is 2 or 3
    if [[ "$STRATEGY" == "2" || "$STRATEGY" == "3" ]]; then
      if [[ $P -ne 1 && $((P & (P - 1))) -ne 0 ]]; then
        continue
      fi
    fi
    # Check if P > 1 and STRATEGY is 0, then break the P loop
    if [[ "$STRATEGY" == "0" && $P -gt 1 ]]; then
      break  # Exit the P loop
    fi
    qsub -v STRATEGY=$STRATEGY P=$P progetto1_pbs_ridotto.pbs
  done
done