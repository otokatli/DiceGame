/******
*
* Written by Dan Morris
* dmorris@cs.stanford.edu
* http://cs.stanford.edu/~dmorris
*
* You can do anything you want with this file as long as this header
* stays on it and I am credited when it's appropriate.
*
******/

#ifndef _BLOCK_LINKED_LIST_H_
#define _BLOCK_LINKED_LIST_H_

// This is a custom data structure that I used for buffering data on
// its way out to disk.  It is not a random access data structure;
// it just buffers up objects of type T and flushes them out to disk.
//
// If you're using flusher and writer threads, the general protocol
// is to only allow the writer to call the push_back function, and
// let the reader use the safe_flush function to do its flushing.
// Then at some point when the writer is done forever, the reader can
// call flush() to flush the remaining blocks that were currently in use
// by the writer.  
//
// This is a lock-free solution to the producer-consumer problem with the
// constraint that immediate consumption is not important.
//
// This addresses a common access pattern for applications that have a time-
// critical thread (e.g. a haptics thread) that generates data that 
// needs to be logged to disk.  This approach avoids any disk access or 
// mutex access on the high-priority thread.  I've used this data 
// structure for haptics (where a high-priority thread generates forces)
// and neurophysiology/psychophysics (where a high-priority thread
// controls an experiment).
//
// Note that in this mode, the "flusher" thread should _not_ also put
// data into the array; i.e. the writer thread "owns" the push_back
// function.


// The node type for the linked list
template <class T, size_t chunk_size> class block_linked_list_node {
public:

	// An array of T's
	T* data;
	block_linked_list_node<T, chunk_size>* next;
	block_linked_list_node() {
		data = new T[chunk_size];
		next = 0;
	}
	~block_linked_list_node() {
		delete[] data;
	}
};

// The data structure itself
template <class T, size_t chunk_size> class block_linked_list {
public:

	// The head of the list
	block_linked_list_node<T, chunk_size>* head;

	// The number of T's in the list
	size_t total_count;

	// A pointer to the current (tail) block
	block_linked_list_node<T, chunk_size>* current_node;

	// The number of T's in the current (tail) block
	size_t current_count;

	block_linked_list() {
		current_node = head = new block_linked_list_node<T, chunk_size>;
		current_count = total_count = 0;
	};

	// Note that since the list itself has no concept of the file
	// handle, no flushing is done at delete-time
	~block_linked_list() {
		kill();
	}

	// Deletes everything in the array and leaves an empty (but valid) head node
	void clear() {
		kill();
		current_node = head = new block_linked_list_node<T, chunk_size>;
		current_count = total_count = 0;
	}

	// Adds an element to the end of the array.  Returns the total
	// number of elements in the array.
	int push_back(const T& in) {

		total_count++;

		// If we just filled up a chunk
		if (current_count == chunk_size) {
			block_linked_list_node<T, chunk_size>* tmp = new block_linked_list_node<T, chunk_size>;
			current_count = 0;
			current_node->next = tmp;
			current_node = tmp;
		}

		current_node->data[current_count] = in;
		current_count++;

		return total_count;
	}

	// Flushes the whole array to the specified file, and clears the 
	// contents of the array (unless the optional second parameter is
	// explicitly set to 0).
	//
	// Returns 0 for success or an ferror() code for failure.
	int flush(FILE* f, int clear_array = 1) {

		block_linked_list_node<T, chunk_size>* cur = head;

		int error = 0;

		while (cur != 0) {

			// Dump the blocks out to disk
			int count = (cur == current_node ? current_count : chunk_size);
			int result = fwrite(cur->data, sizeof(T), count, f);

			// _cprintf("Writing %d T's\n",count);

			if (result != count) {
				error = ferror(f);
				break;
			}

			cur = cur->next;
		}

		if (error) return error;

		if (clear_array) clear();

		fflush(f);

		return 0;
	} // flush(FILE* f)

	// Flushes the whole array _except_ for the current (tail) node.
	// Deletes all the blocks it encounters if clear_array is 1.
	// Note that this may leave a list with two nodes if a new node
	// was added _during_ the operation.
	int safe_flush(FILE* f, int clear_array = 1) {

		block_linked_list_node<T, chunk_size>* cur = head;
		block_linked_list_node<T, chunk_size>* initial_tail = current_node;

		int error = 0;

		while (cur != initial_tail) {

			// Dump the blocks out to disk

			// Always a whole block, since we never touch the current node
			int count = chunk_size;
			int result = fwrite(cur->data, sizeof(T), count, f);
			total_count -= chunk_size;

			if (result != count) {
				error = ferror(f);
				break;
			}

			cur = cur->next;
		}

		if (error) return error;

		if (clear_array) delete_until(initial_tail);

		fflush(f);

		return 0;

	}

	// Used for randomly accessing the array.  This is O(N); this
	// data structure is not well-suited for random access.  Returns
	// 0 if index is invalid.
	T* operator[] (int index) {

		if (index >= total_count || index < 0) {
			return 0;
		}

		block_linked_list_node<T, chunk_size>* cur = head;

		// Which block number is it in
		int block_num = index % total_count;

		// Advance that many blocks forward
		for (int i = 0; i<block_num; i++) {
			cur = cur->next;
		}

		int offset = index % chunk_size;

		return cur->data[offset];
	}

	int size() { return total_count; }

private:

	// Deletes everything in the array, does not leave a valid head node
	void kill() {

		// Delete all the nodes; they will delete their own data
		block_linked_list_node<T, chunk_size>* cur = head;
		while ((cur != 0) && (cur->next != 0)) {
			block_linked_list_node<T, chunk_size>* tmp = cur->next;
			delete cur;
			cur = tmp;
		}

		current_node = 0;
		current_count = total_count = 0;
	}

	// Deletes all nodes up to the specified node, which becomes the head node.
	// If that node is not found, it deletes the whole list and creates a new
	// head node
	void delete_until(block_linked_list_node<T, chunk_size>* stop_point) {

		block_linked_list_node<T, chunk_size>* cur = head;

		while ((cur != stop_point) && (cur != 0)) {

			block_linked_list_node<T, chunk_size>* tmp = cur->next;
			delete cur;
			cur = tmp;

		}

		// If we found a valid stop point, he becomes the head and we're done
		if ((cur == stop_point) && (stop_point != 0)) {
			head = cur;
			return;
		}

		// We must not have found a valid stop point... reset the list...
		clear();
	}

};



#endif