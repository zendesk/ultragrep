install: ug_guts

request.o: request.c request.h
	gcc -O3 -c request.c

rails_req.o: rails_req.c rails_req.h req_matcher.h
	gcc -O3 -c rails_req.c

work_req.o: work_req.c work_req.h req_matcher.h
	gcc -O3 -c work_req.c

ug_guts.o: ug_guts.c req_matcher.h
	gcc -O3 -c ug_guts.c

ug_guts: ug_guts.o rails_req.o work_req.o request.o Makefile
	gcc -O3 -o ug_guts request.o rails_req.o work_req.o ug_guts.o -lpcre

clean:
	rm -rf *.o ug_guts
