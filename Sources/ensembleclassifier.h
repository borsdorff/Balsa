#ifndef ENSEMBLECLASSIFIER_H
#define ENSEMBLECLASSIFIER_H

#include <cassert>
#include <thread>

#include "classifier.h"
#include "classifierstream.h"
#include "datatypes.h"
#include "exceptions.h"
#include "messagequeue.h"

namespace balsa
{

template <typename FeatureIterator, typename OutputIterator>
class EnsembleClassifier: public Classifier<FeatureIterator, OutputIterator>
{
public:

    using typename Classifier<FeatureIterator, OutputIterator>::VoteTable;

    /**
     * Creates an ensemble classifier.
     * \param classifiers A resettable stream of classifiers to apply.
     * \param maxWorkerThreads The maximum number of threads that may be created in addition to the main thread.
     */
    EnsembleClassifier( ClassifierStream<FeatureIterator, OutputIterator> & classifiers, unsigned int maxWorkerThreads = 0 ):
    Classifier<FeatureIterator, OutputIterator>(),
    m_maxWorkerThreads( maxWorkerThreads ),
    m_classifierStream( classifiers ),
    m_classWeights( m_classifierStream.getClassCount(), 1.0 )
    {
    }

    /**
     * Set the relative weights of each class.
     * \param classWeights Multiplication factors that will be applied to each class vote total before determining the maximum score and final label.
     * \pre The weights must be non-negative.
     * \pre There must be a weight for each class.
     */
    void setClassWeights( const std::vector<float> &classWeights )
    {
        assert( classWeights.size() == m_classWeights.size() );
        for ( auto w: classWeights ) assert( w >= 0 );
        m_classWeights = classWeights;
    }

    /**
     * Returns the number of classes distinguished by this classifier.
     */
    unsigned int getClassCount() const
    {
        return m_classifierStream.getClassCount();
    }

    /**
     * Bulk-classifies a sequence of data points.
     */
    void classify( FeatureIterator pointsStart, FeatureIterator pointsEnd, unsigned int featureCount, OutputIterator labelsStart ) const
    {
        // Check the dimensions of the input data.
        if ( featureCount == 0 ) throw ClientError( "Data points must have at least one feature." );
        auto entryCount = std::distance( pointsStart, pointsEnd );
        if ( entryCount % featureCount ) throw ClientError( "Malformed dataset." );

        // Determine the number of points in the input data.
        auto pointCount = entryCount / featureCount;

        // Create a table for the label votes.
        VoteTable voteCounts( pointCount, m_classifierStream.getClassCount() );

        // Let all classifiers vote on the point labels.
        classifyAndVote( pointsStart, pointsEnd, featureCount, voteCounts );

        // Generate the labels.
        for ( unsigned int point = 0; point < pointCount; ++point )
            *labelsStart++ = static_cast<Label>( voteCounts.getColumnOfWeightedRowMaximum( point, m_classWeights ) );
    }

    /**
     * Bulk-classifies a set of points, adding a vote (+1) to the vote table for
     * each point of which the label is 'true'.
     * \param pointsStart An iterator that points to the first feature value of
     *  the first point.
     * \param pointsEnd An itetartor that points to the end of the block of
     *  point data.
     * \param featureCount The number of features for each data point.
     * \param table A table for counting votes.
     * \pre The column count of the vote table must match the number of
     *  features, the row count must match the number of points.
     */
    unsigned int classifyAndVote( FeatureIterator pointsStart, FeatureIterator pointsEnd, unsigned int featureCount, VoteTable & table ) const
    {
        // Dispatch to single- or multithreaded implementation.
        if ( m_maxWorkerThreads > 0 )
            return classifyAndVoteMultiThreaded( pointsStart, pointsEnd, featureCount, table );
        else
            return classifyAndVoteSingleThreaded( pointsStart, pointsEnd, featureCount, table );
    }

private:

    /**
     * A job for the worker thread.
     */
    class WorkerJob
    {
    public:

        WorkerJob( typename Classifier<FeatureIterator, OutputIterator>::ConstSharedPointer classifier ):
        m_classifier( classifier )
        {
        }

        // Pointer to the tree that must be applied. Null indicates the end of processing, causing the worker to finish.
        typename Classifier<FeatureIterator, OutputIterator>::ConstSharedPointer m_classifier;
    };

    /**
     * A thread that runs classifyAndVote on a thread-local vote table.
     */
    class WorkerThread
    {
    public:

        typedef std::shared_ptr<WorkerThread> SharedPointer;

