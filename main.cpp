#include <istream>
#include <ostream>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <stdint.h>
#include <mpi.h>

#include "util.h"
#include "sorter.h"

// Return codes.
enum Status_t
{
	STATUS_SUCCESS      = 0,
	STATUS_BAD_PARAMS   = (1 << 0),
	STATUS_NO_SUCH_FILE = (1 << 1),
	STATUS_BAD_INPUT    = (1 << 1)
};

// Read the input from a file into an output buffer.
Status_t read_input(
	std::ifstream &input,
	std::vector<int32_t> &out_buffer);

// Distribute a part of the input to each of the processors.
Status_t scatter_input(
	int32_t rank,
	const std::vector<size_t> &number_alloc,
	const std::vector<int32_t> &send_buffer,
	std::vector<int32_t> &recv_buffer);

// Gather the output from each of the processors.
Status_t gather_output(
	int32_t rank,
	const std::vector<size_t> &number_alloc,
	const std::vector<int32_t> &send_buffer,
	std::vector<int32_t> &recv_buffer);

// Write the output to console.
Status_t write_output(
	std::vector<int32_t> &numbers);	

int main(int32_t argc, char **argv)
{
	int32_t rank;
	int32_t comm_size;
	size_t number_count;
	std::vector<size_t>  number_alloc;
	std::vector<int32_t> numbers;
	std::vector<int32_t> local_numbers;

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &comm_size);

	// Read the input file if this is the root node
	Status_t status = (argc < 2) ? STATUS_BAD_PARAMS : STATUS_SUCCESS;
	if(status == STATUS_SUCCESS && rank == 0)
	{
		std::ifstream input(argv[1]);
		if(input.is_open())
		{
			status = read_input(input, numbers);
			input.close();
		}
		else
		{
			status = STATUS_NO_SUCH_FILE;
		}
	}

	// Broadcast the total number count
	// This is also used to communicate IO errors
	number_count = (status == STATUS_SUCCESS) ? numbers.size() : 0;	
	MPI_Bcast(&number_count, 1, MPI_INT, 0, MPI_COMM_WORLD);
	status = (number_count > 0) ? STATUS_SUCCESS : STATUS_BAD_INPUT;

	// Calculate how many numbers each processor gets
	number_alloc = allocate_numbers(number_count, comm_size);

	// Distribute the numbers amoung the processors
	if(status == STATUS_SUCCESS)
	{
		status = scatter_input(
			rank,
			number_alloc,
			numbers,
			local_numbers);
	}

	// Sort the numbers with parallel quicksort
	if(status == STATUS_SUCCESS)
	{
		Sorter::quick_sort(local_numbers, number_alloc);
	}

	// Gather the numbers on the root node
	if(status == STATUS_SUCCESS)
	{
		status = gather_output(
			rank,
			number_alloc,
			local_numbers,
			numbers);
	}

	// Output the sorted numbers to stdout
	if(status == STATUS_SUCCESS && rank == 0)
	{
		status = write_output(numbers);
	}

	MPI_Finalize();

	return status;
}

Status_t read_input(std::ifstream &input, std::vector<int32_t> &out_buffer)
{
	size_t number_count;	

	if(!input.good() || !input.is_open())
	{
		return STATUS_BAD_INPUT;
	}

	input >> number_count;
	for(size_t i = 0; i < number_count; i++)
	{
		if(input.good())
		{
			out_buffer.push_back(0);
			input >> out_buffer.back();
		}
		else
		{
			return STATUS_BAD_INPUT;
		}
	}

	return STATUS_SUCCESS;	
}

Status_t scatter_input(
	int32_t rank,
	const std::vector<size_t> &number_alloc,
	const std::vector<int32_t> &send_buffer,
	std::vector<int32_t> &recv_buffer)
{
	std::vector<int32_t> padded_send_buffer;
	std::vector<int32_t> padded_recv_buffer;
	int32_t dummy_buffer[1];
	void* send_buffer_ptr;

	if(rank == 0)
	{
		pad_buffer(number_alloc, send_buffer, padded_send_buffer);
		send_buffer_ptr = (void*) &padded_send_buffer[0];
	}
	else
	{
		send_buffer_ptr = (void*) &dummy_buffer[0];
	}

	size_t padded_send_count = max_allocation(number_alloc);
	padded_recv_buffer.resize(padded_send_count);
	
	MPI_Scatter(
		send_buffer_ptr,
		padded_send_count,
		MPI_INT,
		(void*) &padded_recv_buffer[0],
		padded_send_count,
		MPI_INT,
		0,
		MPI_COMM_WORLD);

	unpad_single_buffer(rank, number_alloc, padded_recv_buffer, recv_buffer);

	return STATUS_SUCCESS;
}

Status_t gather_output(
	int32_t rank,
	const std::vector<size_t> &number_alloc,
	const std::vector<int32_t> &send_buffer,
	std::vector<int32_t> &recv_buffer)
{
	int32_t comm_size;
	size_t local_count;
	size_t max_count;
	std::vector<size_t> global_count;
	std::vector<int32_t> padded_send_buffer;
	std::vector<int32_t> padded_recv_buffer;

	MPI_Comm_size(MPI_COMM_WORLD, &comm_size);
	global_count.resize(comm_size);
	local_count = send_buffer.size();

	// Find out how large each of the processors sorted chunks are	
	MPI_Allgather(
		(void*) &local_count,
		1,
		MPI_INT,
		(void*) &global_count[0],
		1,
		MPI_INT,
		MPI_COMM_WORLD);

	max_count = global_count[0];
	for(size_t i = 0; i < comm_size; i++)
	{
		max_count = std::max(max_count, global_count[i]);
	}
	assert(max_count >= local_count);

	// Add padding to each of the sorted chunks
	padded_send_buffer = send_buffer;
	for(size_t i = 0; i < (max_count - local_count); i++)
	{
		padded_send_buffer.push_back(-1);
	}
	assert(padded_send_buffer.size() == max_count);

	// Transfer the padded, sorted chunks to the root node
	padded_recv_buffer.resize(max_count * comm_size);
	MPI_Gather(
		(void*) &padded_send_buffer[0],
		max_count,
		MPI_INT,
		(void*) &padded_recv_buffer[0],
		max_count,
		MPI_INT,
		0,
		MPI_COMM_WORLD);

	// Build the sorted vector
	recv_buffer.clear();	
	if(rank == 0)
	{
		size_t padded_index = 0;
		for(size_t p = 0; p < comm_size; p++)
		{
			size_t count = global_count[p];
			for(size_t i = 0; i < count; i++)
			{
				int32_t number = padded_recv_buffer[i + padded_index];
				recv_buffer.push_back(number);
			}

			padded_index += max_count;
		}
	}

	return STATUS_SUCCESS;	
}

Status_t write_output(
	std::vector<int32_t> &numbers)
{
	for(size_t i = 0; i < numbers.size(); i++)
	{
		std::cout << numbers[i] << std::endl;
	}

	return STATUS_SUCCESS;
}

