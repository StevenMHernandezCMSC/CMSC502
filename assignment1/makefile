sequential: sequential.cpp
	g++ -pthread -o sequential sequential.cpp

threaded: threaded.cpp
	g++ -pthread -o threaded threaded.cpp

mpi: mpi.cpp
	mpicxx -g -rdynamic -o mpi mpi.cpp

clean:
	rm sequential
	rm threaded
	rm mpi