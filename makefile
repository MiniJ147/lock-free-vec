make:
	g++ main.cpp -o vec_sim.out

lf:	
	g++ main.cpp && ./a.out -lf && rm a.out

lf-leak:
	g++ main.cpp && ./a.out -lf -leak && rm a.out
