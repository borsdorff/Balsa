#ifndef DATAMODEL_H
#define DATAMODEL_H

#include <algorithm>
#include <cassert>
#include <limits>
#include <memory>
#include <vector>

/**
 * One data point in a data set. The data consists of a list of feature-values,
 * where each feature is a double-precision float.
 */
typedef std::vector<double> DataPoint;

/**
 * The unique consecutive ID of a DataPoint.
 */
typedef std::size_t DataPointID;

/**
 * Compute the Gini impurity of a set of totalCount points, where trueCount points are labeled 'true', and the rest is false.
 */
inline double giniImpurity( unsigned int trueCount, unsigned int totalCount )
{
    double t = trueCount;
    auto T   = totalCount;
    return ( 2 * t * ( 1.0 - ( t / T ) ) ) / T;
}

/**
 * A set of DataPoints.
 */
class DataSet
{
public:

  DataSet( unsigned int featureCount ):
  m_featureCount( featureCount )
  {
  }

  /**
   * Append a data point to the set.
   * \pre The number of features in the point must match those in this dataset (dataPoint.size() == this->getFeatureCount()).
   * \return The unique consecutive ID of the point.
   */
  DataPointID appendDataPoint( const DataPoint &dataPoint )
  {
      // Check precondition, append the features of the datapoint to the end of the data array.
      assert( dataPoint.size() == getFeatureCount() );
      std::copy( dataPoint.begin(), dataPoint.end(), std::back_inserter( m_dataRows ) );
      return ( m_dataRows.size() / m_featureCount ) - 1;
  }

  /**
   * Returns the number of features in all datapoints in this dataset.
   */
  unsigned int getFeatureCount() const
  {
      return m_featureCount;
  }

  /**
   * Returns the number of data points in this dataset.
   */
  std::size_t size() const
  {
      return m_dataRows.size() / m_featureCount;
  }

  /**
   * Returns a specific feature-value of a particular point.
   */
  double getFeatureValue( DataPointID pointID, unsigned int featureID ) const
  {
      assert( pointID < size() );
      assert( featureID < m_featureCount );
      return m_dataRows[ pointID * m_featureCount + featureID];
  }

private:

  unsigned int        m_featureCount;
  std::vector<double> m_dataRows    ; // Data is stored in row-major order, one row of size m_featureCount per row;

};

/**
 * A set of DataPoints that includes the known labels of each point.
 */
class TrainingDataSet
{
public:

  typedef std::shared_ptr<const TrainingDataSet> ConstSharedPointer;
  typedef std::shared_ptr<      TrainingDataSet> SharedPointer     ;

  TrainingDataSet( unsigned int featureCount ):
  m_dataSet( featureCount )
  {
  }

  /**
   * Append a data point and its known label to the set.
   * \pre The number of features in the point must match those in this dataset (dataPoint.size() == this->getFeatureCount()).
   * \return The unique consecutive ID of the point.
   */
  DataPointID appendDataPoint( const DataPoint &dataPoint, bool label )
  {
      // Add the datapoint to the dataset.
      auto id = m_dataSet.appendDataPoint( dataPoint );

      // Add the label to the label set.
      m_dataSetLabels.push_back( label );
      assert( m_dataSetLabels.size() == m_dataSet.size() );

      // Return the ID of the created point.
      return id;
  }

  /**
   * Returns the number of features in each point.
   */
  std::size_t getFeatureCount() const
  {
      return m_dataSet.getFeatureCount();
  }

  /**
   * Returns the number of points in the training data set.
   */
  std::size_t size() const
  {
      return m_dataSet.size();
  }

  /**
   * Returns the known label of a point.
   */
  bool getLabel( DataPointID pointID ) const
  {
      return m_dataSetLabels[ pointID ];
  }

  /**
   * Returns a specific feature-value of a particular point.
   */
  double getFeatureValue( DataPointID pointID, unsigned int featureID ) const
  {
      return m_dataSet.getFeatureValue( pointID, featureID );
  }

  void dump()
  {
      // Print all point IDs, features, and labels.
      auto featureCount = m_dataSet.getFeatureCount();
      for ( DataPointID pointID = 0; pointID < m_dataSet.size(); ++pointID )
      {
          std::cout << pointID;
          for ( unsigned int feature = 0; feature < featureCount; ++feature )
          {
              std::cout << ", " << m_dataSet.getFeatureValue( pointID, feature );
          }
          std::cout << ", " << static_cast<unsigned int>( m_dataSetLabels[pointID] ) << std::endl;
      }
  }

private:

