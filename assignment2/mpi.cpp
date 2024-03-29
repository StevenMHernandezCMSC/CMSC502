#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <float.h>
#include <math.h>
#include <iostream>
#include <fstream>
#include <mpi.h>
#include "functions.h"

double *created_double_array_from(points_container &container) {
    double *points_extrapolated = (double *) malloc(container.count * 2 * sizeof(double));

    for (int p = 0; p < container.count; p++) {
        points_extrapolated[p * 2] = container.points[p].x;
        points_extrapolated[(p * 2) + 1] = container.points[p].y;
    }

    return points_extrapolated;
}

points_container *create_points_container_from(double *points_extrapolated, int number_of_points) {
    points_container *container = (points_container *) malloc(sizeof(points_container));
    container->count = number_of_points;
    container->points = (point *) malloc(number_of_points * 2 * sizeof(point));

    for (int i = 0; i < number_of_points; i++) {
        point *p = &(container->points[i]);
        *p = point();

        (*p).x = points_extrapolated[i * 2];
        (*p).y = points_extrapolated[(i * 2) + 1];
    }
    return container;
}

/**
 * Check the cost of connecting consecutive points a1 and a2 from graph A
 * with consecutive points b1 and b2 from graph B.
 *
 * @param a1
 * @param a2
 * @param b1
 * @param b2
 * @return
 */
double swapCost(point *a1, point *a2, point *b1, point *b2) {
    return distance(a1, b2) + distance(b1, a2) - distance(a1, a2) - distance(b1, b2);
}

/**
 * Creates and returns a new point_container with the merged results of the two points_containers
 *
 * @param current
 * @param received
 * @return
 */
points_container *merge_and_create(points_container &current, points_container &received) {
    points_container *merged = (points_container *) malloc(sizeof(points_container));
    merged->count = current.count + received.count;
    merged->points = (point *) malloc((current.count + received.count) * sizeof(point));

    // Store the min value and indices for the first point from each container
    double min = INF;
    int min_curr_i = 0;
    int min_recv_i = 0;

    for (int curr_i = 0; curr_i < current.count; curr_i++) {
        for (int recv_i = 0; recv_i < received.count; recv_i++) {
            int subsequent_curr_i = (curr_i + 1) % current.count;
            int subsequent_recv_i = (recv_i + 1) % received.count;
            double cost = swapCost(&current.points[curr_i], &current.points[subsequent_curr_i],
                                   &received.points[recv_i], &received.points[subsequent_recv_i]);

            if (cost < min) {
                min = cost;
                min_curr_i = curr_i;
                min_recv_i = recv_i;
            }
        }
    }

    int min_subsequent_curr_i = (min_curr_i + 1) % current.count;
    int min_subsequent_recv_i = (min_recv_i + 1) % received.count;

    double distance_of_first_pairing_option = distance(&current.points[min_curr_i],
                                                       &received.points[min_recv_i])
                                              + distance(&current.points[min_subsequent_curr_i],
                                                         &received.points[min_subsequent_recv_i]);

    double distance_of_second_pairing_option = distance(&current.points[min_curr_i],
                                                        &received.points[min_subsequent_recv_i])
                                               + distance(&current.points[min_subsequent_curr_i],
                                                          &received.points[min_recv_i]);

    bool should_reverse = distance_of_first_pairing_option < distance_of_second_pairing_option;

    int counter = 0;
    if (should_reverse) {
        for (int i = 0; i <= min_curr_i; i++) {
            merged->points[counter++] = current.points[i];
        }
        for (int i = min_recv_i; i >= 0; i--) {
            merged->points[counter++] = received.points[i];
        }

        for (int i = received.count - 1; i > min_recv_i; i--) {
            merged->points[counter++] = received.points[i];
        }
        for (int i = min_curr_i + 1; i < current.count; i++) {
            merged->points[counter++] = current.points[i];
        }
    } else {
        for (int i = current.count - 1; i > min_curr_i; i--) {
            merged->points[counter++] = current.points[i];
        }
        for (int i = min_recv_i; i >= 0; i--) {
            merged->points[counter++] = received.points[i];
        }

        for (int i = received.count - 1; i > min_recv_i; i--) {
            merged->points[counter++] = received.points[i];
        }
        for (int i = min_curr_i; i >= 0; i--) {
            merged->points[counter++] = current.points[i];
        }
    }

    free(current.points);
    free(&current);
    free(received.points);
    free(&received);

    return merged;
}

