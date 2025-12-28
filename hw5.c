#include "elevator.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>


typedef struct {
    int pass_from;
    int pass_to;

}PassengerState;

typedef struct {
    int current_floor;          // current floor of elevator
    int door_open;              // 0 = closed, 1 = open
    int direction;              // 1 up, -1 down
    PassengerState passengers[MAX_CAPACITY];
    int passenger_count;        // how many passengers inside
    int from_floor;             // where passenger is waiting
    int to_floor;               // destination floor
    pthread_cond_t condition;
    pthread_barrier_t barrier_e;
    pthread_barrier_t barrier_p;
    pthread_mutex_t lock;
    pthread_mutex_t passenger_lock;
    int has_work;               // whether elevator has work
} ElevatorState;


static ElevatorState elevators[ELEVATORS];
static PassengerState passengero[PASSENGERS];

//initialize
void init_env(void) {
    for (int i = 0; i < ELEVATORS; i++) {
        ElevatorState *e = &elevators[i];
        pthread_mutex_init(&e->lock, 0);
        pthread_cond_init(&e->condition, 0);
        pthread_mutex_init(&e->passenger_lock, 0);
        // Start elevator at floor 0
        e->current_floor = 0;
        e->door_open = 0;
        e->direction = -1;
        e->passenger_count = 0;
        e->from_floor = -1; // no request
        e->to_floor = -1;
        e->has_work = 0;
        //synchronization init
        pthread_barrier_init(&e->barrier_p, NULL, 2);
        pthread_barrier_init(&e->barrier_e, NULL, 2);
    
    }
}

// Passenger controller
void passenger_controller(int passenger, int from_floor, int to_floor,
                          void (*enter_elevator)(int, int), void (*exit_elevator)(int, int)) {
    //assign passenger to elevator
    int elevator_id = passenger % ELEVATORS;
    ElevatorState *e = &elevators[elevator_id];

    pthread_mutex_lock(&e->passenger_lock);

    // Set the passenger request
    e->from_floor = from_floor;
    e->to_floor = to_floor; 
    printf("geldigi yer %d\n", e->from_floor);
    printf("gidicegi yer %d\n", e->to_floor);

    //notify elevator
    pthread_barrier_wait(&e->barrier_p);


    // enter elevator if it arrives to the floor
    if (e->current_floor == from_floor && e->door_open == 1 && e->passenger_count == 0){
        enter_elevator(passenger, elevator_id);
        e->passenger_count++;  
    }
   
   //sync with elevator
    pthread_barrier_wait(&e->barrier_e);
    pthread_barrier_wait(&e->barrier_p);

    //exit the elevator
    if (e->door_open == 1 && e->current_floor == to_floor && e->passenger_count != 0) {
        exit_elevator(passenger, elevator_id);
        e->passenger_count--;
        e->from_floor = -1;
        e->to_floor = -1;
        
    }
    pthread_barrier_wait(&e->barrier_e);

    pthread_mutex_unlock(&e->passenger_lock);
}


//elevator controller
void elevator_controller(int elevator, int at_floor,
                         void (*move_elevator)(int, int),
                         void (*open_door)(int), void (*close_door)(int)) {
    ElevatorState *e = &elevators[elevator];

    pthread_mutex_lock(&e->lock);

    // if (e->has_work == 0) {
    //      pthread_mutex_unlock(&e->lock);
    //      return;
    //  }

    int from_floor = e->from_floor;
    int to_floor = e->to_floor;
    int direct = e->direction;
    int passCount = e->passenger_count;

    //door is closed
    if(e->door_open == 0){
        //picking up and dropping off
        if((passCount == 0 && at_floor == from_floor) || (passCount > 0 && at_floor == to_floor)){
            open_door(elevator);
            e->door_open = 1;
            //sync with passenger
            pthread_barrier_wait(&e->barrier_p);
            pthread_barrier_wait(&e->barrier_e);
        }else{
            e->door_open = 2; // door stays closed
        }
    }
    else if(e->door_open == 1){
            close_door(elevator);
            e->door_open = 2;
    }
    else{
       // if at boundaries
        if ((at_floor  == 0) || (at_floor == FLOORS - 1)){ 
		    e->direction *= -1;
	    }
        //going wrong direction
        else if(direct == -1 && passCount == 0 && at_floor < from_floor){
            e->direction *= -1;
        }
        else if(direct == -1 && passCount == 1 && at_floor < to_floor){
            e->direction *= -1;
        }
        else if(direct == 1  && passCount == 0 && from_floor < at_floor)
        {
            e->direction *= -1;
        }
        else if(direct == 1  && passCount == 1 && to_floor < at_floor){
            e->direction *= -1;
        }


        // if (passCount == 0) {
        //     if (at_floor < from_floor) {
        //         e->direction = 1;
        //     } else if (at_floor > from_floor) {
        //         e->direction = -1;
        //     }
        // } 
        // else {
        //     if (at_floor < to_floor) {
        //         e->direction = 1;
        //     } 
        //     else if (at_floor > to_floor) {
        //         e->direction = -1;
        //     }
        // }

        move_elevator(elevator, e->direction);
        e->current_floor = at_floor + e->direction;

        e->door_open = 0; // reset door state
    
    }

   
    

    pthread_mutex_unlock(&e->lock);
}