  DataSet             m_dataSet      ; // The data points without their labels.
  std::vector< bool > m_dataSetLabels; // The labels of each point in the dataset.
};

/**
 * A node in a decision tree. N.B. this class is intended for evaluation purposes, not for training (see TrainingTreeNode).
 */
class DecisionTreeNode
{
public:

  typedef std::shared_ptr<DecisionTreeNode> SharedPointer;

  DecisionTreeNode()
  {
  }

  virtual ~DecisionTreeNode()
  {
  }

  std::vector< bool > classify( const DataSet &dataSet )
  {
      std::vector< bool > labels(dataSet.size());
      for (std::size_t i = 0; i < dataSet.size(); ++i)
      {
        labels.at(i) = classify(dataSet, i);
      }
      return labels;
  }

  /**
   * Return the classification of all data points in a data set.
   * N.B. This is a naive implementation, suitable for testing and low-performance applications.
   */
  virtual bool classify( const DataSet &dataSet, DataPointID pointID ) = 0;

  /**
   * Return the classification of a data point.
   * N.B. This is a naive implementation, suitable for testing and low-performance applications.
   */
  virtual bool classify( const DataPoint &point ) = 0; // TODO: add efficient bulk classifier.


  /**
   * Print routine for testing purposes.
   */
  virtual void dump( unsigned int indent = 0 ) = 0;

};

/**
 * An internal node in a decision tree.
 */
class DecisionTreeInternalNode: public DecisionTreeNode
{
public:

  /**
   * Constructor.
   * \param featureID The feature on which the node splits the dataset.
   * \param splitValue The value on which the node splits the dataset (x < splitValue is left half, x >= is right half).
   */
    DecisionTreeInternalNode( unsigned int featureID, double splitValue, DecisionTreeNode::SharedPointer leftChild, DecisionTreeNode::SharedPointer rightChild ):
    m_featureID ( featureID  ),
    m_splitValue( splitValue ),
    m_leftChild ( leftChild  ),
    m_rightChild( rightChild )
    {
    }

    /**
     * Implementation of base class method.
     */
    bool classify( const DataSet &dataSet, DataPointID pointID )
    {
        if ( dataSet.getFeatureValue( pointID, m_featureID ) < m_splitValue )
            return m_leftChild->classify( dataSet, pointID );
        return m_rightChild->classify( dataSet, pointID );
    }

    /**
     * Implementation of base class method.
     */
    bool classify( const DataPoint &point )
    {
        if ( point[m_featureID] < m_splitValue ) return m_leftChild->classify( point );
        return m_rightChild->classify( point );
    }

  virtual void dump( unsigned int indent )
  {
      auto tab = std::string( indent, ' ' );
      std::cout << tab << "Feature #" << m_featureID << ", value = " <<  m_splitValue << std::endl;
      std::cout << tab << "Left: " << std::endl;
      m_leftChild->dump( indent + 1 );
      std::cout << tab << "Right: " << std::endl;
      m_rightChild->dump( indent + 1 );
  }

  private:

    unsigned int                    m_featureID ;
    double                          m_splitValue;
    DecisionTreeNode::SharedPointer m_leftChild ;
    DecisionTreeNode::SharedPointer m_rightChild;

};

/**
 * Leaf node in a binary decision tree.
 */
class DecisionTreeLeafNode: public DecisionTreeNode
{
public:

  /**
   * Constructor.
   */
  DecisionTreeLeafNode( bool label ):
  m_label( label )
  {
  }

  /**
   * Implementation of base class method.
   */
  bool classify( const DataSet &, DataPointID )
  {
      return m_label;
  }

  /**
   * Implementation of base class method.
   */
  bool classify( const DataPoint & )
  {
      return m_label;
  }

  /**
   * Implementation of base class method.
   */
  virtual void dump( unsigned int indent )
  {
      auto tab = std::string( indent, ' ' );
      std::cout << tab << ( m_label ? "TRUE" : "FALSE" ) << std::endl;
  }

private:

  bool m_label;

};

/**
 * An index for traversing points in a dataset in order of each feature.
 */
class FeatureIndex
{
public:

  typedef std::tuple< double, bool, DataPointID > Entry;
  typedef std::vector< Entry > SingleFeatureIndex;

