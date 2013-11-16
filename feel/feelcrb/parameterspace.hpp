/* -*- mode: c++ -*-

  This file is part of the Feel library

  Author(s): Christophe Prud'homme <christophe.prudhomme@feelpp.org>
       Date: 2009-08-11

  Copyright (C) 2009 Universit� Joseph Fourier (Grenoble I)

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
/**
   \file parameterspace.hpp
   \author Christophe Prud'homme <christophe.prudhomme@feelpp.org>
   \date 2009-08-11
 */
#ifndef __ParameterSpace_H
#define __ParameterSpace_H 1

#include <vector>
#include <algorithm>
#include <boost/lambda/lambda.hpp>
#include <boost/foreach.hpp>

#include <Eigen/Core>

#if defined(FEELPP_HAS_ANN_H)
#include <ANN/ANN.h>
#endif /* FEELPP_HAS_ANN_H */



#include <feel/feelcore/feel.hpp>
#include <feel/feelmesh/kdtree.hpp>

namespace Feel
{
/**
 * \class ParameterSpace
 * \brief Parameter space class
 *
 * @author Christophe Prud'homme
 * @see
 */
template<int P>
class ParameterSpace: public boost::enable_shared_from_this<ParameterSpace<P> >
{
public:


    /** @name Constants
     */
    //@{

    //! dimension of the parameter space
    static const uint16_type Dimension = P;

    //@}

    /** @name Typedefs
     */
    //@{

    typedef ParameterSpace<Dimension> parameterspace_type;
    typedef boost::shared_ptr<parameterspace_type> parameterspace_ptrtype;

    //@}

    /**
     * \brief element of a parameter space
     */
    class Element : public Eigen::Matrix<double,Dimension,1>
    {
        typedef Eigen::Matrix<double,Dimension,1> super;
    public:
        typedef ParameterSpace<Dimension> parameterspace_type;
        typedef boost::shared_ptr<parameterspace_type> parameterspace_ptrtype;

        /**
         * default constructor
         */
        Element()
            :
            super(),
            M_space()
            {}

        /**
         * default constructor
         */
        Element( Element const& e )
        :
        super( e ),
        M_space( e.M_space )
            {}
        /**
         * construct element from \p space
         */
        Element( parameterspace_ptrtype space )
        :
        super(),
        M_space( space )
            {}
        /**
         * destructor
         */
        ~Element()
            {}

        /**
         * copy constructor
         */
        Element& operator=( Element const& e )
            {
                if ( this != &e )
                {
                    super::operator=( e );
                    M_space = e.M_space;
                }

                return *this;
            }

        template<typename OtherDerived>
        super& operator=( const Eigen::MatrixBase<OtherDerived>& other )
            {
                return super::operator=( other );
            }

        void setParameterSpace( parameterspace_ptrtype const& space )
            {
                M_space = space;
            }


        /**
         * \brief Retuns the parameter space
         */
        parameterspace_ptrtype parameterSpace() const
            {
                return M_space;
            }

        void check() const
            {
                if ( !M_space->check() )
                {
                    LOG(INFO) << "No need to check element since parameter space is no valid (yet)\n";
                    return;
                }
                Element sum;
                sum.setZero();
                // verify that the element is the same on all processors
                mpi::all_reduce( Environment::worldComm().localComm(), *this, sum,
                                 []( Element const& m1, Element const& m2 ) { return m1+m2; } );
                int proc_number = Environment::worldComm().globalRank();
                if( (this->array()-sum.array()/Environment::numberOfProcessors()).abs().maxCoeff() > 1e-7 )
                {
                    std::cout << "Parameter not identical on all processors: "<< "current parameter on proc "<<proc_number<<" : [";
                    for(int i=0; i<this->size(); i++) std::cout <<std::setprecision(15)<< this->operator()(i) <<", ";
                    std::cout<<std::setprecision(15)<<this->operator()(this->size()-1)<<" ]";
                    std::cout <<std::setprecision(15)<< " and test" << (this->array()-sum.array()/Environment::numberOfProcessors()).abs().maxCoeff() << "\n";
                }
                Environment::worldComm().barrier();
                CHECK( (this->array()-sum.array()/Environment::numberOfProcessors()).abs().maxCoeff() < 1e-7 )
                    << "Parameter not identical on all processors(" << Environment::worldComm().masterRank() << "/" << Environment::numberOfProcessors() << ")\n"
                    << "max error: " << (this->array()-sum.array()/Environment::numberOfProcessors()).abs().maxCoeff() << "\n"
                    << "error: " << std::setprecision(15) << (this->array()-sum.array()/Environment::numberOfProcessors()).abs() << "\n"
                    << "current parameter " << std::setprecision(15) << *this << "\n"
                    << "sum parameter " << std::setprecision(15) << sum << "\n"
                    << "space min : " << M_space->min() << "\n"
                    << "space max : " << M_space->max() << "\n";
            }
    private:
        friend class boost::serialization::access;
        template<class Archive>
        void save( Archive & ar, const unsigned int version ) const
            {
                ar & boost::serialization::base_object<super>( *this );
                ar & M_space;
            }

