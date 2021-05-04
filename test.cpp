#include "aacHandler.hpp"
#include "h264Handler.hpp"

int main()
{
    h264_parser("./server.h264");
    h264_parser("./client_dec.h264");

    aac_parser("./server.aac");
    aac_parser("./client_dec.aac");
    return 0;
}