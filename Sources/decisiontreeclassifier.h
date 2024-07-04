#ifndef DECISIONTREECLASSIFIER_H
#define DECISIONTREECLASSIFIER_H

#include <algorithm>
#include <iterator>
#include <numeric>

#include "classifier.h"
#include "datatypes.h"
#include "exceptions.h"
#include "iteratortools.h"

namespace balsa
{

// Forward declaration.
class BalsaFileParser;

// Forward declaration.
class BalsaFileWriter;

// Forward declaration.
template <typename FeatureIterator, typename LabelIterator>
class IndexedDecisionTree;

/**
 * A Classifier based on an internal decision tree.
 */
template <typename FeatureType>
class DecisionTreeClassifier: public Classifier
{
public:

    typedef std::shared_ptr<DecisionTreeClassifier>       SharedPointer;
    typedef std::shared_ptr<const DecisionTreeClassifier> ConstSharedPointer;

    static_assert( std::is_arithmetic<FeatureType>::value, "Feature type should be an integral or floating point type." );

    /**
     * Returns the number of classes distinguished by the classifier.
     */
    unsigned int getClassCount() const
    {
        return m_classCount;
    }

    /**
     * Bulk-classifies a sequence of data points.
     */
    template<typename FeatureIterator, typename LabelOutputIterator>
    void classify( FeatureIterator pointsStart, FeatureIterator pointsEnd, LabelOutputIterator labelsStart ) const
    {
        // Statically check that the label output iterator points to Labels.
        typedef std::remove_cv_t<typename iterator_value_type<LabelOutputIterator>::type>  LabelType;
        static_assert( std::is_same<LabelType, Label>::value, "The labelStart iterator must point to instances of type Label." );

        // Statically check that the FeatureIterator points to an arithmetical type.
        typedef std::remove_cv_t<typename iterator_value_type<FeatureIterator>::type> FeatureIteratedType;
        static_assert( std::is_arithmetic<FeatureIteratedType>::value, "Features must be of an integral or floating point type." );

        // Check the dimensions of the input data.
        auto entryCount = std::distance( pointsStart, pointsEnd );
        if ( entryCount % m_featureCount ) throw ClientError( "Malformed dataset." );

        // Determine the number of points in the input data.
        auto pointCount = entryCount / m_featureCount;

        // Create a table for the label votes.
        VoteTable voteCounts( pointCount, m_classCount );

        // Bulk-classify all points.
        classifyAndVote( pointsStart, pointsEnd, voteCounts );

        // Generate the labels.
        for ( unsigned int point = 0; point < pointCount; ++point )
            *labelsStart++ = static_cast<Label>( voteCounts.getColumnOfRowMaximum( point ) );
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
    template<typename FeatureIterator>
    unsigned int classifyAndVote( FeatureIterator pointsStart, FeatureIterator pointsEnd, VoteTable & table ) const
    {
        // Statically check that the FeatureIterator points to an arithmetical type.
        typedef std::remove_cv_t<typename iterator_value_type<FeatureIterator>::type> FeatureIteratedType;
        static_assert( std::is_arithmetic<FeatureIteratedType>::value, "Features must be of an integral or floating point type." );

        // Check the dimensions of the input data.
        auto entryCount = std::distance( pointsStart, pointsEnd );
        if ( entryCount % m_featureCount ) throw ClientError( "Malformed dataset." );

        // Determine the number of points in the input data.
        auto pointCount = entryCount / m_featureCount;

        // Create a list containing all datapoint IDs (0, 1, 2, etc.).
        std::vector<DataPointID> pointIDs( pointCount );
        std::iota( pointIDs.begin(), pointIDs.end(), 0 );

        // Recursively partition the list of point IDs according to the interior node criteria, and classify them by the leaf node labels.
        recursiveClassifyVote( pointIDs.begin(), pointIDs.end(), pointsStart, table, NodeID( 0 ) );

        // Return the number of classifiers that voted.
        return 1;
    }

private:

    DecisionTreeClassifier( unsigned int classCount, unsigned int featureCount ):
    m_classCount( classCount ),
    m_featureCount( featureCount )
    {
    }

    template<typename FeatureIterator>
    void recursiveClassifyVote( std::vector<DataPointID>::iterator pointIDsStart, std::vector<DataPointID>::iterator pointIDsEnd, FeatureIterator pointsStart, VoteTable & voteTable, NodeID currentNodeID ) const
    {
        // If the current node is an interior node, split the points along the split value, and classify both halves.
        if ( m_leftChildID( currentNodeID, 0 ) > 0 )
        {
            // Extract the split limit and split dimension of this node.
            auto splitValue = m_splitValue( currentNodeID, 0 );
            auto featureID  = m_splitFeatureID( currentNodeID, 0 );

            // Split the point IDs in two halves: points that lie below the split value, and points that lie on or above the feature split value.
            auto featureCount = m_featureCount;
            auto pointIsBelowLimit = [&pointsStart, featureCount, splitValue, featureID]( const unsigned int & pointID )
            {
                return pointsStart[featureCount * pointID + featureID] < splitValue;
            };
            auto secondHalf = std::partition( pointIDsStart, pointIDsEnd, pointIsBelowLimit );

            // Recursively classify-vote both halves.
            recursiveClassifyVote( pointIDsStart, secondHalf, pointsStart, voteTable, m_leftChildID( currentNodeID, 0 ) );
            recursiveClassifyVote( secondHalf, pointIDsEnd, pointsStart, voteTable, m_rightChildID( currentNodeID, 0 ) );
        }

        // If the current node is a leaf node, cast a vote for the node-label for each point.
        else
        {
            auto label = m_label( currentNodeID, 0 );
            for ( auto it( pointIDsStart ), end( pointIDsEnd ); it != end; ++it )
            {
                ++voteTable( *it, label );
            }
        }
    }

    friend class BalsaFileParser;

    friend class BalsaFileWriter;

    template <typename T, typename U>
    friend class IndexedDecisionTree;

    unsigned int       m_classCount;
    unsigned int       m_featureCount;
    Table<NodeID>      m_leftChildID;
    Table<NodeID>      m_rightChildID;
    Table<FeatureID>   m_splitFeatureID;
    Table<FeatureType> m_splitValue;
    Table<Label>       m_label;
};

} // namespace balsa

#endif // DECISIONTREECLASSIFIER_H