        template<class Archive>
        void load( Archive & ar, const unsigned int version )
            {
                ar & boost::serialization::base_object<super>( *this );
                ar & M_space;
            }
        BOOST_SERIALIZATION_SPLIT_MEMBER()

        private:
        parameterspace_ptrtype M_space;
    };

    typedef Element element_type;
    typedef boost::shared_ptr<Element> element_ptrtype;
    element_type element()  { return parameterspace_type::logRandom( this->shared_from_this(), true ); }
    element_ptrtype elementPtr()  { element_ptrtype e( new element_type( this->shared_from_this() ) ); *e=element(); return e; }
    bool check() const
        {
            return (min()-max()).array().abs().maxCoeff() > 1e-10;
        }
    /**
     * \class Sampling
     * \brief Parameter space sampling class
     */
    class Sampling : public std::vector<Element>
    {
        typedef std::vector<Element> super;
    public:

        typedef Sampling sampling_type;
        typedef boost::shared_ptr<sampling_type> sampling_ptrtype;

        typedef ParameterSpace<Dimension> parameterspace_type;
        typedef boost::shared_ptr<parameterspace_type> parameterspace_ptrtype;

        typedef typename parameterspace_type::Element element_type;
        typedef boost::shared_ptr<element_type> element_ptrtype;

#if defined(FEELPP_HAS_ANN_H)
        typedef ANNkd_tree kdtree_type;
        typedef boost::shared_ptr<kdtree_type> kdtree_ptrtype;
#endif /* FEELPP_HAS_ANN_H */


        Sampling( parameterspace_ptrtype space, int N = 1, sampling_ptrtype supersampling = sampling_ptrtype() )
            :
            super( N ),
            M_space( space ),
            M_supersampling( supersampling )
#if defined( FEELPP_HAS_ANN_H )
            ,M_kdtree()
#endif
            {}


        /**
         * \brief create a sampling with elements given by the user
         * \param V : vector of element_type
         */
        void setElements( std::vector< element_type > V )
        {
            CHECK( M_space ) << "Invalid(null pointer) parameter space for parameter generation\n";

            // first empty the set
            this->clear();

            if( Environment::worldComm().globalRank() == Environment::worldComm().masterRank() )
            {
                BOOST_FOREACH( auto mu, V )
                    super::push_back( mu );
            }

            boost::mpi::broadcast( Environment::worldComm() , *this , Environment::worldComm().masterRank() );

        }

        /**
         * \brief create add an element to a sampling
         * \param mu : element_type
         */
        void addElement( element_type const mu )
        {
            CHECK( M_space ) << "Invalid(null pointer) parameter space for parameter generation\n";
            if( Environment::worldComm().globalRank() == Environment::worldComm().masterRank() )
            {
                super::push_back( mu );
            }

            boost::mpi::broadcast( Environment::worldComm() , *this , Environment::worldComm().masterRank() );
        }

        /**
         * \brief create a sampling with random elements
         * \param N the number of samples
         */
        void randomize( int N )
            {
                CHECK( M_space ) << "Invalid(null pointer) parameter space for parameter generation\n";

                // first empty the set
                this->clear();
                //std::srand(static_cast<unsigned>(std::time(0)));

                // fill with log Random elements from the parameter space
                //only with one proc and then broadcast
                if( Environment::worldComm().globalRank() == Environment::worldComm().masterRank() )
                {
                    for ( int i = 0; i < N; ++i )
                    {
                        super::push_back( parameterspace_type::logRandom( M_space, false ) );
                    }

                }

                boost::mpi::broadcast( Environment::worldComm() , *this , Environment::worldComm().masterRank() );

            }

