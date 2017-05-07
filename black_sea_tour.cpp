#include <iostream>
#include <fstream>
#include <iomanip>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>

using namespace std;
#define USAGE                                                                \
"usage:\n"                                                                   \
"  project [options]\n"                                                      \
"options:\n"                                                                 \
"  -d                  The total simulation time in days\n"	     		     \
"  -p                  Number of passengers\n"       						 \
"  -a                  Number of agents\n"	                                 \
"  -t                  Number of tours\n"		 							 \
"  -s                  Number of seats\n"		 							 \
"  -r                  Random number generator seed\n"		 				 \
"  -h                  Show this help message\n"

//mutexes
pthread_mutex_t seatLock[100][100];
pthread_mutex_t tourLock, dayLock;
//conditional variables
pthread_cond_t tourIsAvailable[100];
/*default values of global shared variables*/
int numPassengers=5, numAgents=5, numTours=1, numSeats=20, numDays=1, day=1;
unsigned int simTime = 5;
int randSeed=time(NULL);
struct timeval start_time;
time_t rawtime;
int passengerID, agentID, tourNo, seatNo;
int busses[100][100], seats[100][100], reservedSeats[100][100], boughtSeats[100][100], waitList[100][100];
int numReservations[100], passengerSeeds[100], agentSeeds[100], passengerAgent[100], dReport[100];
int reservationTimes[100][100];

/*Checks if there is available seat(s) on a tour*/
bool isTourFull(int tourID)
{
  int i;
  for(i=1;i<=numSeats;i++){
    if(busses[tourID][i]==0){
      return false;
    }
  }
  return true;
}

/*passengers and agents can buy tickest*/
void buyTicket(int tourID, int seatID, int passengerID, int agentID)
{
  /*Passenger and agents choose a random seat and try to buy them.
  If they do not already have this seat but someone else has it,
  they find a proper seat to buy*/
  if(busses[tourID][seatID]!=passengerID && busses[tourID][seatID]!=0){
    int i;
    for(i=1;i<numSeats;i++){
      if(busses[tourID][i]==0){
        seatID=i;
        break;
      }
    }
  }
  /*Critical section*/
  //Each thread only locks the seat that it tries to buy
  pthread_mutex_lock(&seatLock[tourID][seatID]);
  //if the seat is reserved before, it is bought and removed from reservations
  if(reservedSeats[tourID][seatID]==passengerID){
    busses[tourID][seatID]=passengerID;
    seats[tourID][passengerID]=2;
    boughtSeats[tourID][seatID]=passengerID;
    reservedSeats[tourID][seatID]=0;
    if(waitList[tourID][passengerID]==passengerID){
      waitList[tourID][passengerID]=0;
    }
    time_t rawtime;
    struct tm * currentTime;
    time(&rawtime);
    currentTime = localtime(&rawtime);
    //Event is written in a log file.
    FILE *fp;
    fp=fopen("tickets.log","a");
    fprintf(fp,"%2d:%2d:%2d%10d%10d            B    %12d %12d\n",currentTime->tm_hour,currentTime->tm_min,currentTime->tm_sec,passengerID,agentID,seatID,tourID);
    fclose(fp);
    //if seat is available, then it is bought
  }else if(busses[tourID][seatID]!=passengerID && seats[tourID][seatID]==0){
    busses[tourID][seatID]=passengerID;
    seats[tourID][passengerID]=2;
    boughtSeats[tourID][seatID]=passengerID;
    if(waitList[tourID][passengerID]==passengerID){
      waitList[tourID][passengerID]=0;
    }
    time_t rawtime;
    struct tm * currentTime;
    time(&rawtime);
    currentTime = localtime(&rawtime);
    //Event is written in a log file.
    FILE *fp;
    fp=fopen("tickets.log","a");
    fprintf(fp,"%2d:%2d:%2d%10d%10d            B    %12d %12d\n",currentTime->tm_hour,currentTime->tm_min,currentTime->tm_sec,passengerID,agentID,seatID,tourID);
    fclose(fp);
  }
  pthread_mutex_unlock(&seatLock[tourID][seatID]);
  //End of critical section
}