  FeatureIndex( const TrainingDataSet &dataset ):
  m_trueCount( 0 )
  {
      // Create a sorted index for each feature.
      m_featureIndices.clear();
      m_featureIndices.reserve( dataset.getFeatureCount() );

      for ( unsigned int feature = 0; feature < dataset.getFeatureCount(); ++feature )
      {
          // Create the index for this feature, and give it enough capacity.
          m_featureIndices.push_back( std::vector< Entry >() );
          auto &index = *m_featureIndices.rbegin();
          index.reserve( dataset.size() );

          // Create entries for each point in the dataset, and count the 'true' labels.
          m_trueCount = 0;
          for ( DataPointID pointID( 0 ), end( dataset.size() ); pointID < end; ++pointID )
          {
              bool label = dataset.getLabel( pointID );
              if ( label ) ++m_trueCount;
              index.push_back( Entry( dataset.getFeatureValue( pointID, feature ), label,  pointID ) );
          }

          // Sort the index by feature value (the rest of the fields don't matter).
          std::sort( index.begin(), index.end() );
      }
  }

  SingleFeatureIndex::const_iterator featureBegin( unsigned int featureID ) const
  {
      return m_featureIndices[featureID].begin();
  }

  SingleFeatureIndex::const_iterator featureEnd( unsigned int featureID ) const
  {
      return m_featureIndices[featureID].end();
  }

  /**
   * Returns the number of features.
   */
  unsigned int getFeatureCount() const
  {
      return m_featureIndices.size();
  }

  /**
   * Returns the number of indexed points.
   */
  std::size_t size() const
  {
      return m_featureIndices[0].size();
  }

  /**
   * Returns the number of points labeled 'true'.
   */
  unsigned int getTrueCount() const
  {
      return m_trueCount;
  }

private:

  unsigned int m_trueCount;
  std::vector< SingleFeatureIndex >  m_featureIndices;
};

/**
 * A node in a decision tree, with special annotations for the training process.
 */
class TrainingTreeNode
{
public:

  typedef std::shared_ptr<TrainingTreeNode> SharedPointer;

  TrainingTreeNode():
  m_splitFeatureID    ( 0     ),
  m_splitValue        ( 0     ),
  m_totalCount        ( 0     ),
  m_trueCount         ( 0     ),
  m_totalCountLeftHalf( 0     ),
  m_trueCountLeftHalf ( 0     ),
  m_trueCountRightHalf( 0     ),
  m_currentFeature    ( 0     ),
  m_bestSplitFeature  ( 0     ),
  m_bestSplitValue    ( 0     ),
  m_bestSplitGiniIndex( 0     )
  {
  }

  /**
   * Allows this node to count a data point as one of its descendants, and returns the leaf-node that contains the point.
   * \return A pointer to the leaf node that contains the point (direct parent).
   */
  TrainingTreeNode *registerPoint( DataPointID pointID, const TrainingDataSet &dataset )
  {
      // Count the point if this is a leaf-node, and return this node as the parent.
      if ( !m_leftChild )
      {
          bool label = dataset.getLabel( pointID );
          assert( !m_rightChild );
          ++m_totalCount;
          if ( label ) ++m_trueCount;
          return this;
      }

      // Defer the registration to the correct child.
      if ( dataset.getFeatureValue( pointID, m_splitFeatureID ) < m_splitValue ) return m_leftChild->registerPoint( pointID, dataset );
      return m_rightChild->registerPoint( pointID, dataset );
  }

  /**
   * Returns a stripped, non-training decision tree.
   */
  DecisionTreeNode::SharedPointer finalize()
  {
      // Build an internal node or a leaf node.
      if ( m_leftChild )
      {
          assert( m_rightChild );
          return DecisionTreeNode::SharedPointer( new DecisionTreeInternalNode( m_splitFeatureID, m_splitValue, m_leftChild->finalize(), m_rightChild->finalize() ) );
      }
      else
      {
          return DecisionTreeNode::SharedPointer( new DecisionTreeLeafNode( getLabel() ) );
      }
  }

  /**
   * Returns the most obvious label for this node.
   */
  bool getLabel() const
  {
      return m_totalCount < 2 * m_trueCount;
  }

