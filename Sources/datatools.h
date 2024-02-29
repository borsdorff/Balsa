#ifndef DATATOOLS_H
#define DATATOOLS_H

#include "datatypes.h"

/**
 * A table for counting the number of occurrences of various labels in a set of data points.
 */
class LabelFrequencyTable
{
public:

    /**
     * Constructs a frequency table.
     * \param exclusiveUpperLimit All counted values must be strictly below this limit.
     */
    LabelFrequencyTable( Label exclusiveUpperLimit ):
    m_data( exclusiveUpperLimit ),
    m_total( 0 )
    {
    }

    /**
     * Creates a frequency table from a list of labels.
     */
    template<typename InputIterator>
    LabelFrequencyTable( InputIterator labelBegin, InputIterator labelEnd )
    {
        for ( auto label( labelBegin ); label != labelEnd; ++label )
        {
            // Grow the count table if a large label is found.
            if ( *label >= m_data.size() ) m_data.resize( *label + 1 );

            // Count the label.
            ++m_data[*label];
        }

        // Calculate the total size.
        m_total = std::distance( labelBegin, labelEnd );
    }

    /**
     * Increment the count of a label by 1.
     * \param value The label of a data point.
     * \pre label < exclusiveUpperLimit (constructor parameter).
     */
    void increment( Label label )
    {
        assert( label < m_data.size() );
        ++m_data[label];
        ++m_total;
    }

    /**
     * Subtract 1 from the count of a label.
     * \pre getCount( label ) > 0
     */
    void decrement( Label label )
    {
        assert( m_data[label] > 0 );
        --m_data[label];
        --m_total;
    }

    /**
     * Returns the stored count of a particular label.
     */
    std::size_t getCount( Label label ) const
    {
        assert( label < m_data.size() );
        return m_data[label];
    }

    /**
     * Returns the total of all counts.
     */
    std::size_t getTotal() const
    {
        return m_total;
    }

   /**
    * Returns the number of distinct, consecutive label values that can be counted in this table.
    */
    std::size_t size() const
    {
        return m_data.size();
    }

    /**
     * Calculate the Gini impurity of the dataset, based on the stored label counts.
     * \pre The table may not be empty.
     */
    template <typename FloatType>
    FloatType giniImpurity() const
    {
        assert( m_total > 0 );
        auto squaredCounts = m_data * m_data;
        return FloatType( 1.0 ) - static_cast<FloatType>( squaredCounts.sum() ) / m_total;
    }

  /**
   * Returns the lowest label with the highest count.
   */
  Label getMostFrequentLabel() const
  {
      // Find the lowest label with the highest count.
      std::size_t bestCount = 0;
      Label best = 0;
      for ( Label l = 0; l < m_data.size(); ++l )
      {
          if ( m_data[l] <= bestCount ) continue;
          best = l;
          bestCount = m_data[l];
      }
      return best;
  }

private:

    std::valarray<std::size_t> m_data;
    std::size_t                m_total;
};

/**
 * An axis-aligned division between two sets of points in a multidimensional
 * feature-space.
 */
template <typename FeatureType>
class Split
{
public:

    Split( FeatureID feature = 0, FeatureType value = 0 ):
    m_feature( feature ),
    m_value( value )
    {
    }

    FeatureID getFeatureID() const
    {
        return m_feature;
    }

    FeatureType getFeatureValue() const
    {
        return m_value;
    }

private:

    FeatureID   m_feature;
    FeatureType m_value;
};

#endif // DATATOOLS_H