void reserveTicket(int tourID, int seatID, int passengerID, int agentID)
{
  /*Passenger and agents choose a random seat and try to reserve them.
  If they have not already reserved this seat but someone else has reserved it,
  they find a proper seat to reserve*/
  if(busses[tourID][seatID]!=passengerID && busses[tourID][seatID]!=0){
    int i;
    for(i=1;i<numSeats;i++){
      if(busses[tourID][i]==0){
        seatID=i;
        break;
      }
    }
  }
  /*Critical section*/
  //Each thread only locks the seat that it tries to reserve
  if(busses[tourID][seatID]!=passengerID && seats[tourID][seatID]==0){
    pthread_mutex_lock(&seatLock[tourID][seatID]);
    busses[tourID][seatID]=passengerID;
    seats[tourID][passengerID]=1;
    reservedSeats[tourID][seatID]=passengerID;
    if(waitList[tourID][passengerID]==passengerID){
      waitList[tourID][passengerID]=0;
    }
    numReservations[passengerID]=numReservations[passengerID]+1;
    time_t rawtime;
    struct tm * currentTime;
    time(&rawtime);
    currentTime = localtime(&rawtime);
    struct timeval rTime;
    gettimeofday(&rTime,NULL);
    reservationTimes[tourID][seatID]=rTime.tv_sec;
    pthread_mutex_unlock(&seatLock[tourID][seatID]);
    /*End of critical section*/
    /*Event is written in log file*/
    FILE *fp;
    fp=fopen("tickets.log","a");
    fprintf(fp,"%2d:%2d:%2d%10d%10d            R    %12d %12d\n",currentTime->tm_hour,currentTime->tm_min,currentTime->tm_sec,passengerID,agentID,seatID,tourID);
    fclose(fp);
  }


}

void cancelTicket(int tourID, int seatID, int passengerID, int agentID)
{
  /*Only if the thread has already bought or reserved the seat, it could cancel the ticket*/
  if(busses[tourID][seatID]==passengerID){
    /*Critical section*/
    pthread_mutex_lock(&seatLock[tourID][seatID]);
    busses[tourID][seatID]=0;
    seats[tourID][passengerID]=0;
    if(boughtSeats[tourID][seatID]==passengerID){
      boughtSeats[tourID][seatID]=0;
    }else{
      reservedSeats[tourID][seatID]=0;
      numReservations[passengerID]=numReservations[passengerID]-1;
    }
    /*Send notification to passengers who wait for available seat*/
    pthread_cond_broadcast(&tourIsAvailable[tourID]);
    pthread_mutex_unlock(&seatLock[tourID][seatID]);
    //End of critical section
    time_t rawtime;
    struct tm * currentTime;
    time(&rawtime);
    currentTime = localtime(&rawtime);
    //Event is written in log file.
    FILE *fp;
    fp=fopen("tickets.log","a");
    fprintf(fp,"%2d:%2d:%2d%10d%10d            C    %12d %12d\n",currentTime->tm_hour,currentTime->tm_min,currentTime->tm_sec,passengerID,agentID,seatID,tourID);
    fclose(fp);
  }
}

void view(int passengerID, int agentID)
{
  int i;
  for(i=1;i<numTours;i++){
    int j;
    for(j=1;j<numSeats;j++){
      //Passengers can view only their own tickets
      if(busses[i][j]==passengerID && agentID==0){
        time_t rawtime;
        struct tm * currentTime;
        time(&rawtime);
        currentTime = localtime(&rawtime);
        FILE *fp;
        fp=fopen("tickets.log","a");
        fprintf(fp,"%2d:%2d:%2d%10d%10d            V    %12d %12d\n",currentTime->tm_hour,currentTime->tm_min,currentTime->tm_sec,passengerID,agentID,j,i);
        fclose(fp);
        //Agents can view all tickets
      }else if (busses[i][j]!=0 && agentID!=0) {
        time_t rawtime;
        struct tm * currentTime;
        time(&rawtime);
        currentTime = localtime(&rawtime);
        FILE *fp;
        fp=fopen("tickets.log","a");
        fprintf(fp,"%2d:%2d:%2d%10d%10d            V    %12d %12d\n",currentTime->tm_hour,currentTime->tm_min,currentTime->tm_sec,passengerID,agentID,j,i);
        fclose(fp);
      }
    }
  }
}
/*Periodically checks reservations and if 24 hour past(5 sec in the simulation), then reservation is cancelled.*/
void checkReservation(struct timeval now){
  int i;
  for(i=1;i<=numTours;i++){
    int j;
    for(j=1;j<=numSeats;j++){
      if(reservedSeats[i][j]!=0){
        if((now.tv_sec-reservationTimes[i][j])>=5){
          reservedSeats[i][j]=0;
          int passengerID=busses[i][j];
          busses[i][j]=0;
          seats[i][j]=0;
          numReservations[passengerID]=numReservations[passengerID]-1;
        }
      }
    }
  }
}

