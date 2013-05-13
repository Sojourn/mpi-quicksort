#ifndef UTIL_H
#define UTIL_H

#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <stdint.h>
#include <mpi.h>

// Determine how many numbers each processor gets
std::vector<size_t> allocate_numbers(
	size_t number_count,
	size_t comm_size);

// Return the maximum amount of numbers any processor has.
size_t max_allocation(const std::vector<size_t> &number_alloc);

// Pad a buffer for scatter.
void pad_buffer(
	const std::vector<size_t> &number_alloc,
	const std::vector<int32_t> &in_buffer,
	std::vector<int32_t> &out_buffer);

// Unpad a buffer after a gather operation.
void unpad_buffer(
	const std::vector<size_t> &number_alloc,
	const std::vector<int32_t> &in_buffer,
	std::vector<int32_t> &out_buffer);

// Pad a local buffer for a gather operation.
void pad_single_buffer(
	size_t index,
	const std::vector<size_t> &number_alloc,
	const std::vector<int32_t> &in_buffer,
	std::vector<int32_t> &out_buffer);

// Unpad a local buffer after a scatter operation.
void unpad_single_buffer(
	size_t index,
	const std::vector<size_t> &number_alloc,
	const std::vector<int32_t> &in_buffer,
	std::vector<int32_t> &out_buffer);

// Dump a message if this is the root processor to the console atomically.
inline void root_checkpoint(const char *msg)
{
	int32_t rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	if(rank == 0)
	{
		std::stringstream ss;
		ss << "Checkpoint: " << msg << std::endl;
		std::cout << ss.str();
		MPI_Barrier(MPI_COMM_WORLD);
	}
}

// Dump the contents of a vector to the console atomically.
template<class T>
inline void dump_vector(const char *msg, const std::vector<T> &v)
{
	int32_t rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	std::stringstream ss;
	ss << "[" << rank << "](" << msg << "): ";
	for(size_t i = 0; i < v.size(); i++)
	{
		ss << v[i] << ", ";
	}
	ss << std::endl;
	std::cout << ss.str();
}

#endif // UTIL_H
