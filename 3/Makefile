GCC_FLAGS = -Wextra -Werror -Wall -Wno-gnu-folding-constant

all: test

test:
	gcc $(GCC_FLAGS) userfs.c test.c ../utils/heap_help/heap_help.c -I ../utils/heap_help ../utils/unit.c -I ../utils -o test

run:
	gcc $(GCC_FLAGS) userfs.c main.c ../utils/heap_help/heap_help.c -I ../utils/heap_help ../utils/unit.c -I ../utils -o main


# For automatic testing systems to be able to just build whatever was submitted
# by a student.
test_glob:
	gcc $(GCC_FLAGS) *.c ../utils/unit.c -I ../utils -o test
