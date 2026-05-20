#include "meld/cli/cli_core.hpp"
#include <iostream>

/**
 * Main entry point for the Meld CLI application
 */
int main(int argc, char* argv[]) {
    try {
        meld::cli::CliConfig config;
        config.app_name = "meld";
        config.version = "0.1.0";
        config.enable_colors = true;
        config.enable_logging = true;
        config.log_level = "info";
        
        meld::cli::CliCore cli(config);
        return cli.run(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error occurred" << std::endl;
        return 1;
    }
}