  /**
   * Initializes the search for the optimal split.
   */
  void initializeOptimalSplitSearch()
  {
      // Reset the point counts. Points will be re-counted during the point registration phase.
      m_trueCount  = 0;
      m_totalCount = 0;

      // Reset the best split found so far. This will be re-determined during the feature traversal phase.
      m_bestSplitFeature   = 0;
      m_bestSplitValue     = 0;
      m_bestSplitGiniIndex = std::numeric_limits<double>::max();

      // Initialize any children.
      if ( m_leftChild  ) m_leftChild ->initializeOptimalSplitSearch();
      if ( m_rightChild ) m_rightChild->initializeOptimalSplitSearch();
  }

  /**
   * Instructs this node and its children that a particular feature will be traversed in-order now.
   */
  void startFeatureTraversal( unsigned int featureID )
  {
      // Start feature traversal in the children, if present.
      if ( m_leftChild )
      {
          assert( m_rightChild );
          m_leftChild->startFeatureTraversal( featureID );
          m_rightChild->startFeatureTraversal( featureID );
      }

      // Register which feature is being traversed now, and reset traversal statistics.
      m_currentFeature     = featureID;
      m_trueCountLeftHalf  = 0;
      m_trueCountRightHalf = m_trueCount;
  }

  /**
   * Visit one point during the feature traversal phase.
   */
  void visitPoint( DataPointID, double featureValue, bool label )
  {
      // This must never be called on internal nodes.
      assert( !m_leftChild  );
      assert( !m_rightChild );

      // Update the traversal statistics.
      if ( label )
      {
          ++m_trueCountLeftHalf;
          --m_trueCountRightHalf;
      }
      ++m_totalCountLeftHalf;

      // Compute the Gini index, assuming the split is made at this point.
      auto totalCountRightHalf = m_totalCount - m_totalCountLeftHalf;
      auto giniLeft  = giniImpurity( m_trueCountLeftHalf , m_totalCountLeftHalf );
      auto giniRight = giniImpurity( m_trueCountRightHalf, totalCountRightHalf  );
      auto giniTotal = ( giniLeft * m_totalCountLeftHalf + giniRight * totalCountRightHalf ) / m_totalCount;

      // Save this split if it is the best one so far.
      if ( giniTotal < m_bestSplitGiniIndex )
      {
          m_bestSplitFeature   = m_currentFeature;
          m_bestSplitValue     = featureValue    ;
          m_bestSplitGiniIndex = giniTotal       ;
      }
  }

  /**
   * Split the leaf nodes at the most optimal point, after all features have been traversed.
   */
  void split()
  {
      // If this is an interior node, split the children and quit.
      if ( m_leftChild )
      {
          std::cout << "Splitting interior node recursively." << std::endl;
          assert( m_rightChild );
          m_leftChild ->split();
          m_rightChild->split();
          return;
      }

      // Assert that this is a leaf node.
      std::cout << "Splitting leaf node on Feature #" << m_bestSplitFeature << " at value " << m_bestSplitValue << std::endl;
      assert( !m_leftChild  );
      assert( !m_rightChild );

      // Do not split if this node is completely pure.
      // N.B. the purity cutoff can be made more flexible.
      if ( m_trueCount == m_totalCount ) return;

      // Split this node at the best point that was found.
      m_splitValue     = m_bestSplitValue;
      m_splitFeatureID = m_bestSplitFeature;
      m_leftChild .reset( new TrainingTreeNode() );
      m_rightChild.reset( new TrainingTreeNode() );
  }

  SharedPointer m_leftChild         ; // Null for leaf nodes.
  SharedPointer m_rightChild        ; // Null for leaf nodes.
  unsigned int  m_splitFeatureID    ; // The ID of the feature at which this node is split. Only valid for internal nodes.
  double        m_splitValue        ; // The value at which this node is split, along the specified feature.
  unsigned int  m_totalCount        ; // Total number of points in this node.
  unsigned int  m_trueCount         ; // Total number of points labeled 'true' in this node.

  // Statistics used during traversal:
  unsigned int  m_totalCountLeftHalf; // Total number of points that have been visited during traversal of the current feature.
  unsigned int  m_trueCountLeftHalf ; // Totol number of visited points labeled 'true'.
  unsigned int  m_trueCountRightHalf; // Remaining unvisited points labeled 'true'.
  unsigned int  m_currentFeature    ; // The feature that is currently being traversed.
  unsigned int  m_bestSplitFeature  ; // Best feature for splitting found so far.
  double        m_bestSplitValue    ; // Best value to split at found so far.
  double        m_bestSplitGiniIndex; // Gini-index of the best split point found so far (lowest index).

};