        /**
         * \brief create a sampling with log-equidistributed elements
         * \param N the number of samples
         */
        void logEquidistribute( int N )
            {
                // first empty the set
                this->clear();

                if( Environment::worldComm().globalRank() == Environment::worldComm().masterRank() )
                {
                    // fill with log Random elements from the parameter space
                    for ( int i = 0; i < N; ++i )
                    {
                        double factor = double( i )/( N-1 );
                        super::push_back( parameterspace_type::logEquidistributed( factor,
                                                                                   M_space ) );
                    }
                }
                boost::mpi::broadcast( Environment::worldComm() , *this , Environment::worldComm().masterRank() );
            }

        /**
         * \brief create a sampling with log-equidistributed elements
         * \param N : vector containing the number of samples on each direction
         */
        void logEquidistributeProduct( std::vector<int> N )
        {
            // first empty the set
            this->clear();

            if( Environment::worldComm().globalRank() == Environment::worldComm().masterRank() )
            {
                int number_of_directions = N.size();

                //contains values of parameters on each direction
                std::vector< std::vector< double > > components;
                components.resize( number_of_directions );

                for(int direction=0; direction<number_of_directions; direction++)
                {
                    std::vector<double> coeff_vector = parameterspace_type::logEquidistributeInDirection( M_space , direction , N[direction] );
                    components[direction]= coeff_vector ;
                }

                generateElementsProduct( components );

            }//end of master proc
            boost::mpi::broadcast( Environment::worldComm() , *this , Environment::worldComm().masterRank() );
        }


        /**
         * \brief create a sampling with log-equidistributed and equidistributed elements
         * \param Nlogequi : vector containing the number of log-equidistributed samples on each direction
         * \param Nequi : vector containing the number of equidistributed samples on each direction
         */
        void mixEquiLogEquidistributeProduct( std::vector<int> Nlogequi , std::vector<int> Nequi )
        {
            // first empty the set
            this->clear();

            if( Environment::worldComm().globalRank() == Environment::worldComm().masterRank() )
            {

                int number_of_directions_equi = Nequi.size();
                int number_of_directions_logequi = Nlogequi.size();
                FEELPP_ASSERT( number_of_directions_equi == number_of_directions_logequi )( number_of_directions_equi )( number_of_directions_logequi )
                    .error( "incompatible number of directions, you don't manipulate the same parameter space for log-equidistributed and equidistributed sampling" );

                //contains values of parameters on each direction
                std::vector< std::vector< double > > components_equi;
                components_equi.resize( number_of_directions_equi );
                std::vector< std::vector< double > > components_logequi;
                components_logequi.resize( number_of_directions_logequi );

                for(int direction=0; direction<number_of_directions_equi; direction++)
                {
                    std::vector<double> coeff_vector_equi = parameterspace_type::equidistributeInDirection( M_space , direction , Nequi[direction] );
                    components_equi[direction]= coeff_vector_equi ;
                    std::vector<double> coeff_vector_logequi = parameterspace_type::logEquidistributeInDirection( M_space , direction , Nlogequi[direction] );
                    components_logequi[direction]= coeff_vector_logequi ;
                }
                std::vector< std::vector< std::vector<double> > > vector_components(2);
                vector_components[0]=components_logequi;
                vector_components[1]=components_equi;
                generateElementsProduct( vector_components );

            }//end of master proc
            boost::mpi::broadcast( Environment::worldComm() , *this , Environment::worldComm().masterRank() );
        }


        /*
         * \param a vector of vector containing values of parameters on each direction
         * example if components has 2 vectors : [1,2,3] and [100,200] then it creates the samping :
         * (1,100) - (2,100) - (3,100) - (1,200) - (2,200) - (3,200)
         */
        void generateElementsProduct( std::vector< std::vector< double > > components )
        {
            std::vector< std::vector< std::vector< double > > > vector_components(1);
            vector_components[0]=components;
            generateElementsProduct( vector_components );
        }

