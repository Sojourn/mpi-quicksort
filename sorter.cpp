#include "sorter.h"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <assert.h>
#include "util.h"

Sorter::Sorter(
	std::vector<int32_t> &local_numbers,
	const std::vector<size_t> &number_alloc,
	MPI_Comm comm):
	_local_numbers(local_numbers),
	_number_alloc(number_alloc),
	_comm(comm)
{
	MPI_Comm_rank(_comm, &_rank);
	MPI_Comm_size(_comm, &_comm_size);
}

void Sorter::quick_sort(
	std::vector<int32_t> &local_numbers,
	const std::vector<size_t> &number_alloc)
{
	// All processors need to use the name seed
	// or pivot selection will wait forever.
	srand(0xC0FFEE);
	
	Sorter sorter(local_numbers, number_alloc, MPI_COMM_WORLD);

	// Do parallel partition until this processor is alone
	while(sorter._comm_size > 1)
	{
		int32_t pivot;
		Color_t color;
		std::vector<int32_t> left;
		std::vector<int32_t> right;

		pivot = sorter.getPivot();
		sorter.partition(pivot, left, right);
		color = sorter.disperse(left, right);
		sorter.split(color);
	}

	// The local numbers can be sorted serially at this point
	std::sort(local_numbers.begin(), local_numbers.end());
}

int32_t Sorter::getPivot() const
{
	int32_t pivot;
	int32_t pivot_owner_rank;

	// Find which processor has the next pivot number
	size_t k = rand() % _comm_size;
	for(size_t i = 0; i < _comm_size; i++)
	{
		if(k < _number_alloc[i])
		{
			pivot_owner_rank = i;
			pivot = (pivot_owner_rank == _rank) ? _local_numbers[k] : 0;
		}
		else
		{
			k -= _number_alloc[i];
		}
	}

	// Broadcast the pivot to the other processors
	MPI_Bcast(
		(void*) &pivot,
		1,
		MPI_INT,
		pivot_owner_rank,
		_comm);

	return pivot;
}

Sorter::Color_t Sorter::disperse(const std::vector<int32_t> &left, const std::vector<int32_t> &right)
{
	// Communicate the size of the local partitions
	std::vector<size_t> global_partition(_comm_size * 2);
	
	size_t local_partition[2];
	local_partition[0] = left.size();
	local_partition[1] = right.size();
	
	MPI_Allgather(
		(void*) local_partition,
		2,
		MPI_INT,
		(void*) &global_partition[0],
		2,
		MPI_INT,
		_comm);

	// Calculate the number of processors in each partition
	size_t left_count = 0;
	size_t right_count = 0;
	size_t left_max_count = 0;
	size_t right_max_count = 0;
	for(size_t i = 0; i < global_partition.size(); i += 2)
	{
		left_count += global_partition[i];
		right_count += global_partition[i + 1];
		left_max_count = std::max(left_max_count, global_partition[i]);
		right_max_count = std::max(right_max_count, global_partition[i + 1]);
	}

	size_t left_processors = (_comm_size * left_count) / (left_count + right_count);
	size_t right_processors = _comm_size - left_processors;
	if(left_processors == 0)
	{
		left_processors++;
		right_processors--;
	}
	if(right_processors == 0)
	{
		right_processors++;
		left_processors--;
	}

	// Calculate the local, and maximum transfer distrobutions
	std::vector<size_t> left_max_alloc = allocate_numbers(
		left_max_count,
		left_processors);
	std::vector<size_t> left_alloc = allocate_numbers(
		left.size(),
		left_processors);

	std::vector<size_t> right_max_alloc = allocate_numbers(
		right_max_count,
		right_processors);
	std::vector<size_t> right_alloc = allocate_numbers(
		right.size(),
		right_processors);

	// All transmissions to other processors are padded to this size
	size_t max_transmission = std::max(
		max_allocation(left_max_alloc),
		max_allocation(right_max_alloc));

	std::vector<int32_t> send_buffer(max_transmission * _comm_size);
	std::vector<int32_t> recv_buffer(max_transmission * _comm_size);

	// Pad the buffers 
	size_t send_index = 0;
	size_t left_index = 0;
	size_t right_index = 0;
	for(size_t p = 0; p < left_processors; p++)
	{
		for(size_t i = 0; i < left_alloc[p]; i++)
		{
			send_buffer[send_index++] = left[left_index++];
		}
		for(size_t i = 0; i < (max_transmission - left_alloc[p]); i++)
		{
			send_buffer[send_index++] = 0;	
		}
	}
	for(size_t p = 0; p < right_processors; p++)
	{
		for(size_t i = 0; i < right_alloc[p]; i++)
		{
			send_buffer[send_index++] = right[right_index++];
		}
		for(size_t i = 0; i < (max_transmission - right_alloc[p]); i++)
		{
			send_buffer[send_index++] = -1;
		}
	}

	// Re-distribute left and right partitions to the corresponding processors
	MPI_Alltoall(
		(void*) &send_buffer[0],
		max_transmission,
		MPI_INT,
		(void*) &recv_buffer[0],
		max_transmission,
		MPI_INT,
		_comm);

	// Calculate the color of this node in the new processor partition
	Color_t color;
	if(_rank >= left_processors)
	{
		color = RightColor;
	}
	else
	{
		color = LeftColor;
	}

	// Unpad the buffers received from the other processors
	size_t recv_index = 0;
	_local_numbers.clear();
	for(size_t p = 0; p < _comm_size; p++)
	{
		size_t recv_count;
		if(color == LeftColor)
		{
			std::vector<size_t> recv_alloc = allocate_numbers(
				global_partition[2 * p],
				left_processors);
			recv_count = recv_alloc[_rank];

		}
		else
		{
			std::vector<size_t> recv_alloc = allocate_numbers(
				global_partition[(2 * p) + 1],
				right_processors);
			recv_count = recv_alloc[_rank - left_processors];
		}

		for(size_t i = 0; i < recv_count; i++)
		{
			int32_t number = recv_buffer[recv_index + i];
			_local_numbers.push_back(number);
		}

		recv_index += max_transmission;
	}
	
	return color;
}

void Sorter::partition(int32_t pivot, std::vector<int32_t> &left, std::vector<int32_t> &right)
{
	for(size_t i = 0; i < _local_numbers.size(); i++)
	{
		const int32_t number = _local_numbers[i];
		
		if(number > pivot)
		{
			right.push_back(number);
		}
		else
		{
			left.push_back(number);
		}
	}
}

void Sorter::split(Color_t color)
{
	MPI_Comm new_comm;
	MPI_Comm_split(
		_comm,
		color,
		_rank,
		&new_comm);
	_comm = new_comm;

	MPI_Comm_rank(_comm, &_rank);
	MPI_Comm_size(_comm, &_comm_size);
}
