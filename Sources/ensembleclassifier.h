#ifndef ENSEMBLECLASSIFIER_H
#define ENSEMBLECLASSIFIER_H

#include <cassert>
#include <iostream>
#include <thread>

#include "classifier.h"
#include "classifierstream.h"
#include "datatypes.h"
#include "decisiontreeclassifier.h"
#include "exceptions.h"
#include "iteratortools.h"
#include "messagequeue.h"

namespace balsa
{

/**
 * A Visitor that invokes the classify() template method on a visited Classifier.
 */
template <typename FeatureIterator, typename LabelOutputIterator>
class ClassifyDispatcher: public ClassifierVisitor
{
public:

    ClassifyDispatcher( FeatureIterator featureStart, FeatureIterator featureEnd, LabelOutputIterator labelStart ):
    m_featureStart( featureStart ),
    m_featureEnd( featureEnd ),
    m_labelStart( labelStart )
    {
    }

    void visit( const EnsembleClassifier & classifier );
    void visit( const DecisionTreeClassifier<float> & classifier );
    void visit( const DecisionTreeClassifier<double> & classifier );

private:

    FeatureIterator     m_featureStart;
    FeatureIterator     m_featureEnd;
    LabelOutputIterator m_labelStart;
};

/**
 * A Visitor that invokes the classifyAndVote() template method on a visited Classifier.
 */
template <typename FeatureIterator>
class ClassifyAndVoteDispatcher: public ClassifierVisitor
{
public:

    ClassifyAndVoteDispatcher( FeatureIterator featureStart, FeatureIterator featureEnd, VoteTable & voteTable ):
    m_featureStart( featureStart ),
    m_featureEnd( featureEnd ),
    m_voteTable( voteTable )
    {
    }

    void visit( const EnsembleClassifier & classifier );
    void visit( const DecisionTreeClassifier<float> & classifier );
    void visit( const DecisionTreeClassifier<double> & classifier );

private:

    FeatureIterator m_featureStart;
    FeatureIterator m_featureEnd;
    VoteTable &     m_voteTable;
};

/**
 * A Classifier that invokes multiple underlying Classifiers to come to a vote-based classification.
 */
class EnsembleClassifier: public Classifier
{
public:

    /**
     * Creates an ensemble classifier.
     * \param classifierStream A resettable stream of classifiers to apply.
     * \param maxWorkerThreads The maximum number of threads that may be created in addition to the main thread.
     */
    EnsembleClassifier( ClassifierInputStream & classifierStream, unsigned int maxWorkerThreads = 0 ):
    m_classifierStreamPtr( &classifierStream ),
    m_maxWorkerThreads( maxWorkerThreads ),
    m_classWeights( classifierStream.getClassCount(), 1.0 )
    {
    }

    /**
     * Returns the number of classes distinguished by this classifier.
     */
    unsigned int getClassCount() const
    {
        return m_classifierStreamPtr->getClassCount();
    }

    /**
     * Returns the number of features the classifier expects.
     */
    unsigned int getFeatureCount() const
    {
        return m_classifierStreamPtr->getFeatureCount();
    }

    /**
     * Accept a visitor.
     */
    void visit( ClassifierVisitor & visitor ) const
    {
        visitor.visit( *this );
    }

    /**
     * Set the relative weights of each class.
     * \param classWeights Multiplication factors that will be applied to each class vote total before determining the maximum score and final label.
     * \pre The weights must be non-negative.
     * \pre There must be a weight for each class.
     */
    void setClassWeights( const std::vector<float> & classWeights )
    {
        assert( classWeights.size() == m_classWeights.size() );
        for ( auto w : classWeights ) assert( w >= 0 );
        m_classWeights = classWeights;
    }