        void generateElementsProduct( std::vector< std::vector< std::vector< double > > > vector_components )
        {
            int number_of_comp = vector_components.size();
            for(int comp=0; comp<number_of_comp; comp++)
            {
                int number_of_directions=vector_components[comp].size();
                element_type mu( M_space );

                //initialization
                std::vector<int> idx( number_of_directions );
                for(int direction=0; direction<number_of_directions; direction++)
                    idx[direction]=0;

                int last_direction=number_of_directions-1;

                bool stop=false;
                auto min=M_space->min();
                auto max=M_space->max();

                do
                {
                    bool already_exist = true;
                    //construction of the parameter
                    while( already_exist && !stop )
                    {
                        already_exist=false;
                        for(int direction=0; direction<number_of_directions; direction++)
                        {
                            int index = idx[direction];
                            double value = vector_components[comp][direction][index];
                            mu(direction) = value ;
                        }

                        if( mu == min  && comp>0 )
                            already_exist=true;
                        if( mu == max && comp>0 )
                            already_exist=true;

                        //update values or stop
                        for(int direction=0; direction<number_of_directions; direction++)
                        {
                            if( idx[direction] < vector_components[comp][direction].size()-1 )
                            {
                                idx[direction]++;
                                break;
                            }
                            else
                            {
                                idx[direction]=0;
                                if( direction == last_direction )
                                    stop = true;
                            }
                        }//end loop over directions

                    }//while


                    //add the new parameter
                    super::push_back( mu );

                } while( ! stop );

            }//end loop on vector_component
        }


        /**
         * \brief write the sampling in a file
         * \param file_name : name of the file to read
         * in the file we write :
         * mu_0= [ value0 , value1 , ... ]
         * mu_1= [ value0 , value1 , ... ]
         */
        void writeOnFile( std::string file_name = "list_of_parameters_taken" )
            {
                if( Environment::worldComm().globalRank() == Environment::worldComm().masterRank() )
                {
                    std::ofstream file;
                    file.open( file_name,std::ios::out );
                    element_type mu( M_space );
                    int size = mu.size();
                    int number = 0;
                    BOOST_FOREACH( mu, *this )
                    {
                        file<<std::setprecision(15)<<" mu_"<<number<<"= [ ";
                        for(int i=0; i<size-1; i++)
                            file << mu[i]<<" , ";
                        file<< mu[size-1] << " ] \n" ;
                        number++;
                    }
                    file.close();
                }
            }

        /**
         * \brief read the sampling from a file
         * \param file_name : name of the file to read
         * in the file we expect :
         * mu_0= [ value0 , value1 , ... ]
         * mu_1= [ value0 , value1 , ... ]
         * return the size of the sampling
         */
        double readFromFile( std::string file_name= "list_of_parameters_taken" )
            {
                std::ifstream file ( file_name );
                double mui;
                std::string str;
                int number=0;
                file>>str;
                while( ! file.eof() )
                {
                    element_type mu( M_space );
                    file>>str;
                    int i=0;
                    while ( str!="]" )
                    {
                        file >> mui;
                        mu[i] = mui;
                        file >> str;
                        i++;
                    }
                    super::push_back( mu );
                    number++;
                    file>>str;
                }
                file.close();
                return number;
            }


        /**
         * build the closest sampling with parameters given from the file
         * look in the supersampling closest parameters
         * \param file_name : give the real parameters we want
         * return the index vector of parameters in the supersampling
         */
        std::vector<int> closestSamplingFromFile( std::string file_name="list_of_parameters_taken")
        {
            std::vector<int> index_vector;
            std::ifstream file( file_name );
            double mui;
            std::string str;
            int number=0;
            file>>str;
            while( ! file.eof() )
            {
                element_type mu( M_space );
                file>>str;
                int i=0;
                while ( str!="]" )
                {
                    file >> mui;
                    mu[i] = mui;
                    file >> str;
                    i++;
                }
                //search the closest neighbor of mu in the supersampling
                std::vector<int> local_index_vector;
                auto neighbors = M_supersampling->searchNearestNeighbors( mu, 1 , local_index_vector );
                auto closest_mu = neighbors->at(0);
                int index = local_index_vector[0];
                this->push_back( closest_mu , index );
                number++;
                file>>str;
                index_vector.push_back( index );
            }
            file.close();
            return index_vector;
        }

        /**
         * \brief create a sampling with equidistributed elements
         * \param N the number of samples
         */
        void equidistribute( int N )
            {
                // first empty the set
                this->clear();

                if( Environment::worldComm().globalRank() == Environment::worldComm().masterRank() )
                {
                    // fill with log Random elements from the parameter space
                    for ( int i = 0; i < N; ++i )
                    {
                        double factor = double( i )/( N-1 );
                        super::push_back( parameterspace_type::equidistributed( factor,
                                                                                M_space ) );
                    }
                }
                boost::mpi::broadcast( Environment::worldComm() , *this , Environment::worldComm().masterRank() );
            }