class SingleTreeTrainer
{
public:

  SingleTreeTrainer( const TrainingDataSet &dataset, const FeatureIndex &featureIndex ):
  m_dataSet( dataset ),
  m_featureIndex( featureIndex )
  {
  }

  DecisionTreeNode::SharedPointer train()
  {
      // Create an empty training tree.
      TrainingTreeNode root;

      // Create a list of pointers from data points to their current parent nodes.
      std::vector< TrainingTreeNode * > pointParents( m_featureIndex.size(), &root );

      // Split all leaf nodes in the tree until the depth limit is reached.
      unsigned int LIMIT = 4;
      for ( unsigned int depth = 0; depth < LIMIT; ++depth )
      {
          // Tell all nodes that a round of optimal split searching is starting.
          root.initializeOptimalSplitSearch();

          // Register all points with their respective parent nodes.
          for ( DataPointID pointID( 0 ), end( pointParents.size() ); pointID < end; ++pointID )
          {
              pointParents[pointID] = pointParents[pointID]->registerPoint( pointID, m_dataSet );
          }

          // Traverse all data points once for each feature, in order, so the tree nodes can find the best possible split for them.
          std:: cout << "Traversing all features..." << std::endl;
          for ( unsigned int featureID = 0; featureID < m_featureIndex.getFeatureCount(); ++featureID ) // TODO: random trees should not use all features.
          {
              // Tell the tree that traversal is starting for this feature.
              std:: cout << "Traversing feature #" << featureID <<  "..." << std::endl;
              root.startFeatureTraversal( featureID );

              // Traverse all datapoints in order of this feature.
              for ( auto it( m_featureIndex.featureBegin( featureID ) ), end( m_featureIndex.featureEnd( featureID ) ); it != end; ++it )
              {
                  // Let the parent node of the data point know that it is being traversed.
                  auto &tuple = *it;
                  auto featureValue = std::get<0>( tuple );
                  auto label        = std::get<1>( tuple );
                  auto pointID      = std::get<2>( tuple );
                  pointParents[pointID]->visitPoint( pointID, featureValue, label );
              }
          }

          // Allow all leaf nodes to split, if necessary.
          std::cout << "Before split" << std::endl;
          root.split();
          std::cout << "After split" << std::endl;
      }

      // Return a stripped version of the training tree.
      std::cout << "Finalizing.." << std::endl;
      return root.finalize();
  }

private:

  const TrainingDataSet &m_dataSet     ;
  const FeatureIndex    &m_featureIndex;
};

/**
 * Trains a random binary forest classifier on a TrainingDataSet.
 */
class BinaryRandomForestTrainer
{
public:

  /**
   * Constructor.
   * \param dataset A const reference to a training dataset. Modifying the set after construction of the trainer invalidates the trainer.
   * \param concurrentTrainers The maximum number of trees that may be trained concurrently.
   */
  BinaryRandomForestTrainer( TrainingDataSet::ConstSharedPointer dataset, unsigned int treeCount = 1, unsigned int concurrentTrainers = 1 ):
  m_dataSet( dataset ),
  m_featureIndex( *dataset ),
  m_trainerCount( concurrentTrainers ),
  m_treeCount( treeCount )
  {
  }

  /**
   * Destructor.
   */
  virtual ~BinaryRandomForestTrainer()
  {
  }

  /**
   * Train a forest of random trees on the data.
   */
  void train()
  {
      // Create the specified number of (possibly) concurrent single-tree trainers.
      m_treeTrainers.clear();
      for ( unsigned int i = 0; i < m_trainerCount; ++i )
          m_treeTrainers.push_back( SingleTreeTrainer( *m_dataSet, m_featureIndex ) );

      // Let the trainers train the trees.
      // TODO: launch concurrent trainers.
      auto treesToTrain = m_treeCount;
      while ( treesToTrain-- )
      {
          // EXPERIMENTAL.
          auto tree = m_treeTrainers[0].train();
          tree->dump();
      }
  }

private:

  TrainingDataSet::ConstSharedPointer  m_dataSet     ;
  FeatureIndex                         m_featureIndex;
  unsigned int                         m_trainerCount;
  unsigned int                         m_treeCount   ;
  std::vector< SingleTreeTrainer    >  m_treeTrainers;

};

#endif // DATAMODEL_H
