#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>

#include "matrix.h"

static int g_seed = 0;

static ssize_t g_width = 0;
static ssize_t g_height = 0;
static ssize_t g_elements = 0;

static ssize_t g_nthreads = 1;

typedef union locked_data {
	ssize_t *frequency;
	float *value;
} Locked_Data;

//structure to pass data arround between functions and their jobs for threads
typedef struct job_info {
	const float *matrix;
	const float *matrix_two;
	float *affected_matrix;
	float value;
	ssize_t values;
	ssize_t row;
	ssize_t position;
	Locked_Data variable;
} Job_Info;

pthread_mutex_t lock;

////////////////////////////////
///     UTILITY FUNCTIONS    ///
////////////////////////////////

/**
 * Returns pseudorandom number determined by the seed.
 */
int fast_rand(void)
{

	g_seed = (214013 * g_seed + 2531011);
	return (g_seed >> 16) & 0x7FFF;
}

/**
 * Sets the seed used when generating pseudorandom numbers.
 */
void set_seed(int seed)
{

	g_seed = seed;
}

/**
 * Sets the number of threads available.
 */
void set_nthreads(ssize_t count)
{

	g_nthreads = count;
}

/**
 * Sets the dimensions of the matrix.
 */
void set_dimensions(ssize_t order)
{

	g_width = order;
	g_height = order;

	g_elements = g_width * g_height;
}

/**
 * Displays given matrix.
 */
void display(const float *matrix)
{

	for (ssize_t y = 0; y < g_height; y++) {
		for (ssize_t x = 0; x < g_width; x++) {
			if (x > 0)
				printf(" ");
			printf("%.2f", matrix[y * g_width + x]);
		}

		printf("\n");
	}
}

/**
 * Displays given matrix row.
 */
void display_row(const float *matrix, ssize_t row)
{

	for (ssize_t x = 0; x < g_width; x++) {
		if (x > 0)
			printf(" ");
		printf("%.2f", matrix[row * g_width + x]);
	}

	printf("\n");
}

/**
 * Displays given matrix column.
 */
void display_column(const float *matrix, ssize_t column)
{

	for (ssize_t i = 0; i < g_height; i++) {
		printf("%.2f\n", matrix[i * g_width + column]);
	}
}

/**
 * Displays the value stored at the given element index.
 */
void display_element(const float *matrix, ssize_t row, ssize_t column)
{

	printf("%.2f\n", matrix[row * g_width + column]);
}

////////////////////////////////
///   MATRIX INITALISATIONS  ///
////////////////////////////////

/**
 * Returns new matrix with all elements set to zero.
 */
float *new_matrix(void)
{

	return calloc(g_elements, sizeof(float));
}

/**
 * Returns new identity matrix.
 */
float *identity_matrix(void)
{

	float *result = new_matrix();

	/*
	   TODO

	   1 0
	   0 1
	 */

	ssize_t x = 0;
	ssize_t y = 0;
	while (x < g_width) {
		*(result + x + (y * g_width)) = 1;
		y++;
		x++;
	}
	return result;
}

/**
 * Returns new matrix with elements generated at random using given seed.
 */
