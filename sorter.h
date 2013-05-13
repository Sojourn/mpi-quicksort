#ifndef SORTER_H
#define SORTER_H

#include <mpi.h>
#include <sstream>
#include <vector>
#include <stdint.h>

// A per-processor sorter instance.
class Sorter
{
public:
	// Perform parallel quick-sort given a local number set, and the
	// distribution of numbers across MPI nodes.
	static void quick_sort(
		std::vector<int32_t> &local_numbers,
		const std::vector<size_t> &number_alloc);

private:

	// Colors are used to split the communicator.
	enum Color_t
	{
		RightColor = 0,
		LeftColor  = 1	
	};

	// Ctor taking the local numbers, the number distrobution, and the communicator.
	Sorter(
		std::vector<int32_t> &local_numbers,
		const std::vector<size_t> &number_alloc,
		MPI_Comm comm);

	// Find and communicate the next pivot value.
	int32_t getPivot() const;

	// Given a pivot, partition the local numbers into left and right sublists.	
	void partition(
		int32_t pivot,
		std::vector<int32_t> &left,
		std::vector<int32_t> &right);

	// Re-distribute the numbers between the processors.
	Color_t disperse(
		const std::vector<int32_t> &left,
		const std::vector<int32_t> &right);

	// Split into a new communicator of the given color. Also finds the size, and
	// rank within the new communicator.
	void split(Color_t color);
	
private:
	std::vector<int32_t> &_local_numbers;
	std::vector<size_t> _number_alloc;
	MPI_Comm _comm;
	int32_t _rank;
	int32_t _comm_size;
};

#endif // SORTER_H
