#include <opencv2/opencv.hpp>
#include <omp.h>
#include <iostream>


int main()
{
    #pragma omp parallel for
    for (int i = 0; i < 1000; ++i)
    {
        std::cout << "Parallel: " << i << std::endl;
    }

    return 0;
}