/*Prints daily report at the end of each day*/
void dailyReport(int day)
{
  /*At the end of each day, see daily report*/
  printf("==========================\n");
  printf("Day%d\n",day);
  printf("==========================\n");
  printf("Reserved Seats:\n");
  int i;
  for(i=1;i<=numTours;i++){
    int j;
    printf("Tour %d:",i );
    for(j=1;j<100;j++){
      if(reservedSeats[i][j]!=0){
        printf(" %d ",j );
      }
    }
    printf("\n");
  }
  printf("Bought Seats:\n");
  int k;
  for(k=1;k<=numTours;k++){
    int l;
    printf("Tour %d:",k );
    for(l=1;l<100;l++){
      if(boughtSeats[k][l]!=0){
        printf(" %d ",l );
      }
    }
    printf("\n");
  }
  printf("Wait List (Passenger ID)\n");
  int m;
  for(m=1;m<=numTours;m++){
    printf("Tour %d:",m);
    if(isTourFull(m)){
      int n;
      for(n=1;n<100;n++){
        if(waitList[m][n]!=0){
          printf(" %d ",n );
        }
      }
    }else{
      printf("no passenger is waiting since the tour is not full");
    }
    printf("\n");
  }
  struct timeval current_time;
  gettimeofday(&current_time,NULL);
  checkReservation(current_time);
  dReport[day]=1;
}

//Passenger thread
void *Passenger(void *threadid)
{
  struct timeval current_time;
  gettimeofday(&current_time,NULL);
  int *id_ptr;
  id_ptr = (int *) threadid;
  passengerID = *id_ptr +1;
  agentID=0;
  //Simulation runs until time = start time + simulation time
  while (current_time.tv_sec<=start_time.tv_sec+simTime) {
    //Checks if day ended and if ended, call dailyReport(day) to print daily report.
    /*This should be done once for each day by only one thread. In order to be sure that only one thread does it at any time, we use mutex.
    And also there is  a dReport array to check if daily report of a day is already printed or not*/
    //Critical section
    pthread_mutex_lock(&dayLock);
    if((current_time.tv_sec-start_time.tv_sec)%5==0 && (current_time.tv_sec-start_time.tv_sec)/5==day){
      if(dReport[day]!=1){
        dailyReport(day);
        day++;
      }
    }
    //End of critical section
    pthread_mutex_unlock(&dayLock);
    srand(passengerSeeds[passengerID]);
    int event = rand() % 10 +1;
    seatNo = rand() % numSeats + 1;
    tourNo = rand() % numTours + 1;
    if(event<=4){
      if(isTourFull(tourNo)){
        //If tour is full, passenger will sleep until a notification comes.
        waitList[tourNo][seatNo]=passengerID;
        pthread_cond_wait(&tourIsAvailable[tourNo], &seatLock[tourNo][seatNo]);
      }
      buyTicket(tourNo,seatNo,passengerID,0);
    }else if(event<=6){
      if(numReservations[passengerID]<2){
        if(isTourFull(tourNo)){
          //If tour is full, passenger will sleep until a notification comes.
          waitList[tourNo][seatNo]=passengerID;
          pthread_cond_wait(&tourIsAvailable[tourNo], &seatLock[tourNo][seatNo]);
        }
        reserveTicket(tourNo,seatNo,passengerID,0);
      }
    }else if(event<=8){
      cancelTicket(tourNo,seatNo,passengerID,0);
    }else{
      view(passengerID,agentID);
    }
    gettimeofday(&current_time,NULL);
  }
  exit(1);
}

//Agent thread
void *Agent(void *threadid)
{
  struct timeval current_time;
  int *id_ptr;
  id_ptr = (int *) threadid;
  agentID = *id_ptr + 1;
  gettimeofday(&current_time,NULL);
  //Simulation runs until time = start time + simulation time
  while (current_time.tv_sec<=start_time.tv_sec+simTime) {
    //Checks if day ended and if ended, call dailyReport(day) to print daily report.
    /*This should be done once for each day by only one thread. In order to be sure that only one thread does it at any time, we use mutex.
    And also there is  a dReport array to check if daily report of a day is already printed or not*/
    //Critical section
    pthread_mutex_lock(&dayLock);
    if((current_time.tv_sec-start_time.tv_sec)%5==0 && (current_time.tv_sec-start_time.tv_sec)/5==day){
      if(dReport[day]!=1){
        dailyReport(day);
        day++;
      }
    }
    pthread_mutex_unlock(&dayLock);
    //End of critical section
    srand(agentSeeds[agentID]);
    int event = rand() % 10 +1;
    passengerID = rand() % numPassengers + 1;
    seatNo = rand() % numSeats + 1;
    tourNo = rand() % numTours + 1;
    if(event<=4){
      if(isTourFull(tourNo)){
        waitList[tourNo][passengerID]=passengerID;
      }
      buyTicket(tourNo,seatNo,passengerID,agentID);
    }else if(event>4 && event<=6){
      if(numReservations[passengerID]<2){
        if(isTourFull(tourNo)){
          waitList[tourNo][seatNo]=passengerID;
        }
        reserveTicket(tourNo,seatNo,passengerID,0);
      }
    }else if(event>6 &&event<=8){
      cancelTicket(tourNo,seatNo,passengerID,agentID);
    }else{
      view(passengerID,agentID);
    }
    gettimeofday(&current_time,NULL);
  }
  exit(1);
}