void handleInversions(points_container *container) {
    int point_count = container->count;
    int intersection_count = 1;
    int counter = 0;
    while (intersection_count > 0) {
        intersection_count = 0;
        for (int x0 = 0; x0 < point_count - 2; x0++) {
            int x1 = x0 + 1;

            for (int y_i = 0; y_i < point_count - 1 - 2; y_i++) {
                int y0 = (y_i + x1 + 1) % container->count;
                int y1 = (y0 + 1) % container->count;

                if (pointsIntersect(container->points[x0],
                                    container->points[x1],
                                    container->points[y0],
                                    container->points[y1])) {
                    handleInversion(&container, x1, y0);
                    intersection_count++;
                }
            }
        }
        counter++;
        if (counter == 100) {
            break;
        }
    }
}

/**
 *
 * Traveling Salesman Algorithm
 *
 */

#define INF DBL_MAX

/**
 *
 * MAIN
 *
 */
int main(int argc, char *argv[]) {
    int points_per_block = 20;

    if (argc > 1) {
        points_per_block = atoi(argv[1]);
    }

    int rank, total_tasks;
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &total_tasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Status stat;
    points_container *current_process_point_container;
    int *current_processes_final_path;
    srandom(rank);

    struct timespec start, end;

    if (rank == 0) {
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    }

    points_container *points;

    int NUM_THREADS = total_tasks;

    int blocks_per_dimension = static_cast<int>(sqrt(total_tasks));

    /// Use the MPI Cartesian Topology
    MPI_Comm communicator;
    int dim_size[2] = {blocks_per_dimension, blocks_per_dimension};
    int periods[2] = {0, 0};
    int ierr = MPI_Cart_create(MPI_COMM_WORLD, 2, dim_size, periods, 0, &communicator);

    // Calculate row by row starting from the bottom left
    int block_row = static_cast<int>(floor(rank / blocks_per_dimension));
    int block_col = rank % blocks_per_dimension;

    /**
     * First, we generate all data in parallel AND calculate the TSP in one step.
     */
    current_process_point_container = (points_container *) malloc(sizeof(points_container));
    current_process_point_container->count = points_per_block;
    current_process_point_container->points = (point *) malloc(points_per_block * sizeof(point));

    /// Generate points for the current process
    for (int i = 0; i < points_per_block; i++) {
        point *p = &(current_process_point_container->points[i]);
        *p = point();

        (*p).x = (random() / (double) RAND_MAX) * 100 + (100 * (block_col + 1));
        (*p).y = (random() / (double) RAND_MAX) * 100 + (100 * (block_row + 1));

        //* Print the points generated by the current process.
        //* Format: "identifier,rank,x,y"
        //printf("generated_points,%i,%lf,%lf\n", rank, (*p).x, (*p).y);
    }

    current_processes_final_path = (int *) malloc((points_per_block + 1) * sizeof(int));

    /// Run sub TSP computation
    run_tsp(current_process_point_container, current_processes_final_path);

    /// Sort the points by the tsp
    points_container *sorted_point_container = (points_container *) malloc(sizeof(points_container));
    sorted_point_container->count = points_per_block;
    sorted_point_container->points = (point *) malloc(points_per_block * sizeof(point));
    for (int i = 0; i < points_per_block; i++) {
        int index = current_processes_final_path[i] - 1;
        sorted_point_container->points[i] = current_process_point_container->points[index];
        point *p = &(sorted_point_container->points[i]);

        //* Print the points selected in order as selected by the TSP
        //* Format: "identifier,rank,x,y"
        //printf("sub_tsp_path,%i,%lf,%lf\n", rank, (*p).x, (*p).y);
    }
    free(current_processes_final_path);
    free(current_process_point_container->points);
    free(current_process_point_container);
    current_process_point_container = sorted_point_container;

    // Because we might not have dimensions which are perfect squares
    int num_times_to_merge_per_dim = (int) pow(2, ceil(log2(blocks_per_dimension)));

    /*
     * Merge Columns per Row
     */
    int tagOffset = total_tasks;
    for (int i = 2; i <= num_times_to_merge_per_dim; i = i * 2) {
        if (block_col % i == 0) {
            // receive message from rank + (i / 2)
            int source = rank + (i / 2);

            if (source % blocks_per_dimension > block_col) { // if source doesn't exist, we don't want to cause a deadlock!
                int num_points_receiving;
                MPI_Recv(&num_points_receiving, 1, MPI_INT, source, i + tagOffset, communicator, &stat);

                double *values = (double *) malloc(num_points_receiving * 2 * sizeof(double));
                MPI_Recv(values, num_points_receiving * 2, MPI_DOUBLE, source, i + tagOffset, communicator, &stat);
                points_container *received = create_points_container_from(values, num_points_receiving);
                free(values);

                // merge
                current_process_point_container = merge_and_create(*current_process_point_container, *received);
                handleInversions(current_process_point_container);
            }
        } else if (block_col % i == i / 2) {
            // send message to rank - (i / 2)
            int destination = rank - (i / 2);

            MPI_Send(&current_process_point_container->count, 1, MPI_INT, destination, i + tagOffset, communicator);

            double *points_extrapolated = created_double_array_from(*current_process_point_container);
            MPI_Send(points_extrapolated, current_process_point_container->count * 2, MPI_DOUBLE, destination, i + tagOffset,
                     communicator);
        }
    }

    /*
     * Merge Rows
     */
    tagOffset = (2 * total_tasks);
    for (int i = 2; i <= num_times_to_merge_per_dim; i = i * 2) {
        if (block_row % i == 0) {
            // receive message from rank + ((i / 2) * blocks_per_dimension)
            int source = rank + ((i / 2) * blocks_per_dimension);

            if (source < total_tasks) { // if source doesn't exist, we don't want to cause a deadlock!
                int num_points_receiving;
                MPI_Recv(&num_points_receiving, 1, MPI_INT, source, i + tagOffset, communicator, &stat);

                double *values = (double *) malloc(num_points_receiving * 2 * sizeof(double));
                MPI_Recv(values, num_points_receiving * 2, MPI_DOUBLE, source, i + tagOffset, communicator, &stat);
                points_container *received = create_points_container_from(values, num_points_receiving);
                free(values);

                // merge
                current_process_point_container = merge_and_create(*current_process_point_container, *received);
                handleInversions(current_process_point_container);
            }
        } else if (block_row % i == i / 2) {
            // send message to rank - ((i / 2) * blocks_per_dimension)
            int destination = rank - ((i / 2) * blocks_per_dimension);

            MPI_Send(&current_process_point_container->count, 1, MPI_INT, destination, i + tagOffset, communicator);

            double *points_extrapolated = created_double_array_from(*current_process_point_container);
            MPI_Send(points_extrapolated, current_process_point_container->count * 2, MPI_DOUBLE, destination, i + tagOffset,
                     communicator);
        }
    }

    if (block_col == 0 && block_row == 0) {
        for (int i = 0; i < current_process_point_container->count; i++) {
            point *p = &(current_process_point_container->points[i]);
            //* Print the points after being merged across the row
            //* Format: "identifier,rank,x,y"
            //printf("merged_all,%i,%lf,%lf\n", rank, (*p).x, (*p).y);
        }
    }

    if (rank == 0) {
        double total_distance = 0;

        for (int i = 1; i < current_process_point_container->count; i++) {
            total_distance += distance(&current_process_point_container->points[i - 1],
                                       &current_process_point_container->points[i]);

        }
        total_distance += distance(&current_process_point_container->points[current_process_point_container->count - 1],
                                   &current_process_point_container->points[0]);

        //* Print the length of the TSP for the given number of processors and the number of cities per processor
        //* Format: "identifier,num processors,num cities,time(ms)"
        printf("tsp_length,%i,%i,%lf\n", total_tasks, current_process_point_container->count, total_distance);

        clock_gettime(CLOCK_MONOTONIC_RAW, &end);
        //* Print the time take for the given number of processors used and number of cities total
        //* Format: "identifier,num processors,num cities,time(ms)"
        printf("time_taken_ms,%i,%i,%lf\n", total_tasks, current_process_point_container->count, (1000000000L * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec) / 1e6);
    }

    MPI_Finalize();
    return 0;
}