        /**
         * \brief create a sampling with equidistributed elements
         * \param N : vector containing the number of samples on each direction
         */
        void equidistributeProduct( std::vector<int> N )
        {
            // first empty the set
            this->clear();

            if( Environment::worldComm().globalRank() == Environment::worldComm().masterRank() )
            {
                int number_of_directions = N.size();

                //contains values of parameters on each direction
                std::vector< std::vector< double > > components;
                components.resize( number_of_directions );

                for(int direction=0; direction<number_of_directions; direction++)
                {
                    std::vector<double> coeff_vector = parameterspace_type::equidistributeInDirection( M_space , direction , N[direction] );
                    components[direction] = coeff_vector ;
                }

                generateElementsProduct( components );

            }//end of master proc
            boost::mpi::broadcast( Environment::worldComm() , *this , Environment::worldComm().masterRank() );
        }


        /**
         * \brief Returns the minimum element in the sampling and its index
         */
        boost::tuple<element_type,size_type>
        min() const
            {
                element_type mumin( M_space );
                mumin = M_space->max();

                element_type mu( M_space );
                int index = 0;
                int i = 0;
                BOOST_FOREACH( mu, *this )
                {
                    if ( mu.norm() < mumin.norm()  )
                    {
                        mumin = mu;
                        index = i;
                    }

                    ++i;
                }
                mumin.check();
                return boost::make_tuple( mumin, index );
            }

        /**
         * \brief Returns the maximum element in the sampling and its index
         */
        boost::tuple<element_type,size_type>
        max() const
            {
                element_type mumax( M_space );
                mumax = M_space->min();

                element_type mu( M_space );
                int index = 0;
                int i = 0;
                BOOST_FOREACH( mu, *this )
                {
                    if ( mu.norm() > mumax.norm()  )
                    {
                        mumax = mu;
                        index = i;
                    }

                    ++i;
                }
                mumax.check();
                return boost::make_tuple( mumax, index );
            }
        /**
         * \brief Retuns the parameter space
         */
        parameterspace_ptrtype parameterSpace() const
            {
                return M_space;
            }

        /**
         * \breif Returns the supersampling (might be null)
         */
        sampling_ptrtype const& superSampling() const
            {
                return M_supersampling;
            }

        /**
         * set the super sampling
         */
        void setSuperSampling( sampling_ptrtype const& super )
            {
                M_supersampling = super;
            }

        /**
         * \brief Returns the \p M closest points to \p mu in sampling
         * \param mu the point in parameter whom we want to find the neighbors
         * \param M the number of neighbors to find
         * \return vector of neighbors and index of them in the sampling
         */
        sampling_ptrtype searchNearestNeighbors( element_type const& mu, size_type M , std::vector<int>& index_vector  );

        /**
         * \brief if supersampling is != 0, Returns the complement
         */
        sampling_ptrtype complement() const;

        /**
         * \brief add new parameter \p mu in sampling and store \p index in super sampling
         */
        void push_back( element_type const& mu, size_type index )
            {
                if ( M_supersampling ) M_superindices.push_back( index );

                super::push_back( mu );
            }

        /**
         * \brief given a local index, returns the index in the super sampling
         * \param index index in the local sampling
         * \return the index in the super sampling
         */
        size_type indexInSuperSampling( size_type index ) const
            {
                return M_superindices[ index ];
            }

    private:
        Sampling() {}
    private:
        friend class boost::serialization::access;
        template<class Archive>
        void save( Archive & ar, const unsigned int version ) const
            {
                ar & boost::serialization::base_object<super>( *this );
                ar & M_space;
                ar & M_supersampling;
                ar & M_superindices;
            }

        template<class Archive>
        void load( Archive & ar, const unsigned int version )
            {
                ar & boost::serialization::base_object<super>( *this );
                ar & M_space;
                ar & M_supersampling;
                ar & M_superindices;
            }
        BOOST_SERIALIZATION_SPLIT_MEMBER()
        private:
        parameterspace_ptrtype M_space;

