#include "magic_cli/cli_handler.hpp"
#include <iostream>
#include <cstdlib>

int main(int argc, char *argv[])
{
  try
  {
    // Get API base URL from environment variable
    const char *api_base_url = std::getenv("API_BASE_URL");
    std::string base_url = api_base_url ? api_base_url : "http://127.0.0.1:3030";

    // Create CLI handler
    magic_cli::CliHandler handler(base_url);

    // Parse command line arguments
    magic_cli::CliOptions options = handler.parse_arguments(argc, argv);

    // Execute the command
    handler.execute_command(options);
  }
  catch (const std::exception &e)
  {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}