/**
 * description:
 *  Genetic algorithm for finding heuristic solution of Travelling Salesman Problem
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

// Variable used to generate pseudo-random numbers
unsigned int seed;

// Type definition of a Point
typedef float coord_t;
typedef struct Point
{
    coord_t x;
    coord_t y;
} Point;

// Structure base of chromosome. A chromosome is interpreted as a path
typedef uint8_t tag_t;
typedef struct Chromosome
{
    float distance;
    tag_t*  tour;
} Chromosome;

// Function to generate pseudo-random numbers
int myRandom()
{
    seed = (214013 * seed + 2531011);
    return(seed >> 13);
}

// Function definitions
void generateCities(Point* cities, size_t nCities);
void initPopulation(Chromosome* population, size_t popSize, size_t nCities);
void mutate(Chromosome* population, size_t popSize, size_t nCities);
void computeFitness(Chromosome* population, size_t popSize, Point* cities, size_t nCities);
float computePathDistance(Point* cities, size_t nCities, const tag_t *path);
float distance(Point a, Point b);
void mergeSort(Chromosome* in, size_t size);
void merge(Chromosome* a, size_t aSize, Chromosome* b, size_t bSize, Chromosome* c);
void copyPopulation(Chromosome* in, Chromosome* out, size_t popSize, size_t nCities);
void mate(Chromosome *in, size_t inSize, Chromosome *out, size_t outSize, size_t nCities);
int valid(const tag_t *in, size_t nCities);
void printPath(tag_t *path, Point* cities, size_t nCities);


int main(int argc, char** argv)
{
    size_t    epochs = 500;
    size_t    nCities = 250;
    size_t    popSize = 40000;
    double elitism = 0.1;
    seed = 12345;

    // obtain parameters at run time
    switch (argc)
    {
        case 6: elitism = atof(argv[5]);
        case 5: popSize = atoi(argv[4]);
        case 4: nCities = atoi(argv[3]);
        case 3: seed = atoi(argv[2]);
        case 2: epochs = atoi(argv[1]);
        default: break;
    }

    size_t eliteSize = popSize * elitism;
    Point* cities = malloc(nCities * sizeof(*cities));
    Chromosome* population = malloc(popSize * sizeof(*population));
    Chromosome* tmpPopulation = malloc(popSize * sizeof(*tmpPopulation));

    printf("Find shortest path for %ld cities. %ld Epochs. population Size: %ld\n", nCities, epochs, popSize);

    generateCities(cities, nCities); // generate random cities and initialize genetic population
    initPopulation(population, popSize, nCities);
    initPopulation(tmpPopulation, popSize, nCities);
    // generate random mutations into initial population
    for (size_t i = 0; i < 10; i++)
    {
        mutate(population, popSize, nCities);
    }
    // compute fitness and sort population by lower fitness, to generate elite
    computeFitness(population, popSize, cities, nCities);
    mergeSort(population, popSize);

    size_t outSize = popSize - eliteSize;
    size_t inSize = eliteSize;
    size_t aMateVector[outSize];
    size_t bMateVector[outSize];
    size_t cMateVector[outSize];
    size_t aMutateVector[outSize];
    size_t bMutateVector[outSize];

    // generate new populations from initial population
    for (size_t i = 0; i < epochs; i++)
    {
        #pragma omp parallel num_threads(4)
        {
            #pragma omp single
            {
                // Init random number vectors
                for (size_t m = 0; m < outSize; m++) {
                    aMateVector[m] = myRandom() % inSize;
                    bMateVector[m] = myRandom() % inSize;
                    cMateVector[m] = myRandom() % nCities;
                }

                for (size_t m = 0; m < outSize; m++) {
                    aMutateVector[m] = myRandom() % nCities;
                    bMutateVector[m] = myRandom() % nCities;
                }
            }

            // copy elite population to new generation
            copyPopulation(population, tmpPopulation, eliteSize, nCities);

            // mate(population, eliteSize, tmpPopulation + eliteSize, popSize - eliteSize, nCities); // mate from elite
            #pragma omp for schedule(static)
            for (size_t m = 0; m < outSize; m++)
            {
                // Declare thread private mask
                tag_t mask[nCities];
                memset(mask, 0xFF, nCities * sizeof(*mask));

                // Create new gene in Output population by mating to genes from the elite input population
                // select two random genes from elite population and mate them at random position pos
                size_t i1 = aMateVector[m];
                size_t i2 = bMateVector[m];
                size_t pos = cMateVector[m];
                const tag_t* parentA = population[i1].tour;
                const tag_t* parentB = population[i2].tour;
                tag_t* child = tmpPopulation[m + eliteSize].tour;

                // Copy first part of parent A to child
                memcpy(child, parentA, pos * sizeof(*child));

                for (size_t j = 0; j < pos; j++)
                {
                    mask[parentA[j]] = 0;
                }

                size_t k = pos;
                for (size_t j = 0; j < nCities; ++j)
                {
                    tag_t tmp = mask[parentB[j]];
                    child[k] = (parentB[j] & tmp) | (child[k] & ~tmp);
                    k += tmp & 1;
                }
            }

            // mutate(tmpPopulation + eliteSize, popSize - eliteSize, nCities); // do not affect elite
            #pragma omp for schedule(static)
            for (size_t m = 0; m < outSize; m++)
            {     // generate 2 random positions to swap
                size_t aPos = aMutateVector[m];
                size_t bPos = bMutateVector[m];
                tag_t cityA = tmpPopulation[m + eliteSize].tour[aPos];
                tmpPopulation[m + eliteSize].tour[aPos] = tmpPopulation[m + eliteSize].tour[bPos];
                tmpPopulation[m + eliteSize].tour[bPos] = cityA;
            }

            copyPopulation(tmpPopulation, population, popSize, nCities);

            // computeFitness(population, popSize, cities, nCities);
            #pragma omp for schedule(static)
            for (size_t j = 0; j < popSize; j++)
            {
                float dist = 0.0f;
                Point citiesInPath[nCities];
                for (size_t k = 0; k < nCities; ++k)
                {
                    citiesInPath[k] = cities[population[j].tour[k]];
                }

                for (size_t k = 1; k < nCities; k++)
                {
                    dist += distance(citiesInPath[k - 1], citiesInPath[k]);
                }

                population[j].distance = dist + distance(citiesInPath[nCities - 1], citiesInPath[0]);
            }

            #pragma omp single
            {
                // sort population by lower fitness, to generate new elite
                mergeSort(population, popSize);

                // display progress
                if (i % 50 == 1)
                {
                    // print current best individual
                    printf("Fitness: %f\n", population[0].distance);

                    // sanity check
                    if (!valid(population[0].tour, nCities))
                    {
                        printf("ERROR: tour is not a valid permutation of cities");
                        exit(1);
                    }
                }
            }
        }
    }

    // print final result
    printPath(population[0].tour, cities, nCities);
    // sanity check
    if (!valid(population[0].tour, nCities))
    {
        printf("ERROR: tour is not a valid permutation of cities");
        exit(1);
    }

    // free all allocated memory
    for (size_t i = 0; i < popSize; ++i)
    {
        free(population[i].tour);
        free(tmpPopulation[i].tour);
    }

    free(cities);
    free(population);
    free(tmpPopulation);

    return(0);
}

// Generate random positions for the cities
void generateCities(Point* cities, size_t nCities)
{
    for (size_t i = 0; i < nCities; i++)
    {
        cities[i].x = myRandom() % 4096;
        cities[i].y = myRandom() % 4096;
    }
}

// Initialize a population of popSize Chromosomes
void initPopulation(Chromosome* population, size_t popSize, size_t nCities)
{
    for (size_t i = 0; i < popSize; i++)
    {
        population[i].distance = 0;
        population[i].tour = malloc(nCities * sizeof(*population[i].tour));
        for (size_t j = 0; j < nCities; j++)
        {
            population[i].tour[j] = j;
        }
    }
}

// mutate population: swap cities from two random positions in Chromosome
void mutate(Chromosome* population, size_t popSize, size_t nCities)
{
    size_t aVector[popSize];
    size_t bVector[popSize];
    for (size_t m = 0; m < popSize; m++)
    {
        aVector[m] = myRandom() % nCities;
        bVector[m] = myRandom() % nCities;
    }

    #pragma omp parallel num_threads(4)
    #pragma omp single
    #pragma omp taskloop
    for (size_t m = 0; m < popSize; m++)
    {     // generate 2 random positions to swap
        size_t aPos = aVector[m];
        size_t bPos = bVector[m];
        int cityA = population[m].tour[aPos];
        population[m].tour[aPos] = population[m].tour[bPos];
        population[m].tour[bPos] = cityA;
    }
}

//  Calculate each individual fitness in population. Fitness is path distance
// IDEA:
//  Store distances as a lower triangular matrix instead of computing them in every iteration
//  For matrix n by n you need array (n+1)*n/2 length and transition rule is Matrix[i][j] = Array[i*(i+1)/2+j]
void computeFitness(Chromosome* population, size_t popSize, Point* cities, size_t nCities)
{
    #pragma omp parallel num_threads(4)
    #pragma omp single
    #pragma omp taskloop
    for (size_t i = 0; i < popSize; i++)
    {
        float dist = 0.0f;
        Point citiesInPath[nCities];
        for (size_t j = 0; j < nCities; ++j)
        {
            citiesInPath[j] = cities[population[i].tour[j]];
        }

        for (size_t j = 1; j < nCities; j++)
        {
            dist += distance(citiesInPath[j - 1], citiesInPath[j]);
        }

        population[i].distance = dist + distance(citiesInPath[nCities - 1], citiesInPath[0]);
    }
}

// Distance between two Points
inline float distance(const Point a, const Point b)
{
    return(sqrtf((float)(a.x - b.x) * (float)(a.x - b.x) +
                 (float)(a.y - b.y) * (float)(a.y - b.y)));
}

// Sort array A with n Chromosomes using recursive merge-sort algorithm
void mergeSort(Chromosome* in, size_t size)
{
    if (size < 2)
    {
        return;         // the array is sorted when n=1
    }
    if (size == 2)
    {
        if (in[0].distance > in[1].distance)
        {         // swap values of A[0] and A[1]
            Chromosome temp = in[1];
            in[1] = in[0];
            in[0] = temp;
        }
        return;         // elements sorted
    }

    // divide A into two arrays, A1 and A2
    size_t n1 = size / 2;      // number of elements in A1
    size_t n2 = size - n1;     // number of elements in A2
    Chromosome* firstH = malloc(sizeof(*firstH) * n1);
    Chromosome* secondH = malloc(sizeof(*secondH) * n2);

    // move first n/2 elements to A1
    for (size_t i = 0; i < n1; i++)
    {
        firstH[i] = in[i];         // copy full entry
    }
    // move the rest to A2
    for (size_t i = 0; i < n2; i++)
    {
        secondH[i] = in[i + n1];         // copy full entry
    }

    // recursive calls
    mergeSort(firstH, n1);
    mergeSort(secondH, n2);

    // merge
    merge(firstH, n1, secondH, n2, in);

    // free allocated memory
    free(firstH);
    free(secondH);
}

// Merge two sorted arrays, A with szA Chromosomes and B with szB Chromosomes, into a sorted array C
void merge(Chromosome* a, size_t aSize, Chromosome* b, size_t bSize, Chromosome* c)
{
    size_t i = 0, j = 0;

    while (i + j < aSize + bSize)
    {
        if (j == bSize || ((i < aSize) && (a[i].distance <= b[j].distance)))
        {
            c[i + j] = a[i];             // copy full struct
            i++;
        }
        else
        {
            c[i + j] = b[j];             // copy full struct
            j++;
        }
    }
}

// copy input population to output population
void copyPopulation(Chromosome* in, Chromosome* out, size_t popSize, size_t nCities)
{
    #pragma omp for schedule(static)
    for (size_t i = 0; i < popSize; i++)
    {
        memcpy(out[i].tour, in[i].tour, nCities * sizeof(*out[i].tour));
    }
}

// mate randomly the elite population in in into P_out
void mate(Chromosome *in, size_t inSize, Chromosome *out, size_t outSize, size_t nCities)
{
    // Init random number vectors
    size_t aVector[outSize];
    size_t bVector[outSize];
    size_t cVector[outSize];
    for (size_t m = 0; m < outSize; m++)
    {
        aVector[m] = myRandom() % inSize;
        bVector[m] = myRandom() % inSize;
        cVector[m] = myRandom() % nCities;
    }

    // mate the elite population to generate new genes
    #pragma omp parallel for num_threads(4)
    for (size_t m = 0; m < outSize; m++)
    {
        // Declare thread private mask
        tag_t mask[nCities];
        memset(mask, 0xFF, nCities * sizeof(*mask));

        // Create new gene in Output population by mating to genes from the elite input population
        // select two random genes from elite population and mate them at random position pos
        size_t i1 = aVector[m];
        size_t i2 = bVector[m];
        size_t pos = cVector[m];
        const tag_t* parentA = in[i1].tour;
        const tag_t* parentB = in[i2].tour;
        tag_t* child = out[m].tour;

        // Copy first part of parent A to child
        memcpy(child, parentA, pos * sizeof(*child));

        for (size_t i = 0; i < pos; i++)
        {
            mask[parentA[i]] = 0;
        }

        size_t k = pos;
        for (size_t i = 0; i < nCities; ++i)
        {
            tag_t tmp = mask[parentB[i]];
            child[k] = (parentB[i] & tmp) | (child[k] & ~tmp);
            k += tmp & 1;
        }
    }
}

// Checks is a path is valid: does not contain repeats
int valid(const tag_t *in, size_t nCities)
{
    // clear mask
    tag_t mask[nCities];
    // Clear mask of already visited cities
    memset(mask, 0, nCities * sizeof(*mask));

    // check if city has been already visited, otherwise insert city in mask
    for (size_t i = 0; i < nCities; i++)
    {
        if (mask[in[i]] == 0)
        {
            mask[in[i]] = 1;
        }
        else
        {
            return(0);
        }
    }

    return(1);
}

// Display path into screen
void printPath(tag_t *path, Point* cities, size_t nCities)
{
    size_t   i;
    float dist = 0.0f;

    for (i = 1; i < nCities; i++)
    {
        printf("%d,", path[i - 1]);
        dist = dist + distance(cities[path[i - 1]], cities[path[i]]);
    }
    printf("%d\nTotal Distance: %f\n", path[i - 1],
           dist + distance(cities[path[i - 1]], cities[path[0]]));
}
