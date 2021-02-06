#include <iostream>
#include "mpmc_bounded_queue.h"

using Queue = zoltan::mcmp_bounded_queue<1024, std::string>;

int main() {
    Queue queue;
    std::string str = "test1";
    assert(not queue.dequeue(str));
    queue.enqueue(str);
    str = "nothing";
    queue.dequeue(str);
    assert(str == "test1");
    std::cout << "Simple non threaded test " << str << std::endl;
    return 0;
}
