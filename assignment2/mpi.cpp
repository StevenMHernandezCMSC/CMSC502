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

/**
 *
 * GLOBAL VAR
 *
 */
points_container **point_containers;
int **final_paths;
double *results;
int *grid_degrees;

bool degreeAllowed(int g_i, int count) {
    int degree = grid_degrees[g_i];

    for (int i = 0; i < count; i++) {
        if (grid_degrees[i] < degree) {
            return false;
        }
    }

    return true;
}

void updateDegree(int g_i) {
    grid_degrees[g_i] += 2;
}

int degreesLeft(int count) {
    int counter = 0;

    for (int i = 0; i < count; i++) {
        if (grid_degrees[i] < 2) {
            counter += 1;
        }
    }

    return counter;
}

bool degreesAllEqual2(int count) {
    return degreesLeft(count) == 0;
}

void add_sub_graph(points_container **full_path, int *full_path_index, int g_i, int previous_nn_p, int *p_to_search,
                   int p_i) {
    if (point_containers[g_i]->count > 1) {
        int index_of_initial = find_index_in_path_for_point(previous_nn_p, point_containers[g_i],
                                                            final_paths[g_i]);

        int value_modifier = p_to_search[p_i] == final_paths[g_i][index_of_initial - 1] ? 1 : -1;

        int count_for_this_grid = point_containers[g_i]->count;
        for (int i = 1; i < count_for_this_grid; i++) {
            int calculated_path_index = (
                    ((i * value_modifier) + index_of_initial + count_for_this_grid) %
                    count_for_this_grid);
            int calculated_point_index = final_paths[g_i][calculated_path_index];
            (*full_path)->points[(*full_path_index)++] = point_containers[g_i]->points[
                    calculated_point_index - 1];
        }
    }
}

/**
 *
 * Traveling Salesman Algorithm
 *
 */

#define INF DBL_MAX;

#define POINTS_PER_BLOCK 10

#define GENERATE_POINTS_TAG 1
#define SEND_POINTS_TAG 2
#define RECEIV_PATH_TAG 3

/**
 *
 * MAIN
 *
 */