        WorkerThread( MessageQueue<WorkerJob> & jobQueue, unsigned int classCount, FeatureIterator pointsStart, FeatureIterator pointsEnd, unsigned int featureCount ):
        m_running( false ),
        m_jobQueue( jobQueue ),
        m_pointsStart( pointsStart ),
        m_pointsEnd( pointsEnd ),
        m_featureCount( featureCount ),
        m_voteCounts( 0, 0 ) // Overwrite this below.
        {
            // Check the dimensions of the input data.
            auto entryCount = std::distance( pointsStart, pointsEnd );
            assert( featureCount > 0 );
            assert( ( entryCount % featureCount ) == 0 );

            // Determine the number of points in the input data.
            auto pointCount = entryCount / featureCount;

            // Create a table for the label votes.
            m_voteCounts = VoteTable( pointCount, classCount );
        }

        void start()
        {
            // Launch a thread to process incoming jobs from the job queue.
            assert( !m_running );
            m_running = true;
            m_thread  = std::thread( &EnsembleClassifier::WorkerThread::processJobs, this );
        }

        void join()
        {
            // Wait for the thread to join.
            if ( !m_running ) return;
            m_thread.join();
            m_running = false;
        }

        const VoteTable & getVoteCounts() const
        {
            return m_voteCounts;
        }

    private:

        void processJobs()
        {
            // Process incoming jobs until the null job is received.
            for ( WorkerJob job( m_jobQueue.receive() ); job.m_classifier; job = m_jobQueue.receive() )
            {
                // Let the classifier vote on the data. Accumulate votes in the thread-private vote table.
                job.m_classifier->classifyAndVote( m_pointsStart, m_pointsEnd, m_featureCount, m_voteCounts );
            }
        }

        bool                      m_running;
        MessageQueue<WorkerJob> & m_jobQueue;
        FeatureIterator           m_pointsStart;
        FeatureIterator           m_pointsEnd;
        unsigned int              m_featureCount;
        VoteTable                 m_voteCounts;
        std::thread               m_thread;
    };

    unsigned int classifyAndVoteSingleThreaded( FeatureIterator pointsStart, FeatureIterator pointsEnd, unsigned int featureCount, VoteTable & table ) const
    {
        // Reset the stream of classifiers, and apply each classifier that comes out of it.
        m_classifierStream.rewind();
        unsigned int voterCount = 0;
        for ( auto classifier = m_classifierStream.next(); classifier; classifier = m_classifierStream.next(), ++voterCount )
            classifier->classifyAndVote( pointsStart, pointsEnd, featureCount, table );

        // Return the number of classifiers that have voted.
        return voterCount;
    }

    unsigned int classifyAndVoteMultiThreaded( FeatureIterator pointsStart, FeatureIterator pointsEnd, unsigned int featureCount, VoteTable & table ) const
    {
        // Reset the stream of classifiers.
        m_classifierStream.rewind();
        unsigned int voterCount = 0;

        // Create message queues for communicating with the worker threads.
        MessageQueue<WorkerJob> jobQueue;

        // Record the number of classes.
        unsigned int classCount = m_classifierStream.getClassCount();

        // Create the workers.
        std::vector<typename WorkerThread::SharedPointer> workers;
        for ( unsigned int i = 0; i < m_maxWorkerThreads; ++i )
            workers.push_back( typename WorkerThread::SharedPointer( new WorkerThread( jobQueue, classCount, pointsStart, pointsEnd, featureCount ) ) );

        // Start all the workers.
        for ( auto & worker : workers ) worker->start();

        // Reset the stream of classifiers, and apply each classifier that comes out of it.
        for ( auto classifier = m_classifierStream.next(); classifier; classifier = m_classifierStream.next(), ++voterCount ) jobQueue.send( WorkerJob( classifier ) );

        // Send stop messages for all workers.
        for ( auto i = workers.size(); i > 0; --i ) jobQueue.send( WorkerJob( nullptr ) );

        // Wait for all the workers to join.
        for ( auto & worker : workers ) worker->join();

        // Add the votes accumulated by the workers to the output total.
        for ( auto & worker : workers ) table += worker->getVoteCounts();

        // Return the number of classifiers that have voted.
        return voterCount;
    }

    unsigned int                                        m_maxWorkerThreads;
    ClassifierStream<FeatureIterator, OutputIterator> & m_classifierStream;
    std::vector<float>                                  m_classWeights    ;
};

} // namespace balsa

#endif // ENSEMBLECLASSIFIER_H