        sampling_ptrtype M_supersampling;
        std::vector<size_type> M_superindices;
#if defined( FEELPP_HAS_ANN_H )
        kdtree_ptrtype M_kdtree;
#endif
    };

    typedef Sampling sampling_type;
    typedef boost::shared_ptr<sampling_type> sampling_ptrtype;

    sampling_ptrtype sampling() { return sampling_ptrtype( new sampling_type( this->shared_from_this() ) ); }

    /** @name Constructors, destructor
     */
    //@{

    //! default constructor
    ParameterSpace()
        :
        M_min(),
        M_max()
        {
            M_min.setZero();
            M_max.setZero();
        }
    //! copy constructor
    ParameterSpace( ParameterSpace const & o )
    :
    M_min( o.M_min ),
    M_max( o.M_max )
        {}

    ParameterSpace( element_type const& emin, element_type const& emax )
        :
        M_min( emin ),
        M_max( emax )
        {
            M_min.setParameterSpace( this->shared_from_this() );
            M_max.setParameterSpace( this->shared_from_this() );
        }
    //! destructor
    ~ParameterSpace()
        {}

    /**
     * generate a shared_ptr out of a parameter space
     */
    static parameterspace_ptrtype New()
        {
            return parameterspace_ptrtype(new parameterspace_type);
        }
    //@}

    /** @name Operator overloads
     */
    //@{

    //! copy operator
    ParameterSpace& operator=( ParameterSpace const & o )
        {
            if ( this != &o )
            {
                M_min = o.M_min;
                M_max = o.M_max;
            }

            return *this;
        }
    //@}

    /** @name Accessors
     */
    //@{

    //! \return the parameter space dimension
    int dimension() const
        {
            return Dimension;
        }

    /**
     * return the minimum element
     */
    element_type const& min() const
        {
            return M_min;
        }

    /**
     * return the maximum element
     */
    element_type const& max() const
        {
            return M_max;
        }

    /**
     * \brief the log-middle point of the parameter space
     */
    element_type logMiddle() const
        {
            return ( ( M_min.array().log() + M_max.array().log() )/2. ).log();
        }

    /**
     * \brief the middle point of the parameter space
     */
    element_type middle() const
        {
            return ( ( M_min + M_max )/2. );
        }

    //@}

    /** @name  Mutators
     */
    //@{

    /**
     * set the minimum element
     */
    void setMin( element_type const& min )
        {
            M_min = min;
        }

    /**
     * set the maximum element
     */
    void setMax( element_type const& max )
        {
            M_max = max;
        }

    //@}

    /** @name  Methods
     */
    //@{

    /**
     * \brief Returns a log random element of the parameter space
     */
    static element_type logRandom( parameterspace_ptrtype space, bool broadcast )
        {
            //LOG(INFO) << "call logRandom...\n";
            //LOG(INFO) << "call logRandom broadcast: " << broadcast << "...\n";
            google::FlushLogFiles(google::GLOG_INFO);
            if ( broadcast )
            {
                element_type mu( space );
                if ( !space->check() ) return mu;
                if( Environment::worldComm().globalRank() == Environment::worldComm().masterRank() )
                {
                    //LOG(INFO) << "generate random mu...\n";
                    //google::FlushLogFiles(google::GLOG_INFO);
                    mu = logRandom1( space );
                }
                //LOG(INFO) << "broadcast...\n";
                //google::FlushLogFiles(google::GLOG_INFO);
                boost::mpi::broadcast( Environment::worldComm() , mu , Environment::worldComm().masterRank() );
                Environment::worldComm().barrier();
                //LOG(INFO) << "check...\n";
                mu.check();
                return mu;
            }
            else
            {
                return logRandom1( space );
            }
        }
    static element_type logRandom1( parameterspace_ptrtype space )
        {
            element_type mur( space );
            mur.array() = element_type::Random().array().abs();
            //LOG(INFO) << "random1 generate random mur= " << mur << " \n";
            google::FlushLogFiles(google::GLOG_INFO);
            element_type mu( space );
            mu.array() = ( space->min().array().log()+mur.array()*( space->max().array().log()-space->min().array().log() ) ).array().exp();
            //LOG(INFO) << "random1 generate random mu= " << mu << " \n";
            return mu;
        }