int main(int argc, char *argv[]) {
    /**
     *
     * MPI Code
     *
     */
    int rank, total_tasks;
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &total_tasks); // This will need to be a square number such that y = \sqrt(x) where x and y are positive integers.
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Status stat;
    points_container *current_process_point_container;
    int *current_processes_final_path;

    points_container *points;

    struct timespec start, end;

    int NUM_THREADS = total_tasks;

    int blocks_per_dimension = static_cast<int>(sqrt(total_tasks));

    /// Use the MPI Cartesian Topology
    MPI_Comm communicator;
    int dim_size[2] = {blocks_per_dimension, blocks_per_dimension};
    int periods[2] = {0,0};
    int ierr = MPI_Cart_create(MPI_COMM_WORLD, 2, dim_size, periods, 0, &communicator);

    /**
     * Master Node mallocs the required array spaces to receive path information from the slave processes
     * NOTE: this might not be required fully! I should plan more first!
     */
    if (rank == 0) {
        // TODO
    }

    // Calculate row by row starting from the bottom left
    int block_row = static_cast<int>(floor(rank / blocks_per_dimension));
    int block_col = rank % blocks_per_dimension;

    /**
     * First, we generate all data in parallel AND calculate the TSP in one step.
     */
    current_process_point_container = (points_container *) malloc(sizeof(points_container));
    current_process_point_container->count = POINTS_PER_BLOCK;
    current_process_point_container->points = (point *) malloc(POINTS_PER_BLOCK * sizeof(point));

    /// Generate points for the current process
    for (int i = 0; i < POINTS_PER_BLOCK; i++) {
        point *p = &(current_process_point_container->points[i]);
        *p = point();

        (*p).x = (rand() / (double) RAND_MAX) * 100 + (100 * (block_row + 1));
        (*p).y = (rand() / (double) RAND_MAX) * 100 + (100 * (block_col + 1));

        // Print the points generated by the current process.
        // Format: "identifier,rank,x,y"
//        printf("generated_points,%i,%lf,%lf\n", rank, (*p).x, (*p).y);
    }

    current_processes_final_path = (int *) malloc(POINTS_PER_BLOCK * sizeof(int));

    /// Run sub TSP computation
    run_tsp(current_process_point_container, current_processes_final_path);

    /// Sort the points by the tsp
    points_container *sorted_point_container = (points_container *) malloc(sizeof(points_container));
    sorted_point_container->count = POINTS_PER_BLOCK;
    sorted_point_container->points = (point *) malloc(POINTS_PER_BLOCK * sizeof(point));
    for (int i = 0; i < POINTS_PER_BLOCK; i++) {
        sorted_point_container->points[i] = current_process_point_container->points[current_processes_final_path[i] - 1];
        point *p = &(sorted_point_container->points[i]);

        // Print the points selected in order as selected by the TSP
        // Format: "identifier,rank,x,y"
//        printf("sub_tsp_path,%i,%lf,%lf\n",rank, (*p).x, (*p).y);
    }
    free(current_processes_final_path);
    free(current_process_point_container);
    current_process_point_container = sorted_point_container;


    /**
     * TODO: this is old code clean it up!
     */
     MPI_Finalize();
     return 0;
    if (rank == 0) {
        final_paths[0] = (int *) malloc(point_containers[0]->count * sizeof(int));
        final_paths[0] = current_processes_final_path;

        for (int i = 1; i < NUM_THREADS; i++) {
            final_paths[i] = (int *) malloc(point_containers[i]->count * sizeof(int));
            MPI_Recv(&final_paths[i][0], point_containers[i]->count, MPI_INT, i, RECEIV_PATH_TAG, MPI_COMM_WORLD, &stat);
        }
    } else {
        // send data to master
        MPI_Send(current_processes_final_path, current_process_point_container->count, MPI_INT, 0, RECEIV_PATH_TAG,
                 MPI_COMM_WORLD);

        MPI_Finalize();
    }

    if (rank == 0) {
        grid_degrees = (int *) calloc(NUM_THREADS, sizeof(int));

        points_container *full_path = (points_container *) malloc(sizeof(points_container));
        full_path->points = (point *) malloc(points->count * sizeof(point));
        full_path->count = points->count;
        int full_path_index = 0;

        // First, mark all "already completed" grids as completed
        for (int g_i = 0; g_i < NUM_THREADS; g_i++) {
            points_container *g = point_containers[g_i];
            if (g->count == 0) {
                grid_degrees[g_i] = 2;
            }
        }

        // for all g \in G // this will run in parallel
        for (int g_i = 0; g_i < 1; g_i++) { // TODO: run for all g_i < NUM_THREADS
            points_container *g = point_containers[g_i];
            grid_degrees[g_i] = 1;
            // p-to-search = all p \in g
            int count_p_to_search = g->count;
            int *p_to_search = (int *) malloc((count_p_to_search + 1) * sizeof(int));
            p_to_search = final_paths[g_i];
            bool done = false;
            // while true {
            while (!done) {
                // for all p \in p-to-search
                for (int p_i = 0; p_i < count_p_to_search; p_i++) {
                    /// Get Nearest neighbor
                    int nn_grid_index = -1;
                    int nn_p;
                    int previous_nn_p;
                    double nn_d = INF;
                    // for all g' \in G, g' != g and degreeAllowed(g')
                    for (int g1_i = 0; g1_i < NUM_THREADS; g1_i++) {
                        if (g1_i != g_i && degreeAllowed(g1_i, NUM_THREADS)) {
                            // for all p' \in g'
                            for (int p1_i = 1; p1_i <= point_containers[g1_i]->count; p1_i++) {
                                if (grid_degrees[g1_i] == 0 || point_are_neighbors(full_path->points[0],
                                                                                   point_containers[g1_i]->points[p1_i -
                                                                                                                  1],
                                                                                   point_containers[g1_i],
                                                                                   final_paths[g1_i])) {
                                    // d = distance(p, p')
                                    double d = distance(&point_containers[g_i]->points[p_to_search[p_i] - 1],
                                                        &point_containers[g1_i]->points[p1_i - 1]);
                                    // if d < nn_d
                                    if (d < nn_d) {
                                        nn_grid_index = g1_i;
                                        nn_p = p1_i;
                                        nn_d = d;
                                    }
                                }
                            }
                        }
                    }

                    point *point_we_are_about_to_add = &(point_containers[g_i]->points[p_to_search[p_i] - 1]);

                    if (full_path_index != 0) { // don't add the previous graph if we haven't added anything as of yet
                        // push the values for the grid we are leaving behind
                        // if best neighbor was previous neighbor
                        add_sub_graph(&full_path, &full_path_index, g_i, previous_nn_p, p_to_search, p_i);
                    }

                    full_path->points[full_path_index++] = *point_we_are_about_to_add;
                    full_path->points[full_path_index++] = point_containers[nn_grid_index]->points[nn_p - 1];

                    updateDegree(nn_grid_index);

                    if (degreesAllEqual2(NUM_THREADS)) {
                        int candidate_city_identifier = get_city_identifier(full_path->points[0],
                                                                            point_containers[nn_grid_index]);
                        int *fake_p_to_search = (int *) malloc(sizeof(int));
                        fake_p_to_search[0] = candidate_city_identifier;
                        add_sub_graph(&full_path, &full_path_index, nn_grid_index, nn_p, fake_p_to_search, 0);

                        // break
                        done = true;
                        break;
                    }

                    previous_nn_p = nn_p;
                    g_i = nn_grid_index;

                    count_p_to_search = get_count_p_to_search(point_containers[nn_grid_index]);
                    p_to_search = get_p_to_search(nn_p, point_containers[nn_grid_index], final_paths[nn_grid_index]);
                }
            }
        }


        // handle inversions
        int point_count = full_path_index - 1;
        full_path->count = point_count;
        for (int i = 0; i < 100; i++) {
            int intersection_count = 0;
            for (int x0 = 0; x0 < point_count - 2; x0++) {
                int x1 = x0 + 1;

                for (int y_i = 0; y_i < point_count - 1 - 2; y_i++) {
                    int y0 = (y_i + x1 + 1) % full_path->count;
                    int y1 = (y0 + 1) % full_path->count;

                    if (pointsIntersect(full_path->points[x0], full_path->points[x1], full_path->points[y0],
                                        full_path->points[y1])) {
                        handleInversion(&full_path, x1, y0);
                        intersection_count++;
                    }
                }
            }
        }

//        printf("\nconnected_connectors = [");
//        for (int k = 0; k < full_path_index - 1; k++) {
//            printf("%lf %lf;", full_path->points[k].x, full_path->points[k].y);
//        }
//        printf("]\n");

        clock_gettime(CLOCK_MONOTONIC_RAW, &end);
        uint64_t diff = (1000000000L * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec) / 1e6;

//        printf("time (ms): %llu\n", diff);
        double min = 0;

        for (int k = 0; k < full_path_index - 1 - 1; k++) {
            min += distance(&full_path->points[k], &full_path->points[k + 1]);
//        printf("%lf %lf;", full_path->points[k].x, full_path->points[k].y);
        }

        printf("%lf", min);
    }

    if (rank == 0) {
        // check if things are correct around here.
        MPI_Finalize();
    }

    /**
     *
     * End MPI Specific Code
     *
     */
}