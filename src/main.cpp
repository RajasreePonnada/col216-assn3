#include <iostream>
#include <string>
#include <stdexcept>
#include <unistd.h> // For getopt
#include <cstdlib>  // For exit
#include <cctype>   // For isprint

#include "simulator.h"
#include "defs.h"

void printHelp()
{
    std::cout << "Usage: ./L1simulate [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -t <tracefile_base> : Base name of the 4 trace files (e.g., app1)" << std::endl;
    std::cout << "  -s <s>              : Number of set index bits (S = 2^s)" << std::endl;
    std::cout << "  -E <E>              : Associativity (number of lines per set, E > 0)" << std::endl;
    std::cout << "  -b <b>              : Number of block offset bits (B = 2^b, b >= 2 for 4-byte words)" << std::endl;
    std::cout << "  -o <outputfile>     : (Optional) File to log output for plotting etc." << std::endl;
    std::cout << "  -h                  : Print this help message" << std::endl;
}

int main(int argc, char *argv[])
{
    std::string trace_base_name = "";
    std::string output_filename = "";
    // Use signed int for parsing to easily check for negative, then cast to unsigned
    long s_long = -1, E_long = -1, b_long = -1;
    bool s_set = false, E_set = false, b_set = false, t_set = false;

    int opt;
    // Reset getopt state if necessary (for multiple calls in testing frameworks)
    optind = 1;

    while ((opt = getopt(argc, argv, "t:s:E:b:o:h")) != -1)
    {
        switch (opt)
        {
        case 't':
            trace_base_name = optarg;
            t_set = true;
            break;
        case 's':
            try
            {
                s_long = std::stol(optarg);
                s_set = true;
            }
            catch (...)
            { /* Handle error below */
            }
            break;
        case 'E':
            try
            {
                E_long = std::stol(optarg);
                E_set = true;
            }
            catch (...)
            { /* Handle error below */
            }
            break;
        case 'b':
            try
            {
                b_long = std::stol(optarg);
                b_set = true;
            }
            catch (...)
            { /* Handle error below */
            }
            break;
        case 'o':
            output_filename = optarg;
            break;
        case 'h':
            printHelp();
            return 0;
        case '?': // Unknown option or missing argument
            if (optopt == 't' || optopt == 's' || optopt == 'E' || optopt == 'b' || optopt == 'o')
            {
                std::cerr << "Error: Option -" << (char)optopt << " requires an argument." << std::endl;
            }
            else if (isprint(optopt))
            {
                std::cerr << "Error: Unknown option `-" << (char)optopt << "'." << std::endl;
            }
            else
            {
                std::cerr << "Error: Unknown option character `\\x" << std::hex << optopt << std::dec << "'." << std::endl;
            }
            printHelp();
            return 1;
        default:
            abort(); // Should not happen
        }
    }

    // Check if all required arguments are provided
    if (!t_set || !s_set || !E_set || !b_set)
    {
        std::cerr << "Error: Missing required arguments (-t, -s, -E, -b)." << std::endl;
        printHelp();
        return 1;
    }

    // Validate argument values
    if (s_long < 0)
    {
        std::cerr << "Error: Number of set bits (-s) must be non-negative." << std::endl;
        return 1;
    }
    if (E_long <= 0)
    {
        std::cerr << "Error: Associativity (-E) must be greater than 0." << std::endl;
        return 1;
    }
    if (b_long < 2)
    { // Need at least 2 bits for 4-byte offset within block
        std::cerr << "Error: Block offset bits (-b) must be at least 2 (for 4-byte block minimum)." << std::endl;
        return 1;
    }
    // Add upper bounds? e.g., b < 32, s < 32?
    if (s_long + b_long > 31)
    { // Prevent tag bits from becoming negative/zero if s+b >= 32
        std::cerr << "Warning: s + b (" << s_long << " + " << b_long << ") >= 32. Check address mapping." << std::endl;
        // Allow it, but be aware of potential zero tag bits.
    }

    // Cast to unsigned after validation
    unsigned int s_uint = static_cast<unsigned int>(s_long);
    unsigned int E_uint = static_cast<unsigned int>(E_long);
    unsigned int b_uint = static_cast<unsigned int>(b_long);

    try
    {
        // Create and run the simulator
        Simulator sim(s_uint, E_uint, b_uint, trace_base_name, output_filename);
        sim.run();
        sim.printStats();

        // Example for getting max cycles for report
        cycle_t max_exec_time = sim.getMaxCycles();
        // std::cout << "Maximum Execution Time (Final Cycle): " << max_exec_time << " cycles" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error during simulation setup or run: " << e.what() << std::endl;
        return 1;
    }
    catch (...)
    {
        std::cerr << "An unknown error occurred during simulation." << std::endl;
        return 1;
    }

    return 0;
}