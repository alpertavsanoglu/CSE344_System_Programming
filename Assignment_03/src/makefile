all: clean compile run

compile: park_system.c
	@echo "-------------------------------------------"
	@echo "Compiling..."
	@gcc -o program park_system.c 

run:
	@echo "-------------------------------------------"
	@echo "Running the Program...."
	@echo "================================================================================="
	./program
	@echo "================================================================================="
	@echo "Completed Program...."

clean:
	@echo "-------------------------------------------"
	@echo "Removing compiled files..."
	@rm -f *.o
	@rm -f *.out
	@rm -f program
	@rm -f	*.txt
	@rm -f	*.log


