//Hamit Efe Cinar

#include <iostream>
#include <semaphore.h>

class HeapManager {
private:
    struct Node {
        int ID;     // Thread ID or -1 if the chunk is free
        int size;   // Size of the chunk
        int index;  // Starting address of the chunk
        Node* next; // Pointer to the next node in the list

        Node(int id, int sz, int idx) : ID(id), size(sz), index(idx), next(nullptr) {}
    };

    Node* head;
    sem_t heapSemaphore, printSemaphore;

    void printList() {
        Node* current = head;
        while (current != nullptr) {
            std::cout << "[" << current->ID << "][" << current->size << "][" << current->index << "]";
            if (current->next != nullptr) {
                std::cout << "---";
            }
            current = current->next;
        }
        std::cout << std::endl;
        std::cout.flush();
    }

    void clearHeap() {
        Node* current = head;
        while (current != nullptr) {
            Node* next = current->next;
            delete current;
            current = next;
        }
        head = nullptr;
    }

public:
    HeapManager() : head(nullptr) {
        sem_init(&heapSemaphore, 0, 1);
        sem_init(&printSemaphore, 0, 1);
    }

    ~HeapManager() {
        // Clear the list
        std::cout << "Execution is done" << std::endl;
        std::cout.flush();
        print();
        clearHeap();
        // Destroy the semaphores
        sem_destroy(&heapSemaphore);
        sem_destroy(&printSemaphore);
    }

    int initHeap(int size) {
        if (head != nullptr) {
            clearHeap();
        }
        head = new Node(-1, size, 0);
        std::cout << "Memory Initialized" << std::endl;
        std::cout.flush();
        print();
        return 1;
    }

    int myMalloc(int ID, int size) {
        sem_wait(&heapSemaphore);
        Node* current = head;
        Node* prev = nullptr;
        int allocatedIndex = -1; // Default return value if allocation fails
        while (current != nullptr) {
            if (current->ID == -1 && current->size >= size) {
                if (current->size > size) {
                    Node* newNode = new Node(ID, size, current->index);
                    current->index += size;
                    current->size -= size;

                    if (prev == nullptr) {
                        head = newNode;
                    } else {
                        prev->next = newNode;
                    }
                    newNode->next = current;
                    allocatedIndex = newNode->index;
                } else {
                    current->ID = ID;
                    allocatedIndex = current->index;
                }
                std::cout << "Allocated for thread " << ID << std::endl;
                std::cout.flush();
                print();
                break;
            }
            prev = current;
            current = current->next;
        }

        if (allocatedIndex == -1) {
            std::cout << "Allocation failed for thread " << ID << std::endl;
            print();
            std::cout.flush();
            sem_post(&heapSemaphore);
            return -1;
        }
        std::cout.flush();
        sem_post(&heapSemaphore);
        return allocatedIndex;
    }

    int myFree(int ID, int index) {
        sem_wait(&heapSemaphore);
        Node* current = head;
        Node* prev = nullptr;
        bool freed = false;

        while (current != nullptr) {
            if (current->ID == ID && current->index == index) {
                current->ID = -1; // Mark as free

                // Coalescing with next if it's free
                if (current->next != nullptr && current->next->ID == -1) {
                    Node* next = current->next;
                    current->size += next->size;
                    current->next = next->next;
                    delete next;
                }

                // Coalescing with prev if it's free
                if (prev != nullptr && prev->ID == -1) {
                    prev->size += current->size;
                    prev->next = current->next;
                    delete current;
                    current = prev; // Update current to prev since current is deleted
                }

                std::cout << "Freed from thread " << ID << std::endl;
                print();
                std::cout.flush();
                freed = true;
                break;
            }
            prev = current;
            current = current->next;
        }

        if (!freed) {
            std::cout << "Free failed for thread " << ID << std::endl;
            print();
            std::cout.flush();
            sem_post(&heapSemaphore);
            return -1;
        }
        std::cout.flush();
        sem_post(&heapSemaphore);
        return 1;
    }


    void print() {
        sem_wait(&printSemaphore);
        printList();
        std::cout.flush();
        sem_post(&printSemaphore);
    }
};