int main(int argc, char *argv[])
{
  printf("*********************************************************\n");
  printf("*********************************************************\n");
  printf("*   COMP 304 - OS Project 2 - BLACK SEA TOUR            *\n");
  printf("*   Koç University - 2017 Spring                        *\n");
  printf("*   Type -h to see capabilities                         *\n");
  printf("*   Created by Ahmet Can Turgut & Damla Övek            *\n");
  printf("*   CopyRight.. University Code of Honesty              *\n");
  printf("*********************************************************\n");
  printf("*********************************************************\n");
    
  printf("---");
    

  int option_char = 0;
  day=1;

  //Take input from user.Initialize global variables based on input
  while ((option_char = getopt(argc, argv, "-d:-p:-a:-t:-s:-r:-h:")) != -1) {
    switch (option_char) {
      case 'd':
      numDays = atoi(optarg);
      simTime = atoi(optarg) * 5;
      break;
      case 'p':
      numPassengers = atoi(optarg);
      break;
      case 'a':
      numAgents = atoi(optarg);
      break;
      case 't':
      numTours = atoi(optarg);
      break;
      case 's':
      numSeats = atoi(optarg);
      break;
      case 'r':
      randSeed = atoi(optarg);
      break;
      case 'h': // help
      fprintf(stdout, "%s", USAGE);
      exit(0);
      break;
      default:
      fprintf(stdout, "%s", USAGE);
      exit(1);
    }
  }
  if(simTime <= 0){
    fprintf(stderr, "%s", USAGE);
    exit(1);
  }
  if(randSeed == 0) {
    randSeed = (unsigned int)time(NULL);
  }
  //Assign different seed for each thread.
  int i;
  for(i=1;i<=numPassengers;i++){
    passengerSeeds[i]=randSeed+i;
  }
  for(i=1;i<=numAgents;i++){
    agentSeeds[i]=randSeed-i;
  }

  //Create a log file to keep records of events.
  FILE *fp;
  fp=fopen("tickets.log","w");
  fprintf(fp,"Simulation Arguments:\n");
  fprintf(fp,"Passenger=%d, Agents=%d,  Tours=%d, Seats=%d(each), Simulation Time=%d days\n",numPassengers,numAgents,numTours,numSeats,simTime/5);
  fprintf(fp,"Time             P_ID      A_ID         Operation      Seat No       Tour No\n");
  fclose(fp);

  gettimeofday(&start_time,NULL);

  //Create passenger threads
  pthread_t passengers[numPassengers];
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  int *passengerids[numPassengers];
  int rc,t;
  for(t=0;t<numPassengers;t++) {
    passengerids[t] = (int *) malloc(sizeof(int));
    *passengerids[t] = t;
    rc = pthread_create(&passengers[t], NULL, Passenger, (void *) passengerids[t]);
    if (rc) {
      printf("ERROR; return code from pthread_create() is %d\n", rc);
      exit(-1);
    }
  }

  //Create agent threads
  pthread_t agents[numAgents];
  pthread_attr_t attr2;
  pthread_attr_init(&attr2);
  pthread_attr_setdetachstate(&attr2, PTHREAD_CREATE_DETACHED);
  int *agentids[numAgents];
  for(t=0;t<numAgents;t++) {
    agentids[t] = (int *) malloc(sizeof(int));
    *agentids[t] = t;
    rc = pthread_create(&agents[t], NULL, Agent, (void *) agentids[t]);
    if (rc) {
      printf("ERROR; return code from pthread_create() is %d\n", rc);
      exit(-2);
    }
  }

  /*Wait for simulation to end*/

  //Join passenger threads
  for(t=0;t<numPassengers;t++){
    pthread_join(passengers[t],NULL);
  }

  //Join agent threads
  for(t=0;t<numAgents;t++){
    pthread_join(agents[t],NULL);
  }

  //Destroy attributes, mutexes, conditional variables, threads
  pthread_attr_destroy(&attr);
  pthread_attr_destroy(&attr2);
  pthread_mutex_destroy(&tourLock);
  pthread_mutex_destroy(&dayLock);
  int tour;
  for(tour=0;tour<100;tour++){
    int seat;
    for(seat=0;seat<100;seat++){
      pthread_mutex_destroy(&seatLock[tour][seat]);
    }
  }
  int cond;
  for(cond=0;cond<100;cond++){
    pthread_cond_destroy(&tourIsAvailable[cond]);
  }
  pthread_exit(NULL);
  exit(1);
}
