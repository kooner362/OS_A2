#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "traffic.h"

extern struct intersection isection;

/**
 * Populate the car lists by parsing a file where each line has
 * the following structure:
 *
 * <id> <in_direction> <out_direction>
 *
 * Each car is added to the list that corresponds with
 * its in_direction
 *
 * Note: this also updates 'inc' on each of the lanes
 */
void parse_schedule(char *file_name) {
    int id;
    struct car *cur_car;
    struct lane *cur_lane;
    enum direction in_dir, out_dir;
    FILE *f = fopen(file_name, "r");

    /* parse file */
    while (fscanf(f, "%d %d %d", &id, (int*)&in_dir, (int*)&out_dir) == 3) {

        /* construct car */
        cur_car = malloc(sizeof(struct car));
        cur_car->id = id;
        cur_car->in_dir = in_dir;
        cur_car->out_dir = out_dir;

        /* append new car to head of corresponding list */
        cur_lane = &isection.lanes[in_dir];
        cur_car->next = cur_lane->in_cars;
        cur_lane->in_cars = cur_car;
        cur_lane->inc++;
    }

    fclose(f);
}

/**
 * TODO: Fill in this function
 *
 * Do all of the work required to prepare the intersection
 * before any cars start coming
 *
 */
void init_intersection() {
  int i = 0;
  for(; i < 4; i++){
    //Initialize the intersection locks
    pthread_mutex_init(&isection.quad[i], NULL);
    pthread_mutex_init(&isection.lanes[i].lock, NULL);
    pthread_cond_init(&isection.lanes[i].producer_cv, NULL);
    pthread_cond_init(&isection.lanes[i].consumer_cv, NULL);

    //Initialize lane data
    isection.lanes[i].inc = 0;
    isection.lanes[i].passed = 0;
    isection.lanes[i].head = 0;
    isection.lanes[i].tail = 0;
    isection.lanes[i].in_buf = 0;

    isection.lanes[i].capacity = LANE_LENGTH;
    //Allocate heap memory for the lane
    isection.lanes[i].buffer = malloc(sizeof(struct car*) * LANE_LENGTH);

  }
}

/**
 * TODO: Fill in this function
 *
 * Populates the corresponding lane with cars as room becomes
 * available. Ensure to notify the cross thread as new cars are
 * added to the lane.
 *
 */
void *car_arrive(void *arg) {
    struct lane *l = arg;
    pthread_mutex_lock(&l->lock);
    struct car *cur_car = l->in_cars;
    while (cur_car != NULL){
      /* If the lane is full, wait on the other threads to make
      room in the lane before proceeding */
      if(l->in_buf == l->capacity){
        pthread_cond_wait(&l->producer_cv, &l->lock);
      }

      l->buffer[l->tail] = cur_car;
      l->tail = (l->tail + 1) % LANE_LENGTH;

      l->in_buf++;
      cur_car = cur_car->next;
      pthread_cond_signal(&l->consumer_cv);
    }
    pthread_mutex_unlock(&l->lock);

    return NULL;
}

/**
 * TODO: Fill in this function
 *
 * Moves cars from a single lane across the intersection. Cars
 * crossing the intersection must abide the rules of the road
 * and cross along the correct path. Ensure to notify the
 * arrival thread as room becomes available in the lane.
 *
 * Note: After crossing the intersection the car should be added
 * to the out_cars list of the lane that corresponds to the car's
 * out_dir. Do not free the cars!
 *
 *
 * Note: For testing purposes, each car which gets to cross the
 * intersection should print the following three numbers on a
 * new line, separated by spaces:
 *  - the car's 'in' direction, 'out' direction, and id.
 *
 * You may add other print statements, but in the end, please
 * make sure to clear any prints other than the one specified above,
 * before submitting your final code.
 */
