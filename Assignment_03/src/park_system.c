#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

#define AUTOMOBILE_PARK 8					// Total park number for automobiles
#define PICKUP_PARK 4						// Total park number for pickups
#define RUNTIME 21						// Program running 21 second (it can be changed)
#define VECHILE_COME_TIMER_PER 2				// Every vechile coming  temporary park every 2  sec (it can be changed)
#define VECHILE_LEFT_TIMER_PER 10				// Every vechile leaving temporary park after 10 sec (it can be changed)

sem_t newAutomobile, newPickup, inChargeforAutomobile, inChargeforPickup, parkSystemCheck;// Semaphores to ensure only one vehicle enter park system at a time

pthread_mutex_t mutex_for_car, mutex_for_pickup;	// Mutexes to changing counter values

int mFree_automobile = AUTOMOBILE_PARK;			// Counter variables
int mFree_pickup = PICKUP_PARK;				// Counter variables
int parked_car[AUTOMOBILE_PARK] = {0};			// To Track which park taken, 0-free, 1-parked by automobile
int parked_pickup[PICKUP_PARK] = {0};			// To Track which park taken, 0-free, 1-parked by pickup

time_t car_park_time[AUTOMOBILE_PARK] = {0};		// To Track the arrival time of automobile
time_t pickup_park_time[PICKUP_PARK] = {0};		// To Track the arrival time of pickup

void *carOwner(void *thr){											// Thread function
	while(1){
		sem_wait(&parkSystemCheck);									// Wait for access to park system
		int vehicleType = rand() % 2;									// Random selector 0 for car, 1 for pickup
		sem_t *sem_for_vec = (vehicleType == 0) ? &newAutomobile : &newPickup;
		pthread_mutex_t *mut_for_vec = (vehicleType == 0) ? &mutex_for_car : &mutex_for_pickup;
		int *mFree_park_place = (vehicleType == 0) ? &mFree_automobile : &mFree_pickup;
		int *parked_plc = (vehicleType == 0) ? parked_car : parked_pickup;
		time_t *park_come_time = (vehicleType == 0) ? car_park_time : pickup_park_time;
		int number_of_park_places = (vehicleType == 0) ? AUTOMOBILE_PARK : PICKUP_PARK;
		pthread_mutex_lock(mut_for_vec);								// Lock the mutex to modify shared variables
		if(*mFree_park_place > 0){
			for(int i = 0; i < number_of_park_places; i++){
				if(parked_plc[i] == 0){								// Check for an available parking place
					parked_plc[i] = 1;							// Mark the place as taken
					park_come_time[i] = time(NULL);						// Record the parking time
					(*mFree_park_place)--;							// Decrease the count of available spots
					pthread_mutex_unlock(mut_for_vec);
					printf("%s parked in %s spot %d. Remaining %s spots: %d\n", vehicleType == 0 ? "Automobile" : "Pickup",
					vehicleType == 0 ? "Automobile" : "Pickup",i+1,vehicleType == 0 ? "Automobile" : "Pickup",  *mFree_park_place);
					sem_post(sem_for_vec); 							// Signal the attendant that a vehicle has parked
					break;
				}
			}
		}
		else{
			pthread_mutex_unlock(mut_for_vec);							// Unlock the mutex if no place are available
			printf("No free spots for %s. Leaving...\n", vehicleType == 0 ? "Automobile" : "Pickup");
		}
		sem_post(&parkSystemCheck);									// Release the semaphore to allow other vehicles to enter
		sleep(VECHILE_COME_TIMER_PER);									// Wait 2 sec for other vechile
	}
	return NULL;
}

void *carAttendant(void *thr){											// Thread function
	int vehicleType = *(int *)thr;										// 0 for car, 1 for pickup
	sem_t *sem_for_nvec = (vehicleType == 0) ? &newAutomobile : &newPickup;
	pthread_mutex_t *mut_for_vec = (vehicleType == 0) ? &mutex_for_car : &mutex_for_pickup;
	int number_of_park_places = (vehicleType == 0) ? AUTOMOBILE_PARK : PICKUP_PARK;
	int *parked_plc = (vehicleType == 0) ? parked_car : parked_pickup;
	time_t *park_come_time = (vehicleType == 0) ? car_park_time : pickup_park_time;
	int *mFree_park_place = (vehicleType == 0) ? &mFree_automobile : &mFree_pickup;
	while(1){												// Continuously check for vehicles to process
		pthread_mutex_lock(mut_for_vec);								// Lock the mutex to safely access shared variables
		int processed = 0;										// Flag to indicate if a vehicle has been processed
		time_t currentTime = time(NULL);
		for(int i = 0; i < number_of_park_places; i++){
			if(parked_plc[i] == 1 && (currentTime - park_come_time[i] >= VECHILE_LEFT_TIMER_PER)){
				parked_plc[i] = 0;								// Mark the spot as available
				park_come_time[i] = 0;								// Reset the parking time
				(*mFree_park_place)++;								// Increase the count of available spots
				printf("%s left from %s spot %d. Free %s spots now: %d\n",  vehicleType == 0 ? "Automobile" : "Pickup",vehicleType == 0 ? "Automobile" : "Pickup", i+1,
                    		vehicleType == 0 ? "Automobile" : "Pickup", *mFree_park_place);
				processed = 1;									// Set the processed flag
				break;										// Process one vehicle at a time
			}
		}
		pthread_mutex_unlock(mut_for_vec);								// Unlock the mutex
		if(!processed){											// If no vehicle was processed, wait a bit before checking again
			sleep(1);
		}
	}
	return NULL;
}

int main(){
	srand(time(NULL));										// For random number generator
	pthread_t vech_own_thr, car_park_thr, pickup_park_thr;
	int car_park = 0;
	int pickup_park = 1;

	sem_init(&newAutomobile, 0, 0);									// Initializing semaphores
	sem_init(&inChargeforAutomobile, 0, 0);								// Initializing semaphores
	sem_init(&newPickup, 0, 0);									// Initializing semaphores
	sem_init(&inChargeforPickup, 0, 0);								// Initializing semaphores
	sem_init(&parkSystemCheck, 0, 1);								// Initializing semaphores

	pthread_mutex_init(&mutex_for_car, NULL);							// Initializing mutexes
	pthread_mutex_init(&mutex_for_pickup, NULL);							// Initializing mutexes

	pthread_create(&vech_own_thr, NULL, carOwner, NULL);						// Create threads for car owners and attendants
	pthread_create(&car_park_thr, NULL, carAttendant, &car_park);					// Create threads for car owners and attendants
	pthread_create(&pickup_park_thr, NULL, carAttendant, &pickup_park);				// Create threads for car owners and attendants

	sleep(RUNTIME);								// Allow simulation to run for specified time (e.g., 21 seconds), then stop
	pthread_cancel(vech_own_thr);
	pthread_cancel(car_park_thr);
	pthread_cancel(pickup_park_thr);

	pthread_join(vech_own_thr, NULL);
	pthread_join(car_park_thr, NULL);
	pthread_join(pickup_park_thr, NULL);

	sem_destroy(&newAutomobile);									// Destroy semaphores
	sem_destroy(&inChargeforAutomobile);								// Destroy semaphores
	sem_destroy(&newPickup);									// Destroy semaphores
	sem_destroy(&inChargeforPickup);								// Destroy semaphores
	sem_destroy(&parkSystemCheck);									// Destroy semaphores
	pthread_mutex_destroy(&mutex_for_car);								// Destroy mutexes
	pthread_mutex_destroy(&mutex_for_pickup);							// Destroy mutexes

	return 0;
}
