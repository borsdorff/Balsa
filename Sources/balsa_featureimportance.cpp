#include <cassert>
#include <iostream>
#include <random>
#include <sstream>
#include <string>

#include "exceptions.h"
#include "modelevaluation.h"
#include "randomforestclassifier.h"
#include "table.h"

using namespace balsa;

namespace
{
class Options
{
public:

    Options():
    threadCount( 1 ),
    maxPreload( 1 ),
    repeatCount( 5 )
    {
    }

    static std::string getUsage()
    {
        std::stringstream ss;
        ss << "Usage:" << std::endl
           << std::endl
           << "   balsa_featureimportance [options] <model file> <data input file> <label input file>" << std::endl
           << std::endl
           << " Options:" << std::endl
           << std::endl
           << "   -t <thread count> : Number of threads (default: 1)." << std::endl
           << "   -p <preload count>: Number of trees to preload (default: 1)." << std::endl
           << "   -r <repeats>      : Number of repeats used to determine feature importance (default: 5)." << std::endl;
        return ss.str();
    }

    static Options parseOptions( int argc, char ** argv )
    {
        // Put all arguments in a stringstream.
        std::stringstream args;
        for ( int i = 0; i < argc; ++i ) args << ' ' << argv[i];

        // Discard the executable name.
        std::string token;
        args >> token;
        token = "";

        // Parse all flags.
        Options options;
        while ( args >> token )
        {
            // Stop if the token is not a flag.
            assert( token.size() );
            if ( token[0] != '-' ) break;

            if ( token == "-t" )
            {
                if ( !( args >> options.threadCount ) ) throw ParseError( "Missing parameter to -t option." );
            }
            else if ( token == "-p" )
            {
                if ( !( args >> options.maxPreload ) ) throw ParseError( "Missing parameter to -p option." );
            }
            if ( token == "-r" )
            {
                if ( !( args >> options.repeatCount ) ) throw ParseError( "Missing parameter to -r option." );
                if ( options.repeatCount < 1 ) throw ParseError( "Repeat count must be positive." );
            }
        }

        // Parse the filenames.
        if ( token.size() == 0 ) throw ParseError( getUsage() );
        options.modelFile = token;
        if ( !( args >> options.dataFile ) ) throw ParseError( "Missing data file." );
        if ( !( args >> options.labelFile ) ) throw ParseError( "Missing label file." );

        // Return  results.
        return options;
    }

    std::string  modelFile;
    std::string  dataFile;
    std::string  labelFile;
    unsigned int threadCount;
    unsigned int maxPreload;
    unsigned int repeatCount;
};
} // namespace

int main( int argc, char ** argv )
{
    try
    {
        // Parse the command-line arguments.
        Options options = Options::parseOptions( argc, argv );

        // Load the test data.
        auto dataSet = readTableAs<double>( options.dataFile );
        auto labels  = readTableAs<Label>( options.labelFile );

        // Create a classifier for the model.
        RandomForestClassifier classifier( options.modelFile, options.threadCount - 1, options.maxPreload );

        // Calculate the feature importance and print them.
        std::cout << "Analyzing feature importance..." << std::endl;
        FeatureImportances importances( classifier, dataSet.begin(), dataSet.end(), labels.begin(), dataSet.getColumnCount(), options.repeatCount );
        std::cout << "Done." << std::endl;
        std::cout << importances;
    }
    catch ( Exception & e )
    {
        std::cerr << e.getMessage() << std::endl;
        return EXIT_FAILURE;
    }

    // Finish.
    return EXIT_SUCCESS;
}