void *car_cross(void *arg) {
    struct lane *l = arg;
    pthread_mutex_lock(&l->lock);

    int j = 0;

    //If there are cars in this lane, compute their paths
    while (l->inc != l->passed){
      //If there are no cars in this lane, wait for one to enter
      if(l->in_buf == 0){
        pthread_cond_wait(&l->consumer_cv, &l->lock);
      }

      struct car *cur_car = l->buffer[l->head];
      struct lane *out_lane = &isection.lanes[cur_car->out_dir];
      int *path = compute_path(cur_car->in_dir, cur_car->out_dir);

      /* Aqcuire the locks for each quadrant in the path */
      for(j=0; j < 4; j++){
        if (path[j] != 0){
          pthread_mutex_lock(&isection.quad[j]);
        }
      }

      //Print out the car info
      printf("%d %d %d\n", cur_car->in_dir, cur_car->out_dir, cur_car->id);

      //Set the car to its exit lane
      cur_car->next = out_lane->out_cars;
      out_lane->out_cars = cur_car;

      l->passed++;
      l->in_buf--;
      l->head = (l->head + 1)% LANE_LENGTH;

      /* Release the locks for each quadrant in the path */
      for (j = 3; j >= 0; j--) {
          if (path[j] != 0) {
             pthread_mutex_unlock(&isection.quad[j]);
          }
      }
      pthread_cond_signal(&l->producer_cv);
    }
    if (l->in_buf == 0){
      free(l->buffer);
    }

    //Release the lane lock
    pthread_mutex_unlock(&l->lock);
    return NULL;
}

/**
 * TODO: Fill in this function
 *
 * Given a car's in_dir and out_dir return a sorted
 * list of the quadrants the car will pass through.
 *
 */
int *compute_path(enum direction in_dir, enum direction out_dir) {
  int path[4];
  int *path_ptr;

  if (in_dir == NORTH){
    if (out_dir == NORTH){
      path[0] = 1;
      path[1] = 2;
      path[2] = 3;
      path[3] = 4;
    }
    else if (out_dir == SOUTH){
      path[0] = 2;
      path[1] = 3;
      path[2] = 0;
      path[3] = 0;
    }
    else if (out_dir == EAST){
      path[0] = 2;
      path[1] = 3;
      path[2] = 4;
      path[3] = 0;
    }
    else if (out_dir == WEST){
      path[0] = 2;
      path[1] = 0;
      path[2] = 0;
      path[3] = 0;
    }
  }
  else if (in_dir == SOUTH){
    if (out_dir == NORTH){
      path[0] = 1;
      path[1] = 4;
      path[2] = 0;
      path[3] = 0;
    }
    else if (out_dir == SOUTH){
      path[0] = 1;
      path[1] = 2;
      path[2] = 3;
      path[3] = 4;
    }
    else if (out_dir == EAST){
      path[0] = 4;
      path[1] = 0;
      path[2] = 0;
      path[3] = 0;
    }
    else if (out_dir == WEST){
      path[0] = 1;
      path[1] = 2;
      path[2] = 4;
      path[3] = 0;
    }
  }
  else if (in_dir == EAST){
    if (out_dir == NORTH){
      path[0] = 1;
      path[1] = 0;
      path[2] = 0;
      path[3] = 0;
    }
    else if (out_dir == SOUTH){
      path[0] = 1;
      path[1] = 2;
      path[2] = 3;
      path[3] = 0;
    }
    else if (out_dir == EAST){
      path[0] = 1;
      path[1] = 2;
      path[2] = 3;
      path[3] = 4;
    }
    else if (out_dir == WEST){
      path[0] = 1;
      path[1] = 2;
      path[2] = 0;
      path[3] = 0;
    }
  }
  else if (in_dir == WEST){
    if (out_dir == NORTH){
      path[0] = 1;
      path[1] = 3;
      path[2] = 4;
      path[3] = 0;
    }
    else if (out_dir == SOUTH){
      path[0] = 3;
      path[1] = 0;
      path[2] = 0;
      path[3] = 0;
    }
    else if (out_dir == EAST){
      path[0] = 3;
      path[1] = 4;
      path[2] = 0;
      path[3] = 0;
    }
    else if (out_dir == WEST){
      path[0] = 1;
      path[1] = 2;
      path[2] = 3;
      path[3] = 4;
    }
  }
  path_ptr = path;
  return path_ptr;
}
