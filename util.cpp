#include "util.h"
#include <algorithm>

size_t max_allocation(const std::vector<size_t> &number_alloc)
{
	size_t max_alloc = number_alloc[0];
	for(size_t i = 0; i < number_alloc.size(); i++)
	{
		max_alloc = std::max(number_alloc[0], max_alloc);	
	}

	return max_alloc;
}

std::vector<size_t> allocate_numbers(
	size_t number_count,
	size_t comm_size)
{
	size_t div = number_count / comm_size;
	size_t mod = number_count % comm_size;
	std::vector<size_t> result;

	for(size_t i = 0; i < comm_size; i++)
	{
		size_t local_count = div;
		
		if(mod > 0)
		{
			local_count++;
			mod--;
		}

		result.push_back(local_count);	
	}

	return result;
}

void pad_buffer(
	const std::vector<size_t> &number_alloc,
	const std::vector<int32_t> &in_buffer,
	std::vector<int32_t> &out_buffer)
{
	out_buffer.clear();
	for(size_t p = 0, index = 0; p < number_alloc.size(); p++)
	{
		for(size_t i = 0; i < number_alloc[p]; i++)
		{
			out_buffer.push_back(in_buffer[index]);
			index++;
		}
		for(size_t i = 0; i < (max_allocation(number_alloc) - number_alloc[p]); i++)
		{
			out_buffer.push_back(0);
		}
	}
}

void unpad_buffer(
	const std::vector<size_t> &number_alloc,
	const std::vector<int32_t> &in_buffer,
	std::vector<int32_t> &out_buffer)
{
	out_buffer.clear();
	for(size_t p = 0, index = 0; p < number_alloc.size(); p++)
	{
		for(size_t i = 0; i < number_alloc[p]; i++)
		{
			out_buffer.push_back(in_buffer[index]);
			index++;
		}

		index += max_allocation(number_alloc) - number_alloc[p]; 
	}
}

void pad_single_buffer(
	size_t index,
	const std::vector<size_t> &number_alloc,
	const std::vector<int32_t> &in_buffer,
	std::vector<int32_t> &out_buffer)
{
	out_buffer.clear();
	for(size_t i = 0; i < number_alloc[index]; i++)
	{
		out_buffer.push_back(in_buffer[i]);
	}
	for(size_t i = 0; i < (max_allocation(number_alloc) - number_alloc[index]); i++)
	{
		out_buffer.push_back(0);
	}
}

void unpad_single_buffer(
	size_t index,
	const std::vector<size_t> &number_alloc,
	const std::vector<int32_t> &in_buffer,
	std::vector<int32_t> &out_buffer)
{
	out_buffer.clear();
	for(size_t i = 0; i < number_alloc[index]; i++)
	{
		out_buffer.push_back(in_buffer[i]);
	}
}
