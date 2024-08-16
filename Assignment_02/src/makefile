all: clean compile run

compile: ipc_comm.c
	@echo "-------------------------------------------"
	@echo "Compiling..."
	@gcc -o program ipc_comm.c -lm 

run:
	@echo "-------------------------------------------"
	@echo "Running the tests...."
	@echo "================================================================================="
	./program 3
	@echo "================================================================================="
	@echo "Completed tests...."

clean:
	@echo "-------------------------------------------"
	@echo "Removing compiled files..."
	@rm -f *.o
	@rm -f *.out
	@rm -f program
	@rm -f	*.txt
	@rm -f	*.log