float *random_matrix(int seed)
{

	float *matrix = new_matrix();

	set_seed(seed);

	for (ssize_t i = 0; i < g_elements; i++) {
		matrix[i] = fast_rand();
	}

	return matrix;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  uniform_matrix_job
 *  Description:  Is the job that sets the elements to given value
 * =====================================================================================
 */

void *uniform_matrix_job(void *info)
{
	struct job_info *data = (struct job_info *)info;
	float *affected_matrix = data->affected_matrix;
	ssize_t x = 0;
	while (x < g_width) {
		*(affected_matrix + x + (g_width * data->row)) = data->value;
		x++;
	}
	free(info);
	return NULL;
}				/* -----  end of function uniform_matrix_job  ----- */

/**
 * Returns new matrix with all elements set to given value.
 */
float *uniform_matrix(float value)
{

	float *result = new_matrix();

	pthread_t thread_ids[g_nthreads];
	ssize_t row = 0;
	while (g_height - row >= g_nthreads) {
		ssize_t thread = 0;
		while (thread < g_nthreads) {
			Job_Info *data = malloc(sizeof(Job_Info));
			data->row = row;
			data->affected_matrix = result;
			data->value = value;
			pthread_create(thread_ids + thread, NULL,
				       uniform_matrix_job, data);
			thread++;
			row++;
		}
		thread = 0;
		while (thread < g_nthreads) {
			pthread_join(thread_ids[thread], NULL);
			thread++;
		}
	}
	if (row < g_height - 1) {
		ssize_t thread = 0;
		while (row < g_height - 1) {
			Job_Info *data = malloc(sizeof(Job_Info));
			data->row = row;
			data->affected_matrix = result;
			data->value = value;
			pthread_create(thread_ids + thread, NULL,
				       uniform_matrix_job, data);
			thread++;
			row++;
		}
		while (thread >= 0) {
			pthread_join(*(thread_ids + thread), NULL);
			thread--;
		}
	}
	return result;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  sequence_matrix_job
 *  Description:  Is the job that sets the elements to the right value.
 * =====================================================================================
 */

void *sequence_matrix_job(void *info)
{
	Job_Info *data = (Job_Info *) info;
	float *affected_matrix = data->affected_matrix;
	ssize_t x = 0;
	float value =
	    *data->variable.value + (data->value * data->row * g_width);
	while (x < g_width) {
		*(affected_matrix + x + (data->row * g_width)) = value;
		value = value + data->value;
		x++;
	}
	free(info);
	return NULL;
}				/* -----  end of function sequence_matrix_job  ----- */

/**
 * Returns new matrix with elements in sequence from given start and step
 */
float *sequence_matrix(float start, float step)
{

	float *result = new_matrix();

	pthread_t thread_ids[g_nthreads];
	ssize_t row = 0;
	while (g_height - row >= g_nthreads) {
		ssize_t thread = 0;
		while (thread < g_nthreads) {
			Job_Info *data = malloc(sizeof(Job_Info));
			data->row = row;
			data->affected_matrix = result;
			data->value = step;
			data->variable.value = &start;
			pthread_create(thread_ids + thread, NULL,
				       sequence_matrix_job, data);
			thread++;
			row++;
		}
		thread = 0;
		while (thread < g_nthreads) {
			pthread_join(thread_ids[thread], NULL);
			thread++;
		}
	}
	if (row <= g_height - 1) {
		ssize_t thread = 0;
		while (row < g_height - 1) {
			Job_Info *data = malloc(sizeof(Job_Info));
			data->row = row;
			data->affected_matrix = result;
			data->value = step;
			data->variable.value = &start;
			pthread_create(thread_ids + thread, NULL,
				       sequence_matrix_job, data);
			thread++;
			row++;
		}
		while (thread >= 0) {
			pthread_join(*(thread_ids + thread), NULL);
			thread--;
		}
	}
	return result;
}

////////////////////////////////
///     MATRIX OPERATIONS    ///
////////////////////////////////

/**
 * Returns new matrix with elements cloned from given matrix.
 */
float *cloned(const float *matrix)
{

	float *result = new_matrix();

	for (ssize_t y = 0; y < g_height; y++) {
		for (ssize_t x = 0; x < g_width; x++) {
			result[y * g_width + x] = matrix[y * g_width + x];
		}
	}

	return result;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  float_compare
 *  Description:  
 * =====================================================================================
 */

int float_compare(const void *a, const void *b)
{
	return (*(float *)a - *(float *)b);
}				/* -----  end of function float_compare  ----- */

/**
 * Returns new matrix with elements in ascending order.
 */
float *sorted(const float *matrix)
{
	float *result = cloned(matrix);
	qsort(result, g_elements, sizeof(float), float_compare);
	return result;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  rotate_job
 *  Description:  
 * =====================================================================================
 */

void *rotate_job(void *info)
{
	Job_Info *data = (Job_Info *) info;
	float *affected_matrix = data->affected_matrix;
	ssize_t y = g_height - 1;
	ssize_t x = 0;
	while (x < g_width) {
		*(affected_matrix + x + g_width * data->row) =
		    *(data->matrix + data->row + g_width * y);
		x++;
		y--;
	}
	free(info);
	return NULL;
}				/* -----  end of function rotate_job  ----- */

/**
 * Returns new matrix with elements rotated 90 degrees clockwise.
 */
float *rotated(const float *matrix)
{

	float *result = new_matrix();

	pthread_t thread_ids[g_nthreads];
	ssize_t row = 0;
	while (g_height - row >= g_nthreads) {
		ssize_t thread = 0;
		while (thread < g_nthreads) {
			Job_Info *data = malloc(sizeof(Job_Info));
			data->row = row;
			data->affected_matrix = result;
			data->matrix = matrix;
			pthread_create(thread_ids + thread, NULL, rotate_job,
				       data);
			thread++;
			row++;
		}
		thread = 0;
		while (thread < g_nthreads) {
			pthread_join(*(thread_ids + thread), NULL);
			thread++;
		}
	}
	if (row < g_height - 1) {
		ssize_t thread = 0;
		while (row < g_height - 1) {
			Job_Info *data = malloc(sizeof(Job_Info));
			data->row = row;
			data->affected_matrix = result;
			data->matrix = matrix;
			pthread_create(thread_ids + thread, NULL, rotate_job,
				       data);
			thread++;
			row++;
		}
		while (thread >= 0) {
			pthread_join(*(thread_ids + thread), NULL);
			thread--;
		}
	}
	return result;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  reverse_job
 *  Description:  
 * =====================================================================================
 */

void *reverse_job(void *info)
{
	Job_Info *data = (Job_Info *) info;
	float *affected_matrix = data->affected_matrix;
	ssize_t x = 0;
	ssize_t y = g_height - 1;
	while (x < g_width) {
		*(affected_matrix + x + (g_width * data->row)) =
		    *(data->matrix + (g_width - x - 1) +
		      (y - data->row) * g_width);
		x++;
	}
	free(info);
	return NULL;
}				/* -----  end of function reverse_job  ----- */

/**
 * Returns new matrix with elements ordered in reverse.
 */
float *reversed(const float *matrix)
{

	float *result = new_matrix();

	/*
	   TODO

	   1 2    4 3
	   3 4 => 2 1
	 */

	pthread_t thread_ids[g_nthreads];
	ssize_t row = 0;
	while (g_height - row >= g_nthreads) {
		ssize_t thread = 0;
		while (thread < g_nthreads) {
			Job_Info *data = malloc(sizeof(Job_Info));
			data->row = row;
			data->affected_matrix = result;
			data->matrix = matrix;
			pthread_create(thread_ids + thread, NULL, reverse_job,
				       data);
			thread++;
			row++;
		}
		thread = 0;
		while (thread < g_nthreads) {
			pthread_join(*(thread_ids + thread), NULL);
			thread++;
		}
	}
	if (row < g_height - 1) {
		ssize_t thread = 0;
		while (row < g_height - 1) {
			Job_Info *data = malloc(sizeof(Job_Info));
			data->row = row;
			data->affected_matrix = result;
			data->matrix = matrix;
			pthread_create(thread_ids + thread, NULL, reverse_job,
				       data);
			thread++;
			row++;
		}

		while (thread > -1) {
			pthread_join(*(thread_ids + thread), NULL);
			thread--;
		}
	}
	return result;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  transpose_job
 *  Description:  
 * =====================================================================================
 */

void *transpose_job(void *info)
{
	Job_Info *data = (Job_Info *) info;
	float *affected_matrix = data->affected_matrix;
	ssize_t x = 0;
	ssize_t y = 0;
	while (x < g_width) {
		*(affected_matrix + x + data->row * g_width) =
		    *(data->matrix + data->row + y * g_width);
		x++;
		y++;
	}
	free(info);
	return NULL;
}				/* -----  end of function transpose_job  ----- */

/**
 * Returns new transposed matrix.
 */
float *transposed(const float *matrix)
{

	float *result = new_matrix();

	pthread_t thread_ids[g_nthreads];
	ssize_t row = 0;
	while (g_height - row >= g_nthreads) {
		ssize_t thread = 0;
		while (thread < g_nthreads) {
			Job_Info *data = malloc(sizeof(Job_Info));
			data->row = row;
			data->affected_matrix = result;
			data->matrix = matrix;
			pthread_create(thread_ids + thread, NULL, transpose_job,
				       data);
			thread++;
			row++;
		}
		thread = 0;
		while (thread < g_nthreads) {
			pthread_join(*(thread_ids + thread), NULL);
			thread++;
		}
	}
	if (row < g_height - 1) {
		ssize_t thread = 0;
		while (row < g_height - 1) {
			Job_Info *data = malloc(sizeof(Job_Info));
			data->row = row;
			data->affected_matrix = result;
			data->matrix = matrix;
			pthread_create(thread_ids + thread, NULL, transpose_job,
				       data);
			thread++;
			row++;
		}
		while (thread >= 0) {
			pthread_join(*(thread_ids + thread), NULL);
			thread--;
		}
	}

	return result;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  scalar_add_job
 *  Description:  
 * =====================================================================================
 */

void *scalar_add_job(void *info)
{
	Job_Info *data = (Job_Info *) info;
	float *affected_matrix = data->affected_matrix;
	ssize_t x = 0;
	while (x < g_width) {
		*(affected_matrix + x + g_width * data->row) =
		    *(data->matrix + x + g_width * data->row) + data->value;
		x++;
	}
	free(data);
	return NULL;
}				/* -----  end of function scalar_add_job  ----- */

/**
 * Returns new matrix with scalar added to each element.
 */
float *scalar_add(const float *matrix, float scalar)
{

	float *result = new_matrix();

	/*
	   TODO

	   1 0        2 1
	   0 1 + 1 => 1 2

	   1 2        5 6
	   3 4 + 4 => 7 8
	 */

	pthread_t thread_ids[g_nthreads];
	ssize_t row = 0;
	while (g_height - row >= g_nthreads) {
		ssize_t thread = 0;
		while (thread < g_nthreads) {
			Job_Info *data = malloc(sizeof(Job_Info));
			data->row = row;
			data->affected_matrix = result;
			data->matrix = matrix;
			data->value = scalar;
			pthread_create(thread_ids + thread, NULL,
				       scalar_add_job, data);
			thread++;
			row++;
		}
		thread = 0;
		while (thread < g_nthreads) {
			pthread_join(*(thread_ids + thread), NULL);
			thread++;
		}
	}
	if (row < g_height - 1) {
		ssize_t thread = 0;
		while (row < g_height - 1) {
			Job_Info *data = malloc(sizeof(Job_Info));
			data->row = row;
			data->affected_matrix = result;
			data->matrix = matrix;
			data->value = scalar;
			pthread_create(thread_ids + thread, NULL,
				       scalar_add_job, data);
			thread++;
			row++;
		}
		while (thread >= 0) {
			pthread_join(*(thread_ids + thread), NULL);
			thread--;
		}
	}
	return result;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  scalar_mul_job
 *  Description:  
 * =====================================================================================
 */

void *scalar_mul_job(void *info)
{
	Job_Info *data = (Job_Info *) info;
	float *affected_matrix = data->affected_matrix;
	ssize_t x = 0;
	while (x < g_width) {
		*(affected_matrix + x + g_width * data->row) =
		    *(data->matrix + x + g_width * data->row) * data->value;
		x++;
	}
	free(data);
	return NULL;
}				/* -----  end of function scalar_mul_job  ----- */

/**
 * Returns new matrix with scalar multiplied to each element.
 */
float *scalar_mul(const float *matrix, float scalar)
{

	float *result = new_matrix();

	/*
	   TODO

	   1 0        2 0
	   0 1 x 2 => 0 2

	   1 2        2 4
	   3 4 x 2 => 6 8
	 */

	pthread_t thread_ids[g_nthreads];
	ssize_t row = 0;
	while (g_height - row >= g_nthreads) {
		ssize_t thread = 0;
		while (thread < g_nthreads) {
			Job_Info *data = malloc(sizeof(Job_Info));
			data->row = row;
			data->affected_matrix = result;
			data->matrix = matrix;
			data->value = scalar;
			pthread_create(thread_ids + thread, NULL,
				       scalar_mul_job, data);
			thread++;
			row++;
		}
		thread = 0;
		while (thread < g_nthreads) {
			pthread_join(*(thread_ids + thread), NULL);
			thread++;
		}
	}
	if (row < g_height - 1) {
		ssize_t thread = 0;
		while (row < g_height - 1) {
			Job_Info *data = malloc(sizeof(Job_Info));
			data->row = row;
			data->affected_matrix = result;
			data->matrix = matrix;
			data->value = scalar;
			pthread_create(thread_ids + thread, NULL,
				       scalar_mul_job, data);
			thread++;
			row++;
		}
		while (thread >= 0) {
			pthread_join(*(thread_ids + thread), NULL);
			thread--;
		}
	}

	return result;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  matrix_add_job
 *  Description:  
 * =====================================================================================
 */

void *matrix_add_job(void *info)
{
	Job_Info *data = (Job_Info *) info;
	float *affected_matrix = data->affected_matrix;
	ssize_t x = 0;
	while (x < g_width) {
		*(affected_matrix + x + data->row * g_width) =
		    *(data->matrix + x + data->row * g_width) +
		    *(data->matrix_two + x + data->row * g_width);
		x++;
	}
	free(info);
	return NULL;
}				/* -----  end of function matrix_add_job  ----- */

/**
 * Returns new matrix that is the result of
 * adding the two given matrices together.
 */
float *matrix_add(const float *matrix_a, const float *matrix_b)
{

	float *result = new_matrix();

	/*
	   TODO

	   1 0   0 1    1 1
	   0 1 + 1 0 => 1 1

	   1 2   4 4    5 6
	   3 4 + 4 4 => 7 8
	 */

	pthread_t thread_ids[g_nthreads];
	ssize_t row = 0;
	while (g_height - row >= g_nthreads) {
		ssize_t thread = 0;
		while (thread < g_nthreads) {
			Job_Info *data = malloc(sizeof(Job_Info));
			data->row = row;
			data->affected_matrix = result;
			data->matrix = matrix_a;
			data->matrix_two = matrix_b;
			pthread_create(thread_ids + thread, NULL,
				       matrix_add_job, data);
			thread++;
			row++;
		}
		thread = 0;
		while (thread < g_nthreads) {
			pthread_join(*(thread_ids + thread), NULL);
			thread++;
		}
	}
	if (row < g_height - 1) {
		ssize_t thread = 0;
		while (row < g_height - 1) {
			Job_Info *data = malloc(sizeof(Job_Info));
			data->row = row;
			data->affected_matrix = result;
			data->matrix = matrix_a;
			data->matrix_two = matrix_b;
			pthread_create(thread_ids + thread, NULL,
				       matrix_add_job, data);
			thread++;
			row++;
		}
		while (thread >= 0) {
			pthread_join(*(thread_ids + thread), NULL);
			thread--;
		}
	}

	return result;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  matrix_mul_job
 *  Description:  
 * =====================================================================================
 */

void *matrix_mul_job(void *info)
{
	float product = 0;
	Job_Info *data = (Job_Info *) info;
	ssize_t i = 0;
	while (i < g_width) {
		product = product +
		    *(data->matrix + i + data->row * g_width) *
		    *(data->matrix_two + data->position + i * g_width);
		i++;

	}
	*(data->affected_matrix + data->position + data->row * g_width) =
	    product;
	free(info);
	return NULL;
}				/* -----  end of function matrix_mul_job  ----- */

/**
 * Returns new matrix that is the result of
 * multiplying the two matrices together.
 */
float *matrix_mul(const float *matrix_a, const float *matrix_b)
{

	float *result = new_matrix();

	/*
	   TODO

	   1 2   1 0    1 2
	   3 4 x 0 1 => 3 4

	   1 2   5 6    19 22
	   3 4 x 7 8 => 43 50
	 */

	ssize_t thread = 0;
	pthread_t thread_ids[g_nthreads];
	ssize_t point = 0;
	ssize_t row = 0;
	while (row < g_height) {
		point = 0;
		while (point < g_width) {
			thread = 0;
			while (thread < g_nthreads) {
				Job_Info *data = malloc(sizeof(Job_Info));
				data->row = row;
				data->position = point;
				data->matrix = matrix_a;
				data->matrix_two = matrix_b;
				data->affected_matrix = result;
				pthread_create(thread_ids + thread, NULL,
					       matrix_mul_job, data);
				thread++;
			}
			thread = 0;
			while (thread < g_nthreads) {
				pthread_join(*(thread_ids + thread), NULL);
				thread++;
			}
			point++;
		}
		row++;
	}
	return result;
}

/**
 * Returns new matrix that is the result of
 * powering the given matrix to the exponent.
 */
float *matrix_pow(const float *matrix, int exponent)
{

	/*
	   TODO

	   1 2        1 0
	   3 4 ^ 0 => 0 1

	   1 2        1 2
	   3 4 ^ 1 => 3 4

	   1 2        199 290
	   3 4 ^ 4 => 435 634
	 */

	float *result;
	float *temp;
	if (exponent != 0) {
		result = cloned(matrix);
		int i = 1;
		while (i < exponent) {
			temp = matrix_mul(result, matrix);
			free(result);
			result = temp;
			i++;
		}
	} else {
		result = identity_matrix();
	}
	return result;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  top_left_corner
 *  Description:  
 * =====================================================================================
 */

void top_left_corner(Job_Info * data)
{
	float total = 0;
	total = total + *(data->matrix) * (*data->matrix_two +
					   *(data->matrix_two + 1) +
					   *(data->matrix_two + 3) +
					   *(data->matrix_two + 4));
	total =
	    total + *(data->matrix + 1) * (*(data->matrix_two + 2) +
					   *(data->matrix_two + 5));
	total =
	    total + *(data->matrix + g_width) * (*(data->matrix_two + 6) +
						 *(data->matrix_two + 7));
	total = total + *(data->matrix + g_width + 1) * *(data->matrix_two + 8);

	*data->affected_matrix = total;
}				/* -----  end of function top_left_corner  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  top_right_corner
 *  Description:  
 * =====================================================================================
 */

void top_right_corner(Job_Info * data)
{
	float total = 0;
	total =
	    total + *(data->matrix + g_width - 1) * (*(data->matrix_two + 1) +
						     *(data->matrix_two + 5) +
						     *(data->matrix_two + 2) +
						     *(data->matrix_two + 4));
	total =
	    total + *(data->matrix + g_width - 2) * (*(data->matrix_two) +
						     *(data->matrix_two + 3));
	total =
	    total + *(data->matrix + 2 * g_width) * (*(data->matrix_two + 8) +
						     *(data->matrix_two + 7));
	total =
	    total + *(data->matrix + 2 * g_width - 1) * *(data->matrix_two + 6);

	*(data->affected_matrix + data->position) = total;
}				/* -----  end of function top_right_corner  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  bottom_left_corner
 *  Description:  
 * =====================================================================================
 */

void bottom_left_corner(Job_Info * data)
{
	float total = 0;
	total = total + *(data->matrix + g_width * (g_height - 1)) *
	    (*(data->matrix_two + 3) + *(data->matrix_two + 4) +
	     *(data->matrix_two + 6) + *(data->matrix_two + 7));
	total = total + *(data->matrix + g_width * (g_height - 2))
	    * (*(data->matrix_two) + *(data->matrix_two + 1));
	total = total + *(data->matrix + g_width * (g_height - 1) + 1) *
	    (*(data->matrix_two + 5) + *(data->matrix_two + 8));
	total = total + *(data->matrix + g_width + 1) * *(data->matrix_two + 2);

	*(data->affected_matrix + data->position) = total;
}				/* -----  end of function bottom_left_corner  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  bottom_right_corner
 *  Description:  
 * =====================================================================================
 */

void bottom_right_corner(Job_Info * data)
{
	float total = 0;
	total = total + *(data->matrix + g_elements - 1) *
	    (*(data->matrix_two + 4) + *(data->matrix_two + 5) +
	     *(data->matrix_two + 7) + *(data->matrix_two + 8));
	total = total + *(data->matrix + g_elements - 1 - g_width) *
	    (*(data->matrix_two + 1) + *(data->matrix_two + 2));
	total = total + *(data->matrix + g_elements - 2) *
	    (*(data->matrix_two + 3) + *(data->matrix_two + 6));
	total = total + *(data->matrix + g_elements - 2 - g_width) *
	    *(data->matrix_two);

	*(data->affected_matrix + data->position) = total;
}				/* -----  end of function bottom_right_corner  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  top_side
 *  Description:  
 * =====================================================================================
 */

void top_side(Job_Info * data)
{
	float total = 0;
	total =
	    total + *(data->matrix - 1 +
		      data->position) * (*(data->matrix_two) +
					 *(data->matrix_two + 3));
	total =
	    total + *(data->matrix +
		      data->position) * (*(data->matrix_two + 1) +
					 *(data->matrix_two + 4));
	total =
	    total + *(data->matrix + 1 +
		      data->position) * (*(data->matrix_two + 2) +
					 *(data->matrix_two + 5));
	total =
	    total + *(data->matrix - 1 + g_width +
		      data->position) * *(data->matrix_two + 6);
	total =
	    total + *(data->matrix + g_width +
		      data->position) * *(data->matrix_two + 7);
	total =
	    total + *(data->matrix + 1 + g_width +
		      data->position) * *(data->matrix_two + 8);
	*(data->affected_matrix + data->position) = total;
}				/* -----  end of function top_side  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  left_side
 *  Description:  
 * =====================================================================================
 */

void left_side(Job_Info * data)
{
	float total = 0;
	total =
	    total + *(data->matrix - g_width +
		      data->position) * (*(data->matrix_two) +
					 *(data->matrix_two + 1));
	total =
	    total + *(data->matrix +
		      data->position) * (*(data->matrix_two + 3) +
					 *(data->matrix_two + 4));
	total =
	    total + *(data->matrix + g_width +
		      data->position) * (*(data->matrix_two + 6) +
					 *(data->matrix_two + 7));
	total =
	    total + *(data->matrix + 1 - g_width +
		      data->position) * *(data->matrix_two + 2);
	total =
	    total + *(data->matrix + 1 + data->position) * *(data->matrix_two +
							     5);
	total =
	    total + *(data->matrix + 1 + g_width +
		      data->position) * *(data->matrix_two + 8);
	*(data->affected_matrix + data->position) = total;
}				/* -----  end of function left_side  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  right_side
 *  Description:  
 * =====================================================================================
 */

void right_side(Job_Info * data)
{
	float total = 0;
	total =
	    total + *(data->matrix - g_width +
		      data->position) * (*(data->matrix_two + 1) +
					 *(data->matrix_two + 2));
	total =
	    total + *(data->matrix +
		      data->position) * (*(data->matrix_two + 4) +
					 *(data->matrix_two + 5));
	total =
	    total + *(data->matrix + g_width +
		      data->position) * (*(data->matrix_two + 7) +
					 *(data->matrix_two + 8));
	total =
	    total + *(data->matrix - 1 - g_width +
		      data->position) * *(data->matrix_two);
	total =
	    total + *(data->matrix - 1 + data->position) * *(data->matrix_two +
							     3);
	total =
	    total + *(data->matrix - 1 + g_width +
		      data->position) * *(data->matrix_two + 6);
	*(data->affected_matrix + data->position) = total;
}				/* -----  end of function right_side  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  bottom_side
 *  Description:  
 * =====================================================================================
 */

void bottom_side(Job_Info * data)
{
	float total = 0;
	total =
	    total + *(data->matrix - 1 +
		      data->position) * (*(data->matrix_two + 3) +
					 *(data->matrix_two + 6));
	total =
	    total + *(data->matrix +
		      data->position) * (*(data->matrix_two + 4) +
					 *(data->matrix_two + 7));
	total =
	    total + *(data->matrix + 1 +
		      data->position) * (*(data->matrix_two + 5) +
					 *(data->matrix_two + 8));
	total =
	    total + *(data->matrix - 1 - g_width +
		      data->position) * *(data->matrix_two);
	total =
	    total + *(data->matrix - g_width +
		      data->position) * *(data->matrix_two + 1);
	total =
	    total + *(data->matrix + 1 - g_width +
		      data->position) * *(data->matrix_two + 2);
	*(data->affected_matrix + data->position) = total;
}				/* -----  end of function bottom_side  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  inside
 *  Description:  
 * =====================================================================================
 */

void inside(Job_Info * data)
{
	float total = 0;
	total = total + *(data->matrix + data->position - g_width - 1) *
	    *(data->matrix_two);
	total = total + *(data->matrix + data->position - g_width) *
	    *(data->matrix_two + 1);
	total = total + *(data->matrix + data->position - g_width + 1) *
	    *(data->matrix_two + 2);
	total = total + *(data->matrix + data->position - 1) *
	    *(data->matrix_two + 3);
	total = total + *(data->matrix + data->position) *
	    *(data->matrix_two + 4);
	total = total + *(data->matrix + data->position + 1) *
	    *(data->matrix_two + 5);
	total = total + *(data->matrix + data->position + g_width - 1) *
	    *(data->matrix_two + 6);
	total = total + *(data->matrix + data->position + g_width) *
	    *(data->matrix_two + 7);
	total = total + *(data->matrix + data->position + g_width + 1) *
	    *(data->matrix_two + 8);

	*(data->affected_matrix + data->position) = total;

}				/* -----  end of function inside  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  matrix_conv_job
 *  Description:  
 * =====================================================================================
 */

void *matrix_conv_job(void *info)
{
	Job_Info *data = (Job_Info *) info;
	if (data->position == 0) {
		top_left_corner(data);
	} else if (data->position == g_width - 1) {
		top_right_corner(data);
	} else if (data->position == g_width * (g_height - 1)) {
		bottom_left_corner(data);
	} else if (data->position == g_elements - 1) {
		bottom_right_corner(data);
	} else if (data->position < g_width) {
		top_side(data);
	} else if (data->position % g_width == 0) {
		left_side(data);
	} else if ((data->position + 1) % g_width == 0) {
		right_side(data);
	} else if (data->position > g_width * (g_height - 1)) {
		bottom_side(data);
	} else {
		inside(data);
	}
	free(info);
	return NULL;
}				/* -----  end of function matrix_conv_job  ----- */

/**
 * Returns new matrix that is the result of
 * convolving given matrix with a 3x3 kernel matrix.
 */
float *matrix_conv(const float *matrix, const float *kernel)
{

	float *result = new_matrix();

	/*
	   TODO

	   Convolution is the process in which the values of a matrix are
	   computed according to the weighted sum of each value and it's
	   neighbours, where the weights are given by the kernel matrix.
	 */

	pthread_t thread_ids[g_nthreads];
	ssize_t thread = 0;
	ssize_t position = 0;
	while (position < g_elements) {
		while (thread < g_nthreads) {
			Job_Info *data = malloc(sizeof(Job_Info));
			data->position = position;
			data->matrix = matrix;
			data->matrix_two = kernel;
			data->affected_matrix = result;
			pthread_create(thread_ids + thread, NULL,
				       matrix_conv_job, data);
			thread++;
			position++;
		}
		thread = 0;
		while (thread < g_nthreads) {
			pthread_join(*(thread_ids + thread), NULL);
			thread++;
		}

	}

	return result;
}

////////////////////////////////
///       COMPUTATIONS       ///
////////////////////////////////

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  get_sum_job
 *  Description:  
 * =====================================================================================
 */

void *get_sum_job(void *info)
{
	Job_Info *data = (Job_Info *) info;
	ssize_t x = 0;
	float total = 0;
	while (x < g_width) {
		total = *(data->matrix + x + g_width * data->row) + total;
		x++;
	}
	pthread_mutex_lock(&lock);
	*data->variable.value = *data->variable.value + total;
	pthread_mutex_unlock(&lock);
	free(info);
	return NULL;
}				/* -----  end of function get_sum_job  ----- */

/**
 * Returns the sum of all elements.
 */
float get_sum(const float *matrix)
{

	/*
	   TODO

	   2 1
	   1 2 => 6

	   1 1
	   1 1 => 4
	 */

	float total = 0;
	pthread_t thread_ids[g_nthreads];
	ssize_t row = 0;
	while (g_height - row >= g_nthreads) {
		ssize_t thread = 0;
		while (thread < g_nthreads) {
			Job_Info *data = malloc(sizeof(Job_Info));
			data->row = row;
			data->matrix = matrix;
			data->variable.value = &total;
			pthread_create(thread_ids + thread, NULL,
				       get_sum_job, data);
			thread++;
			row++;
		}
		thread = 0;
		while (thread < g_nthreads) {
			pthread_join(*(thread_ids + thread), NULL);
			thread++;
		}
	}
	if (row < g_height - 1) {
		ssize_t thread = 0;
		while (row < g_height - 1) {
			Job_Info *data = malloc(sizeof(Job_Info));
			data->row = row;
			data->matrix = matrix;
			data->variable.value = &total;
			pthread_create(thread_ids + thread, NULL,
				       get_sum_job, data);
			thread++;
			row++;
		}
		while (thread >= 0) {
			pthread_join(*(thread_ids + thread), NULL);
			thread--;
		}
	}

	return total;
}

/**
 * Returns the trace of the matrix.
 */
float get_trace(const float *matrix)
{

	/*
	   TODO

	   1 0
	   0 1 => 2

	   2 1
	   1 2 => 4
	 */

	float total = 0;
	ssize_t x = 0;
	while (x < g_width) {
		total = total + *(matrix + x + x * g_width);
		x++;
	}

	return total;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  get_minimum_job
 *  Description:  
 * =====================================================================================
 */

void *get_minimum_job(void *info)
{
	Job_Info *data = (Job_Info *) info;
	float minimum = *(data->matrix + g_width * data->row);
	ssize_t x = 0;
	while (x < g_width) {
		if (*(data->matrix + g_width * data->row + x) < minimum) {
			minimum = *(data->matrix + g_width * data->row + x);
		}
		x++;
	}
	pthread_mutex_lock(&lock);
	if (minimum < *data->variable.value) {
		*data->variable.value = minimum;
	}
	pthread_mutex_unlock(&lock);
	free(info);
	return NULL;
}				/* -----  end of function get_minimum_job  ----- */

/**
 * Returns the smallest value in the matrix.
 */
float get_minimum(const float *matrix)
{

	/*
	   TODO

	   1 2
	   3 4 => 1

	   4 3
	   2 1 => 1
	 */

	float minimum = *matrix;
	pthread_t thread_ids[g_nthreads];
	ssize_t row = 0;
	while (g_height - row >= g_nthreads) {
		ssize_t thread = 0;
		while (thread < g_nthreads) {
			Job_Info *data = malloc(sizeof(Job_Info));
			data->row = row;
			data->matrix = matrix;
			data->variable.value = &minimum;
			pthread_create(thread_ids + thread, NULL,
				       get_minimum_job, data);
			thread++;
			row++;
		}
		thread = 0;
		while (thread < g_nthreads) {
			pthread_join(*(thread_ids + thread), NULL);
			thread++;
		}
	}
	if (row < g_height - 1) {
		ssize_t thread = 0;
		while (row < g_height - 1) {
			Job_Info *data = malloc(sizeof(Job_Info));
			data->row = row;
			data->matrix = matrix;
			data->variable.value = &minimum;
			pthread_create(thread_ids + thread, NULL,
				       get_minimum_job, data);
			thread++;
			row++;
		}
		while (thread >= 0) {
			pthread_join(*(thread_ids + thread), NULL);
			thread--;
		}
	}

	return minimum;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  get_maximum_job
 *  Description:  
 * =====================================================================================
 */

void *get_maximum_job(void *info)
{
	Job_Info *data = (Job_Info *) info;
	float maximum = *(data->matrix + g_width * data->row);
	ssize_t x = 0;
	while (x < g_width) {
		if (*(data->matrix + g_width * data->row + x) > maximum) {
			maximum = *(data->matrix + g_width * data->row + x);
		}
		x++;
	}
	pthread_mutex_lock(&lock);
	if (maximum > *data->variable.value) {
		*data->variable.value = maximum;
	}
	pthread_mutex_unlock(&lock);
	free(info);
	return NULL;
}				/* -----  end of function get_maximum_job  ----- */

/**
 * Returns the largest value in the matrix.
 */
float get_maximum(const float *matrix)
{

	/*
	   TODO

	   1 2
	   3 4 => 4

	   4 3
	   2 1 => 4
	 */

	float maximum = *matrix;
	pthread_t thread_ids[g_nthreads];
	ssize_t row = 0;
	while (g_height - row >= g_nthreads) {
		ssize_t thread = 0;
		while (thread < g_nthreads) {
			Job_Info *data = malloc(sizeof(Job_Info));
			data->row = row;
			data->matrix = matrix;
			data->variable.value = &maximum;
			pthread_create(thread_ids + thread, NULL,
				       get_maximum_job, data);
			thread++;
			row++;
		}
		thread = 0;
		while (thread < g_nthreads) {
			pthread_join(*(thread_ids + thread), NULL);
			thread++;
		}
	}
	if (row < g_height - 1) {
		ssize_t thread = 0;
		while (row < g_height - 1) {
			Job_Info *data = malloc(sizeof(Job_Info));
			data->row = row;
			data->matrix = matrix;
			data->variable.value = &maximum;
			pthread_create(thread_ids + thread, NULL,
				       get_maximum_job, data);
			thread++;
			row++;
		}
		while (thread >= 0) {
			pthread_join(*(thread_ids + thread), NULL);
			thread--;
		}
	}

	return maximum;

}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  calc_determinant_recursive
 *  Description:  
 * =====================================================================================
 */
float calc_determinant_recursive(const float *matrix, ssize_t size)
{
	float determinant = 0;
	float *new_matrix = NULL;
	if (size == 1) {
		determinant = *matrix;
	} else if (size == 2) {
		determinant =
		    *matrix * *(matrix + 3) - *(matrix + 1) * *(matrix + 2);
	} else {
		ssize_t i = 0;
		while (i < size) {
			new_matrix = malloc((size - 1) * (size - 1) * sizeof(float));
			ssize_t j = 0;
			ssize_t k = size;
			while (j < (size - 1) * (size - 1)) {
				if ((k - j) % size == 0) {
					k++;
				}
				*(new_matrix + j) = *(matrix + k);
				k++;
				j++;
			}
			if (i % 2 == 0) {
				determinant = determinant + *(matrix + i) *
					calc_determinant_recursive(new_matrix, size - 1);
			} else {
				determinant = determinant - *(matrix + i) *
					calc_determinant_recursive(new_matrix, size - 1);
			}
			free(new_matrix);
			i++;
		}
	}
	return determinant;
}				/* -----  end of function calc_determinant_recursive  ----- */

/**
 * Returns the determinant of the matrix.
 */
float get_determinant(const float *matrix)
{

	/*
	   TODO

	   1 0
	   0 1 => 1

	   1 2
	   3 4 => -2

	   8 0 2
	   0 4 0
	   2 0 8 => 240
	 */

	return calc_determinant_recursive(matrix, g_width);
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  get_fequency_job
 *  Description:  
 * =====================================================================================
 */

void *get_frequency_job(void *info)
{
	Job_Info *data = (Job_Info *) info;
	float total = 0;
	ssize_t x = 0;
	while (x < g_width) {
		if (*(data->matrix + g_width * data->row + x) == data->value) {
			total++;
		}
		x++;
	}
	pthread_mutex_lock(&lock);
	*data->variable.frequency = *data->variable.frequency + total;
	pthread_mutex_unlock(&lock);
	free(info);
	return NULL;;
}				/* -----  end of function get_fequency_job  ----- */

/**
 * Returns the frequency of the given value in the matrix.
 */
ssize_t get_frequency(const float *matrix, float value)
{

	/*
	   TODO

	   1 1
	   1 1 :: 1 => 4

	   1 0
	   0 1 :: 2 => 0
	 */

	ssize_t frequency = 0;
	pthread_t thread_ids[g_nthreads];
	ssize_t row = 0;
	while (g_height - row >= g_nthreads) {
		ssize_t thread = 0;
		while (thread < g_nthreads) {
			Job_Info *data = malloc(sizeof(Job_Info));
			data->row = row;
			data->matrix = matrix;
			data->value = value;
			data->variable.frequency = &frequency;
			pthread_create(thread_ids + thread, NULL,
				       get_frequency_job, data);
			thread++;
			row++;
		}
		thread = 0;
		while (thread < g_nthreads) {
			pthread_join(*(thread_ids + thread), NULL);
			thread++;
		}
	}
	if (row < g_height - 1) {
		ssize_t thread = 0;
		while (row < g_height - 1) {
			Job_Info *data = malloc(sizeof(Job_Info));
			data->row = row;
			data->matrix = matrix;
			data->variable.frequency = &frequency;
			pthread_create(thread_ids + thread, NULL,
				       get_frequency_job, data);
			thread++;
			row++;
		}
		while (thread >= 0) {
			pthread_join(*(thread_ids + thread), NULL);
			thread--;
		}
	}

	return frequency;
}