    /**
     * Bulk-classifies a sequence of data points.
     */
    template <typename FeatureIterator, typename LabelOutputIterator>
    void classify( FeatureIterator pointsStart, FeatureIterator pointsEnd, LabelOutputIterator labelsStart ) const
    {
        // Statically check that the FeatureIterator points to an arithmetical type.
        typedef std::remove_cv_t<typename iterator_value_type<FeatureIterator>::type> FeatureIteratedType;
        static_assert( std::is_arithmetic<FeatureIteratedType>::value, "Features must be of an integral or floating point type." );

        // Check the dimensions of the input data.
        unsigned int featureCount = m_classifierStreamPtr->getFeatureCount();
        auto entryCount = std::distance( pointsStart, pointsEnd );
        assert( featureCount > 0 );
        if ( entryCount % featureCount ) throw ClientError( "Malformed dataset." );

        // Determine the number of points in the input data.
        auto pointCount = entryCount / featureCount;

        // Create a table for the label votes.
        VoteTable voteCounts( pointCount, m_classifierStreamPtr->getClassCount() );

        // Let all classifiers vote on the point labels.
        classifyAndVote( pointsStart, pointsEnd, voteCounts );

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
     * \param table A table for counting votes.
     * \pre The column count of the vote table must match the number of
     *  features, the row count must match the number of points.
     */
    template <typename FeatureIterator>
    unsigned int classifyAndVote( FeatureIterator pointsStart, FeatureIterator pointsEnd, VoteTable & table ) const
    {
        // Statically check that the FeatureIterator points to an arithmetical type.
        typedef std::remove_cv_t<typename iterator_value_type<FeatureIterator>::type> FeatureIteratedType;
        static_assert( std::is_arithmetic<FeatureIteratedType>::value, "Features must be of an integral or floating point type." );

        // Dispatch to single- or multithreaded implementation.
        if ( m_maxWorkerThreads > 0 )
            return classifyAndVoteMultiThreaded( pointsStart, pointsEnd, table );
        else
            return classifyAndVoteSingleThreaded( pointsStart, pointsEnd, table );
    }

protected:

    /**
     * Constructs an ensemble classifier without an associated classifier
     * stream.
     *
     * This constructor is intended to facilitate derived (convenience) classes
     * that own a classifier stream instance. After this constructor finishes,
     * the object is not yet in a usable state. Derived classes using this
     * constructor should call \c init() in their constructor body to ensure
     * the base class object as a whole is well-defined.
     */
    explicit EnsembleClassifier():
    m_classifierStreamPtr( nullptr ),
    m_maxWorkerThreads( 0 )
    {
    }

    /**
     * Finish the initialization of an ensemble classifier constructed using the
     * default constructor.
     */
    void init( ClassifierInputStream & classifierStream, unsigned int maxWorkerThreads )
    {
        assert( !m_classifierStreamPtr );
        m_classifierStreamPtr = &classifierStream;
        m_maxWorkerThreads    = maxWorkerThreads;
        m_classWeights.resize( m_classifierStreamPtr->getClassCount(), 1.0 );
    }

private:

    /**
     * A job for the worker thread.
     */
    class WorkerJob
    {
    public:

        WorkerJob( Classifier::ConstSharedPointer classifier ):
        m_classifier( classifier )
        {
        }

        // Pointer to the tree that must be applied. Null indicates the end of processing, causing the worker to finish.
        Classifier::ConstSharedPointer m_classifier;
    };

    /**
     * A thread that runs classifyAndVote on a thread-local vote table.
     */
    template <typename FeatureIterator>
    class WorkerThread
    {
    public:

        typedef std::shared_ptr<WorkerThread> SharedPointer;

        WorkerThread( MessageQueue<WorkerJob> & jobQueue, unsigned int classCount, unsigned int featureCount, FeatureIterator pointsStart, FeatureIterator pointsEnd ):
        m_running( false ),
        m_jobQueue( jobQueue ),
        m_pointsStart( pointsStart ),
        m_pointsEnd( pointsEnd ),
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
            m_thread  = std::thread( &EnsembleClassifier::WorkerThread<FeatureIterator>::processJobs, this );
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
                ClassifyAndVoteDispatcher voter( m_pointsStart, m_pointsEnd, m_voteCounts );
                job.m_classifier->visit( voter );
            }
        }

        bool                      m_running;
        MessageQueue<WorkerJob> & m_jobQueue;
        FeatureIterator           m_pointsStart;
        FeatureIterator           m_pointsEnd;
        VoteTable                 m_voteCounts;
        std::thread               m_thread;
    };