    /**
     * \brief Returns a log random element of the parameter space
     */
    static element_type random( parameterspace_ptrtype space, bool broadcast = true )
        {
            std::srand( static_cast<unsigned>( std::time( 0 ) ) );
            element_type mur( space );
            if ( broadcast )
            {
                if( Environment::worldComm().globalRank() == Environment::worldComm().masterRank() )
                {
                    mur.array() = element_type::Random().array().abs();
                }
                boost::mpi::broadcast( Environment::worldComm() , mur , Environment::worldComm().masterRank() );

            }
            else
                mur.array() = element_type::Random().array().abs();
            //std::cout << "mur= " << mur << "\n";
            //mur.setRandom()/RAND_MAX;
            //std::cout << "mur= " << mur << "\n";
            element_type mu( space );
            mu.array() = space->min()+mur.array()*( space->max()-space->min() );
            if ( broadcast )
                mu.check();
            return mu;
        }

    /**
     * \brief Returns a vector representing the values of log equidistributed element of the parameter space
     * in the given direction
     * \param direction
     * \param N : number of samples in the direction
     */
    static std::vector<double> logEquidistributeInDirection( parameterspace_ptrtype space, int direction , int N)
    {
        std::vector<double> result(N);

        auto min_element = space->min();
        auto max_element = space->max();
        int space_dimension=space->dimension();
        FEELPP_ASSERT( space_dimension > direction )( space_dimension )( direction ).error( "bad dimension of vector containing number of samples on each direction" );
        FEELPP_ASSERT( direction >= 0 )( direction ).error( "the direction must be positive number" );

        double min = min_element(direction);
        double max = max_element(direction);

        for(int i=0; i<N; i++)
        {
            double factor = (double)(i)/(N-1);
            result[i] = math::exp(math::log(min)+factor*( math::log(max)-math::log(min) ) );
        }

        return result;
    }


    /**
     * \brief Returns a log equidistributed element of the parameter space
     * \param factor is a factor in [0,1]
     */
    static element_type logEquidistributed( double factor, parameterspace_ptrtype space )
        {
            element_type mu( space );
            mu.array() = ( space->min().array().log()+factor*( space->max().array().log()-space->min().array().log() ) ).exp();
            return mu;
        }


    /**
     * \brief Returns a vector representing the values of equidistributed element of the parameter space
     * in the given direction
     * \param direction
     * \param N : number of samples in the direction
     */
    static std::vector<double> equidistributeInDirection( parameterspace_ptrtype space, int direction , int N)
    {
        std::vector<double> result(N);

        auto min_element = space->min();
        auto max_element = space->max();

        int mu_size = min_element.size();
        if( (mu_size < direction) || (direction < 0)  )
            throw std::logic_error( "[ParameterSpace::equidistributeInDirection] ERROR : bad dimension of vector containing number of samples on each direction" );

        double min = min_element(direction);
        double max = max_element(direction);

        for(int i=0; i<N; i++)
        {
            double factor = (double)(i)/(N-1);
            result[i] = min+factor*( max-min );
        }

        return result;
    }


    /**
     * \brief Returns a equidistributed element of the parameter space
     * \param factor is a factor in [0,1]
     */
    static element_type equidistributed( double factor, parameterspace_ptrtype space )
        {
            element_type mu( space );
            mu = space->min()+factor*( space->max()-space->min() );
            return mu;
        }


    //@}



protected:

private:
    friend class boost::serialization::access;
    template<class Archive>
    void save( Archive & ar, const unsigned int version ) const
        {
            ar & M_min;
            ar & M_max;
        }

    template<class Archive>
    void load( Archive & ar, const unsigned int version )
        {
            ar & M_min;
            ar & M_max;
        }
    BOOST_SERIALIZATION_SPLIT_MEMBER()

    private:

    //! min element
    element_type M_min;

