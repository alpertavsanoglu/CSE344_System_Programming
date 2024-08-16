all: clean compile

compile: 210104004142_main.c
	@echo "-------------------------------------------"
	@echo "Compiled."
	@gcc -o MWCp 210104004142_main.c -lpthread

run:
	@echo "-------------------------------------------"
	@echo "Running the Program...."
	@echo "================================================================================="
	./MWCp 10 10 ../testdir/src/libvterm ../tocopy
	@echo "================================================================================="
	@echo "Completed Program...."

clean:
	@echo "-------------------------------------------"
	@echo "Removing compiled files..."
	@rm -f *.o
	@rm -f *.out
	@rm -f MWCp
	@rm -f	*.txt
	@rm -f	*.log
