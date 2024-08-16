all: clean compile run

compile: student_grade_manage_system.c
	@echo "-------------------------------------------"
	@echo "Compiling..."
	@gcc -o grade_manage_system student_grade_manage_system.c -lm 

run:
	@echo "-------------------------------------------"
	@echo "Running the tests...."
	@echo "================================================================================="
	./grade_manage_system
	@echo "================================================================================="
	@echo "Completed tests...."

clean:
	@echo "-------------------------------------------"
	@echo "Removing compiled files..."
	@rm -f *.o
	@rm -f *.out
	@rm -f grade_manage_system
	@rm -f	*.txt
	@rm -f	*.log