    //! max element
    element_type M_max;
};

template<int P> const uint16_type ParameterSpace<P>::Dimension;

template<int P>
//typename ParameterSpace<P>::sampling_ptrtype
boost::shared_ptr<typename ParameterSpace<P>::Sampling>
ParameterSpace<P>::Sampling::complement() const
{
    //std::cout << "[ParameterSpace::Sampling::complement] start\n";
    sampling_ptrtype complement;

    if ( M_supersampling )
    {
        //  std::cout << "[ParameterSpace::Sampling::complement] super sampling available\n";
        //  std::cout << "[ParameterSpace::Sampling::complement] this->size = " << this->size() << std::endl;
        //  std::cout << "[ParameterSpace::Sampling::complement] supersampling->size = " << M_supersampling->size() << std::endl;
        //  std::cout << "[ParameterSpace::Sampling::complement] complement->size = " << M_supersampling->size()-this->size() << std::endl;
        complement = sampling_ptrtype( new sampling_type( M_space, 1, M_supersampling ) );
        complement->clear();

        if ( !M_superindices.empty() )
        {
            for ( size_type i = 0, j = 0; i < M_supersampling->size(); ++i )
            {
                if ( std::find( M_superindices.begin(), M_superindices.end(), i ) == M_superindices.end() )
                {
                    //std::cout << "inserting " << j << "/" << i << " in complement of size (" << complement->size() << " out of " << M_supersampling->size() << std::endl;
                    if ( j < M_supersampling->size()-this->size() )
                    {
                        //complement->at( j ) = M_supersampling->at( i );
                        complement->push_back( M_supersampling->at( i ),i );
                        ++j;
                    }
                }
            }
        }

        return complement;
    }

    return complement;
}
template<int P>
boost::shared_ptr<typename ParameterSpace<P>::Sampling>
ParameterSpace<P>::Sampling::searchNearestNeighbors( element_type const& mu,
                                                     size_type _M ,
                                                     std::vector<int>& index_vector)
{
    size_type M=_M;
    sampling_ptrtype neighbors( new sampling_type( M_space, M ) );
#if defined(FEELPP_HAS_ANN_H)

    if( Environment::worldComm().globalRank() == Environment::worldComm().masterRank() )
    {
        //std::cout << "[ParameterSpace::Sampling::searchNearestNeighbors] start\n";
        //if ( !M_kdtree )
        //{
        //std::cout << "[ParameterSpace::Sampling::searchNearestNeighbors] building data points\n";
        ANNpointArray data_pts;
        data_pts = annAllocPts( this->size(), M_space->Dimension );

        for ( size_type i = 0; i < this->size(); ++i )
        {
            std::copy( this->at( i ).data(), this->at( i ).data()+M_space->Dimension, data_pts[i] );
            FEELPP_ASSERT( data_pts[i] != 0 )( i ) .error( "invalid pointer" );
        }

        //std::cout << "[ParameterSpace::Sampling::searchNearestNeighbors] building tree in R^" <<  M_space->Dimension << "\n";
        M_kdtree = kdtree_ptrtype( new kdtree_type( data_pts, this->size(), M_space->Dimension ) );
        //}


        // make sure that the sampling set is big enough
        if ( this->size() < M )
            M=this->size();

        std::vector<int> nnIdx( M );
        std::vector<double> dists( M );
        double eps = 0;
        M_kdtree->annkSearch( // search
                             const_cast<double*>( mu.data() ), // query point
                             M,       // number of near neighbors
                             nnIdx.data(),   // nearest neighbors (returned)
                             dists.data(),   // distance (returned)
                             eps );   // error bound



        if ( M_supersampling )
        {
            neighbors->setSuperSampling( M_supersampling );

            if ( !M_superindices.empty() )
                neighbors->M_superindices.resize( M );
        }

        //std::cout << "[parameterspace::sampling::searchNearestNeighbors] neighbor size = " << neighbors->size() <<std::endl;
        for ( size_type i = 0; i < M; ++i )
        {
            //std::cout << "[parameterspace::sampling::searchNearestNeighbors] neighbor: " <<i << " distance = " << dists[i] << "\n";
            neighbors->at( i ) = this->at( nnIdx[i] );
            index_vector.push_back( nnIdx[i] );

            //std::cout<<"[parameterspace::sampling::searchNearestNeighbors] M_superindices.size() : "<<M_superindices.size()<<std::endl;
            if ( M_supersampling && !M_superindices.empty() )
                neighbors->M_superindices[i] = M_superindices[ nnIdx[i] ];
            //std::cout << "[parameterspace::sampling::searchNearestNeighbors] " << neighbors->at( i ) << "\n";
        }
        annDeallocPts( data_pts );
    }//end of procMaster

    boost::mpi::broadcast( Environment::worldComm() , neighbors , Environment::worldComm().masterRank() );
    boost::mpi::broadcast( Environment::worldComm() , index_vector , Environment::worldComm().masterRank() );


#endif /* FEELPP_HAS_ANN_H */

    return neighbors;
}
} // Feel
#endif /* __ParameterSpace_H */
