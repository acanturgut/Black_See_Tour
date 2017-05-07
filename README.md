# Black See Tour
COMP 304 OS - pThread 

In this project, a simulation of a travel agency, which provides bus tours to Black Sea for its passengers, is implemented by using C/C++. In the simulation, there are passenger and agent threads that are capable of buying, reserving, canceling and viewing buss tickets.
Pthreads are used for this project and mutexes and conditional variables are utilized in order to mutually exclusively perform buy, reserve and cancel operations. View operation is performed simultaneously by multiple threads.
Passenger are capable of cancel and/or view their bought or reserved tickets whereas agents view all tickets and buy, reserve, cancel tickets for their passengers.
