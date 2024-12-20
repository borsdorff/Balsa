#include <cassert>
#include <iostream>
#include <random>
#include <sstream>
#include <string>

#include "classifierfilestream.h"
#include "config.h"
#include "exceptions.h"
#include "randomforesttrainer.h"
#include "table.h"
#include "timing.h"
#include "weightedcoin.h"

using namespace balsa;

namespace
{
class Options
{
public:

    Options():
    maxDepth( std::numeric_limits<unsigned int>::max() ),
    minPurity( 1.0 ),
    treeCount( 150 ),
    threadCount( 1 ),
    featuresToConsider( 0 ), // Will be chosen internally by trainer if 0.
    seed( std::random_device{}() ),
    writeDotty( false )
    {
    }

    static std::string getUsage()
    {
        std::stringstream ss;
        ss << "Usage:" << std::endl
           << std::endl
           << "   balsa_train [options] <data input file> <label input file> <model output file>" << std::endl
           << std::endl
           << " Options:" << std::endl
           << std::endl
           << "   -t <thread count>: Number of threads (default: 1)." << std::endl
           << "   -d <max depth>   : Maximum tree depth (default: +inf)." << std::endl
           << "   -p <min purity>  : Minimum Gini purity (default: 1)." << std::endl
           << "   -c <tree count>  : Number of trees (default: 150)." << std::endl
           << "   -s <random seed> : Random seed (default: a random value)." << std::endl
           << "   -f <count>       : Number of (randomly selected) features to consider per" << std::endl
           << "                      split (default: floor(sqrt(feature count))." << std::endl
           << "   -g               : Generates Graphviz/Dotty files of all trees." << std::endl;
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
            else if ( token == "-d" )
            {
                if ( !( args >> options.maxDepth ) ) throw ParseError( "Missing parameter to -d option." );
            }
            else if ( token == "-p" )
            {
                if ( !( args >> options.minPurity ) ) throw ParseError( "Missing parameter to -p option." );
            }
            else if ( token == "-c" )
            {
                if ( !( args >> options.treeCount ) ) throw ParseError( "Missing parameter to -c option." );
            }
            else if ( token == "-s" )
            {
                if ( !( args >> options.seed ) ) throw ParseError( "Missing parameter to -s option." );
            }
            else if ( token == "-f" )
            {
                if ( !( args >> options.featuresToConsider ) ) throw ParseError( "Missing parameter to -f option." );
            }
            else if ( token == "-g" )
            {
                options.writeDotty = true;
            }
            else
            {
                throw ParseError( std::string( "Unknown option: " ) + token );
            }
        }

        // Parse the filenames.
        if ( token.size() == 0 ) throw ParseError( getUsage() );
        options.dataFile = token;
        if ( !( args >> options.labelFile ) ) throw ParseError( getUsage() );
        if ( !( args >> options.outputFile ) ) throw ParseError( getUsage() );

        // Return  results.
        return options;
    }

    std::string                     dataFile;
    std::string                     labelFile;
    std::string                     outputFile;
    unsigned int                    maxDepth;
    double                          minPurity;
    unsigned int                    treeCount;
    unsigned int                    threadCount;
    unsigned int                    featuresToConsider;
    std::random_device::result_type seed;
    bool                            writeDotty;
};
} // namespace

int main( int argc, char ** argv )
{
    try
    {
        // Parse the command-line arguments.
        Options options = Options::parseOptions( argc, argv );

        // Debug.
        std::cout << "Data File        : " << options.dataFile << std::endl;
        std::cout << "Label File       : " << options.labelFile << std::endl;
        std::cout << "Output File      : " << options.outputFile << std::endl;
        std::cout << "Max. Depth       : " << options.maxDepth << std::endl;
        std::cout << "Min. Purity      : " << options.minPurity << std::endl;
        std::cout << "Tree Count       : " << options.treeCount << std::endl;
        std::cout << "Threads          : " << options.threadCount << std::endl;
        std::cout << "Feat. to Consider: " << options.featuresToConsider << std::endl;
        std::cout << "Random Seed      : " << options.seed << std::endl;

        // Seed master seed sequence.
        getMasterSeedSequence().seed( options.seed );

        // Load training data set.
        StopWatch watch;
        std::cout << "Ingesting data..." << std::endl;
        watch.start();
        auto dataSet = readTableAs<double>( options.dataFile );
        auto labels  = readTableAs<Label>( options.labelFile );
        if ( labels.getRowCount() != dataSet.getRowCount() ) throw ParseError( "Point file and label file have different row counts." );
        if ( labels.getColumnCount() != 1 ) throw ParseError( "Invalid label file: table has too many columns." );
        std::cout << "Dataset loaded: " << dataSet.getRowCount() << " points. (" << watch.stop() << " seconds)." << std::endl;
        const auto dataLoadTime = watch.getElapsedTime();

        // Train a random forest on the data.
        std::cout << "Training..." << std::endl;
        EnsembleFileOutputStream outputStream( options.outputFile, "balsa_train", balsa_VERSION_MAJOR, balsa_VERSION_MINOR, balsa_VERSION_PATCH );
        RandomForestTrainer      trainer( outputStream, options.featuresToConsider, options.maxDepth, options.minPurity, options.treeCount, options.threadCount, options.writeDotty );
        watch.start();
        trainer.train( dataSet.begin(), dataSet.end(), dataSet.getColumnCount(), labels.begin() );
        std::cout << "Done (" << watch.stop() << " seconds)." << std::endl;
        const auto trainingTime = watch.getElapsedTime();

        std::cout << "Timings:" << std::endl
                  << "Data Load Time: " << dataLoadTime << std::endl
                  << "Training Time: " << trainingTime << std::endl;
    }
    catch ( Exception & e )
    {
        std::cerr << e.getMessage() << std::endl;
        return EXIT_FAILURE;
    }

    // Finish.
    return EXIT_SUCCESS;
}