    template <typename FeatureIterator>
    unsigned int classifyAndVoteSingleThreaded( FeatureIterator pointsStart, FeatureIterator pointsEnd, VoteTable & table ) const
    {
        // Reset the stream of classifiers.
        m_classifierStreamPtr->rewind();

        // Apply each classifier to the data.
        unsigned int voterCount = 0;
        for ( auto classifier = m_classifierStreamPtr->next(); classifier; classifier = m_classifierStreamPtr->next(), ++voterCount )
        {
            ClassifyAndVoteDispatcher voter( pointsStart, pointsEnd, table );
            classifier->visit( voter );
        }

        // Return the number of classifiers that have voted.
        return voterCount;
    }

    template <typename FeatureIterator>
    unsigned int classifyAndVoteMultiThreaded( FeatureIterator pointsStart, FeatureIterator pointsEnd, VoteTable & table ) const
    {
        // Reset the stream of classifiers.
        m_classifierStreamPtr->rewind();

        // Create message queues for communicating with the worker threads.
        MessageQueue<WorkerJob> jobQueue;

        // Create the workers.
        std::vector<typename WorkerThread<FeatureIterator>::SharedPointer> workers;
        const unsigned int classCount = m_classifierStreamPtr->getClassCount();
        const unsigned int featureCount = m_classifierStreamPtr->getFeatureCount();
        for ( unsigned int i = 0; i < m_maxWorkerThreads; ++i )
            workers.push_back( typename WorkerThread<FeatureIterator>::SharedPointer( new WorkerThread<FeatureIterator>( jobQueue, classCount, featureCount, pointsStart, pointsEnd ) ) );

        // Start all the workers.
        for ( auto & worker : workers ) worker->start();

        // Reset the stream of classifiers, and apply each classifier that comes out of it.
        unsigned int voterCount = 0;
        for ( auto classifier = m_classifierStreamPtr->next(); classifier; classifier = m_classifierStreamPtr->next(), ++voterCount ) jobQueue.send( WorkerJob( classifier ) );

        // Send stop messages for all workers.
        for ( auto i = workers.size(); i > 0; --i ) jobQueue.send( WorkerJob( nullptr ) );

        // Wait for all the workers to join.
        for ( auto & worker : workers ) worker->join();

        // Add the votes accumulated by the workers to the output total.
        for ( auto & worker : workers ) table += worker->getVoteCounts();

        // Return the number of classifiers that have voted.
        return voterCount;
    }

    ClassifierInputStream * m_classifierStreamPtr;
    unsigned int            m_maxWorkerThreads;
    std::vector<float>      m_classWeights;
};

template <typename FeatureIterator, typename LabelOutputIterator>
void ClassifyDispatcher<FeatureIterator, LabelOutputIterator>::visit( const EnsembleClassifier & classifier )
{
    (void) classifier;
    assert( false );
    // classifier.classify( m_featureStart, m_featureEnd, m_labelStart );
}

template <typename FeatureIterator, typename LabelOutputIterator>
void ClassifyDispatcher<FeatureIterator, LabelOutputIterator>::visit( const DecisionTreeClassifier<float> & classifier )
{
    classifier.classify( m_featureStart, m_featureEnd, m_labelStart );
}

template <typename FeatureIterator, typename LabelOutputIterator>
void ClassifyDispatcher<FeatureIterator, LabelOutputIterator>::visit( const DecisionTreeClassifier<double> & classifier )
{
    classifier.classify( m_featureStart, m_featureEnd, m_labelStart );
}

template <typename FeatureIterator>
void ClassifyAndVoteDispatcher<FeatureIterator>::visit( const EnsembleClassifier & classifier )
{
    (void) classifier;
    assert( false );
    // classifier.classifyAndVote( m_featureStart, m_featureEnd, m_voteTable );
}

template <typename FeatureIterator>
void ClassifyAndVoteDispatcher<FeatureIterator>::visit( const DecisionTreeClassifier<float> & classifier )
{
    classifier.classifyAndVote( m_featureStart, m_featureEnd, m_voteTable );
}

template <typename FeatureIterator>
void ClassifyAndVoteDispatcher<FeatureIterator>::visit( const DecisionTreeClassifier<double> & classifier )
{
    classifier.classifyAndVote( m_featureStart, m_featureEnd, m_voteTable );
}

} // namespace balsa

#endif // ENSEMBLECLASSIFIER_H
