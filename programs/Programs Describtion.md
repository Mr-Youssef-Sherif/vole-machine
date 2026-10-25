### Program 1

Program 1 is a counter that counts from 0 till 10.

To acheive the addition, we need 3 variables: 1 our current count, 2 our target, and the addend (which is 1 in this case). And the program keeps being in a loop, where it initalizes these 3 variables and then starts with checking if our target is equal to our current count, if no then it adds our addend and then copys that result to the current counter and then jumps back to the start, and once the counter is equal to the target, it jumps to the halt instruction and the program terminates.

### Program 2

Program 2 is a reverse counter that counts from 10 till 0.

The machine has no dedicated subtraction instruction, but it uses 8-bit two's-complement arithmetic. In that system, FF represents -1, so adding FF subtracts one, with wraparound.