/* -*- mode: c++ -*-

  This file is part of the Feel library

  Author(s): Christophe Prud'homme <christophe.prudhomme@feelpp.org>
       Date: 2009-11-24

  Copyright (C) 2009-2012 Universit� Joseph Fourier (Grenoble I)

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
   \file crb.hpp
   \author Christophe Prud'homme <christophe.prudhomme@feelpp.org>
   \date 2009-11-24
 */
#ifndef __CRB_H
#define __CRB_H 1


#include <boost/multi_array.hpp>
#include <boost/tuple/tuple.hpp>
#include "boost/tuple/tuple_io.hpp"
#include <boost/format.hpp>
#include <boost/foreach.hpp>
#include <boost/bimap.hpp>
#include <boost/bimap/support/lambda.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/math/special_functions/fpclassify.hpp>
#include <fstream>


#include <boost/serialization/vector.hpp>
#include <boost/serialization/list.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/version.hpp>
#include <boost/serialization/split_member.hpp>

#include <vector>

#include <Eigen/Core>
#include <Eigen/LU>
#include <Eigen/Dense>

#include <feel/feelalg/solvereigen.hpp>
#include <feel/feelcore/feel.hpp>
#include <feel/feelcore/environment.hpp>
#include <feel/feelcore/parameter.hpp>
#include <feel/feelcrb/parameterspace.hpp>
#include <feel/feelcrb/crbdb.hpp>
#include <feel/feelcrb/crbscm.hpp>
#include <feel/feelcore/serialization.hpp>
#include <feel/feelcrb/pod.hpp>
#include <feel/feeldiscr/bdf2.hpp>
#include <feel/feelfilters/exporter.hpp>

#include <feel/feelcrb/crbelementsdb.hpp>


namespace Feel
{
/**
 * CRBErrorType
 * Determine the type of error estimation used
 * - CRB_RESIDUAL : use the residual error estimation without algorithm SCM
 * - CRB_RESIDUAL_SCM : use the residual error estimation and also algorithm SCM
 * - CRB_NO_RESIDUAL : in this case we don't compute error estimation
 * - CRB_EMPIRICAL : compute |S_n - S_{n-1}| where S_n is the output obtained by using a reduced basis with n elements
 */
enum CRBErrorType { CRB_RESIDUAL = 0, CRB_RESIDUAL_SCM=1, CRB_NO_RESIDUAL=2 , CRB_EMPIRICAL=3};

/**
 * \class CRB
 * \brief Certifed Reduced Basis class
 *
 * Implements the certified reduced basis method
 *
 * @author Christophe Prud'homme
 * @see
 */
template<typename TruthModelType>
class CRB : public CRBDB
{
    typedef  CRBDB super;
public:


    /** @name Constants
     */
    //@{


    //@}

    /** @name Typedefs
     */
    //@{

    typedef TruthModelType truth_model_type;
    typedef truth_model_type model_type;
    typedef boost::shared_ptr<truth_model_type> truth_model_ptrtype;

    typedef double value_type;
    typedef boost::tuple<double,double> bounds_type;

    typedef ParameterSpace<TruthModelType::ParameterSpaceDimension> parameterspace_type;
    typedef boost::shared_ptr<parameterspace_type> parameterspace_ptrtype;
    typedef typename parameterspace_type::element_type parameter_type;
    typedef typename parameterspace_type::element_ptrtype parameter_ptrtype;
    typedef typename parameterspace_type::sampling_type sampling_type;
    typedef typename parameterspace_type::sampling_ptrtype sampling_ptrtype;

    typedef boost::tuple<double, parameter_type, size_type, double, double> relative_error_type;
    typedef relative_error_type max_error_type;

    typedef boost::tuple<double, std::vector< std::vector<double> > , std::vector< std::vector<double> >, double, double > error_estimation_type;
    typedef boost::tuple<double, std::vector<double> > residual_error_type;

    typedef boost::bimap< int, boost::tuple<double,double,double> > convergence_type;

    typedef typename convergence_type::value_type convergence;

    typedef CRB self_type;

    //! scm
    typedef CRBSCM<truth_model_type> scm_type;
    typedef boost::shared_ptr<scm_type> scm_ptrtype;

    //! elements database
    typedef CRBElementsDB<truth_model_type> crb_elements_db_type;
    typedef boost::shared_ptr<crb_elements_db_type> crb_elements_db_ptrtype;

    //! POD
    typedef POD<truth_model_type> pod_type;
    typedef boost::shared_ptr<pod_type> pod_ptrtype;

    //! function space type
    typedef typename model_type::functionspace_type functionspace_type;
    typedef typename model_type::functionspace_ptrtype functionspace_ptrtype;


    //! element of the functionspace type
    typedef typename model_type::element_type element_type;
    typedef typename model_type::element_ptrtype element_ptrtype;

    typedef typename model_type::backend_type backend_type;
    typedef boost::shared_ptr<backend_type> backend_ptrtype;
    typedef typename model_type::sparse_matrix_ptrtype sparse_matrix_ptrtype;
    typedef typename model_type::vector_ptrtype vector_ptrtype;
    typedef typename model_type::beta_vector_type beta_vector_type;


    typedef Eigen::VectorXd y_type;
    typedef std::vector<y_type> y_set_type;
    typedef std::vector<boost::tuple<double,double> > y_bounds_type;

    typedef std::vector<element_type> wn_type;
    typedef boost::tuple< std::vector<wn_type> , std::vector<std::string> > export_vector_wn_type;

    typedef std::vector<double> vector_double_type;
    typedef boost::shared_ptr<vector_double_type> vector_double_ptrtype;

    typedef Eigen::VectorXd vectorN_type;
    typedef Eigen::MatrixXd matrixN_type;

    typedef Eigen::Map< Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic> > map_dense_matrix_type;
    typedef Eigen::Map< Eigen::Matrix<double, Eigen::Dynamic, 1> > map_dense_vector_type;

    typedef boost::tuple< vectorN_type , vectorN_type > solutions_tuple;

    typedef std::vector<element_type> mode_set_type;
    typedef boost::shared_ptr<mode_set_type> mode_set_ptrtype;

    typedef boost::multi_array<value_type, 2> array_2_type;
    typedef boost::multi_array<vectorN_type, 2> array_3_type;
    typedef boost::multi_array<matrixN_type, 2> array_4_type;

    //! mesh type
    typedef typename model_type::mesh_type mesh_type;
    typedef boost::shared_ptr<mesh_type> mesh_ptrtype;

    //! space type
    typedef typename model_type::space_type space_type;
    typedef boost::shared_ptr<space_type> space_ptrtype;

    //! time discretization
    typedef Bdf<space_type>  bdf_type;
    typedef boost::shared_ptr<bdf_type> bdf_ptrtype;

    // ! export
    typedef Exporter<mesh_type> export_type;
    typedef boost::shared_ptr<export_type> export_ptrtype;

    typedef Preconditioner<double> preconditioner_type;
    typedef boost::shared_ptr<preconditioner_type> preconditioner_ptrtype;

    //here a fusion vector containing sequence 0 ... nb_spaces
    //useful to acces to a component of a composite space in ComputeIntegrals
    static const int nb_spaces = functionspace_type::nSpaces;
    typedef typename mpl::if_< boost::is_same< mpl::int_<nb_spaces> , mpl::int_<2> > , fusion::vector< mpl::int_<0>, mpl::int_<1> >  ,
                       typename mpl::if_ < boost::is_same< mpl::int_<nb_spaces> , mpl::int_<3> > , fusion::vector < mpl::int_<0> , mpl::int_<1> , mpl::int_<2> > ,
                                  typename mpl::if_< boost::is_same< mpl::int_<nb_spaces> , mpl::int_<4> >, fusion::vector< mpl::int_<0>, mpl::int_<1>, mpl::int_<2>, mpl::int_<3> >,
                                                     fusion::vector< mpl::int_<0>, mpl::int_<1>, mpl::int_<2>, mpl::int_<3>, mpl::int_<4> >
                                                     >::type >::type >::type index_vector_type;


    //@}

    /** @name Constructors, destructor
     */
    //@{

    //! default constructor
    CRB()
        :
        super(),
        M_elements_database(),
        M_nlsolver( SolverNonLinear<double>::build( SOLVERS_PETSC, Environment::worldComm() ) ),
        M_model(),
        M_output_index( 0 ),
        M_tolerance( 1e-2 ),
        M_iter_max( 3 ),
        M_factor( -1 ),
        M_error_type( CRB_NO_RESIDUAL ),
        M_Dmu( new parameterspace_type ),
        M_Xi( new sampling_type( M_Dmu ) ),
        M_WNmu( new sampling_type( M_Dmu, 1, M_Xi ) ),
        M_WNmu_complement(),
        exporter( Exporter<mesh_type>::New( "ensight" ) )
    {

    }

    //! constructor from command line options
    CRB( std::string  name,
         po::variables_map const& vm )
        :
        super( ( boost::format( "%1%" ) % vm["crb.error-type"].template as<int>() ).str(),
               name,
               ( boost::format( "%1%-%2%-%3%" ) % name % vm["crb.output-index"].template as<int>() % vm["crb.error-type"].template as<int>() ).str(),
               vm ),
        M_elements_database(
                            ( boost::format( "%1%" ) % vm["crb.error-type"].template as<int>() ).str(),
                            name,
                            ( boost::format( "%1%-%2%-%3%-elements" ) % name % vm["crb.output-index"].template as<int>() % vm["crb.error-type"].template as<int>() ).str(),
                            vm ),
        M_nlsolver( SolverNonLinear<double>::build( SOLVERS_PETSC, Environment::worldComm() ) ),
        M_model(),
        M_backend( backend_type::build( vm ) ),
        M_backend_primal( backend_type::build( vm ) ),
        M_backend_dual( backend_type::build( vm ) ),
        M_output_index( vm["crb.output-index"].template as<int>() ),
        M_tolerance( vm["crb.error-max"].template as<double>() ),
        M_iter_max( vm["crb.dimension-max"].template as<int>() ),
        M_factor( vm["crb.factor"].template as<int>() ),
        M_error_type( CRBErrorType( vm["crb.error-type"].template as<int>() ) ),
        M_Dmu( new parameterspace_type ),
        M_Xi( new sampling_type( M_Dmu ) ),
        M_WNmu( new sampling_type( M_Dmu, 1, M_Xi ) ),
        M_WNmu_complement(),
        M_scmA( new scm_type( name+"_a", vm ) ),
        M_scmM( new scm_type( name+"_m", vm ) ),
        exporter( Exporter<mesh_type>::New( vm, "BasisFunction" ) )
    {
        // this is too early to load the DB, we don't have the model yet and the
        //associated function space for the reduced basis function if
        // do the loadDB() in setTruthModel()
        //this->loadDB() )

    }

    //! constructor from command line options
    CRB( std::string  name,
         po::variables_map const& vm,
         truth_model_ptrtype const & model )
        :
        super( ( boost::format( "%1%" ) % vm["crb.error-type"].template as<int>() ).str(),
               name,
               ( boost::format( "%1%-%2%-%3%" ) % name % vm["crb.output-index"].template as<int>() % vm["crb.error-type"].template as<int>() ).str(),
               vm ),
        M_elements_database(
                            ( boost::format( "%1%" ) % vm["crb.error-type"].template as<int>() ).str(),
                            name,
                            ( boost::format( "%1%-%2%-%3%-elements" ) % name % vm["crb.output-index"].template as<int>() % vm["crb.error-type"].template as<int>() ).str(),
                            vm ,
                            model ),
        M_nlsolver( SolverNonLinear<double>::build( SOLVERS_PETSC, Environment::worldComm() ) ),
        M_model(),
        M_backend( backend_type::build( vm ) ),
        M_backend_primal( backend_type::build( vm ) ),
        M_backend_dual( backend_type::build( vm ) ),
        M_output_index( vm["crb.output-index"].template as<int>() ),
        M_tolerance( vm["crb.error-max"].template as<double>() ),
        M_iter_max( vm["crb.dimension-max"].template as<int>() ),
        M_factor( vm["crb.factor"].template as<int>() ),
        M_error_type( CRBErrorType( vm["crb.error-type"].template as<int>() ) ),
        M_Dmu( new parameterspace_type ),
        M_Xi( new sampling_type( M_Dmu ) ),
        M_WNmu( new sampling_type( M_Dmu, 1, M_Xi ) ),
        M_WNmu_complement(),
        M_scmA( new scm_type( name+"_a", vm , model , false /*not scm for mass mastrix*/ )  ),
        M_scmM( new scm_type( name+"_m", vm , model , true /*scm for mass matrix*/ ) ),
        exporter( Exporter<mesh_type>::New( vm, "BasisFunction" ) )
    {
        this->setTruthModel( model );
        if ( this->loadDB() )
            LOG(INFO) << "Database " << this->lookForDB() << " available and loaded\n";
        M_elements_database.setMN( M_N );
        if( M_elements_database.loadDB() )
            LOG(INFO) << "Element Database " << M_elements_database.lookForDB() << " available and loaded\n";
        auto basis_functions = M_elements_database.wn();
        M_WN = basis_functions.template get<0>();
        M_WNdu = basis_functions.template get<1>();

        M_preconditioner_primal = preconditioner(_pc=(PreconditionerType) M_backend_primal->pcEnumType(), // by default : lu in seq or wirh mumps, else gasm in parallel
                                                 _backend= M_backend_primal,
                                                 _pcfactormatsolverpackage=(MatSolverPackageType) M_backend_primal->matSolverPackageEnumType(),// mumps if is installed ( by defaut )
                                                 _worldcomm=M_backend_primal->comm(),
                                                 _prefix=M_backend_primal->prefix() ,
                                                 _rebuild=true);
        M_preconditioner_dual = preconditioner(_pc=(PreconditionerType) M_backend_dual->pcEnumType(), // by default : lu in seq or wirh mumps, else gasm in parallel
                                               _backend= M_backend_dual,
                                               _pcfactormatsolverpackage=(MatSolverPackageType) M_backend_dual->matSolverPackageEnumType(),// mumps if is installed ( by defaut )
                                               _worldcomm=M_backend_dual->comm(),
                                               _prefix=M_backend_dual->prefix() ,
                                               _rebuild=true);
    }


    //! copy constructor
    CRB( CRB const & o )
        :
        super( o ),
        M_elements_database( o.M_elements_database ),
        M_nlsolver( o.M_nlsolver ),
        M_output_index( o.M_output_index ),
        M_tolerance( o.M_tolerance ),
        M_iter_max( o.M_iter_max ),
        M_factor( o.M_factor ),
        M_error_type( o.M_error_type ),
        M_maxerror( o.M_maxerror ),
        M_Dmu( o.M_Dmu ),
        M_Xi( o.M_Xi ),
        M_WNmu( o.M_WNmu ),
        M_WNmu_complement( o.M_WNmu_complement ),
        M_C0_pr( o.M_C0_pr ),
        M_C0_du( o.M_C0_du ),
        M_Lambda_pr( o.M_Lambda_pr ),
        M_Lambda_du( o.M_Lambda_du ),
        M_Gamma_pr( o.M_Gamma_pr ),
        M_Gamma_du( o.M_Gamma_du ),
        M_Cmf_pr( o.M_Cmf_pr ),
        M_Cmf_du( o.M_Cmf_du ),
        M_Cmf_du_ini( o.M_Cmf_du_ini ),
        M_Cma_pr( o.M_Cma_pr ),
        M_Cma_du( o.M_Cma_du ),
        M_Cmm_pr( o.M_Cmm_pr ),
        M_Cmm_du( o.M_Cmm_du ),
        M_coeff_pr_ini_online( o.M_coeff_pr_ini_online ),
        M_coeff_du_ini_online( o.M_coeff_du_ini_online )
    {}

    //! destructor
    ~CRB()
    {}


    //@}

    /** @name Operator overloads
     */
    //@{

    //! copy operator
    CRB& operator=( CRB const & o )
    {
        if ( this != &o )
        {
        }

        return *this;
    }
    //@}

    /** @name Accessors
     */
    //@{


    //! return factor
    int factor() const
    {
        return factor;
    }
    //! \return max iterations
    int maxIter() const
    {
        return M_iter_max;
    }

    //! \return the parameter space
    parameterspace_ptrtype Dmu() const
    {
        return M_Dmu;
    }

    //! \return the output index
    int outputIndex() const
    {
        return M_output_index;
    }

    //! \return the dimension of the reduced basis space
    int dimension() const
    {
        return M_N;
    }

    //! \return the train sampling used to generate the reduced basis space
    sampling_ptrtype trainSampling() const
    {
        return M_Xi;
    }

    //! \return the error type
    CRBErrorType errorType() const
    {
        return M_error_type;
    }

    //! \return the scm object
    scm_ptrtype scm() const
    {
        return M_scmA;
    }


    //@}

    /** @name  Mutators
     */
    //@{

    //! set the output index
    void setOutputIndex( uint16_type oindex )
    {
        int M_prev_o = M_output_index;
        M_output_index = oindex;

        if ( ( size_type )M_output_index >= M_model->Nl()  )
            M_output_index = M_prev_o;

        //std::cout << " -- crb set output index to " << M_output_index << " (max output = " << M_model->Nl() << ")\n";
        this->setDBFilename( ( boost::format( "%1%-%2%-%3%.crbdb" ) % this->name() % M_output_index % M_error_type ).str() );

        if ( M_output_index != M_prev_o )
            this->loadDB();

        //std::cout << "Database " << this->lookForDB() << " available and loaded\n";

    }

    //! set the crb error type
    void setCRBErrorType( CRBErrorType error )
    {
        M_error_type = error;
    }

    //! set offline tolerance
    void setTolerance( double tolerance )
    {
        M_tolerance = tolerance;
    }

    //! set the truth offline model
    void setTruthModel( truth_model_ptrtype const& model )
    {
        M_model = model;
        M_Dmu = M_model->parameterSpace();
        M_Xi = sampling_ptrtype( new sampling_type( M_Dmu ) );

        if ( ! loadDB() )
            M_WNmu = sampling_ptrtype( new sampling_type( M_Dmu ) );
        else
        {
            LOG(INFO) << "Database " << this->lookForDB() << " available and loaded\n";
        }

        //M_scmA->setTruthModel( M_model );
        //M_scmM->setTruthModel( M_model );
    }

    //! set max iteration number
    void setMaxIter( int K )
    {
        M_iter_max = K;
    }

    //! set factor
    void setFactor( int Factor )
    {
        M_factor = Factor;
    }

    //! set boolean indicates if we are in offline_step or not
    void setOfflineStep( bool b )
    {
        M_offline_step = b;
    }

    struct ComputePhi
    {

        ComputePhi( wn_type v)
        :
        M_curs( 0 ),
        M_vect( v )
        {}

        template< typename T >
        void
        operator()( const T& t ) const
        {
            mesh_ptrtype mesh = t.functionSpace()->mesh();
            double surface = integrate( _range=elements(mesh), _expr=vf::cst(1.) ).evaluate()(0,0);
            double mean = integrate( _range=elements(mesh), _expr=vf::idv( t ) ).evaluate()(0,0);
            mean /= surface;
            auto element_mean = vf::project(t.functionSpace(), elements(mesh), vf::cst(mean) );
            auto element_t = vf::project(t.functionSpace(), elements(mesh), vf::idv(t) );
            int first_dof = t.start();
            int nb_dof = t.functionSpace()->nLocalDof();
            for(int dof=0 ; dof<nb_dof; dof++)
                M_vect[M_curs][first_dof + dof] = element_t[dof] - element_mean[dof];

            ++M_curs;
        }

        wn_type vectorPhi()
        {
            return M_vect;
        }
        mutable int M_curs;
        mutable wn_type M_vect;
    };


    struct ComputeIntegrals
    {

        ComputeIntegrals( element_type const composite_e1 ,
                          element_type const composite_e2 )
        :
            M_composite_e1 ( composite_e1 ),
            M_composite_e2 ( composite_e2 )
        {}

        template< typename T >
        void
        operator()( const T& t ) const
        {
            auto e1 = M_composite_e1.template element< T::value >();
            auto e2 = M_composite_e2.template element< T::value >();
            mesh_ptrtype mesh = e1.functionSpace()->mesh();
            double integral = integrate( _range=elements(mesh) , _expr=trans( vf::idv( e1 ) ) * vf::idv( e2 ) ).evaluate()(0,0);

            M_vect.push_back( integral );
        }

        std::vector< double > vectorIntegrals()
        {
            return M_vect;
        }

        mutable std::vector< double > M_vect;
        element_type M_composite_e1;
        element_type M_composite_e2;
    };

    struct ComputeIntegralsSquare
    {

        ComputeIntegralsSquare( element_type const composite_e1 ,
                                element_type const composite_e2 )
        :
            M_composite_e1 ( composite_e1 ),
            M_composite_e2 ( composite_e2 )
        {}

        template< typename T >
        void
        operator()( const T& t ) const
        {
            int i = T::value;
            if( i == 0 )
                M_error.resize( 1 );
            else
                M_error.conservativeResize( i+1 );

            auto e1 = M_composite_e1.template element< T::value >();
            auto e2 = M_composite_e2.template element< T::value >();

            auto Xh = M_composite_e1.functionSpace();
            mesh_ptrtype mesh = Xh->mesh();
            double integral = integrate( _range=elements(mesh) ,
                                         _expr=trans( vf::idv( e1 ) - vf::idv( e2 ) )
                                         *     ( vf::idv( e1 ) - vf::idv( e2 ) )
                                         ).evaluate()(0,0);
            M_error(i) = math::sqrt( integral ) ;
        }

        vectorN_type vectorErrors()
        {
            return M_error;
        }

        mutable vectorN_type M_error;
        element_type M_composite_e1;
        element_type M_composite_e2;
    };

    //@}

    /** @name  Methods
     */
    //@{

    /**
     * orthonormalize the basis
     */
    void orthonormalize( size_type N, wn_type& wn, int Nm = 1 );

    void checkResidual( parameter_type const& mu, std::vector< std::vector<double> > const& primal_residual_coeffs, std::vector< std::vector<double> > const& dual_residual_coeffs ) const;

    void compareResidualsForTransientProblems(int N, parameter_type const& mu, std::vector<element_ptrtype> const & Un, std::vector<element_ptrtype> const & Unold, std::vector<element_ptrtype> const& Undu, std::vector<element_ptrtype> const & Unduold, std::vector< std::vector<double> > const& primal_residual_coeffs,  std::vector< std::vector<double> > const& dual_residual_coeffs  ) const ;


    void buildFunctionFromRbCoefficients(int N, std::vector< vectorN_type > const & RBcoeff, wn_type const & WN, std::vector<element_ptrtype> & FEMsolutions );

    /*
     * check orthonormality
     */
    //void checkOrthonormality( int N, const wn_type& wn) const;
    void checkOrthonormality( int N, const wn_type& wn ) const;

    /**
     * check the reduced basis space invariant properties
     * \param N dimension of \f$W_N\f$
     */
    void check( size_type N )  const;

    /**
     * compute effectivity indicator of the error estimation overall a given
     * parameter space
     *
     * \param max_ei : maximum efficiency indicator (output)
     * \param min_ei : minimum efficiency indicator (output)
     * \param Dmu (input) parameter space
     * \param N : sampling size (optional input with default value)
     */
    void computeErrorEstimationEfficiencyIndicator ( parameterspace_ptrtype const& Dmu, double& max_ei, double& min_ei,int N = 4 );


    /**
     * export basis functions to visualize it
     * \param wn : tuple composed of a vector of wn_type and a vector of string (used to name basis)
     */
    void exportBasisFunctions( const export_vector_wn_type& wn )const ;

    /**
     * Returns the lower bound of the output
     *
     * \param mu \f$ \mu\f$ the parameter at which to evaluate the output
     * \param N the size of the reduced basis space to use
     * \param uN primal solution
     * \param uNdu dual solution
     * \param K : index of time ( time = K*dt) at which we want to evaluate the output
     * Note : K as a default value for non time-dependent problems
     *
     *\return compute online the lower bound
     *\and also condition number of matrix A
     */

    //    boost::tuple<double,double> lb( size_type N, parameter_type const& mu, std::vector< vectorN_type >& uN, std::vector< vectorN_type >& uNdu , std::vector<vectorN_type> & uNold=std::vector<vectorN_type>(), std::vector<vectorN_type> & uNduold=std::vector<vectorN_type>(), int K=0) const;
    boost::tuple<double,double> lb( size_type N, parameter_type const& mu, std::vector< vectorN_type >& uN, std::vector< vectorN_type >& uNdu , std::vector<vectorN_type> & uNold, std::vector<vectorN_type> & uNduold, int K=0 ) const;

    /*
     * update the jacobian
     * \param map_X : solution
     * \param map_J : jacobian
     * \param mu : current parameter
     * \param N : dimension of the reduced basis
     */
    void updateJacobian( const map_dense_vector_type& map_X, map_dense_matrix_type& map_J , const parameter_type & mu , int N) const ;

    /*
     * update the residual
     * \param map_X : solution
     * \param map_R : residual
     * \param mu : current parameter
     * \param N : dimension of the reduced basis
     */
    void updateResidual( const map_dense_vector_type& map_X, map_dense_vector_type& map_R , const parameter_type & mu, int N ) const ;

    /*
     * compute the projection of the initial guess
     * \param mu : current parameter
     * \param N : dimension of the reduced basis
     * \param initial guess
     */
    void computeProjectionInitialGuess( const parameter_type & mu, int N , vectorN_type& initial_guess ) const ;

    /*
     * newton algorithm to solve non linear problem
     * \param N : dimension of the reduced basis
     * \param mu : current parameter
     * \param uN : reduced solution ( vectorN_type )
     * \param condition number of the reduced jacobian
     */
    void newton(  size_type N, parameter_type const& mu , vectorN_type & uN  , double& condition_number, double& output) const ;

    /*
     * fixed point for primal problem ( offline step )
     * \param mu : current parameter
     * \param A : matrix of the bilinear form (fill the matrix)
     * \param zero_iteration : don't perfom iterations if true ( for linear problems for example )
     */
    element_type offlineFixedPointPrimal( parameter_type const& mu, sparse_matrix_ptrtype & A, bool zero_iteration ) ;

    /*
     * \param mu : current parameter
     * \param dual_initial_field : to be filled
     * \param A : matrix of the primal problem, needed only to check dual properties
     * \param u : solution of the primal problem, needed only to check dual properties
     * \param zero_iteration : don't perfom iterations if true ( for linear problems for example )
     */
    element_type offlineFixedPointDual( parameter_type const& mu , element_ptrtype & dual_initial_field, const sparse_matrix_ptrtype & A, const element_type & u, bool zero_iteration ) ;

    /*
     * fixed point ( primal problem ) - ONLINE step
     * \param N : dimension of the reduced basis
     * \param mu :current parameter
     * \param uN : dual reduced solution ( vectorN_type )
     * \param uNold : dual old reduced solution ( vectorN_type )
     * \param condition number of the reduced matrix A
     * \param output : vector of outpus at each time step
     * \param K : number of time step ( default value, must be >0 if used )
     */
    void fixedPointPrimal( size_type N, parameter_type const& mu, std::vector< vectorN_type > & uN,  std::vector<vectorN_type> & uNold, double& condition_number,
                           std::vector< double > & output_vector, int K=0) const;

    /*
     * fixed point ( dual problem ) - ONLINE step
     * \param N : dimension of the reduced basis
     * \param mu :current parameter
     * \param uNdu : dual reduced solution ( vectorN_type )
     * \param uNduold : dual old reduced solution ( vectorN_type )
     * \param output : vector of outpus at each time step
     * \param K : number of time step ( default value, must be >0 if used )
     */
    void fixedPointDual(  size_type N, parameter_type const& mu, std::vector< vectorN_type > & uNdu,  std::vector<vectorN_type> & uNduold, std::vector< double > & output_vector, int K=0) const;

    /**
     * fixed point ( main ) - ONLINE step
     * \param N : dimension of the reduced basis
     * \param mu :current parameter
     * \param uN : dual reduced solution ( vectorN_type )
     * \param uNdu : dual reduced solution ( vectorN_type )
     * \param uNold : dual old reduced solution ( vectorN_type )
     * \param uNduold : dual old reduced solution ( vectorN_type )
     * \param output : vector of outpus at each time step
     * \param K : number of time step ( default value, must be >0 if used )
     */
    void fixedPoint(  size_type N, parameter_type const& mu, std::vector< vectorN_type > & uN, std::vector< vectorN_type > & uNdu,  std::vector<vectorN_type> & uNold, std::vector<vectorN_type> & uNduold,
                 double& condition_number, std::vector< double > & output_vector, int K=0) const;

    /**
     * computation of the conditioning number of a given matrix
     * \param A : reduced matrix
     */
    double computeConditioning( matrixN_type & A ) const ;


    /**
     * Returns the lower bound of the output
     *
     * \param mu \f$ \mu\f$ the parameter at which to evaluate the output
     * \param N the size of the reduced basis space to use
     * \param uN primal solution
     * \param uNdu dual solution
     *
     *\return compute online the lower bound and condition number of matrix A
     */
    boost::tuple<double,double> lb( parameter_ptrtype const& mu, size_type N, std::vector< vectorN_type >& uN, std::vector< vectorN_type >& uNdu ) const
    {
        return lb( N, *mu, uN, uNdu );
    }

    /**
     * Returns the upper bound of the output associed to \f$\mu\f$
     *
     * \param mu \f$ \mu\f$ the parameter at which to evaluate the output
     * \param N the dimension of \f$W_N\f$
     * \param uN primal solution
     * \param uNdu dual solution
     *
     *\return compute online the lower bound
     */


    value_type ub( size_type N, parameter_type const& mu, std::vector< vectorN_type >& uN, std::vector< vectorN_type >& uNdu ) const
    {
        auto o = lb( N, mu, uN, uNdu );
        auto e = delta( N, mu, uN, uNdu );
        return o.template get<0>() + e.template get<0>();
    }

    /**
     * Returns the error bound on the output
     *
     * \param mu \f$ \mu\f$ the parameter at which to evaluate the output
     * \param N the size of the reduced basis space to use
     * \param uN primal solution
     * \param uNdu dual solution
     *
     *\return compute online the lower bound
     */
    value_type delta( size_type N, parameter_ptrtype const& mu, std::vector< vectorN_type > const& uN, std::vector< vectorN_type > const& uNdu, std::vector<vectorN_type> const& uNold,  std::vector<vectorN_type> const& uNduold , int k=0 ) const
    {
        auto e = delta( N, mu, uN, uNdu );
        return e.template get<0>();
    }

    /**
     * Returns the error bound on the output associed to \f$\mu\f$
     *
     * \param mu \f$ \mu\f$ the parameter at which to evaluate the output
     * \param N the dimension of \f$W_N\f$
     * \param uN primal solution
     * \param uNdu dual solution
     *
     *\return compute online the lower bound
     */
    error_estimation_type delta( size_type N, parameter_type const& mu, std::vector< vectorN_type > const& uN, std::vector< vectorN_type > const& uNdu, std::vector<vectorN_type> const& uNold, std::vector<vectorN_type> const& uNduold, int k=0 ) const;

    /**
     * Returns the upper bound of the output associed to \f$\mu\f$
     *
     * \param mu \f$ \mu\f$ the parameter at which to evaluate the output
     * \param N the dimension of \f$W_N\f$
     *
     *\return compute online the lower bound
     */
    value_type ub( size_type K, parameter_ptrtype const& mu, std::vector< vectorN_type >& uN, std::vector< vectorN_type >& uNdu ) const
    {
        return ub( K, *mu, uN, uNdu );
    }

    /**
     * Offline computation
     *
     * \return the convergence history (max error)
     */
    convergence_type offline();


    /**
     * \brief Retuns maximum value of the relative error
     * \param N number of elements in the reduced basis <=> M_N
     */
    max_error_type maxErrorBounds( size_type N ) const;

    /**
     * evaluate online the residual
     */

    residual_error_type transientPrimalResidual( int Ncur, parameter_type const& mu,  vectorN_type const& Un, vectorN_type const& Unold=vectorN_type(), double time_step=1, double time=1e30 ) const;
    residual_error_type steadyPrimalResidual( int Ncur, parameter_type const& mu,  vectorN_type const& Un, double time=0 ) const;


    residual_error_type transientDualResidual( int Ncur, parameter_type const& mu,  vectorN_type const& Un, vectorN_type const& Unold=vectorN_type(), double time_step=1, double time=1e30 ) const;
    residual_error_type steadyDualResidual( int Ncur, parameter_type const& mu,  vectorN_type const& Un, double time=0 ) const;


    value_type initialDualResidual( int Ncur, parameter_type const& mu, vectorN_type const& Uduini, double time_step ) const ;


    /**
     * generate offline the residual
     */
    void offlineResidual( int Ncur , int number_of_added_elements=1 );
    void offlineResidual( int Ncur, mpl::bool_<true> ,int number_of_added_elements=1 );
    void offlineResidual( int Ncur, mpl::bool_<false> , int number_of_added_elements=1 );

    /*
     * compute empirical error estimation, ie : |S_n - S{n-1}|
     * \param Ncur : number of elements in the reduced basis <=> M_N
     * \param mu : parameters value (one value per parameter)
     * \output : empirical error estimation
     */
    value_type empiricalErrorEstimation ( int Ncur, parameter_type const& mu, int k ) const ;


    /**
     * return the crb expansion at parameter \p \mu, ie \f$\sum_{i=0}^N u^N_i
     * \phi_i\f$ where $\phi_i, i=1...N$ are the basis function of the reduced
     * basis space
     * if N>0 take the N^th first elements, else take all elements
     */
    element_type expansion( parameter_type const& mu , int N=-1, int time_index=-1);

    /**
     * return the crb expansion at parameter \p \mu, ie \f$\sum_{i=0}^N u^N_i
     * \phi_i\f$ where $\phi_i, i=1...N$ are the basis function of the reduced
     * basis space
     */
    element_type expansion( vectorN_type const& u , int const N, wn_type const & WN ) const;


    void checkInitialGuess( const element_type expansion_uN , parameter_type const& mu, vectorN_type & error ) const ;
    void checkInitialGuess( const element_type expansion_uN , parameter_type const& mu, vectorN_type & error , mpl::bool_<true> ) const ;
    void checkInitialGuess( const element_type expansion_uN , parameter_type const& mu, vectorN_type & error , mpl::bool_<false> ) const ;

    /**
     * run the certified reduced basis with P parameters and returns 1 output
     */
    //boost::tuple<double,double,double> run( parameter_type const& mu, double eps = 1e-6 );
    //boost::tuple<double,double,double,double> run( parameter_type const& mu, double eps = 1e-6 );
    //by default N=-1 so we take dimension-max but if N>0 then we take N basis functions toperform online step
    boost::tuple<double,double, solutions_tuple, double, double, double, boost::tuple<double,double,double> > run( parameter_type const& mu, double eps = 1e-6, int N = -1 );

    /**
     * run the certified reduced basis with P parameters and returns 1 output
     */
    void run( const double * X, unsigned long N, double * Y, unsigned long P );
    void run( const double * X, unsigned long N, double * Y, unsigned long P, mpl::bool_<true> );
    void run( const double * X, unsigned long N, double * Y, unsigned long P, mpl::bool_<false> );

    /**
     * \return a random sampling
     */
    sampling_type randomSampling( int N )
    {
        M_Xi->randomize( N );
        return *M_Xi;
    }

    /**
     * \return a equidistributed sampling
     */
    sampling_type equidistributedSampling( int N )
    {
        M_Xi->equidistribute( N );
        return *M_Xi;
    }

    sampling_ptrtype wnmu ( ) const
    {
        return M_WNmu;
    }

    wn_type wn() const
    {
        return M_WN;
    }

    wn_type wndu() const
    {
        return M_WNdu;
    }

    /**
     * save the CRB database
     */
    void saveDB();

    /**
     * load the CRB database
     */
    bool loadDB();

    /**
     *  do the projection on the POD space of u (for transient problems)
     *  \param u : the solution to project (input parameter)
     *  \param projection : the projection (output parameter)
     *  \param name_of_space : primal or dual
     */
    void projectionOnPodSpace( const element_type & u , element_ptrtype& projection ,const std::string& name_of_space="primal" );


    bool useWNmu()
    {
        bool use = this->vm()["crb.run-on-WNmu"].template as<bool>();
        return use;
    }


    /**
     * if true, rebuild the database (if already exist)
     */
    bool rebuildDB() ;

    /**
     * if true, show the mu selected during the offline stage
     */
    bool showMuSelection() ;

    /**
     * if true, print the max error (absolute) during the offline stage
     */
    bool printErrorDuringOfflineStep();

    /**
     * print parameters set mu selected during the offline stage
     */
    void printMuSelection( void );

    /**
     * print max errors (total error and also primal and dual contributions)
     * during offline stage
     */
    void printErrorsDuringRbConstruction( void );
    /*
     * compute correction terms for output
     * \param mu \f$ \mu\f$ the parameter at which to evaluate the output
     * \param uN : primal solution
     * \param uNdu : dual solution
     * \pram uNold : old primal solution
     * \param K : time index
     */
    double correctionTerms(parameter_type const& mu, std::vector< vectorN_type > const & uN, std::vector< vectorN_type > const & uNdu , std::vector<vectorN_type> const & uNold,  int const K=0) const;

    /*
     * build matrix to store functions used to compute the variance output
     * \param N : number of elements in the reduced basis
     */
    void buildVarianceMatrixPhi(int const N);
    void buildVarianceMatrixPhi(int const N , mpl::bool_<true> );
    void buildVarianceMatrixPhi(int const N , mpl::bool_<false> );

    WorldComm const& worldComm() const { return Environment::worldComm() ; }

    /**
     * evaluate online time via the option crb.computational-time-neval
     */
    void computationalTimeStatistics( std::string appname );

    //@}


private:
    crb_elements_db_type M_elements_database;

    boost::shared_ptr<SolverNonLinear<double> > M_nlsolver;

    truth_model_ptrtype M_model;

    backend_ptrtype M_backend;
    backend_ptrtype M_backend_primal;
    backend_ptrtype M_backend_dual;

    int M_output_index;

    double M_tolerance;

    size_type M_iter_max;

    int M_factor;

    CRBErrorType M_error_type;

    double M_maxerror;

    // parameter space
    parameterspace_ptrtype M_Dmu;

    // fine sampling of the parameter space
    sampling_ptrtype M_Xi;

    // sampling of parameter space to build WN
    sampling_ptrtype M_WNmu;
    sampling_ptrtype M_WNmu_complement;

    //scm
    scm_ptrtype M_scmA;
    scm_ptrtype M_scmM;

    //export
    export_ptrtype exporter;

#if 0
    array_2_type M_C0_pr;
    array_2_type M_C0_du;
    array_3_type M_Lambda_pr;
    array_3_type M_Lambda_du;
    array_4_type M_Gamma_pr;
    array_4_type M_Gamma_du;
    array_3_type M_Cmf_pr;
    array_3_type M_Cmf_du;
    array_4_type M_Cma_pr;
    array_4_type M_Cma_du;
    array_4_type M_Cmm_pr;
    array_4_type M_Cmm_du;
#endif
    std::vector< std::vector< std::vector< std::vector< double > > > > M_C0_pr;
    std::vector< std::vector< std::vector< std::vector< double > > > > M_C0_du;
    std::vector< std::vector< std::vector< std::vector< vectorN_type > > > > M_Lambda_pr;
    std::vector< std::vector< std::vector< std::vector< vectorN_type > > > > M_Lambda_du;
    std::vector< std::vector< std::vector< std::vector< matrixN_type > > > > M_Gamma_pr;
    std::vector< std::vector< std::vector< std::vector< matrixN_type > > > > M_Gamma_du;
    std::vector< std::vector< std::vector< std::vector< vectorN_type > > > > M_Cmf_pr;
    std::vector< std::vector< std::vector< std::vector< vectorN_type > > > > M_Cmf_du;
    std::vector< std::vector< std::vector< std::vector< vectorN_type > > > > M_Cmf_du_ini;
    std::vector< std::vector< std::vector< std::vector< matrixN_type > > > > M_Cma_pr;
    std::vector< std::vector< std::vector< std::vector< matrixN_type > > > > M_Cma_du;
    std::vector< std::vector< std::vector< std::vector< matrixN_type > > > > M_Cmm_pr;
    std::vector< std::vector< std::vector< std::vector< matrixN_type > > > > M_Cmm_du;


    std::vector<double> M_coeff_pr_ini_online;
    std::vector<double> M_coeff_du_ini_online;


    friend class boost::serialization::access;
    // When the class Archive corresponds to an output archive, the
    // & operator is defined similar to <<.  Likewise, when the class Archive
    // is a type of input archive the & operator is defined similar to >>.
    template<class Archive>
    void save( Archive & ar, const unsigned int version ) const;

    template<class Archive>
    void load( Archive & ar, const unsigned int version ) ;

    BOOST_SERIALIZATION_SPLIT_MEMBER()

    // reduced basis space
    wn_type M_WN;
    wn_type M_WNdu;

    size_type M_N;
    size_type M_Nm;

    bool orthonormalize_primal;
    bool orthonormalize_dual;
    bool solve_dual_problem;

    convergence_type M_rbconv;

    //time
    bdf_ptrtype M_bdf_primal;
    bdf_ptrtype M_bdf_primal_save;
    bdf_ptrtype M_bdf_dual;
    bdf_ptrtype M_bdf_dual_save;

    // left hand side
    std::vector < std::vector<matrixN_type> > M_Aqm_pr;
    std::vector < std::vector<matrixN_type> > M_Aqm_du;
    std::vector < std::vector<matrixN_type> > M_Aqm_pr_du;

    //jacobian
    std::vector < std::vector<matrixN_type> > M_Jqm_pr;

    //residual
    std::vector < std::vector<vectorN_type> > M_Rqm_pr;

    //mass matrix
    std::vector < std::vector<matrixN_type> > M_Mqm_pr;
    std::vector < std::vector<matrixN_type> > M_Mqm_du;
    std::vector < std::vector<matrixN_type> > M_Mqm_pr_du;

    // right hand side
    std::vector < std::vector<vectorN_type> > M_Fqm_pr;
    std::vector < std::vector<vectorN_type> > M_Fqm_du;
    // output
    std::vector < std::vector<vectorN_type> > M_Lqm_pr;
    std::vector < std::vector<vectorN_type> > M_Lqm_du;

    //initial guess
    std::vector < std::vector<vectorN_type> > M_InitialGuessV_pr;

    std::vector<int> M_index;
    int M_mode_number;

    std::vector < matrixN_type > M_variance_matrix_phi;

    bool M_compute_variance;
    bool M_rbconv_contains_primal_and_dual_contributions;
    parameter_type M_current_mu;
    int M_no_residual_index;

    bool M_database_contains_variance_info;

    bool M_use_newton;

    bool M_offline_step;

    preconditioner_ptrtype M_preconditioner;
    preconditioner_ptrtype M_preconditioner_primal;
    preconditioner_ptrtype M_preconditioner_dual;

};

po::options_description crbOptions( std::string const& prefix = "" );




template<typename TruthModelType>
typename CRB<TruthModelType>::element_type
CRB<TruthModelType>::offlineFixedPointPrimal(parameter_type const& mu, sparse_matrix_ptrtype & A, bool zero_iteration )
{
    auto u = M_model->functionSpace()->element();

    sparse_matrix_ptrtype M = M_model->newMatrix();
    int nl = M_model->Nl();  //number of outputs
    std::vector< vector_ptrtype > F( nl );
    for(int l=0; l<nl; l++)
        F[l]=M_model->newVector();

    M_backend_primal = backend_type::build( BACKEND_PETSC );
    bool reuse_prec = this->vm()["crb.reuse-prec"].template as<bool>() ;

    M_bdf_primal = bdf( _space=M_model->functionSpace(), _vm=this->vm() , _name="bdf_primal" );
    M_bdf_primal_save = bdf( _space=M_model->functionSpace(), _vm=this->vm() , _name="bdf_primal_save" );

    //set parameters for time discretization
    M_bdf_primal->setTimeInitial( M_model->timeInitial() );
    M_bdf_primal->setTimeStep( M_model->timeStep() );
    M_bdf_primal->setTimeFinal( M_model->timeFinal() );
    M_bdf_primal->setOrder( M_model->timeOrder() );

    M_bdf_primal_save->setTimeInitial( M_model->timeInitial() );
    M_bdf_primal_save->setTimeStep( M_model->timeStep() );
    M_bdf_primal_save->setTimeFinal( M_model->timeFinal() );
    M_bdf_primal_save->setOrder( M_model->timeOrder() );

    M_bdf_primal_save->setRankProcInNameOfFiles( true );
    M_bdf_primal->setRankProcInNameOfFiles( true );

    //initialization of unknown
    auto elementptr = M_model->functionSpace()->elementPtr();
    M_model->initializationField( elementptr, mu );

    u = *elementptr;

    auto Apr = M_model->newMatrix();


    int max_fixedpoint_iterations  = option(_name="crb.max-fixedpoint-iterations").template as<int>();
    double increment_fixedpoint_tol  = option(_name="crb.increment-fixedpoint-tol").template as<double>();
    double fixedpoint_critical_value  = option(_name="crb.fixedpoint-critical-value").template as<double>();
    int iteration=0;
    double increment_norm=1e3;

    if( zero_iteration )
        increment_norm = 0;

    double bdf_coeff;

    auto vec_bdf_poly = M_backend_primal->newVector( M_model->functionSpace() );

    //assemble the initial guess for the given mu
    if ( M_model->isSteady() )
    {
        elementptr = M_model->assembleInitialGuess( mu ) ;
        u = *elementptr ;
    }

    auto uold = M_model->functionSpace()->element();

    element_ptrtype uproj( new element_type( M_model->functionSpace() ) );

    vector_ptrtype Rhs( M_backend_primal->newVector( M_model->functionSpace() ) );

    for ( M_bdf_primal->start(u),M_bdf_primal_save->start(u);
          !M_bdf_primal->isFinished() , !M_bdf_primal_save->isFinished();
          M_bdf_primal->next() , M_bdf_primal_save->next() )
    {

        bdf_coeff = M_bdf_primal->polyDerivCoefficient( 0 );

        auto bdf_poly = M_bdf_primal->polyDeriv();

        do
        {
            boost::tie( M, Apr, F) = M_model->update( mu , u, M_bdf_primal->time() );
            //boost::tie( M, Apr, F) = M_model->update( mu , M_bdf_primal->time() );

            if( iteration == 0 )
                A = Apr;

            if ( ! M_model->isSteady() )
            {
                Apr->addMatrix( bdf_coeff, M );
                *Rhs = *F[0];
                *vec_bdf_poly = bdf_poly;
                Rhs->addVector( *vec_bdf_poly, *M );
            }
            else
                *Rhs = *F[0];
            //Apr->close();

            //backup for non linear problems
            uold = u;

            //solve
            M_preconditioner_primal->setMatrix( Apr );
            if ( reuse_prec )
            {
                auto ret = M_backend_primal->solve( _matrix=Apr, _solution=u, _rhs=Rhs,  _prec=M_preconditioner_primal, _reuse_prec=( M_bdf_primal->iteration() >=2 ) );
                if  ( !ret.template get<0>() )
                    LOG(INFO)<<"[CRB] WARNING : at time "<<M_bdf_primal->time()<<" we have not converged ( nb_it : "<<ret.template get<1>()<<" and residual : "<<ret.template get<2>() <<" ) \n";
            }
            else
            {

                auto ret = M_backend_primal->solve( _matrix=Apr, _solution=u, _rhs=Rhs ,  _prec=M_preconditioner_primal );
                if ( !ret.template get<0>() )
                    LOG(INFO)<<"[CRB] WARNING : at time "<<M_bdf_primal->time()<<" we have not converged ( nb_it : "<<ret.template get<1>()<<" and residual : "<<ret.template get<2>() <<" ) \n";
            }

            //on each subspace the norme of the increment is computed and then we perform the sum
            increment_norm = M_model->computeNormL2( u , uold );
            iteration++;

        }while( increment_norm > increment_fixedpoint_tol && iteration < max_fixedpoint_iterations );

        M_bdf_primal->shiftRight( u );

        if ( ! M_model->isSteady() )
        {
            element_ptrtype projection ( new element_type ( M_model->functionSpace() ) );
            projectionOnPodSpace ( u , projection, "primal" );
            *uproj=u;
            M_bdf_primal_save->shiftRight( *uproj );
        }

        if( increment_norm > fixedpoint_critical_value )
            throw std::logic_error( (boost::format("[CRB::offlineFixedPointPrimal]  at time %1% ERROR : increment > critical value " ) %M_bdf_primal->time() ).str() );

        for ( size_type l = 0; l < M_model->Nl(); ++l )
        {
            F[l]->close();
            element_ptrtype eltF( new element_type( M_model->functionSpace() ) );
            *eltF = *F[l];
            LOG(INFO) << "u^T F[" << l << "]= " << inner_product( u, *eltF ) << " at time : "<<M_bdf_primal->time()<<"\n";
        }
        LOG(INFO) << "[CRB::offlineWithErrorEstimation] energy = " << A->energy( u, u ) << "\n";

    }//end of loop over time

    return u;
#if 0
    //*u = M_model->solve( mu );
    do
    {
        //merge all matrices/vectors contributions from affine decomposition
        //result : a tuple : M , A and F
        auto merge = M_model->update( mu );
        A = merge.template get<1>();
        F = merge.template get<2>();
        //backup
        uold = un;

        //solve
        //M_backend->solve( _matrix=A , _solution=un, _rhs=F[0]);
        M_preconditioner_primal->setMatrix( A );
        M_backend_primal->solve( _matrix=A , _solution=un, _rhs=F[0] , _prec=M_preconditioner );

        //on each subspace the norme of the increment is computed and then we perform the sum
        increment_norm = M_model->computeNormL2( un , uold );
        iteration++;

    } while( increment_norm > increment_fixedpoint_tol && iteration < max_fixedpoint_iterations );

    element_ptrtype eltF( new element_type( M_model->functionSpace() ) );
    *eltF = *F[0];
    LOG(INFO) << "[CRB::offlineFixedPoint] u^T F = " << inner_product( *u, *eltF ) ;
    LOG(INFO) << "[CRB::offlineFixedPoint] energy = " << A->energy( *u, *u ) ;
#endif
}//offline fixed point


template<typename TruthModelType>
typename CRB<TruthModelType>::element_type
CRB<TruthModelType>::offlineFixedPointDual(parameter_type const& mu, element_ptrtype & dual_initial_field, const sparse_matrix_ptrtype & A, const element_type & u, bool zero_iteration )
{

    M_backend_dual = backend_type::build( BACKEND_PETSC );
    bool reuse_prec = this->vm()["crb.reuse-prec"].template as<bool>() ;

    auto udu = M_model->functionSpace()->element();

    sparse_matrix_ptrtype M = M_model->newMatrix();
    int nl = M_model->Nl();  //number of outputs
    std::vector< vector_ptrtype > F( nl );
    for(int l=0; l<nl; l++)
        F[l]=M_model->newVector();


    M_bdf_dual = bdf( _space=M_model->functionSpace(), _vm=this->vm() , _name="bdf_dual" );
    M_bdf_dual_save = bdf( _space=M_model->functionSpace(), _vm=this->vm() , _name="bdf_dual_save" );

    M_bdf_dual->setTimeInitial( M_model->timeFinal()+M_model->timeStep() );

    M_bdf_dual->setTimeStep( -M_model->timeStep() );
    M_bdf_dual->setTimeFinal( M_model->timeInitial()+M_model->timeStep() );
    M_bdf_dual->setOrder( M_model->timeOrder() );

    M_bdf_dual_save->setTimeInitial( M_model->timeFinal()+M_model->timeStep() );
    M_bdf_dual_save->setTimeStep( -M_model->timeStep() );
    M_bdf_dual_save->setTimeFinal( M_model->timeInitial()+M_model->timeStep() );
    M_bdf_dual_save->setOrder( M_model->timeOrder() );

    M_bdf_dual_save->setRankProcInNameOfFiles( true );
    M_bdf_dual->setRankProcInNameOfFiles( true );

    auto Adu = M_model->newMatrix();
    auto Apr = M_model->newMatrix();

    double dt = M_model->timeStep();

    int max_fixedpoint_iterations  = option(_name="crb.max-fixedpoint-iterations").template as<int>();
    double increment_fixedpoint_tol  = option(_name="crb.increment-fixedpoint-tol").template as<double>();
    double fixedpoint_critical_value  = option(_name="crb.fixedpoint-critical-value").template as<double>();
    int iteration=0;
    double increment_norm=1e3;

    vector_ptrtype Rhs( M_backend_dual->newVector( M_model->functionSpace() ) );

    if( zero_iteration )
        increment_norm = 0;

    double bdf_coeff;

    auto vec_bdf_poly = M_backend_dual->newVector( M_model->functionSpace() );

    if ( M_model->isSteady() )
        udu.zero() ;
    else
    {
        boost::tie( M, Apr, F) = M_model->update( mu , M_bdf_dual->timeInitial() );

#if 0
        Apr->addMatrix( 1./dt, M );
        Apr->transpose( Adu );
        *Rhs=*F[M_output_index];
        Rhs->scale( 1./dt );
        // M->scale(1./dt);

        M_preconditioner_dual->setMatrix( Adu );
        M_backend_dual->solve( _matrix=Adu, _solution=dual_initial_field, _rhs=Rhs, _prec=M_preconditioner_dual );
#else
        *Rhs=*F[M_output_index];
        //Rhs->scale( 1./dt );
        //M->scale(1./dt);
        M_preconditioner_dual->setMatrix( M );
        M_backend_dual->solve( _matrix=M, _solution=dual_initial_field, _rhs=Rhs, _prec=M_preconditioner_dual );
#endif
        udu=*dual_initial_field;
    }

    auto uold = M_model->functionSpace()->element();

    element_ptrtype uproj( new element_type( M_model->functionSpace() ) );


    for ( M_bdf_dual->start(udu),M_bdf_dual_save->start(udu);
          !M_bdf_dual->isFinished() , !M_bdf_dual_save->isFinished();
          M_bdf_dual->next() , M_bdf_dual_save->next() )
    {

        bdf_coeff = M_bdf_dual->polyDerivCoefficient( 0 );

        auto bdf_poly = M_bdf_dual->polyDeriv();

        do
        {
            boost::tie( M, Apr, F) = M_model->update( mu , udu, M_bdf_dual->time() );

            if( ! M_model->isSteady() )
            {
                Apr->addMatrix( bdf_coeff, M );
                Rhs->zero();
                *vec_bdf_poly = bdf_poly;
                Rhs->addVector( *vec_bdf_poly, *M );
            }
            else
            {
                *Rhs = *F[M_output_index];
                Rhs->scale( -1 );
            }


            if( option("crb.use-symmetric-matrix").template as<bool>() )
                Adu = Apr;
            else
                Apr->transpose( Adu );
            //Adu->close();
            //Rhs->close();

            //backup for non linear problems
            uold = udu;

            //solve
            M_preconditioner_dual->setMatrix( Adu );
            if ( reuse_prec )
            {
                auto ret = M_backend_dual->solve( _matrix=Adu, _solution=udu, _rhs=Rhs,  _prec=M_preconditioner_dual, _reuse_prec=( M_bdf_primal->iteration() >=2 ) );
                if  ( !ret.template get<0>() )
                    LOG(INFO)<<"[CRB] WARNING : at time "<<M_bdf_primal->time()<<" we have not converged ( nb_it : "<<ret.template get<1>()<<" and residual : "<<ret.template get<2>() <<" ) \n";
            }
            else
            {
                auto ret = M_backend_dual->solve( _matrix=Adu, _solution=udu, _rhs=Rhs ,  _prec=M_preconditioner_dual );
                if ( !ret.template get<0>() )
                    LOG(INFO)<<"[CRB] WARNING : at time "<<M_bdf_primal->time()<<" we have not converged ( nb_it : "<<ret.template get<1>()<<" and residual : "<<ret.template get<2>() <<" ) \n";
            }

            //on each subspace the norme of the increment is computed and then we perform the sum
            increment_norm = M_model->computeNormL2( udu , uold );
            iteration++;

        }while( increment_norm > increment_fixedpoint_tol && iteration < max_fixedpoint_iterations );

        M_bdf_dual->shiftRight( udu );

        //check dual property
        double term1 = A->energy( udu, u );
        double term2 = Adu->energy( u, udu );
        double diff = math::abs( term1-term2 );
        LOG(INFO) << "< A u , udu > - < u , A* udu > = "<<diff<<"\n";

        if ( ! M_model->isSteady() )
        {
            element_ptrtype projection ( new element_type ( M_model->functionSpace() ) );
            projectionOnPodSpace ( udu , projection, "dual" );
            *uproj=udu;
            M_bdf_dual_save->shiftRight( *uproj );
        }

        if( increment_norm > fixedpoint_critical_value )
            throw std::logic_error( (boost::format("[CRB::offlineFixedPointDual]  at time %1% ERROR : increment > critical value " ) %M_bdf_dual->time() ).str() );

    }//end of loop over time

    return udu;
}//offline fixed point


template<typename TruthModelType>
typename CRB<TruthModelType>::convergence_type
CRB<TruthModelType>::offline()
{
    M_database_contains_variance_info = this->vm()["crb.save-information-for-variance"].template as<bool>();

    int proc_number = this->worldComm().globalRank();
    M_rbconv_contains_primal_and_dual_contributions = true;

    bool rebuild_database = this->vm()["crb.rebuild-database"].template as<bool>() ;
    orthonormalize_primal = this->vm()["crb.orthonormalize-primal"].template as<bool>() ;
    orthonormalize_dual = this->vm()["crb.orthonormalize-dual"].template as<bool>() ;
    solve_dual_problem = this->vm()["crb.solve-dual-problem"].template as<bool>() ;

    M_use_newton = this->vm()["crb.use-newton"].template as<bool>() ;

    //if ( this->worldComm().globalSize() > 1 )
    //    solve_dual_problem = false;

    if ( ! solve_dual_problem ) orthonormalize_dual=false;

    M_Nm = this->vm()["crb.Nm"].template as<int>() ;
    bool seek_mu_in_complement = this->vm()["crb.seek-mu-in-complement"].template as<bool>() ;

    boost::timer ti;
    LOG(INFO)<< "Offline CRB starts, this may take a while until Database is computed...\n";
    //LOG(INFO) << "[CRB::offline] Starting offline for output " << M_output_index << "\n";
    //LOG(INFO) << "[CRB::offline] initialize underlying finite element model\n";
    M_model->init();
    //LOG(INFO) << " -- model init done in " << ti.elapsed() << "s\n";

    parameter_type mu( M_Dmu );

    double delta_pr;
    double delta_du;
    size_type index;

    //if M_N == 0 then there is not an already existing database
    if ( rebuild_database || M_N == 0)
    {

        ti.restart();
        //scm_ptrtype M_scm = scm_ptrtype( new scm_type( M_vm ) );
        //M_scm->setTruthModel( M_model );
        //    std::vector<boost::tuple<double,double,double> > M_rbconv2 = M_scm->offline();

        LOG(INFO) << "[CRB::offline] compute random sampling\n";

        int sampling_size = this->vm()["crb.sampling-size"].template as<int>();
        std::string file_name = ( boost::format("M_Xi_%1%") % sampling_size ).str();
        std::ifstream file ( file_name );
        if( ! file )
        {
            // random sampling
            M_Xi->randomize( sampling_size );
            //M_Xi->equidistribute( this->vm()["crb.sampling-size"].template as<int>() );
            M_Xi->writeOnFile(file_name);
        }
        else
        {
            M_Xi->clear();
            M_Xi->readFromFile(file_name);
        }

        M_WNmu->setSuperSampling( M_Xi );


        if( proc_number == 0 ) std::cout<<"[CRB offline] M_error_type = "<<M_error_type<<std::endl;

        if( Environment::worldComm().globalRank() == Environment::worldComm().masterRank() )
            std::cout << " -- sampling init done in " << ti.elapsed() << "s\n";
        ti.restart();

        if ( M_error_type == CRB_RESIDUAL || M_error_type == CRB_RESIDUAL_SCM )
        {
            int __QLhs = M_model->Qa();
            int __QRhs = M_model->Ql( 0 );
            int __QOutput = M_model->Ql( M_output_index );
            int __Qm = M_model->Qm();

            //typename array_2_type::extent_gen extents2;
            //M_C0_pr.resize( extents2[__QRhs][__QRhs] );
            //M_C0_du.resize( extents2[__QOutput][__QOutput] );
            M_C0_pr.resize( __QRhs );
            for( int __q1=0; __q1< __QRhs; __q1++)
            {
                int __mMaxQ1=M_model->mMaxF(0,__q1);
                M_C0_pr[__q1].resize( __mMaxQ1 );
                for( int __m1=0; __m1< __mMaxQ1; __m1++)
                {
                    M_C0_pr[__q1][__m1].resize(  __QRhs );
                    for( int __q2=0; __q2< __QRhs; __q2++)
                    {
                        int __mMaxQ2=M_model->mMaxF(0,__q2);
                        M_C0_pr[__q1][__m1][__q2].resize( __mMaxQ2 );
                    }
                }
            }

            M_C0_du.resize( __QOutput );
            for( int __q1=0; __q1< __QOutput; __q1++)
            {
                int __mMaxQ1=M_model->mMaxF(M_output_index,__q1);
                M_C0_du[__q1].resize( __mMaxQ1 );
                for( int __m1=0; __m1< __mMaxQ1; __m1++)
                {
                    M_C0_du[__q1][__m1].resize(  __QOutput );
                    for( int __q2=0; __q2< __QOutput; __q2++)
                    {
                        int __mMaxQ2=M_model->mMaxF(M_output_index,__q2);
                        M_C0_du[__q1][__m1][__q2].resize( __mMaxQ2 );
                    }
                }
            }

            //typename array_3_type::extent_gen extents3;
            //M_Lambda_pr.resize( extents3[__QLhs][__QRhs] );
            //M_Lambda_du.resize( extents3[__QLhs][__QOutput] );
            M_Lambda_pr.resize( __QLhs );
            for( int __q1=0; __q1< __QLhs; __q1++)
            {
                int __mMaxQ1=M_model->mMaxA(__q1);
                M_Lambda_pr[__q1].resize( __mMaxQ1 );
                for( int __m1=0; __m1< __mMaxQ1; __m1++)
                {
                    M_Lambda_pr[__q1][__m1].resize(  __QRhs );
                    for( int __q2=0; __q2< __QRhs; __q2++)
                    {
                        int __mMaxQ2=M_model->mMaxF(0,__q2);
                        M_Lambda_pr[__q1][__m1][__q2].resize( __mMaxQ2 );
                    }
                }
            }

            M_Lambda_du.resize( __QLhs );
            for( int __q1=0; __q1< __QLhs; __q1++)
            {
                int __mMaxQ1=M_model->mMaxA(__q1);
                M_Lambda_du[__q1].resize( __mMaxQ1 );
                for( int __m1=0; __m1< __mMaxQ1; __m1++)
                {
                    M_Lambda_du[__q1][__m1].resize(  __QOutput );
                    for( int __q2=0; __q2< __QOutput; __q2++)
                    {
                        int __mMaxQ2=M_model->mMaxF(M_output_index,__q2);
                        M_Lambda_du[__q1][__m1][__q2].resize( __mMaxQ2 );
                    }
                }
            }

            //typename array_4_type::extent_gen extents4;
            //M_Gamma_pr.resize( extents4[__QLhs][__QLhs] );
            //M_Gamma_du.resize( extents4[__QLhs][__QLhs] );
            M_Gamma_pr.resize( __QLhs );
            for( int __q1=0; __q1< __QLhs; __q1++)
            {
                int __mMaxQ1=M_model->mMaxA(__q1);
                M_Gamma_pr[__q1].resize( __mMaxQ1 );
                for( int __m1=0; __m1< __mMaxQ1; __m1++)
                {
                    M_Gamma_pr[__q1][__m1].resize(  __QLhs );
                    for( int __q2=0; __q2< __QLhs; __q2++)
                    {
                        int __mMaxQ2=M_model->mMaxA(__q2);
                        M_Gamma_pr[__q1][__m1][__q2].resize( __mMaxQ2 );
                    }
                }
            }

            M_Gamma_du.resize( __QLhs );
            for( int __q1=0; __q1< __QLhs; __q1++)
            {
                int __mMaxQ1=M_model->mMaxA(__q1);
                M_Gamma_du[__q1].resize( __mMaxQ1 );
                for( int __m1=0; __m1< __mMaxQ1; __m1++)
                {
                    M_Gamma_du[__q1][__m1].resize(  __QLhs );
                    for( int __q2=0; __q2< __QLhs; __q2++)
                    {
                        int __mMaxQ2=M_model->mMaxA(__q2);
                        M_Gamma_du[__q1][__m1][__q2].resize( __mMaxQ2 );
                    }
                }
            }

            if ( model_type::is_time_dependent )
            {
                // M_Cmf_pr.resize( extents3[__Qm][__QRhs] );
                // M_Cmf_du.resize( extents3[__Qm][__QRhs] );
                M_Cmf_pr.resize( __Qm );
                for( int __q1=0; __q1< __Qm; __q1++)
                {
                    int __mMaxQ1=M_model->mMaxM(__q1);
                    M_Cmf_pr[__q1].resize( __mMaxQ1 );
                    for( int __m1=0; __m1< __mMaxQ1; __m1++)
                    {
                        M_Cmf_pr[__q1][__m1].resize(  __QRhs );
                        for( int __q2=0; __q2< __QRhs; __q2++)
                        {
                            int __mMaxQ2=M_model->mMaxF(0,__q2);
                            M_Cmf_pr[__q1][__m1][__q2].resize( __mMaxQ2 );
                        }
                    }
                }

                M_Cmf_du.resize( __Qm );
                for( int __q1=0; __q1< __Qm; __q1++)
                {
                    int __mMaxQ1=M_model->mMaxM(__q1);
                    M_Cmf_du[__q1].resize( __mMaxQ1 );
                    for( int __m1=0; __m1< __mMaxQ1; __m1++)
                    {
                        M_Cmf_du[__q1][__m1].resize( __QOutput );
                        for( int __q2=0; __q2< __QOutput; __q2++)
                        {
                            int __mMaxQ2=M_model->mMaxF(M_output_index,__q2);
                            M_Cmf_du[__q1][__m1][__q2].resize( __mMaxQ2 );
                        }
                    }
                }
                M_Cmf_du_ini.resize( __Qm );
                for( int __q1=0; __q1< __Qm; __q1++)
                {
                    int __mMaxQ1=M_model->mMaxM(__q1);
                    M_Cmf_du_ini[__q1].resize( __mMaxQ1 );
                    for( int __m1=0; __m1< __mMaxQ1; __m1++)
                    {
                        M_Cmf_du_ini[__q1][__m1].resize( __QOutput );
                        for( int __q2=0; __q2< __QOutput; __q2++)
                        {
                            int __mMaxQ2=M_model->mMaxF(M_output_index,__q2);
                            M_Cmf_du_ini[__q1][__m1][__q2].resize( __mMaxQ2 );
                        }
                    }
                }

                // M_Cma_pr.resize( extents4[__Qm][__QLhs] );
                // M_Cma_du.resize( extents4[__Qm][__QLhs] );
                M_Cma_pr.resize( __Qm );
                for( int __q1=0; __q1< __Qm; __q1++)
                {
                    int __mMaxQ1=M_model->mMaxM(__q1);
                    M_Cma_pr[__q1].resize( __mMaxQ1 );
                    for( int __m1=0; __m1< __mMaxQ1; __m1++)
                    {
                        M_Cma_pr[__q1][__m1].resize(  __QLhs );
                        for( int __q2=0; __q2< __QLhs; __q2++)
                        {
                            int __mMaxQ2=M_model->mMaxA(__q2);
                            M_Cma_pr[__q1][__m1][__q2].resize( __mMaxQ2 );
                        }
                    }
                }

                M_Cma_du.resize( __Qm );
                for( int __q1=0; __q1< __Qm; __q1++)
                {
                    int __mMaxQ1=M_model->mMaxM(__q1);
                    M_Cma_du[__q1].resize( __mMaxQ1 );
                    for( int __m1=0; __m1< __mMaxQ1; __m1++)
                    {
                        M_Cma_du[__q1][__m1].resize( __QLhs );
                        for( int __q2=0; __q2< __QLhs; __q2++)
                        {
                            int __mMaxQ2=M_model->mMaxA(__q2);
                            M_Cma_du[__q1][__m1][__q2].resize( __mMaxQ2 );
                        }
                    }
                }

                // M_Cmm_pr.resize( extents4[__Qm][__Qm] );
                // M_Cmm_du.resize( extents4[__Qm][__Qm] );
                M_Cmm_pr.resize( __Qm );
                for( int __q1=0; __q1< __Qm; __q1++)
                {
                    int __mMaxQ1=M_model->mMaxM(__q1);
                    M_Cmm_pr[__q1].resize( __mMaxQ1 );
                    for( int __m1=0; __m1< __mMaxQ1; __m1++)
                    {
                        M_Cmm_pr[__q1][__m1].resize(  __Qm );
                        for( int __q2=0; __q2< __Qm; __q2++)
                        {
                            int __mMaxQ2=M_model->mMaxM(__q2);
                            M_Cmm_pr[__q1][__m1][__q2].resize( __mMaxQ2 );
                        }
                    }
                }

                M_Cmm_du.resize( __Qm );
                for( int __q1=0; __q1< __Qm; __q1++)
                {
                    int __mMaxQ1=M_model->mMaxM(__q1);
                    M_Cmm_du[__q1].resize( __mMaxQ1 );
                    for( int __m1=0; __m1< __mMaxQ1; __m1++)
                    {
                        M_Cmm_du[__q1][__m1].resize( __Qm );
                        for( int __q2=0; __q2< __Qm; __q2++)
                        {
                            int __mMaxQ2=M_model->mMaxM(__q2);
                            M_Cmm_du[__q1][__m1][__q2].resize( __mMaxQ2 );
                        }
                    }
                }

            }//end of if ( model_type::is_time_dependent )
        }//end of if ( M_error_type == CRB_RESIDUAL || M_error_type == CRB_RESIDUAL_SCM )
        if( Environment::worldComm().globalRank() == Environment::worldComm().masterRank() )
            std::cout << " -- residual data init done in " << ti.elapsed() << std::endl;
        ti.restart();

        // empty sets
        M_WNmu->clear();

        if( M_error_type == CRB_NO_RESIDUAL )
            mu = M_Dmu->element();
        else
        {
            // start with M_C = { arg min mu, mu \in Xi }
            boost::tie( mu, index ) = M_Xi->min();
        }

        int size = mu.size();
        if( proc_number == 0 )
        {
            std::cout << "  -- start with mu = [ ";
            for ( int i=0; i<size-1; i++ ) std::cout<<mu( i )<<" ";
            std::cout<<mu( size-1 )<<" ]"<<std::endl;
        }
        //std::cout << " -- WN size :  " << M_WNmu->size() << "\n";

        // dimension of reduced basis space
        M_N = 0;

        // scm offline stage: build C_K
        if ( M_error_type == CRB_RESIDUAL_SCM )
        {
            M_scmA->setScmForMassMatrix( false );
            if( ! M_scmA->loadDB() )
                std::vector<boost::tuple<double,double,double> > M_rbconv2 = M_scmA->offline();

            if ( ! M_model->isSteady() )
            {
                M_scmM->setScmForMassMatrix( true );
                if( ! M_scmM->loadDB() )
                    std::vector<boost::tuple<double,double,double> > M_rbconv3 = M_scmM->offline();
            }
        }

        M_maxerror = 1e10;
        delta_pr = 0;
        delta_du = 0;
        //boost::tie( M_maxerror, mu, index ) = maxErrorBounds( N );

        LOG(INFO) << "[CRB::offline] allocate reduced basis data structures\n";
        M_Aqm_pr.resize( M_model->Qa() );
        M_Aqm_du.resize( M_model->Qa() );
        M_Aqm_pr_du.resize( M_model->Qa() );

        if( M_use_newton )
            M_Jqm_pr.resize( M_model->Qa() );

        for(int q=0; q<M_model->Qa(); q++)
        {
            M_Aqm_pr[q].resize( M_model->mMaxA(q) );
            M_Aqm_du[q].resize( M_model->mMaxA(q) );
            M_Aqm_pr_du[q].resize( M_model->mMaxA(q) );

            if(M_use_newton)
                M_Jqm_pr[q].resize( M_model->mMaxA(q) );
        }

        M_Mqm_pr.resize( M_model->Qm() );
        M_Mqm_du.resize( M_model->Qm() );
        M_Mqm_pr_du.resize( M_model->Qm() );
        for(int q=0; q<M_model->Qm(); q++)
        {
            M_Mqm_pr[q].resize( M_model->mMaxM(q) );
            M_Mqm_du[q].resize( M_model->mMaxM(q) );
            M_Mqm_pr_du[q].resize( M_model->mMaxM(q) );
        }

        int QInitialGuessV = M_model->QInitialGuess();
        M_InitialGuessV_pr.resize( QInitialGuessV );
        for(int q=0; q<QInitialGuessV; q++)
        {
            M_InitialGuessV_pr[q].resize( M_model->mMaxInitialGuess(q) );
        }

        M_Fqm_pr.resize( M_model->Ql( 0 ) );
        M_Fqm_du.resize( M_model->Ql( 0 ) );
        M_Lqm_pr.resize( M_model->Ql( M_output_index ) );
        M_Lqm_du.resize( M_model->Ql( M_output_index ) );

        if(M_use_newton)
            M_Rqm_pr.resize( M_model->Ql( 0 ) );

        for(int q=0; q<M_model->Ql( 0 ); q++)
        {
            M_Fqm_pr[q].resize( M_model->mMaxF( 0 , q) );
            M_Fqm_du[q].resize( M_model->mMaxF( 0 , q) );

            if(M_use_newton)
                M_Rqm_pr[q].resize( M_model->mMaxF( 0 , q) );
        }
        for(int q=0; q<M_model->Ql( M_output_index ); q++)
        {
            M_Lqm_pr[q].resize( M_model->mMaxF( M_output_index , q) );
            M_Lqm_du[q].resize( M_model->mMaxF( M_output_index , q) );
        }
    }//end of if( rebuild_database )
    else
    {
        mu = M_current_mu;
        if( proc_number == 0 )
        {
            std::cout<<"we are going to enrich the reduced basis"<<std::endl;
            std::cout<<"there are "<<M_N<<" elements in the database"<<std::endl;
        }
        LOG(INFO) <<"we are going to enrich the reduced basis"<<std::endl;
        LOG(INFO) <<"there are "<<M_N<<" elements in the database"<<std::endl;
    }//end of else associated to if ( rebuild_databse )

    //sparse_matrix_ptrtype M,Adu,At;
    //element_ptrtype InitialGuess;
    //vector_ptrtype MF;
    //std::vector<vector_ptrtype> L;

    LOG(INFO) << "[CRB::offline] compute affine decomposition\n";
    std::vector< std::vector<sparse_matrix_ptrtype> > Aqm;
    std::vector< std::vector<sparse_matrix_ptrtype> > Mqm;
    //to project the initial guess on the reduced basis we solve A u = F
    //with A = \int_\Omega u v ( mass matrix )
    //F = \int_\Omega initial_guess v
    //so InitialGuessV is the mu-independant part of the vector F
    std::vector< std::vector<element_ptrtype> > InitialGuessV;
    std::vector< std::vector<std::vector<vector_ptrtype> > > Fqm,Lqm;
    sparse_matrix_ptrtype Aq_transpose = M_model->newMatrix();

    sparse_matrix_ptrtype A = M_model->newMatrix();
    int nl = M_model->Nl();
    std::vector< vector_ptrtype > F( nl );
    for(int l=0; l<nl; l++)
        F[l]=M_model->newVector();

    std::vector< std::vector<sparse_matrix_ptrtype> > Jqm;
    std::vector< std::vector<std::vector<vector_ptrtype> > > Rqm;

    InitialGuessV = M_model->computeInitialGuessVAffineDecomposition();


    if( M_use_newton )
        boost::tie( Mqm , Jqm, Rqm ) = M_model->computeAffineDecomposition();
    else
    {
        if( option("crb.stock-matrices").template as<bool>() )
            boost::tie( Mqm, Aqm, Fqm ) = M_model->computeAffineDecomposition();
    }

    element_ptrtype dual_initial_field( new element_type( M_model->functionSpace() ) );
    element_ptrtype uproj( new element_type( M_model->functionSpace() ) );
    auto u = M_model->functionSpace()->element();
    auto udu = M_model->functionSpace()->element();

    M_mode_number=1;

    LOG(INFO) << "[CRB::offline] starting offline adaptive loop\n";

    bool reuse_prec = this->vm()["crb.reuse-prec"].template as<bool>() ;

    bool use_predefined_WNmu = this->vm()["crb.use-predefined-WNmu"].template as<bool>() ;

    int N_log_equi = this->vm()["crb.use-logEquidistributed-WNmu"].template as<int>() ;
    int N_equi = this->vm()["crb.use-equidistributed-WNmu"].template as<int>() ;

    if( N_log_equi > 0 || N_equi > 0 )
        use_predefined_WNmu = true;

    if ( use_predefined_WNmu )
    {
        std::string file_name = ( boost::format("SamplingWNmu") ).str();
        std::ifstream file ( file_name );
        if( ! file )
        {
            throw std::logic_error( "[CRB::offline] ERROR the file SamplingWNmu doesn't exist so it's impossible to known which parameters you want to use to build the database" );
        }
        else
        {
            M_WNmu->clear();
            int sampling_size = M_WNmu->readFromFile(file_name);
            M_iter_max = sampling_size;
        }
        mu = M_WNmu->at( M_N ); // first element

        if( proc_number == this->worldComm().masterRank() )
            std::cout<<"[CRB::offline] read WNmu ( sampling size : "<<M_iter_max<<" )"<<std::endl;
    }


    LOG(INFO) << "[CRB::offline] strategy "<< M_error_type <<"\n";
    if( proc_number == this->worldComm().masterRank() ) std::cout << "[CRB::offline] strategy "<< M_error_type <<std::endl;

    if( M_error_type == CRB_NO_RESIDUAL || use_predefined_WNmu )
    {
        //in this case it makes no sens to check the estimated error
        M_maxerror = 1e10;
    }

    while ( M_maxerror > M_tolerance && M_N < M_iter_max  )
    {

        boost::timer timer, timer2;
        LOG(INFO) <<"========================================"<<"\n";

        if ( M_error_type == CRB_NO_RESIDUAL )
            LOG(INFO) << "N=" << M_N << "/"  << M_iter_max << " ( nb proc : "<<worldComm().globalSize()<<")";
        else
            LOG(INFO) << "N=" << M_N << "/"  << M_iter_max << " maxerror=" << M_maxerror << " / "  << M_tolerance << "( nb proc : "<<worldComm().globalSize()<<")";

        // for a given parameter \p mu assemble the left and right hand side
        u.setName( ( boost::format( "fem-primal-N%1%-proc%2%" ) % (M_N)  % proc_number ).str() );
        udu.setName( ( boost::format( "fem-dual-N%1%-proc%2%" ) % (M_N)  % proc_number ).str() );

        if ( M_model->isSteady() && ! M_use_newton )
        {

            //we need to treat nonlinearity also in offline step
            //because in online step we have to treat nonlinearity ( via a fixed point for example )
            bool zero_iteration=false;
            u = offlineFixedPointPrimal( mu , A , zero_iteration );
            if( solve_dual_problem )
                udu = offlineFixedPointDual( mu , dual_initial_field ,  A , u, zero_iteration );
        }

        if ( M_model->isSteady() && M_use_newton )
        {
            mu.check();
            u.zero();

            timer2.restart();
            LOG(INFO) << "[CRB::offline] solving primal" << "\n";
            u = M_model->solve( mu );
            if( proc_number == this->worldComm().masterRank() )
                LOG(INFO) << "  -- primal problem solved in " << timer2.elapsed() << "s";
            timer2.restart();
        }


        if( ! M_model->isSteady() )
        {
            bool zero_iteration=true;
            u = offlineFixedPointPrimal( mu,  A , zero_iteration );
            if ( solve_dual_problem || M_error_type==CRB_RESIDUAL || M_error_type == CRB_RESIDUAL_SCM )
                udu = offlineFixedPointDual( mu , dual_initial_field ,  A , u, zero_iteration );
        }


        if( ! use_predefined_WNmu )
            M_WNmu->push_back( mu, index );

        M_WNmu_complement = M_WNmu->complement();

        bool norm_zero = false;

        if ( M_model->isSteady() )
        {
            M_WN.push_back( u );
            M_WNdu.push_back( udu );
        }//end of steady case

        else
        {
            if ( M_N == 0 && orthonormalize_primal==false )
            {
                //add initial solution as element in the reduced basis if it's not zero
                //else there will be problems during orthonormalization step
                //note : at this step *u contains solution of primal probleme at final time
                //so it's the initial solution of the dual problem
                //in the case where orthonormalize_primal==true, even with initial solution in
                //the reduced basis we will commit an approximation in the online part
                //when initializing uN.
                //if initial solution is zero then the system the matrix used in online will be singular
                element_ptrtype primal_initial_field ( new element_type ( M_model->functionSpace() ) );

                M_model->initializationField( primal_initial_field, mu ); //fill initial_field
                int norm_ini = M_model->scalarProduct( *primal_initial_field, *primal_initial_field );

                if ( norm_ini != 0 )
                {
                    M_WN.push_back( *primal_initial_field );
                    M_WNdu.push_back( *dual_initial_field );
                }

                else
                {
                    norm_zero=true;
                }
            } // end of if ( M_N == 0 )

            //POD in time
            LOG(INFO)<<"[CRB::offline] start of POD \n";

            pod_ptrtype POD = pod_ptrtype( new pod_type(  ) );


            if ( M_mode_number == 1 )
            {
                //in this case, it's the first time that we add mu
                POD->setNm( M_Nm );
            }

            else
            {
                //in this case, mu has been chosen twice (at least)
                //so we add the M_mode_number^th mode in the basis
                POD->setNm( M_mode_number*M_Nm );
                int size = mu.size();
                LOG(INFO)<<"... CRB M_mode_number = "<<M_mode_number<<"\n";
                LOG(INFO)<<"for mu = [ ";

                for ( int i=0; i<size-1; i++ ) LOG(INFO)<<mu[i]<<" , ";

                LOG(INFO)<<mu[ size-1 ];
                LOG(INFO)<<" ]\n";

                double Tf = M_model->timeFinal();
                double dt = M_model->timeStep();
                int nb_mode_max = Tf/dt;

                if ( M_mode_number>=nb_mode_max-1 )
                {
                    std::cout<<"Error : we access to "<<M_mode_number<<"^th mode"<<std::endl;
                    std::cout<<"parameter choosen : [ ";

                    for ( int i=0; i<size-1; i++ ) std::cout<<mu[i]<<" , ";

                    std::cout<<mu[ size-1 ]<<" ] "<<std::endl;
                    throw std::logic_error( "[CRB::offline] ERROR during the construction of the reduced basis, one parameter has been choosen too many times" );
                }
            }

            POD->setBdf( M_bdf_primal_save );
            POD->setModel( M_model );
            POD->setTimeInitial( M_model->timeInitial() );
            mode_set_type ModeSet;

            size_type number_max_of_mode = POD->pod( ModeSet,true );

            if ( number_max_of_mode < M_Nm )
            {
                std::cout<<"With crb.Nm = "<<M_Nm<<" there was too much too small eigenvalues so the value";
                std::cout<<" of crb.Nm as been changed to "<<number_max_of_mode<<std::endl;
                M_Nm = number_max_of_mode;
            }

            //now : loop over number modes per mu

            if ( !seek_mu_in_complement )
            {
                for ( size_type i=0; i<M_Nm; i++ )
                    M_WN.push_back( ModeSet[M_mode_number*M_Nm-1+i] ) ;
                //M_WN.push_back( ModeSet[M_mode_number-1] ) ;
            }

            else
            {
                for ( size_type i=0; i<M_Nm; i++ )
                    M_WN.push_back( ModeSet[i] ) ;
            }

            //and now the dual
            if ( solve_dual_problem || M_error_type==CRB_RESIDUAL || M_error_type == CRB_RESIDUAL_SCM )
            {
                POD->setBdf( M_bdf_dual );
                POD->setTimeInitial( M_model->timeFinal()+M_model->timeStep() );
                mode_set_type ModeSetdu;
                POD->pod( ModeSetdu,false );

                if ( !seek_mu_in_complement )
                {
                    for ( size_type i=0; i<M_Nm; i++ )
                        M_WNdu.push_back( ModeSetdu[M_mode_number*M_Nm-1+i] ) ;
                }

                else
                {
                    for ( size_type i=0; i<M_Nm; i++ )
                        M_WNdu.push_back( ModeSetdu[i] ) ;
                }
            }

            else
            {
                element_ptrtype element_zero ( new element_type ( M_model->functionSpace() ) );

                for ( size_type i=0; i<M_Nm; i++ )
                {
                    M_WNdu.push_back( *element_zero ) ;
                }
            }


        }//end of transient case

        //in the case of transient problem, we can add severals modes for a same mu
        //Moreover, if the case where the initial condition is not zero and we don't orthonormalize elements in the basis,
        //we add the initial condition in the basis (so one more element)
        size_type number_of_added_elements = M_Nm + ( M_N==0 && orthonormalize_primal==false && norm_zero==false && !M_model->isSteady() );

        //in the case of steady problems, we add only one element
        if( M_model->isSteady() )
            number_of_added_elements=1;

        M_N+=number_of_added_elements;

        if ( orthonormalize_primal )
        {
            orthonormalize( M_N, M_WN, number_of_added_elements );
            orthonormalize( M_N, M_WN, number_of_added_elements );
            orthonormalize( M_N, M_WN, number_of_added_elements );
        }

        if ( orthonormalize_dual )
        {
            orthonormalize( M_N, M_WNdu, number_of_added_elements );
            orthonormalize( M_N, M_WNdu, number_of_added_elements );
            orthonormalize( M_N, M_WNdu, number_of_added_elements );
        }

        if( ! M_use_newton )
        {
            LOG(INFO) << "[CRB::offline] compute Aq_pr, Aq_du, Aq_pr_du" << "\n";

            for  (size_type q = 0; q < M_model->Qa(); ++q )
            {
                for( size_type m = 0; m < M_model->mMaxA(q); ++m )
                {
                    M_Aqm_pr[q][m].conservativeResize( M_N, M_N );
                    M_Aqm_du[q][m].conservativeResize( M_N, M_N );
                    M_Aqm_pr_du[q][m].conservativeResize( M_N, M_N );

                    // only compute the last line and last column of reduced matrices
                    for ( size_type i = M_N-number_of_added_elements; i < M_N; i++ )
                    {
                        for ( size_type j = 0; j < M_N; ++j )
                        {
                            M_Aqm_pr[q][m]( i, j ) = M_model->Aqm(q , m , M_WN[i], M_WN[j] );//energy
                            M_Aqm_du[q][m]( i, j ) = M_model->Aqm( q , m , M_WNdu[i], M_WNdu[j], true );
                            M_Aqm_pr_du[q][m]( i, j ) = M_model->Aqm(q , m , M_WNdu[i], M_WN[j] );
                        }
                    }

                    for ( size_type j=M_N-number_of_added_elements; j < M_N; j++ )
                    {
                        for ( size_type i = 0; i < M_N; ++i )
                        {
                            M_Aqm_pr[q][m]( i, j ) = M_model->Aqm(q , m , M_WN[i], M_WN[j] );
                            M_Aqm_du[q][m]( i, j ) = M_model->Aqm(q , m , M_WNdu[i], M_WNdu[j], true );
                            M_Aqm_pr_du[q][m]( i, j ) = M_model->Aqm(q , m , M_WNdu[i], M_WN[j] );
                        }
                    }
                }//loop over m
            }//loop over q

            LOG(INFO) << "[CRB::offline] compute Mq_pr, Mq_du, Mq_pr_du" << "\n";


            LOG(INFO) << "[CRB::offline] compute Fq_pr, Fq_du" << "\n";

            for ( size_type q = 0; q < M_model->Ql( 0 ); ++q )
            {
                for( size_type m = 0; m < M_model->mMaxF( 0, q ); ++m )
                {
                    M_Fqm_pr[q][m].conservativeResize( M_N );
                    M_Fqm_du[q][m].conservativeResize( M_N );
                    for ( size_type l = 1; l <= number_of_added_elements; ++l )
                    {
                        int index = M_N-l;
                        M_Fqm_pr[q][m]( index ) = M_model->Fqm( 0, q, m, M_WN[index] );
                        M_Fqm_du[q][m]( index ) = M_model->Fqm( 0, q, m, M_WNdu[index] );
                    }
                }//loop over m
            }//loop over q

        }//end of "if ! use_newton"


        if( M_use_newton )
        {
            LOG(INFO) << "[CRB::offline] compute Jq_pr " << "\n";

            for  (size_type q = 0; q < M_model->Qa(); ++q )
            {
                for( size_type m = 0; m < M_model->mMaxA(q); ++m )
                {
                    M_Jqm_pr[q][m].conservativeResize( M_N, M_N );

                    // only compute the last line and last column of reduced matrices
                    for ( size_type i = M_N-number_of_added_elements; i < M_N; i++ )
                    {
                        for ( size_type j = 0; j < M_N; ++j )
                        {
                            M_Jqm_pr[q][m]( i, j ) = Jqm[q][m]->energy( M_WN[i], M_WN[j] );
                        }
                    }

                    for ( size_type j=M_N-number_of_added_elements; j < M_N; j++ )
                    {
                        for ( size_type i = 0; i < M_N; ++i )
                        {
                            M_Jqm_pr[q][m]( i, j ) = Jqm[q][m]->energy( M_WN[i], M_WN[j] );
                        }
                    }
                }//loop over m
            }//loop over q


            LOG(INFO) << "[CRB::offline] compute Rq_pr" << "\n";

            for ( size_type q = 0; q < M_model->Ql( 0 ); ++q )
            {
                for( size_type m = 0; m < M_model->mMaxF( 0, q ); ++m )
                {
                    M_Rqm_pr[q][m].conservativeResize( M_N );
                    for ( size_type l = 1; l <= number_of_added_elements; ++l )
                    {
                        int index = M_N-l;
                        M_Rqm_pr[q][m]( index ) = M_model->Fqm( 0, q, m, M_WN[index] );
                    }
                }//loop over m
            }//loop over q

        }//end if use_newton case


        LOG(INFO) << "[CRB::offline] compute MFqm" << "\n";
        int q_max = M_model->QInitialGuess();
        for ( size_type q = 0; q < q_max; ++q )
        {
            int m_max =M_model->mMaxInitialGuess(q);
            for( size_type m = 0; m < m_max; ++m )
            {
                M_InitialGuessV_pr[q][m].conservativeResize( M_N );
                for ( size_type j = 0; j < M_N; ++j )
                    M_InitialGuessV_pr[q][m]( j ) = inner_product( *InitialGuessV[q][m] , M_WN[j] );
            }
        }


        for ( size_type q = 0; q < M_model->Qm(); ++q )
        {
            for( size_type m = 0; m < M_model->mMaxM(q); ++m )
            {
                M_Mqm_pr[q][m].conservativeResize( M_N, M_N );
                M_Mqm_du[q][m].conservativeResize( M_N, M_N );
                M_Mqm_pr_du[q][m].conservativeResize( M_N, M_N );

                // only compute the last line and last column of reduced matrices
                for ( size_type i=M_N-number_of_added_elements ; i < M_N; i++ )
                {
                    for ( size_type j = 0; j < M_N; ++j )
                    {
                        M_Mqm_pr[q][m]( i, j ) = M_model->Mqm(q , m , M_WN[i], M_WN[j] );
                        M_Mqm_du[q][m]( i, j ) = M_model->Mqm(q , m , M_WNdu[i], M_WNdu[j], true );
                        M_Mqm_pr_du[q][m]( i, j ) = M_model->Mqm( q , m , M_WNdu[i], M_WN[j] );
                    }
                }
                for ( size_type j = M_N-number_of_added_elements; j < M_N ; j++ )
                {
                    for ( size_type i = 0; i < M_N; ++i )
                    {
                        M_Mqm_pr[q][m]( i, j ) = M_model->Mqm(q , m , M_WN[i], M_WN[j] );
                        M_Mqm_du[q][m]( i, j ) = M_model->Mqm(q , m , M_WNdu[i], M_WNdu[j], true );
                        M_Mqm_pr_du[q][m]( i, j ) = M_model->Mqm(q , m , M_WNdu[i], M_WN[j] );
                    }
                }
            }//loop over m
        }//loop over q

        LOG(INFO) << "[CRB::offline] compute Lq_pr, Lq_du" << "\n";

        for ( size_type q = 0; q < M_model->Ql( M_output_index ); ++q )
        {
            for( size_type m = 0; m < M_model->mMaxF( M_output_index, q ); ++m )
            {
                M_Lqm_pr[q][m].conservativeResize( M_N );
                M_Lqm_du[q][m].conservativeResize( M_N );

                for ( size_type l = 1; l <= number_of_added_elements; ++l )
                {
                    int index = M_N-l;
                    M_Lqm_pr[q][m]( index ) = M_model->Fqm( M_output_index, q, m, M_WN[index] );
                    M_Lqm_du[q][m]( index ) = M_model->Fqm( M_output_index, q, m, M_WNdu[index] );
                }
            }//loop over m
        }//loop over q


        LOG(INFO) << "compute coefficients needed for the initialization of unknown in the online step\n";

        element_ptrtype primal_initial_field ( new element_type ( M_model->functionSpace() ) );
        element_ptrtype projection    ( new element_type ( M_model->functionSpace() ) );
        M_model->initializationField( primal_initial_field, mu ); //fill initial_field
        if ( model_type::is_time_dependent || !M_model->isSteady() )
        {
            if ( orthonormalize_primal )
            {
                for ( size_type elem=M_N-number_of_added_elements; elem<M_N; elem++ )
                {
                    //primal
                    double k =  M_model->scalarProduct( *primal_initial_field, M_WN[elem] );
                    M_coeff_pr_ini_online.push_back( k );
                }
            }

            else if ( !orthonormalize_primal )
            {
                matrixN_type MN ( ( int )M_N, ( int )M_N ) ;
                vectorN_type FN ( ( int )M_N );

                //primal
                for ( size_type i=0; i<M_N; i++ )
                {
                    for ( size_type j=0; j<i; j++ )
                    {
                        MN( i,j ) = M_model->scalarProduct( M_WN[j] , M_WN[i] );
                        MN( j,i ) = MN( i,j );
                    }

                    MN( i,i ) = M_model->scalarProduct( M_WN[i] , M_WN[i] );
                    FN( i ) = M_model->scalarProduct( *primal_initial_field,M_WN[i] );
                }

                vectorN_type projectionN ( ( int ) M_N );
                projectionN = MN.lu().solve( FN );

                for ( size_type i=M_N-number_of_added_elements; i<M_N; i++ )
                {
                    M_coeff_pr_ini_online.push_back( projectionN( i ) );
                }
            }

            if ( solve_dual_problem )
            {
                if ( orthonormalize_dual )
                {
                    for ( size_type elem=M_N-number_of_added_elements; elem<M_N; elem++ )
                    {
                        double k =  M_model->scalarProduct( *dual_initial_field, M_WNdu[elem] );
                        M_coeff_du_ini_online.push_back( k );
                    }
                }

                else if ( !orthonormalize_dual )
                {
                    matrixN_type MNdu ( ( int )M_N, ( int )M_N ) ;
                    vectorN_type FNdu ( ( int )M_N );

                    //dual
                    for ( size_type i=0; i<M_N; i++ )
                    {
                        for ( size_type j=0; j<i; j++ )
                        {
                            MNdu( i,j ) = M_model->scalarProduct( M_WNdu[j] , M_WNdu[i] );
                            MNdu( j,i ) = MNdu( i,j );
                        }

                        MNdu( i,i ) = M_model->scalarProduct( M_WNdu[i] , M_WNdu[i] );
                        FNdu( i ) = M_model->scalarProduct( *dual_initial_field,M_WNdu[i] );
                    }

                    vectorN_type projectionN ( ( int ) M_N );
                    projectionN = MNdu.lu().solve( FNdu );

                    for ( size_type i=M_N-number_of_added_elements; i<M_N; i++ )
                    {
                        M_coeff_du_ini_online.push_back( projectionN( i ) );
                    }
                }
            }
        }

        timer2.restart();

        M_compute_variance = this->vm()["crb.compute-variance"].template as<bool>();
        if ( M_database_contains_variance_info )
            throw std::logic_error( "[CRB::offline] ERROR : build variance is not actived" );
        //buildVarianceMatrixPhi( M_N );

        if ( M_error_type==CRB_RESIDUAL || M_error_type == CRB_RESIDUAL_SCM )
        {
            if( Environment::worldComm().globalRank() == Environment::worldComm().masterRank() )
                std::cout << "  -- offlineResidual update starts\n";
            offlineResidual( M_N, number_of_added_elements );
            LOG(INFO)<<"[CRB::offline] end of call offlineResidual and M_N = "<< M_N <<"\n";
            if( Environment::worldComm().globalRank() == Environment::worldComm().masterRank() )
                std::cout << "  -- offlineResidual updated in " << timer2.elapsed() << "s\n";
            timer2.restart();
        }


        if ( M_error_type == CRB_NO_RESIDUAL && ! use_predefined_WNmu )
        {
            //M_maxerror=M_iter_max-M_N;

            bool already_exist;
            do
            {
                //initialization
                already_exist=false;
                //pick randomly an element
                mu = M_Dmu->element();
                //make sure that the new mu is not already is M_WNmu
                BOOST_FOREACH( auto _mu, *M_WNmu )
                {
                    if( mu == _mu )
                        already_exist=true;
                }
            }
            while( already_exist );

            M_current_mu = mu;
        }
        else if ( use_predefined_WNmu )
        {
            //remmber that in this case M_iter_max = sampling size
            if( M_N < M_iter_max )
            {
                mu = M_WNmu->at( M_N );
                M_current_mu = mu;
            }
        }
        else
        {
            boost::tie( M_maxerror, mu, index , delta_pr , delta_du ) = maxErrorBounds( M_N );

            M_index.push_back( index );
            M_current_mu = mu;

            int count = std::count( M_index.begin(),M_index.end(),index );
            M_mode_number = count;

            if( Environment::worldComm().globalRank() == Environment::worldComm().masterRank() )
                std::cout << "  -- max error bounds computed in " << timer2.elapsed() << "s\n";

            timer2.restart();
        }

        M_rbconv.insert( convergence( M_N, boost::make_tuple(M_maxerror,delta_pr,delta_du) ) );

        //mu = M_Xi->at( M_N );//M_WNmu_complement->min().template get<0>();

        check( M_WNmu->size() );

        if ( this->vm()["crb.check.rb"].template as<int>() == 1 )std::cout << "  -- check reduced basis done in " << timer2.elapsed() << "s\n";

        timer2.restart();
        LOG(INFO) << "time: " << timer.elapsed() << "\n";
        if( proc_number == 0 ) std::cout << "============================================================\n";
        LOG(INFO) <<"========================================"<<"\n";

        //save DB after adding an element
        this->saveDB();
	M_elements_database.setWn( boost::make_tuple( M_WN , M_WNdu ) );
	M_elements_database.saveDB();

    }

    if( proc_number == 0 )
        std::cout<<"number of elements in the reduced basis : "<<M_N<<" ( nb proc : "<<worldComm().globalSize()<<")"<<std::endl;
    LOG(INFO) << " index choosen : ";
    BOOST_FOREACH( auto id, M_index )
    LOG(INFO)<<id<<" ";
    LOG(INFO)<<"\n";
    bool visualize_basis = this->vm()["crb.visualize-basis"].template as<bool>() ;

    if ( visualize_basis )
    {
        std::vector<wn_type> wn;
        std::vector<std::string> names;
        wn.push_back( M_WN );
        names.push_back( "primal" );
        wn.push_back( M_WNdu );
        names.push_back( "dual" );
        exportBasisFunctions( boost::make_tuple( wn ,names ) );

        if ( orthonormalize_primal || orthonormalize_dual )
        {
            std::cout<<"[CRB::offline] Basis functions have been exported but warning elements have been orthonormalized"<<std::endl;
        }
    }


    //    this->saveDB();
    //if( proc_number == 0 ) std::cout << "Offline CRB is done\n";

    return M_rbconv;

}


template<typename TruthModelType>
void
CRB<TruthModelType>::checkInitialGuess( const element_type expansion_uN , parameter_type const& mu, vectorN_type & error) const
{
    static const bool is_composite = functionspace_type::is_composite;
    checkInitialGuess( expansion_uN, mu, error , mpl::bool_< is_composite >() );
}

template<typename TruthModelType>
void
CRB<TruthModelType>::checkInitialGuess( const element_type expansion_uN , parameter_type const& mu, vectorN_type & error, mpl::bool_<false>) const
{
    error.resize(1);
    const element_ptrtype initial_guess = M_model->assembleInitialGuess( mu );
    auto Xh = expansion_uN.functionSpace();
    auto mesh = Xh->mesh();
    error(0) = math::sqrt(
                          integrate( _range=elements(mesh) ,
                                     _expr=vf::idv( initial_guess ) * vf::idv( expansion_uN )
                                     * vf::idv( initial_guess ) * vf::idv( expansion_uN )
                                     ).evaluate()(0,0)
                          );


}

template<typename TruthModelType>
void
CRB<TruthModelType>::checkInitialGuess( const element_type expansion_uN , parameter_type const& mu, vectorN_type & error, mpl::bool_<true>) const
{
    //using namespace Feel::vf;
    index_vector_type index_vector;
    const element_ptrtype initial_guess = M_model->assembleInitialGuess( mu );
    ComputeIntegralsSquare compute_integrals_square( *initial_guess , expansion_uN );
    fusion::for_each( index_vector , compute_integrals_square );
    error.resize( functionspace_type::nSpaces );
    error = compute_integrals_square.vectorErrors();

}


template<typename TruthModelType>
void
CRB<TruthModelType>::buildVarianceMatrixPhi( int const N )
{
    static const bool is_composite = functionspace_type::is_composite;
    return buildVarianceMatrixPhi( N , mpl::bool_< is_composite >() );
}
template<typename TruthModelType>
void
CRB<TruthModelType>::buildVarianceMatrixPhi( int const N , mpl::bool_<true> )
{

    std::vector<std::string> s;
    static const int nb_spaces = functionspace_type::nSpaces;

    if( N == 1 )
        M_variance_matrix_phi.resize( nb_spaces );


    for(int i=0;i<nb_spaces;i++)
        M_variance_matrix_phi[i].conservativeResize( M_N , M_N );

    //introduction of the new variable phi
    int size_WN = M_WN.size();
    wn_type phi;

    for(int i = 0; i < size_WN; ++i)
    {
        auto sub = subelements( M_WN[i] , s );
        element_type global_element = M_model->functionSpace()->element();
        wn_type v( nb_spaces , global_element);
        ComputePhi compute_phi(v);
        fusion::for_each( sub , compute_phi ) ;
        auto vect = compute_phi.vectorPhi();

        //now we want to have only one element_type (global_element)
        //which sum the contribution of each space
        global_element.zero();
        BOOST_FOREACH( auto element , vect )
            global_element += element;
        phi.push_back( global_element );
    }


    index_vector_type index_vector;
    for(int space=0; space<nb_spaces; space++)
        M_variance_matrix_phi[space].conservativeResize( N , N );


    for(int i = 0; i < M_N; ++i)
    {

        for(int j = i+1; j < M_N; ++j)
        {
            ComputeIntegrals compute_integrals( phi[i] , phi[j] );
            fusion::for_each( index_vector , compute_integrals );
            auto vect = compute_integrals.vectorIntegrals();
            for(int space=0; space<nb_spaces; space++)
                M_variance_matrix_phi[space](i,j)=vect[space];
        } //j
        ComputeIntegrals compute_integrals ( phi[i] , phi[i] );
        fusion::for_each( index_vector , compute_integrals );
        auto vect = compute_integrals.vectorIntegrals();
        for(int space=0; space<nb_spaces; space++)
            M_variance_matrix_phi[space](i,i)=vect[space];
    }// i

}


template<typename TruthModelType>
void
CRB<TruthModelType>::buildVarianceMatrixPhi( int const N , mpl::bool_<false> )
{
    std::vector<std::string> s;
    int nb_spaces = functionspace_type::nSpaces;

    if( N == 1 )
        M_variance_matrix_phi.resize( nb_spaces );

    M_variance_matrix_phi[0].conservativeResize( M_N , M_N );

    //introduction of the new variable phi
    int size_WN = M_WN.size();
    //wn_type phi ( size_WN );
    wn_type phi;

    mesh_ptrtype mesh = M_WN[0].functionSpace()->mesh();

    for(int i = 0; i < size_WN; ++i)
    {
        double surface = integrate( _range=elements(mesh), _expr=vf::cst(1.) ).evaluate()(0,0);
        double mean =  integrate( _range=elements(mesh), _expr=vf::idv( M_WN[i] ) ).evaluate()(0,0);
        mean /= surface;
        auto element_mean = vf::project(M_WN[i].functionSpace(), elements(mesh), vf::cst(mean) );
        phi.push_back( M_WN[i] - element_mean );
    }

    for(int space=0; space<nb_spaces; space++)
        M_variance_matrix_phi[space].conservativeResize( N , N );

    for(int i = 0; i < M_N; ++i)
    {
        for(int j = i+1; j < M_N; ++j)
        {
            M_variance_matrix_phi[0]( i , j ) = integrate( _range=elements(mesh) , _expr=vf::idv( phi[i] ) * vf::idv( phi[j] ) ).evaluate()(0,0);
            M_variance_matrix_phi[0]( j , i ) =  M_variance_matrix_phi[0]( i , j );
        } //j

        M_variance_matrix_phi[0]( i , i ) = integrate( _range=elements(mesh) , _expr=vf::idv( phi[i] ) * vf::idv( phi[i] ) ).evaluate()(0,0);
    }// i

}



template<typename TruthModelType>
void
CRB<TruthModelType>::buildFunctionFromRbCoefficients(int N, std::vector< vectorN_type > const & RBcoeff, wn_type const & WN, std::vector<element_ptrtype> & FEMsolutions )
{

    if( WN.size() == 0 )
        throw std::logic_error( "[CRB::buildFunctionFromRbCoefficients] ERROR : reduced basis space is empty" );


    int nb_solutions = RBcoeff.size();

    for( int i = 0; i < nb_solutions; i++ )
    {
        element_ptrtype FEMelement ( new element_type( M_model->functionSpace() ) );
        FEMelement->setZero();
        for( int j = 0; j < N; j++ )
            FEMelement->add( RBcoeff[i](j) , WN[j] );
        FEMsolutions.push_back( FEMelement );
    }
}

template<typename TruthModelType>
void
CRB<TruthModelType>::compareResidualsForTransientProblems( int N, parameter_type const& mu, std::vector<element_ptrtype> const & Un, std::vector<element_ptrtype> const & Unold, std::vector<element_ptrtype> const& Undu, std::vector<element_ptrtype> const & Unduold, std::vector< std::vector<double> > const& primal_residual_coeffs,  std::vector < std::vector<double> > const& dual_residual_coeffs ) const
{

    LOG( INFO ) <<"\n compareResidualsForTransientProblems \n";

    backend_ptrtype backend = backend_type::build( BACKEND_PETSC ) ;

    if ( M_model->isSteady() )
    {
        throw std::logic_error( "[CRB::compareResidualsForTransientProblems] ERROR : to check residual in a steady case, use checkResidual and not compareResidualsForTransientProblems" );
    }

    sparse_matrix_ptrtype A,AM,M,Adu;
    //vector_ptrtype MF;
    std::vector<vector_ptrtype> F,L;

    vector_ptrtype Rhs( backend->newVector( M_model->functionSpace() ) );
    vector_ptrtype Aun( backend->newVector( M_model->functionSpace() ) );
    vector_ptrtype AduUn( backend->newVector( M_model->functionSpace() ) );

    vector_ptrtype Mun( backend->newVector( M_model->functionSpace() ) );
    vector_ptrtype Munold( backend->newVector( M_model->functionSpace() ) );
    vector_ptrtype Frhs( backend->newVector( M_model->functionSpace() ) );
    vector_ptrtype un( backend->newVector( M_model->functionSpace() ) );
    vector_ptrtype unold( backend->newVector( M_model->functionSpace() ) );
    vector_ptrtype undu( backend->newVector( M_model->functionSpace() ) );
    vector_ptrtype unduold( backend->newVector( M_model->functionSpace() ) );

    //set parameters for time discretization
    auto bdf_primal = bdf( _space=M_model->functionSpace(), _vm=this->vm() , _name="bdf_primal_check_residual_transient" );
    bdf_primal->setTimeInitial( M_model->timeInitial() );
    bdf_primal->setTimeStep( M_model->timeStep() );
    bdf_primal->setTimeFinal( M_model->timeFinal() );
    bdf_primal->setOrder( M_model->timeOrder() );


    double time_step = bdf_primal->timeStep();
    int time_index=0;
    for ( bdf_primal->start() ; !bdf_primal->isFinished(); bdf_primal->next() )
    {
        auto bdf_poly = bdf_primal->polyDeriv();

        boost::tie( M, A, F) = M_model->update( mu , bdf_primal->time() );

        A->close();

        *un = *Un[time_index];
        *unold = *Unold[time_index];
        A->multVector( un, Aun );
        M->multVector( un, Mun );
        M->multVector( unold, Munold );
        Aun->scale( -1 );
        Mun->scale( -1 );
        *Frhs = *F[0];

        vector_ptrtype __ef_pr(  backend->newVector( M_model->functionSpace() ) );
        vector_ptrtype __ea_pr(  backend->newVector( M_model->functionSpace() ) );
        vector_ptrtype __emu_pr(  backend->newVector( M_model->functionSpace() ) );
        vector_ptrtype __emuold_pr(  backend->newVector( M_model->functionSpace() ) );
        M_model->l2solve( __ef_pr, Frhs );
        M_model->l2solve( __ea_pr, Aun );
        M_model->l2solve( __emu_pr, Mun );
        M_model->l2solve( __emuold_pr, Munold );

        double check_Cff_pr = M_model->scalarProduct( __ef_pr,__ef_pr );
        double check_Caf_pr = 2*M_model->scalarProduct( __ef_pr,__ea_pr );
        double check_Caa_pr = M_model->scalarProduct( __ea_pr,__ea_pr );
        double check_Cmf_pr = 2./time_step*( M_model->scalarProduct( __emu_pr , __ef_pr )+M_model->scalarProduct( __emuold_pr , __ef_pr ) );
        double check_Cma_pr = 2./time_step*( M_model->scalarProduct( __emu_pr , __ea_pr )+M_model->scalarProduct( __emuold_pr , __ea_pr ) );
        double check_Cmm_pr = 1./(time_step*time_step)*( M_model->scalarProduct( __emu_pr , __emu_pr ) + 2*M_model->scalarProduct( __emu_pr , __emuold_pr ) + M_model->scalarProduct( __emuold_pr , __emuold_pr ) );

        double Cff_pr = primal_residual_coeffs[time_index][0];
        double Caf_pr = primal_residual_coeffs[time_index][1];
        double Caa_pr = primal_residual_coeffs[time_index][2];
        double Cmf_pr = primal_residual_coeffs[time_index][3];
        double Cma_pr = primal_residual_coeffs[time_index][4];
        double Cmm_pr = primal_residual_coeffs[time_index][5];

        LOG(INFO)<<" --- time : "<<bdf_primal->time()<<std::endl;
        LOG(INFO)<<"Cff : "<< check_Cff_pr <<"  -  "<<Cff_pr<<"  =>  "<<check_Cff_pr-Cff_pr<<std::endl;
        LOG(INFO)<<"Caf : "<< check_Caf_pr <<"  -  "<<Caf_pr<<"  =>  "<<check_Caf_pr-Caf_pr<<std::endl;
        LOG(INFO)<<"Caa : "<< check_Caa_pr <<"  -  "<<Caa_pr<<"  =>  "<<check_Caa_pr-Caa_pr<<std::endl;
        LOG(INFO)<<"Cmf : "<< check_Cmf_pr <<"  -  "<<Cmf_pr<<"  =>  "<<check_Cmf_pr-Cmf_pr<<std::endl;
        LOG(INFO)<<"Cma : "<< check_Cma_pr <<"  -  "<<Cma_pr<<"  =>  "<<check_Cma_pr-Cma_pr<<std::endl;
        LOG(INFO)<<"Cmm : "<< check_Cmm_pr <<"  -  "<<Cmm_pr<<"  =>  "<<check_Cmm_pr-Cmm_pr<<std::endl;
        time_index++;
    }

    time_index--;

    bool solve_dual_problem = this->vm()["crb.solve-dual-problem"].template as<bool>();
    //if( this->worldComm().globalSize() > 1 )
    //    solve_dual_problem=false;

    double sum=0;
    if( solve_dual_problem )
    {

        Adu = M_model->newMatrix();

        auto bdf_dual = bdf( _space=M_model->functionSpace(), _vm=this->vm() , _name="bdf_dual_check_residual_transient" );

        bdf_dual->setTimeInitial( M_model->timeFinal()+M_model->timeStep() );
        bdf_dual->setTimeStep( -M_model->timeStep() );
        bdf_dual->setTimeFinal( M_model->timeInitial()+M_model->timeStep() );
        bdf_dual->setOrder( M_model->timeOrder() );
        bdf_dual->start();

        //element_ptrtype dual_initial_field( new element_type( M_model->functionSpace() ) );
        LOG(INFO)<<"**********dual problem************* "<<std::endl;

        boost::tie( M, A, F) = M_model->update( mu , bdf_dual->timeInitial() );

        vectorN_type dual_initial ( N );
        for(int i=0; i<N; i++)
            dual_initial(i) = M_coeff_du_ini_online[i];
        auto dual_initial_field = this->expansion( dual_initial , N , M_WNdu );

        *undu = dual_initial_field;
        M->multVector( undu, Mun );
        *Frhs = *F[0];


        auto R = backend->newVector( M_model->functionSpace() );
        R = Frhs;
        R->add( -1 , *Mun ); //R -= Mun;
        //std::cout<<"[COMPARE] R->l2Norm() : "<<R->l2Norm()<<std::endl;

        vector_ptrtype __ef_du(  backend->newVector( M_model->functionSpace() ) );
        vector_ptrtype __emu_du(  backend->newVector( M_model->functionSpace() ) );
        M_model->l2solve( __ef_du, Frhs );
        M_model->l2solve( __emu_du, Mun );
        double check_Cff_du = M_model->scalarProduct( __ef_du,__ef_du );
        double check_Cmf_du = 2*M_model->scalarProduct( __ef_du,__emu_du );
        double check_Cmm_du = M_model->scalarProduct( __emu_du,__emu_du );
        double residual_final_condition = math::abs( check_Cff_du + check_Cmf_du + check_Cmm_du );
        //std::cout<<"residual on final condition : "<<residual_final_condition<<std::endl;
        //initialization
        time_step = bdf_dual->timeStep();


        for ( bdf_dual->start(); !bdf_dual->isFinished() ; bdf_dual->next() )
        {
            auto bdf_poly = bdf_dual->polyDeriv();

            boost::tie( M, A, F ) = M_model->update( mu , bdf_dual->time() );
            if( option("crb.use-symmetric-matrix").template as<bool>() )
                Adu = A;
            else
                A->transpose( Adu );
            *undu = *Undu[time_index];
            *unduold = *Unduold[time_index];
            Adu->multVector( undu, AduUn );
            M->multVector( undu, Mun );
            M->multVector( unduold, Munold );
            AduUn->scale( -1 );
            Munold->scale( -1 );
            *Frhs = *F[0];

            vector_ptrtype __ea_du(  backend->newVector( M_model->functionSpace() ) );
            vector_ptrtype __emu_du(  backend->newVector( M_model->functionSpace() ) );
            vector_ptrtype __emuold_du(  backend->newVector( M_model->functionSpace() ) );
            M_model->l2solve( __ea_du, AduUn );
            M_model->l2solve( __emu_du, Mun );
            M_model->l2solve( __emuold_du, Munold );
            double check_Caa_du = M_model->scalarProduct( __ea_du,__ea_du );
            double check_Cma_du = 2./time_step*( M_model->scalarProduct( __emu_du , __ea_du )+M_model->scalarProduct( __emuold_du , __ea_du ) );
            double check_Cmm_du = 1./(time_step*time_step)*( M_model->scalarProduct( __emu_du , __emu_du ) + 2*M_model->scalarProduct( __emu_du , __emuold_du ) + M_model->scalarProduct( __emuold_du , __emuold_du ) );


            double Cff_du =  dual_residual_coeffs[time_index][0];
            double Caf_du =  dual_residual_coeffs[time_index][1];
            double Caa_du =  dual_residual_coeffs[time_index][2];
            double Cmf_du =  dual_residual_coeffs[time_index][3];
            double Cma_du =  dual_residual_coeffs[time_index][4];
            double Cmm_du =  dual_residual_coeffs[time_index][5];
            LOG(INFO)<<" --- time : "<<bdf_dual->time()<<std::endl;
            LOG(INFO)<<"Caa : "<< check_Caa_du <<"  -  "<<Caa_du<<"  =>  "<<check_Caa_du-Caa_du<<std::endl;
            LOG(INFO)<<"Cma : "<< check_Cma_du <<"  -  "<<Cma_du<<"  =>  "<<check_Cma_du-Cma_du<<std::endl;
            LOG(INFO)<<"Cmm : "<< check_Cmm_du <<"  -  "<<Cmm_du<<"  =>  "<<check_Cmm_du-Cmm_du<<std::endl;
            time_index--;
            //std::cout<<"[CHECK] ------ time "<<bdf_dual->time()<<std::endl;
            //std::cout<<"[CHECK] Caa_du : "<<check_Caa_du<<std::endl;
            //std::cout<<"[CHECK] Cma_du : "<<check_Cma_du<<std::endl;
            //std::cout<<"[CHECK] Cmm_du : "<<check_Cmm_du<<std::endl;
            sum += math::abs( check_Caa_du + check_Cma_du + check_Cmm_du );
        }
        //std::cout<<"[CHECK] dual_sum : "<<sum<<std::endl;
    }//solve-dual-problem
}


template<typename TruthModelType>
void
CRB<TruthModelType>::checkResidual( parameter_type const& mu, std::vector< std::vector<double> > const& primal_residual_coeffs, std::vector< std::vector<double> > const& dual_residual_coeffs  ) const
{


    if ( orthonormalize_primal || orthonormalize_dual )
    {
        throw std::logic_error( "[CRB::checkResidual] ERROR : to check residual don't use orthonormalization" );
    }

    if ( !M_model->isSteady() )
    {
        throw std::logic_error( "[CRB::checkResidual] ERROR : to check residual select steady state" );
    }

    if ( M_error_type==CRB_NO_RESIDUAL || M_error_type==CRB_EMPIRICAL )
    {
        throw std::logic_error( "[CRB::checkResidual] ERROR : to check residual set option crb.error-type to 0 or 1" );
    }

    int size = mu.size();

    if ( 0 )
    {
        std::cout<<"[CRB::checkResidual] use mu = [";
        for ( int i=0; i<size-1; i++ ) std::cout<< mu[i] <<" , ";
        std::cout<< mu[size-1]<<" ]"<<std::endl;
    }

    sparse_matrix_ptrtype A,At,M;
    //vector_ptrtype MF;
    std::vector<vector_ptrtype> F,L;

    backend_ptrtype backendA = backend_type::build( BACKEND_PETSC );
    backend_ptrtype backendAt = backend_type::build( BACKEND_PETSC );

    element_ptrtype u( new element_type( M_model->functionSpace() ) );
    element_ptrtype udu( new element_type( M_model->functionSpace() ) );
    vector_ptrtype U( M_backend->newVector( M_model->functionSpace() ) );
    vector_ptrtype Rhs( M_backend->newVector( M_model->functionSpace() ) );

    boost::timer timer, timer2;

    boost::tie( boost::tuples::ignore, A, F, boost::tuples::ignore ) = M_model->update( mu );

    LOG(INFO) << "  -- updated model for parameter in " << timer2.elapsed() << "s\n";
    timer2.restart();

    LOG(INFO) << "[CRB::checkResidual] transpose primal matrix" << "\n";
    At = M_model->newMatrix();
    if( option("crb.use-symmetric-matrix").template as<bool>() )
        At = A;
    else
        A->transpose( At );
    u->setName( ( boost::format( "fem-primal-%1%" ) % ( M_N ) ).str() );
    udu->setName( ( boost::format( "fem-dual-%1%" ) % ( M_N ) ).str() );

    LOG(INFO) << "[CRB::checkResidual] solving primal" << "\n";
    backendA->solve( _matrix=A,  _solution=u, _rhs=F[0] );
    LOG(INFO) << "  -- primal problem solved in " << timer2.elapsed() << "s\n";
    timer2.restart();
    *Rhs = *F[M_output_index];
    Rhs->scale( -1 );
    backendAt->solve( _matrix=At,  _solution=udu, _rhs=Rhs );
    LOG(INFO) << "  -- dual problem solved in " << timer2.elapsed() << "s\n";
    timer2.restart();


    vector_ptrtype Aun( M_backend->newVector( M_model->functionSpace() ) );
    vector_ptrtype Atun( M_backend->newVector( M_model->functionSpace() ) );
    vector_ptrtype Un( M_backend->newVector( M_model->functionSpace() ) );
    vector_ptrtype Undu( M_backend->newVector( M_model->functionSpace() ) );
    vector_ptrtype Frhs( M_backend->newVector( M_model->functionSpace() ) );
    vector_ptrtype Lrhs( M_backend->newVector( M_model->functionSpace() ) );
    *Un = *u;
    *Undu = *udu;
    A->multVector( Un, Aun );
    At->multVector( Undu, Atun );
    Aun->scale( -1 );
    Atun->scale( -1 );
    *Frhs = *F[0];
    *Lrhs = *F[M_output_index];
    LOG(INFO) << "[CRB::checkResidual] residual (f,f) " << M_N-1 << ":=" << M_model->scalarProduct( Frhs, Frhs ) << "\n";
    LOG(INFO) << "[CRB::checkResidual] residual (f,A) " << M_N-1 << ":=" << 2*M_model->scalarProduct( Frhs, Aun ) << "\n";
    LOG(INFO) << "[CRB::checkResidual] residual (A,A) " << M_N-1 << ":=" << M_model->scalarProduct( Aun, Aun ) << "\n";

    LOG(INFO) << "[CRB::checkResidual] residual (l,l) " << M_N-1 << ":=" << M_model->scalarProduct( Lrhs, Lrhs ) << "\n";
    LOG(INFO) << "[CRB::checkResidual] residual (l,At) " << M_N-1 << ":=" << 2*M_model->scalarProduct( Lrhs, Atun ) << "\n";
    LOG(INFO) << "[CRB::checkResidual] residual (At,At) " << M_N-1 << ":=" << M_model->scalarProduct( Atun, Atun ) << "\n";


    Lrhs->scale( -1 );


    vector_ptrtype __ef_pr(  M_backend->newVector( M_model->functionSpace() ) );
    vector_ptrtype __ea_pr(  M_backend->newVector( M_model->functionSpace() ) );
    M_model->l2solve( __ef_pr, Frhs );
    M_model->l2solve( __ea_pr, Aun );
    double check_C0_pr = M_model->scalarProduct( __ef_pr,__ef_pr );
    double check_Lambda_pr = 2*M_model->scalarProduct( __ef_pr,__ea_pr );
    double check_Gamma_pr = M_model->scalarProduct( __ea_pr,__ea_pr );

    vector_ptrtype __ef_du(  M_backend->newVector( M_model->functionSpace() ) );
    vector_ptrtype __ea_du(  M_backend->newVector( M_model->functionSpace() ) );
    M_model->l2solve( __ef_du, Lrhs );
    M_model->l2solve( __ea_du, Atun );
    double check_C0_du = M_model->scalarProduct( __ef_du,__ef_du );
    double check_Lambda_du = 2*M_model->scalarProduct( __ef_du,__ea_du );
    double check_Gamma_du = M_model->scalarProduct( __ea_du,__ea_du );

    double primal_sum =  check_C0_pr + check_Lambda_pr + check_Gamma_pr ;
    double dual_sum   =  check_C0_du + check_Lambda_du + check_Gamma_du ;

    LOG(INFO)<<"[CRB::checkResidual] primal_sum = "<<check_C0_pr<<" + "<<check_Lambda_pr<<" + "<<check_Gamma_pr<<" = "<<primal_sum<<"\n";
    LOG(INFO)<<"[CRB::checkResidual] dual_sum = "<<check_C0_du<<" + "<<check_Lambda_du<<" + "<<check_Gamma_du<<" = "<<dual_sum<<"\n";


    int time_index=0;
    double C0_pr = primal_residual_coeffs[time_index][0];
    double Lambda_pr = primal_residual_coeffs[time_index][1];
    double Gamma_pr = primal_residual_coeffs[time_index][2];

    double C0_du,Lambda_du,Gamma_du;
    C0_du = dual_residual_coeffs[time_index][0];
    Lambda_du = dual_residual_coeffs[time_index][1];
    Gamma_du = dual_residual_coeffs[time_index][2];

    double err_C0_pr = math::abs( C0_pr - check_C0_pr ) ;
    double err_Lambda_pr = math::abs( Lambda_pr - check_Lambda_pr ) ;
    double err_Gamma_pr = math::abs( Gamma_pr - check_Gamma_pr ) ;
    double err_C0_du = math::abs( C0_du - check_C0_du ) ;
    double err_Lambda_du = math::abs( Lambda_du - check_Lambda_du ) ;
    double err_Gamma_du = math::abs( Gamma_du - check_Gamma_du ) ;

    int start_dual_index = 6;

    std::cout<<"[CRB::checkResidual]"<<std::endl;
    std::cout<<"====primal coefficients==== "<<std::endl;
    std::cout<<"              c0_pr \t\t lambda_pr \t\t gamma_pr"<<std::endl;
    std::cout<<"computed : ";

    for ( int i=0; i<3; i++ ) std::cout<<std::setprecision( 16 )<<primal_residual_coeffs[time_index][i]<<"\t";

    std::cout<<"\n";
    std::cout<<"true     : ";
    std::cout<<std::setprecision( 16 )<<check_C0_pr<<"\t"<<check_Lambda_pr<<"\t"<<check_Gamma_pr<<"\n"<<std::endl;
    std::cout<<"====dual coefficients==== "<<std::endl;
    std::cout<<"              c0_du \t\t lambda_du \t\t gamma_du"<<std::endl;
    std::cout<<"computed : ";

    for ( int i=0; i<3; i++ ) std::cout<<std::setprecision( 16 )<<dual_residual_coeffs[time_index][i]<<"\t";

    std::cout<<"\ntrue     : ";
    std::cout<<std::setprecision( 16 )<<check_C0_du<<"\t"<<check_Lambda_du<<"\t"<<check_Gamma_du<<"\n"<<std::endl;
    std::cout<<std::setprecision( 16 )<<"primal_true_sum = "<<check_C0_pr+check_Lambda_pr+check_Gamma_pr<<std::endl;
    std::cout<<std::setprecision( 16 )<<"dual_true_sum = "<<check_C0_du+check_Lambda_du+check_Gamma_du<<"\n"<<std::endl;

    std::cout<<std::setprecision( 16 )<<"primal_computed_sum = "<<C0_pr+Lambda_pr+Gamma_pr<<std::endl;
    std::cout<<std::setprecision( 16 )<<"dual_computed_sum = "<<C0_du+Lambda_du+Gamma_du<<"\n"<<std::endl;

    std::cout<<"errors committed on coefficients "<<std::endl;
    std::cout<<std::setprecision( 16 )<<"C0_pr : "<<err_C0_pr<<"\tLambda_pr : "<<err_Lambda_pr<<"\tGamma_pr : "<<err_Gamma_pr<<std::endl;
    std::cout<<std::setprecision( 16 )<<"C0_du : "<<err_C0_du<<"\tLambda_du : "<<err_Lambda_du<<"\tGamma_du : "<<err_Gamma_du<<std::endl;
    std::cout<<"and now relative error : "<<std::endl;
    double errC0pr = err_C0_pr/check_C0_pr;
    double errLambdapr = err_Lambda_pr/check_Lambda_pr;
    double errGammapr = err_Gamma_pr/check_Gamma_pr;
    double errC0du = err_C0_du/check_C0_du;
    double errLambdadu = err_Lambda_pr/check_Lambda_pr;
    double errGammadu = err_Gamma_pr/check_Gamma_pr;
    std::cout<<std::setprecision( 16 )<<errC0pr<<"\t"<<errLambdapr<<"\t"<<errGammapr<<std::endl;
    std::cout<<std::setprecision( 16 )<<errC0du<<"\t"<<errLambdadu<<"\t"<<errGammadu<<std::endl;

    //residual r(v)
    Aun->add( *Frhs );
    //Lrhs->scale( -1 );
    Atun->add( *Lrhs );
    //to have dual norm of residual we need to solve ( e , v ) = r(v) and then it's given by ||e||
    vector_ptrtype __e_pr(  M_backend->newVector( M_model->functionSpace() ) );
    M_model->l2solve( __e_pr, Aun );
    double dual_norm_pr = math::sqrt ( M_model->scalarProduct( __e_pr,__e_pr ) );
    std::cout<<"[CRB::checkResidual] dual norm of primal residual without isolate terms (c0,lambda,gamma) = "<<dual_norm_pr<<"\n";
    //idem for the dual equation
    vector_ptrtype __e_du(  M_backend->newVector( M_model->functionSpace() ) );
    M_model->l2solve( __e_du, Atun );
    double dual_norm_du = math::sqrt ( M_model->scalarProduct( __e_du,__e_du ) );
    std::cout <<"[CRB::checkResidual] dual norm of dual residual without isolate terms = "<<dual_norm_du<<"\n";

    double err_primal = math::sqrt ( M_model->scalarProduct( Aun, Aun ) );
    double err_dual = math::sqrt ( M_model->scalarProduct( Atun, Atun ) );
    LOG(INFO) << "[CRB::checkResidual] true primal residual for reduced basis function " << M_N-1 << ":=" << err_primal << "\n";
    LOG(INFO) << "[CRB::checkResidual] true dual residual for reduced basis function " << M_N-1 << ":=" << err_dual << "\n";

}


template<typename TruthModelType>
void
CRB<TruthModelType>::check( size_type N ) const
{

    if ( this->vm()["crb.check.rb"].template as<int>() == 0 && this->vm()["crb.check.residual"].template as<int>()==0 )
        return;

    std::cout << "  -- check reduced basis\n";


    LOG(INFO) << "----------------------------------------------------------------------\n";

    // check that for each mu associated to a basis function of \f$W_N\f$
    //for( int k = std::max(0,(int)N-2); k < N; ++k )
    for ( size_type k = 0; k < N; ++k )
    {
        LOG(INFO) << "**********************************************************************\n";
        parameter_type const& mu = M_WNmu->at( k );
        std::vector< vectorN_type > uN; //uN.resize( N );
        std::vector< vectorN_type > uNdu; //( N );
        std::vector< vectorN_type > uNold;
        std::vector< vectorN_type > uNduold;
        auto tuple = lb( N, mu, uN, uNdu, uNold, uNduold );
        double s = tuple.template get<0>();
        auto error_estimation = delta( N, mu, uN, uNdu , uNold, uNduold );
        double err = error_estimation.template get<0>() ;


#if 0
        //if (  err > 1e-5 )
        // {
        std::cout << "[check] error bounds are not < 1e-10\n";
        std::cout << "[check] k = " << k << "\n";
        std::cout << "[check] mu = " << mu << "\n";
        std::cout << "[check] delta = " <<  err << "\n";
        std::cout << "[check] uN( " << k << " ) = " << uN( k ) << "\n";
#endif
        // }
        element_type u_fem; bool need_to_solve=false;
        u_fem = M_model->solveFemUsingOfflineEim ( mu );
        double sfem = M_model->output( M_output_index, mu , u_fem , need_to_solve );
        int size = mu.size();
        std::cout<<"    o mu = [ ";

        for ( int i=0; i<size-1; i++ ) std::cout<< mu[i] <<" , ";

        std::cout<< mu[size-1]<<" ]"<<std::endl;

        LOG(INFO) << "[check] s= " << s << " +- " << err  << " | sfem= " << sfem << " | abs(sfem-srb) =" << math::abs( sfem - s ) << "\n";
        std::cout <<"[check] s = " << s << " +- " << err  << " | sfem= " << sfem << " | abs(sfem-srb) =" << math::abs( sfem - s )<< "\n";

        if ( this->vm()["crb.check.residual"].template as<int>() == 1 )
        {
	     std::vector < std::vector<double> > primal_residual_coefficients = error_estimation.template get<1>();
	     std::vector < std::vector<double> > dual_residual_coefficients = error_estimation.template get<2>();
	     //std::vector<double> coefficients = error_estimation.template get<1>();
            //checkResidual( mu , coefficients );

        }

    }

    LOG(INFO) << "----------------------------------------------------------------------\n";

}

template<typename TruthModelType>
void
CRB<TruthModelType>::computeErrorEstimationEfficiencyIndicator ( parameterspace_ptrtype const& Dmu, double& max_ei, double& min_ei,int N )
{
    std::vector< vectorN_type > uN; //( N );
    std::vector< vectorN_type > uNdu;//( N );

    //sampling of parameter space Dmu
    sampling_ptrtype Sampling;
    Sampling = sampling_ptrtype( new sampling_type( M_Dmu ) );
    //Sampling->equidistribute( N );
    Sampling->randomize( N );
    //Sampling->logEquidistribute( N );

    int RBsize = M_WNmu->size();

    y_type ei( Sampling->size() );

    for ( size_type k = 0; k < Sampling->size(); ++k )
    {
        parameter_type const& mu = M_Xi->at( k );
        double s = lb( RBsize, mu, uN, uNdu );//output
        element_type u_fem; bool need_to_solve = true;
        double sfem = M_model->output( M_output_index, mu , u_fem , need_to_solve); //true ouput
        double error_estimation = delta( RBsize,mu,uN,uNdu );
        ei( k ) = error_estimation/math::abs( sfem-s );
        std::cout<<" efficiency indicator = "<<ei( k )<<" for parameters {";

        for ( int i=0; i<mu.size(); i++ ) std::cout<<mu[i]<<" ";

        std::cout<<"}  --  |sfem - s| = "<<math::abs( sfem-s )<<" and error estimation = "<<error_estimation<<std::endl;
    }

    Eigen::MatrixXf::Index index_max_ei;
    max_ei = ei.array().abs().maxCoeff( &index_max_ei );
    Eigen::MatrixXf::Index index_min_ei;
    min_ei = ei.array().abs().minCoeff( &index_min_ei );

    std::cout<<"[EI] max_ei = "<<max_ei<<" and min_ei = "<<min_ei<<min_ei<<" sampling size = "<<N<<std::endl;

}//end of computeErrorEstimationEfficiencyIndicator





template< typename TruthModelType>
double
CRB<TruthModelType>::correctionTerms(parameter_type const& mu, std::vector< vectorN_type > const & uN, std::vector< vectorN_type > const & uNdu,  std::vector<vectorN_type> const & uNold, int const k ) const
{
    int N = uN[0].size();

    matrixN_type Aprdu ( (int)N, (int)N ) ;
    matrixN_type Mprdu ( (int)N, (int)N ) ;
    vectorN_type Fdu ( (int)N );
    vectorN_type du ( (int)N );
    vectorN_type pr ( (int)N );
    vectorN_type oldpr ( (int)N );


    double time = 1e30;

    beta_vector_type betaAqm;
    beta_vector_type betaMqm;
    std::vector<beta_vector_type> betaFqm;

    double correction=0;

    if( M_model->isSteady() )
    {
        Aprdu.setZero( N , N );
        Fdu.setZero( N );

        boost::tie( betaMqm, betaAqm, betaFqm ) = M_model->computeBetaQm( mu ,time);

        for(size_type q = 0;q < M_model->Ql(0); ++q)
        {
            for(int m=0; m < M_model->mMaxF(0,q); m++)
                Fdu += betaFqm[0][q][m]*M_Fqm_du[q][m].head(N);
        }
        for(size_type q = 0;q < M_model->Qa(); ++q)
        {
            for(int m=0; m < M_model->mMaxA(q); m++)
                Aprdu += betaAqm[q][m]*M_Aqm_pr_du[q][m].block(0,0,N,N);
        }

        du = uNdu[0];
        pr = uN[0];
        correction = -( Fdu.dot( du ) - du.dot( Aprdu*pr )  );
    }
    else
    {

        double dt = M_model->timeStep();
        double Tf = M_model->timeFinal();
        int K = Tf/dt;
        int time_index;

        for( int kp=1; kp<=k; kp++)
        {

            Aprdu.setZero( N , N );
            Mprdu.setZero( N , N );
            Fdu.setZero( N );

            time_index = K-k+kp;
            time = time_index*dt;

            boost::tie( betaMqm, betaAqm, betaFqm) = M_model->computeBetaQm( mu ,time);

            time_index--;


            for(size_type q = 0;q < M_model->Ql(0); ++q)
            {
                for(int m=0; m < M_model->mMaxF(0,q); m++)
                    Fdu += betaFqm[0][q][m]*M_Fqm_du[q][m].head(N);
            }


            for(size_type q = 0;q < M_model->Qa(); ++q)
            {
                for(int m=0;  m < M_model->mMaxA(q); m++)
                    Aprdu += betaAqm[q][m]*M_Aqm_pr_du[q][m].block(0,0,N,N);
            }


            for(size_type q = 0;q < M_model->Qm(); ++q)
            {
                for(int m=0; m<M_model->mMaxM(q); m++)
                    Mprdu += betaMqm[q][m]*M_Mqm_pr_du[q][m].block(0,0,N,N);
            }


            du = uNdu[K-1-time_index];
            pr = uN[time_index];
            oldpr = uNold[time_index];
            correction += dt*( Fdu.dot( du ) - du.dot( Aprdu*pr ) ) - du.dot(Mprdu*pr) + du.dot(Mprdu*oldpr) ;
        }
    }

    return correction;

}


template<typename TruthModelType>
void
CRB<TruthModelType>::computeProjectionInitialGuess( const parameter_type & mu, int N , vectorN_type& initial_guess ) const
{
    VLOG(2) <<"Compute projection of initial guess\n";
    beta_vector_type betaMqm;
    beta_vector_type beta_initial_guess;

    matrixN_type Mass ( ( int )N, ( int )N ) ;
    vectorN_type F ( ( int )N );

    //beta coefficients of the initial guess ( mu-dependant part)
    beta_initial_guess = M_model->computeBetaInitialGuess( mu );

    //in steady case
    //for the mass matrix, the beta coefficient is 1
    //and the mu-indepenant part is assembled in crbmodel
    //WARNING : for unsteady case, don't call computeBetaQm to have beta coefficients
    //of the mass matrix, because computeBetaQm will compute all beta coefficients ( M A and F )
    //and beta coeff need to solve a model if we don't give an approximation of the unknown
    Mass.setZero( N,N );
    int q_max = M_Mqm_pr.size();
    for ( size_type q = 0; q < q_max; ++q )
    {
        int m_max = M_Mqm_pr[q].size();
        for(int m=0; m<m_max; m++)
        {
            Mass += 1*M_Mqm_pr[q][m].block( 0,0,N,N );
        }
    }

    F.setZero( N );
    q_max = M_InitialGuessV_pr.size();
    for ( size_type q = 0; q < q_max; ++q )
    {
        int m_max = M_InitialGuessV_pr[q].size();
        for(int m=0; m<m_max; m++)
            F += beta_initial_guess[q][m]*M_InitialGuessV_pr[q][m].head( N );
    }

    initial_guess = Mass.lu().solve( F );
}

template<typename TruthModelType>
void
CRB<TruthModelType>::updateJacobian( const map_dense_vector_type& map_X, map_dense_matrix_type& map_J , const parameter_type & mu , int N) const
{
    //map_J.setZero( N , N );
    map_J.setZero( );
    beta_vector_type betaJqm;
    boost::tie( boost::tuples::ignore, betaJqm, boost::tuples::ignore ) = M_model->computeBetaQm( this->expansion( map_X , N , M_WN ), mu , 0 );
    for ( size_type q = 0; q < M_model->Qa(); ++q )
    {
        for(int m=0; m<M_model->mMaxA(q); m++)
            map_J += betaJqm[q][m]*M_Jqm_pr[q][m].block( 0,0,N,N );
    }
}

template<typename TruthModelType>
void
CRB<TruthModelType>::updateResidual( const map_dense_vector_type& map_X, map_dense_vector_type& map_R , const parameter_type & mu, int N ) const
{
    map_R.setZero( );
    std::vector< beta_vector_type > betaRqm;
    boost::tie( boost::tuples::ignore, boost::tuples::ignore, betaRqm ) = M_model->computeBetaQm( this->expansion( map_X , N , M_WN ), mu , 0 );
    for ( size_type q = 0; q < M_model->Ql( 0 ); ++q )
    {
        for(int m=0; m<M_model->mMaxF(0,q); m++)
            map_R += betaRqm[0][q][m]*M_Rqm_pr[q][m].head( N );
    }
}

template<typename TruthModelType>
void
CRB<TruthModelType>::newton(  size_type N, parameter_type const& mu , vectorN_type & uN  , double& condition_number, double& output) const
{
    matrixN_type J ( ( int )N, ( int )N ) ;
    vectorN_type R ( ( int )N );

    double *r_data = R.data();
    double *j_data = J.data();
    double *uN_data = uN.data();

    Eigen::Map< Eigen::Matrix<double, Eigen::Dynamic , 1> > map_R ( r_data, N );
    Eigen::Map< Eigen::Matrix<double, Eigen::Dynamic , 1> > map_uN ( uN_data, N );
    Eigen::Map< Eigen::Matrix<double, Eigen::Dynamic , Eigen::Dynamic> > map_J ( j_data, N , N );

    computeProjectionInitialGuess( mu , N , uN );

    M_nlsolver->map_dense_jacobian = boost::bind( &self_type::updateJacobian, boost::ref( *this ), _1, _2  , mu , N );
    M_nlsolver->map_dense_residual = boost::bind( &self_type::updateResidual, boost::ref( *this ), _1, _2  , mu , N );
    M_nlsolver->solve( map_J , map_uN , map_R, 1e-12, 100);

    condition_number=0;

    if( option(_name="crb.compute-conditioning").template as<bool>() )
        condition_number = computeConditioning( J );

    //compute output

    vectorN_type L ( ( int )N );
    std::vector<beta_vector_type> betaFqm;
    boost::tie( boost::tuples::ignore, boost::tuples::ignore, betaFqm ) = M_model->computeBetaQm( this->expansion( uN , N , M_WN  ), mu , 0 );
    L.setZero( N );
    for ( size_type q = 0; q < M_model->Ql( M_output_index ); ++q )
    {
        for(int m=0; m < M_model->mMaxF(M_output_index,q); m++)
            L += betaFqm[M_output_index][q][m]*M_Lqm_pr[q][m].head( N );
    }

    output = L.dot( uN );


}

template<typename TruthModelType>
double
CRB<TruthModelType>::computeConditioning( matrixN_type & A ) const
{
    Eigen::SelfAdjointEigenSolver< matrixN_type > eigen_solver;
    eigen_solver.compute( A );
    int number_of_eigenvalues =  eigen_solver.eigenvalues().size();
    //we copy eigenvalues in a std::vector beacause it's easier to manipulate it
    std::vector<double> eigen_values( number_of_eigenvalues );


    for ( int i=0; i<number_of_eigenvalues; i++ )
    {
        if ( imag( eigen_solver.eigenvalues()[i] )>1e-12 )
        {
            throw std::logic_error( "[CRB::lb] ERROR : complex eigenvalues were found" );
        }

        eigen_values[i]=real( eigen_solver.eigenvalues()[i] );
    }

    int position_of_largest_eigenvalue=number_of_eigenvalues-1;
    int position_of_smallest_eigenvalue=0;
    double eig_max = eigen_values[position_of_largest_eigenvalue];
    double eig_min = eigen_values[position_of_smallest_eigenvalue];
    return eig_max / eig_min;
}



template<typename TruthModelType>
void
CRB<TruthModelType>::fixedPointDual(  size_type N, parameter_type const& mu, std::vector< vectorN_type > & uNdu,  std::vector<vectorN_type> & uNduold, std::vector< double > & output_vector, int K) const
{

    double time_for_output;
    double time_step;
    double time_final;
    int number_of_time_step=1;
    size_type Qm;

    if ( M_model->isSteady() )
    {
        time_step = 1e30;
        time_for_output = 1e30;
        Qm = 0;
        //number_of_time_step=1;
    }

    else
    {
        Qm = M_model->Qm();
        time_step = M_model->timeStep();
        time_final = M_model->timeFinal();

        if ( K > 0 )
            time_for_output = K * time_step;

        else
        {
            number_of_time_step = time_final / time_step;
            time_for_output = number_of_time_step * time_step;
        }
    }

    beta_vector_type betaAqm;
    beta_vector_type betaMqm;
    beta_vector_type betaMFqm;
    std::vector<beta_vector_type> betaFqm, betaLqm;

    matrixN_type Adu ( ( int )N, ( int )N ) ;
    matrixN_type Mdu ( ( int )N, ( int )N ) ;
    vectorN_type Fdu ( ( int )N );
    vectorN_type Ldu ( ( int )N );

    matrixN_type Aprdu( ( int )N, ( int )N );
    matrixN_type Mprdu( ( int )N, ( int )N );

    double time;
    if ( M_model->isSteady() )
    {
        time = 1e30;

        boost::tie( betaMqm, betaAqm, betaFqm ) = M_model->computeBetaQm( mu ,time );
        Adu.setZero( N,N );
        Ldu.setZero( N );

        for ( size_type q = 0; q < M_model->Qa(); ++q )
        {
            for(int m=0; m < M_model->mMaxA(q); m++)
                Adu += betaAqm[q][m]*M_Aqm_du[q][m].block( 0,0,N,N );
        }

        for ( size_type q = 0; q < M_model->Ql( M_output_index ); ++q )
        {
            for(int m=0; m < M_model->mMaxF(M_output_index,q); m++)
                Ldu += betaFqm[M_output_index][q][m]*M_Lqm_du[q][m].head( N );
        }

        uNdu[0] = Adu.lu().solve( -Ldu );
    }

    else
    {
        int time_index = number_of_time_step-1;

#if 0
        double initial_dual_time = time_for_output+time_step;
        //std::cout<<"initial_dual_time = "<<initial_dual_time<<std::endl;
        boost::tie( betaMqm, betaAqm, betaFqm, betaMFqm ) = M_model->computeBetaQm( mu ,initial_dual_time );
        Mdu.setZero( N,N );

        for ( size_type q = 0; q < M_model->Qm(); ++q )
        {
            for ( size_type m = 0; m < M_model->mMaxM( q ); ++m )
            {
                for(int m=0; m < M_model->mMaxM(q); m++)
                    Mdu += betaMqm[q][m]*M_Mqm_du[q][m].block( 0,0,N,N );
            }
        }

        Ldu.setZero( N );

        for ( size_type q = 0; q < M_model->Ql( M_output_index ); ++q )
        {
            for ( size_type m = 0; m < M_model->mMaxF(  M_output_index , q ); ++m )
            {
                for(int m=0; m < M_model->mMaxF(M_output_index,q); m++)
                    Ldu += betaFqm[M_output_index][q][m]*M_Lqm_du[q][m].head( N );
            }
        }

        /*
        vectorN_type coeff(N);
        for(int i=0; i<N; i++) coeff(i) = M_coeff_du_ini_online[i];
        vectorN_type diff2 = uNduold[time_index] - coeff;
        std::cout<<"et maintenant le deuxieme diff = \n"<<diff2<<"\n";
        */
#else

        for ( size_type n=0; n<N; n++ )
        {
            uNduold[time_index]( n ) = M_coeff_du_ini_online[n];
        }

#endif

        for ( time=time_for_output; time>=time_step; time-=time_step )
        {

            boost::tie( betaMqm, betaAqm, betaFqm ) = M_model->computeBetaQm( mu ,time );
            Adu.setZero( N,N );

            for ( size_type q = 0; q < M_model->Qa(); ++q )
            {
                for(int m=0; m < M_model->mMaxA(q); m++)
                    Adu += betaAqm[q][m]*M_Aqm_du[q][m].block( 0,0,N,N );
            }

            //No Rhs for adjoint problem except mass contribution
            Fdu.setZero( N );

            for ( size_type q = 0; q < Qm; ++q )
            {
                for(int m=0; m < M_model->mMaxM(q); m++)
                {
                    Adu += betaMqm[q][m]*M_Mqm_du[q][m].block( 0,0,N,N )/time_step;
                    Fdu += betaMqm[q][m]*M_Mqm_du[q][m].block( 0,0,N,N )*uNduold[time_index]/time_step;
                }
            }

            uNdu[time_index] = Adu.lu().solve( Fdu );

            if ( time_index>0 )
            {
                uNduold[time_index-1] = uNdu[time_index];
            }

            time_index--;
        }

    }//end of non steady case

}

template<typename TruthModelType>
void
CRB<TruthModelType>::fixedPointPrimal(  size_type N, parameter_type const& mu, std::vector< vectorN_type > & uN,  std::vector<vectorN_type> & uNold, double& condition_number, std::vector< double > & output_vector , int K) const
{

    double time_for_output;

    double time_step;
    double time_final;
    int number_of_time_step=1;
    size_type Qm;
    int time_index=0;
    double output=0;

    if ( M_model->isSteady() )
    {
        time_step = 1e30;
        time_for_output = 1e30;
        Qm = 0;
        //number_of_time_step=1;
    }

    else
    {
        Qm = M_model->Qm();
        time_step = M_model->timeStep();
        time_final = M_model->timeFinal();

        if ( K > 0 )
            time_for_output = K * time_step;

        else
        {
            number_of_time_step = time_final / time_step;
            time_for_output = number_of_time_step * time_step;
        }
    }
    beta_vector_type betaAqm;
    beta_vector_type betaMqm;
    beta_vector_type betaMFqm;
    beta_vector_type beta_initial_guess;

    std::vector<beta_vector_type> betaFqm, betaLqm;

    matrixN_type A ( ( int )N, ( int )N ) ;
    vectorN_type F ( ( int )N );
    vectorN_type L ( ( int )N );

    if ( !M_model->isSteady() )
    {
        for ( size_type n=0; n<N; n++ )
            uNold[0]( n ) = M_coeff_pr_ini_online[n];
    }


    int max_fixedpoint_iterations  = option("crb.max-fixedpoint-iterations").template as<int>();
    double increment_fixedpoint_tol  = option("crb.increment-fixedpoint-tol").template as<double>();
    double output_fixedpoint_tol  = option("crb.output-fixedpoint-tol").template as<double>();
    bool fixedpoint_verbose  = option("crb.fixedpoint-verbose").template as<bool>();
    double fixedpoint_critical_value  = option(_name="crb.fixedpoint-critical-value").template as<double>();
    for ( double time=time_step; time<time_for_output+time_step; time+=time_step )
    {

        computeProjectionInitialGuess( mu , N , uN[time_index] );

        //vectorN_type error;
        //const element_type expansion_uN = this->expansion( uN[time_index] , N , M_WN);
        //checkInitialGuess( expansion_uN , mu , error);
        //std::cout<<"***************************************************************error.sum : "<<error.sum()<<std::endl;

        VLOG(2) << "lb: start fix point\n";

        vectorN_type previous_uN( M_N );

        int fi=0;

        double old_output;
#if 0
        //in the case were we want to control output error for fixed point
        L.setZero( N );
        for ( size_type q = 0; q < M_model->Ql( M_output_index ); ++q )
        {
            for(int m=0; m < M_model->mMaxF(M_output_index,q); m++)
            {
                L += betaFqm[M_output_index][q][m]*M_Lqm_pr[q][m].head( N );
            }
        }
        old_output = L.dot( uN[time_index] );
#endif

        do
        {

            boost::tie( betaMqm, betaAqm, betaFqm ) = M_model->computeBetaQm( this->expansion( uN[time_index] , N , M_WN ), mu ,time );

            A.setZero( N,N );
            for ( size_type q = 0; q < M_model->Qa(); ++q )
            {
                for(int m=0; m<M_model->mMaxA(q); m++)
                    A += betaAqm[q][m]*M_Aqm_pr[q][m].block( 0,0,N,N );
            }

            F.setZero( N );
            for ( size_type q = 0; q < M_model->Ql( 0 ); ++q )
            {
                for(int m=0; m<M_model->mMaxF(0,q); m++)
                    F += betaFqm[0][q][m]*M_Fqm_pr[q][m].head( N );
            }

            for ( size_type q = 0; q < Qm; ++q )
            {
                for(int m=0; m<M_model->mMaxM(q); m++)
                {
                    A += betaMqm[q][m]*M_Mqm_pr[q][m].block( 0,0,N,N )/time_step;
                    F += betaMqm[q][m]*M_Mqm_pr[q][m].block( 0,0,N,N )*uNold[time_index]/time_step;
                }
            }

            // backup uN
            previous_uN = uN[time_index];

            // solve for new fix point iteration
            uN[time_index] = A.lu().solve( F );

            if ( time_index<number_of_time_step-1 )
                uNold[time_index+1] = uN[time_index];

            L.setZero( N );
            for ( size_type q = 0; q < M_model->Ql( M_output_index ); ++q )
            {
                for(int m=0; m < M_model->mMaxF(M_output_index,q); m++)
                {
                    L += betaFqm[M_output_index][q][m]*M_Lqm_pr[q][m].head( N );
                }
            }
            old_output = output;
            output = L.dot( uN[time_index] );

            //output_vector.push_back( output );
            output_vector[time_index] = output;
            DVLOG(2) << "iteration " << fi << " increment error: " << (uN[time_index]-previous_uN).norm() << "\n";
            fi++;

            if( fixedpoint_verbose  && this->worldComm().globalRank()==this->worldComm().masterRank() )
                VLOG(2)<<"[CRB::lb] fixedpoint iteration " << fi << " increment error: " << (uN[time_index]-previous_uN).norm()<<std::endl;

            double residual_norm = (A * uN[time_index] - F).norm() ;
            VLOG(2) << " residual_norm :  "<<residual_norm;
        }
        while ( (uN[time_index]-previous_uN).norm() > increment_fixedpoint_tol && fi<max_fixedpoint_iterations );
        //while ( math::abs(output - old_output) >  output_fixedpoint_tol && fi < max_fixedpoint_iterations );

        if( (uN[time_index]-previous_uN).norm() > increment_fixedpoint_tol )
            DVLOG(2)<<"[CRB::lb] fixed point, proc "<<this->worldComm().globalRank()
                    <<" fixed point has no converged : norm(uN-uNold) = "<<(uN[time_index]-previous_uN).norm()
                    <<" and tolerance : "<<increment_fixedpoint_tol<<" so "<<max_fixedpoint_iterations<<" iterations were done"<<std::endl;

        if( (uN[time_index]-previous_uN).norm() > fixedpoint_critical_value )
            throw std::logic_error( "[CRB::lb] fixed point ERROR : norm(uN-uNold) > critical value " );

        if ( time_index<number_of_time_step-1 )
            time_index++;
    }

    condition_number = 0;
    if( option(_name="crb.compute-conditioning").template as<bool>() )
        condition_number = computeConditioning( A );
}

template<typename TruthModelType>
void
CRB<TruthModelType>::fixedPoint(  size_type N, parameter_type const& mu, std::vector< vectorN_type > & uN, std::vector< vectorN_type > & uNdu,  std::vector<vectorN_type> & uNold, std::vector<vectorN_type> & uNduold, double& condition_number, std::vector< double > & output_vector, int K) const
{

    double time_for_output;
    double time_step;
    double time_final;
    int number_of_time_step;

    if ( M_model->isSteady() )
    {
        time_step = 1e30;
        time_for_output = 1e30;
        number_of_time_step=1;
    }
    else
    {
        time_step = M_model->timeStep();
        time_final = M_model->timeFinal();
        if ( K > 0 )
            time_for_output = K * time_step;
        else
        {
            number_of_time_step = time_final / time_step;
            time_for_output = number_of_time_step * time_step;
        }
    }
    fixedPointPrimal( N, mu , uN , uNold , condition_number, output_vector, K ) ;

    int size=output_vector.size();
    double o =output_vector[size-1];
    bool solve_dual_problem = this->vm()["crb.solve-dual-problem"].template as<bool>();
    //if( this->worldComm().globalSize() > 1 )
    //    solve_dual_problem=false;

    if( solve_dual_problem )
    {
        fixedPointDual( N, mu , uNdu , uNduold , output_vector , K ) ;

        int time_index=0;

        for ( double time=time_step; time<=time_for_output; time+=time_step )
        {
            int k = time_index+1;
            output_vector[time_index]+=correctionTerms(mu, uN , uNdu, uNold, k );
            time_index++;
        }
    }

}

template<typename TruthModelType>
boost::tuple<double,double>
CRB<TruthModelType>::lb( size_type N, parameter_type const& mu, std::vector< vectorN_type > & uN, std::vector< vectorN_type > & uNdu,  std::vector<vectorN_type> & uNold, std::vector<vectorN_type> & uNduold,int K  ) const
{

    bool save_output_behavior = this->vm()["crb.save-output-behavior"].template as<bool>();

    //if K>0 then the time at which we want to evaluate output is defined by
    //time_for_output = K * time_step
    //else it's the default value and in this case we take final time
    double time_for_output;

    double time_step;
    double time_final;
    int number_of_time_step = 0;
    size_type Qm;

    if ( M_model->isSteady() )
    {
        time_step = 1e30;
        time_for_output = 1e30;
        Qm = 0;
        number_of_time_step=1;
    }

    else
    {
        Qm = M_model->Qm();
        time_step = M_model->timeStep();
        time_final = M_model->timeFinal();

        if ( K > 0 )
            time_for_output = K * time_step;

        else
        {
            number_of_time_step = time_final / time_step;
            time_for_output = number_of_time_step * time_step;
        }
    }
    if ( N > M_N ) N = M_N;

    uN.resize( number_of_time_step );
    uNdu.resize( number_of_time_step );
    uNold.resize( number_of_time_step );
    uNduold.resize( number_of_time_step );

    int index=0;
    BOOST_FOREACH( auto elem, uN )
    {
        uN[index].resize( N );
        uNdu[index].resize( N );
        uNold[index].resize( N );
        uNduold[index].resize( N );
        index++;
    }
    double condition_number;
    //-- end of initialization step

    //vector containing outputs from time=time_step until time=time_for_output
    std::vector<double>output_vector;
    output_vector.resize( number_of_time_step );
    double output;
    int time_index=0;

    // init by 1, the model could provide better init
    //uN[0].setOnes(M_N);
    uN[0].setOnes(N);
    if( M_use_newton )
        newton( N , mu , uN[0] , condition_number , output_vector[0] );
    else
        fixedPoint( N ,  mu , uN , uNdu , uNold , uNduold , condition_number , output_vector , K );


    if( M_compute_variance )
    {

        if( ! M_database_contains_variance_info )
            throw std::logic_error( "[CRB::offline] ERROR there are no information available in the DataBase for variance computing, please set option crb.save-information-for-variance=true and rebuild the database" );

        int nb_spaces = functionspace_type::nSpaces;

        int space=0;

        time_index=0;
        for ( double time=time_step; time<=time_for_output; time+=time_step )
        {
            vectorN_type uNsquare = uN[time_index].array().pow(2);
            double first = uNsquare.dot( M_variance_matrix_phi[space].block(0,0,N,N).diagonal() );

            double second = 0;
            for(int k = 1; k <= N-1; ++k)
            {
                for(int j = 1; j <= N-k; ++j)
                {
                    second += 2 * uN[time_index](k-1) * uN[time_index](k+j-1) * M_variance_matrix_phi[space](k-1 , j+k-1) ;
                }
            }
            output_vector[time_index] = first + second;

            time_index++;
        }
    }


    if ( save_output_behavior )
    {
        time_index=0;
        std::ofstream file_output;
        std::string mu_str;

        for ( int i=0; i<mu.size(); i++ )
        {
            mu_str= mu_str + ( boost::format( "_%1%" ) %mu[i] ).str() ;
        }

        std::string name = "output_evolution" + mu_str;
        file_output.open( name.c_str(),std::ios::out );

        for ( double time=time_step; time<=time_for_output; time+=time_step )
        {
            file_output<<time<<"\t"<<output_vector[time_index]<<"\n";
            time_index++;
        }

        file_output.close();
    }

    int size=output_vector.size();
    return boost::make_tuple( output_vector[size-1], condition_number);

}

template<typename TruthModelType>
typename CRB<TruthModelType>::error_estimation_type
CRB<TruthModelType>::delta( size_type N,
                            parameter_type const& mu,
                            std::vector<vectorN_type> const& uN,
                            std::vector<vectorN_type> const& uNdu,
                            std::vector<vectorN_type> const& uNold,
                            std::vector<vectorN_type> const& uNduold,
                            int k ) const
{

    std::vector< std::vector<double> > primal_residual_coeffs;
    std::vector< std::vector<double> > dual_residual_coeffs;

    double delta_pr=0;
    double delta_du=0;
    if ( M_error_type == CRB_NO_RESIDUAL )
        return boost::make_tuple( -1,primal_residual_coeffs,dual_residual_coeffs,delta_pr,delta_du );

    else if ( M_error_type == CRB_EMPIRICAL )
        return boost::make_tuple( empiricalErrorEstimation ( N, mu , k ) , primal_residual_coeffs, dual_residual_coeffs , delta_pr, delta_du);

    else
    {
        //we assume that we want estimate the error committed on the final output
        double Tf = M_model->timeFinal();
        //double Ti = M_model->timeInitial();
        double dt = M_model->timeStep();

        int time_index=0;
        double primal_sum=0;
        double dual_sum=0;

        //vectors to store residual coefficients
        int K = Tf/dt;
        primal_residual_coeffs.resize( K );
        dual_residual_coeffs.resize( K );
        for ( double time=dt; time<=Tf; time+=dt )
        {
            auto pr = transientPrimalResidual( N, mu, uN[time_index], uNold[time_index], dt, time );
            primal_sum += pr.template get<0>();
            primal_residual_coeffs[time_index].resize( pr.template get<1>().size() );
            primal_residual_coeffs[time_index] = pr.template get<1>() ;
            time_index++;
        }//end of time loop for primal problem

        time_index--;

        double dual_residual=0;

        if ( !M_model->isSteady() ) dual_residual = initialDualResidual( N,mu,uNduold[time_index],dt );
        bool solve_dual_problem = this->vm()["crb.solve-dual-problem"].template as<bool>() ;

        if( solve_dual_problem )
        {
            for ( double time=Tf; time>=dt; time-=dt )
            {
                auto du = transientDualResidual( N, mu, uNdu[time_index], uNduold[time_index], dt, time );
                dual_sum += du.template get<0>();
                dual_residual_coeffs[time_index].resize( du.template get<1>().size() );
                dual_residual_coeffs[time_index] = du.template get<1>();
                time_index--;
            }//end of time loop for dual problem
        }


        bool show_residual = this->vm()["crb.show-residual"].template as<bool>() ;
        if( ! M_offline_step && show_residual )
        {
            double sum=0;
            bool seek_mu_in_complement = this->vm()["crb.seek-mu-in-complement"].template as<bool>() ;
            LOG( INFO ) <<" =========== Residual with "<<N<<" basis functions - seek mu in complement of WNmu : "<<seek_mu_in_complement<<"============ \n";
            time_index=0;
            for ( double time=dt; time<=Tf; time+=dt )
            {
                auto pr = transientPrimalResidual( N, mu, uN[time_index], uNold[time_index], dt, time );
                //LOG(INFO) << "primal residual at time "<<time<<" : "<<pr.template get<0>()<<"\n";
                sum+=pr.template get<0>();
                time_index++;
            }
            LOG(INFO) << "sum of primal residuals  "<<sum<<std::endl;

            time_index--;
            sum=0;

            if (solve_dual_problem )
            {
                for ( double time=Tf; time>=dt; time-=dt )
                {
                    auto du = transientDualResidual( N, mu, uNdu[time_index], uNduold[time_index], dt, time );
                    //LOG(INFO) << "dual residual at time "<<time<<" : "<<du.template get<0>()<<"\n";
                    sum += du.template get<0>();
                    time_index--;
                }
            }
            LOG(INFO) << "sum of dual residuals  "<<sum<<std::endl;
            LOG( INFO ) <<" ================================= \n";
            //std::cout<<"[REAL ] duam_sum : "<<sum<<std::endl;
        }//if show_residual_convergence

        double alphaA=1,alphaM=1;

        if ( M_error_type == CRB_RESIDUAL_SCM )
        {
            double alphaA_up, lbti;
            M_scmA->setScmForMassMatrix( false );
            boost::tie( alphaA, lbti ) = M_scmA->lb( mu );
            if( option(_name="crb.scm.use-scm").template as<bool>() )
                boost::tie( alphaA_up, lbti ) = M_scmA->ub( mu );
            //LOG( INFO ) << "alphaA_lo = " << alphaA << " alphaA_hi = " << alphaA_up ;

            if ( ! M_model->isSteady() )
            {
                M_scmM->setScmForMassMatrix( true );
                double alphaM_up, lbti;
                boost::tie( alphaM, lbti ) = M_scmM->lb( mu );
                if( option(_name="crb.scm.use-scm").template as<bool>() )
                    boost::tie( alphaM_up, lbti ) = M_scmM->ub( mu );
                //LOG( INFO ) << "alphaM_lo = " << alphaM << " alphaM_hi = " << alphaM_up ;
            }
        }

        double output_upper_bound;
        double solution_upper_bound;
        double solution_dual_upper_bound;
        //alphaA=1;
        //dual_residual=0;
        if ( M_model->isSteady() )
        {
            delta_pr = math::sqrt( primal_sum ) / math::sqrt( alphaA );
            if( solve_dual_problem )
                delta_du = math::sqrt( dual_sum ) / math::sqrt( alphaA );
            else
                delta_du = 1;
            output_upper_bound = delta_pr * delta_du;
            //solution_upper_bound =  delta_pr;
            //solution_dual_upper_bound =  delta_du;
        }
        else
        {
            delta_pr = math::sqrt( dt/alphaA * primal_sum );
            delta_du = math::sqrt( dt/alphaA * dual_sum + dual_residual/alphaM );
            output_upper_bound = delta_pr * delta_du;
            //solution_upper_bound = delta_pr;
            //solution_dual_upper_bound =  delta_du;
        }

        //return boost::make_tuple( output_upper_bound, primal_residual_coeffs, dual_residual_coeffs , delta_pr, delta_du , solution_upper_bound, solution_dual_upper_bound);
        return boost::make_tuple( output_upper_bound, primal_residual_coeffs, dual_residual_coeffs , delta_pr, delta_du );

    }//end of else
}

template<typename TruthModelType>
typename CRB<TruthModelType>::max_error_type
CRB<TruthModelType>::maxErrorBounds( size_type N ) const
{
    bool seek_mu_in_complement = this->vm()["crb.seek-mu-in-complement"].template as<bool>() ;

    std::vector< vectorN_type > uN;
    std::vector< vectorN_type > uNdu;
    std::vector< vectorN_type > uNold;
    std::vector< vectorN_type > uNduold;

    y_type err( M_Xi->size() );
    y_type vect_delta_pr( M_Xi->size() );
    y_type vect_delta_du( M_Xi->size() );
    std::vector<double> check_err( M_Xi->size() );

    double delta_pr=0;
    double delta_du=0;
    if ( M_error_type == CRB_EMPIRICAL )
    {
        if ( M_WNmu->size()==1 )
        {
            parameter_type mu ( M_Dmu );
            size_type id;
            boost::tie ( mu, id ) = M_Xi->max();
            return boost::make_tuple( 1e5, M_Xi->at( id ), id , delta_pr, delta_du);
        }

        else
        {
            err.resize( M_WNmu_complement->size() );
            check_err.resize( M_WNmu_complement->size() );

            for ( size_type k = 0; k < M_WNmu_complement->size(); ++k )
            {
                parameter_type const& mu = M_WNmu_complement->at( k );
                //double _err = delta( N, mu, uN, uNdu, uNold, uNduold, k);
                auto error_estimation = delta( N, mu, uN, uNdu, uNold, uNduold, k );
                double _err = error_estimation.template get<0>();
                vect_delta_pr( k ) = error_estimation.template get<3>();
                vect_delta_du( k ) = error_estimation.template get<4>();
                err( k ) = _err;
                check_err[k] = _err;
            }
        }

    }//end of if ( M_error_type == CRB_EMPIRICAL)

    else
    {
        if ( seek_mu_in_complement )
        {
            err.resize( M_WNmu_complement->size() );
            check_err.resize( M_WNmu_complement->size() );


            for ( size_type k = 0; k < M_WNmu_complement->size(); ++k )
            {
                parameter_type const& mu = M_WNmu_complement->at( k );
                lb( N, mu, uN, uNdu , uNold ,uNduold );
                auto error_estimation = delta( N, mu, uN, uNdu, uNold, uNduold, k );
                double _err = error_estimation.template get<0>();
                vect_delta_pr( k ) = error_estimation.template get<3>();
                vect_delta_du( k ) = error_estimation.template get<4>();
                err( k ) = _err;
                check_err[k] = _err;
            }

        }//end of seeking mu in the complement

        else
        {
            for ( size_type k = 0; k < M_Xi->size(); ++k )
            {
                //std::cout << "--------------------------------------------------\n";
                parameter_type const& mu = M_Xi->at( k );
                //std::cout << "[maxErrorBounds] mu=" << mu << "\n";
                lb( N, mu, uN, uNdu , uNold ,uNduold );
                //auto tuple = lb( N, mu, uN, uNdu , uNold ,uNduold );
                //double o = tuple.template get<0>();
                //std::cout << "[maxErrorBounds] output=" << o << "\n";
                auto error_estimation = delta( N, mu, uN, uNdu, uNold, uNduold, k );
                double _err = error_estimation.template get<0>();
                vect_delta_pr( k ) = error_estimation.template get<3>();
                vect_delta_du( k ) = error_estimation.template get<4>();
                //std::cout << "[maxErrorBounds] error=" << _err << "\n";
                err( k ) = _err;
                check_err[k] = _err;
            }
        }//else ( seek_mu_in_complement )
    }//else

    Eigen::MatrixXf::Index index;
    double maxerr = err.array().abs().maxCoeff( &index );
    delta_pr = vect_delta_pr( index );
    delta_du = vect_delta_du( index );
    parameter_type mu;

    std::vector<double>::iterator it = std::max_element( check_err.begin(), check_err.end() );
    int check_index = it - check_err.begin() ;
    double check_maxerr = *it;

    if ( index != check_index || maxerr != check_maxerr )
    {
        std::cout<<"[CRB::maxErrorBounds] index = "<<index<<" / check_index = "<<check_index<<"   and   maxerr = "<<maxerr<<" / "<<check_maxerr<<std::endl;
        throw std::logic_error( "[CRB::maxErrorBounds] index and check_index have different values" );
    }

    int _index=0;

    if ( seek_mu_in_complement )
    {
        LOG(INFO) << "[maxErrorBounds] WNmu_complement N=" << N << " max Error = " << maxerr << " at index = " << index << "\n";
        mu = M_WNmu_complement->at( index );
        _index = M_WNmu_complement->indexInSuperSampling( index );
    }

    else
    {
        LOG(INFO) << "[maxErrorBounds] N=" << N << " max Error = " << maxerr << " at index = " << index << "\n";
        mu = M_Xi->at( index );
        _index = index;
    }

    int proc_number = this->worldComm().globalRank();
    if( Environment::worldComm().globalRank() == Environment::worldComm().masterRank() )
        std::cout<< std::setprecision(15)<<"[CRB maxerror] proc "<< proc_number<<" delta_pr : "<<delta_pr<<" - delta_du : "<<delta_du<<" at index : "<<_index<<std::endl;
    lb( N, mu, uN, uNdu , uNold ,uNduold );

    return boost::make_tuple( maxerr, mu , _index , delta_pr, delta_du);


}
template<typename TruthModelType>
void
CRB<TruthModelType>::orthonormalize( size_type N, wn_type& wn, int Nm )
{
    int proc_number = this->worldComm().globalRank();
    if( proc_number == 0 ) std::cout << "  -- orthonormalization (Gram-Schmidt)\n";
    DVLOG(2) << "[CRB::orthonormalize] orthonormalize basis for N=" << N << "\n";
    DVLOG(2) << "[CRB::orthonormalize] orthonormalize basis for WN="
             << wn.size() << "\n";
    DVLOG(2) << "[CRB::orthonormalize] starting ...\n";

    for ( size_type i = 0; i < N; ++i )
    {
        for ( size_type j = std::max( i+1,N-Nm ); j < N; ++j )
        {
            value_type __rij_pr = M_model->scalarProduct(  wn[i], wn[ j ] );
            wn[j].add( -__rij_pr, wn[i] );
        }
    }
    // normalize
    for ( size_type i =N-Nm; i < N; ++i )
    {
        value_type __rii_pr = math::sqrt( M_model->scalarProduct(  wn[i], wn[i] ) );
        wn[i].scale( 1./__rii_pr );
    }

    DVLOG(2) << "[CRB::orthonormalize] finished ...\n";
    DVLOG(2) << "[CRB::orthonormalize] copying back results in basis\n";

    if ( this->vm()["crb.check.gs"].template as<int>() )
        checkOrthonormality( N , wn );
}

template <typename TruthModelType>
void
CRB<TruthModelType>::checkOrthonormality ( int N, const wn_type& wn ) const
{

    if ( wn.size()==0 )
    {
        throw std::logic_error( "[CRB::checkOrthonormality] ERROR : size of wn is zero" );
    }

    if ( orthonormalize_primal*orthonormalize_dual==0 )
    {
        std::cout<<"Warning : calling checkOrthonormality is called but ";
        std::cout<<" orthonormalize_dual = "<<orthonormalize_dual;
        std::cout<<" and orthonormalize_primal = "<<orthonormalize_primal<<std::endl;
    }

    matrixN_type A, I;
    A.setZero( N, N );
    I.setIdentity( N, N );

    for ( int i = 0; i < N; ++i )
    {
        for ( int j = 0; j < N; ++j )
        {
            A( i, j ) = M_model->scalarProduct(  wn[i], wn[j] );
        }
    }

    A -= I;
    DVLOG(2) << "orthonormalization: " << A.norm() << "\n";
    std::cout << "    o check : " << A.norm() << " (should be 0)\n";
    //FEELPP_ASSERT( A.norm() < 1e-14 )( A.norm() ).error( "orthonormalization failed.");
}


template <typename TruthModelType>
void
CRB<TruthModelType>::exportBasisFunctions( const export_vector_wn_type& export_vector_wn )const
{


    std::vector<wn_type> vect_wn=export_vector_wn.template get<0>();
    std::vector<std::string> vect_names=export_vector_wn.template get<1>();

    if ( vect_wn.size()==0 )
    {
        throw std::logic_error( "[CRB::exportBasisFunctions] ERROR : there are no wn_type to export" );
    }


    auto first_wn = vect_wn[0];
    auto first_element = first_wn[0];

    exporter->step( 0 )->setMesh( first_element.functionSpace()->mesh() );
    int basis_number=0;
    BOOST_FOREACH( auto wn , vect_wn )
    {

        if ( wn.size()==0 )
        {
            throw std::logic_error( "[CRB::exportBasisFunctions] ERROR : there are no element to export" );
        }

        int element_number=0;
        parameter_type mu;

        BOOST_FOREACH( auto element, wn )
        {

            std::string basis_name = vect_names[basis_number];
            std::string number = ( boost::format( "%1%_with_parameters" ) %element_number ).str() ;
            mu = M_WNmu->at( element_number );
            std::string mu_str;

            for ( int i=0; i<mu.size(); i++ )
            {
                mu_str= mu_str + ( boost::format( "_%1%" ) %mu[i] ).str() ;
            }

            std::string name =   basis_name + number + mu_str;
            exporter->step( 0 )->add( name, element );
            element_number++;
        }
        basis_number++;
    }

    exporter->save();

}


//error estimation only to build reduced basis
template <typename TruthModelType>
typename CRB<TruthModelType>::value_type
CRB<TruthModelType>::empiricalErrorEstimation ( int Nwn, parameter_type const& mu , int second_index ) const
{
    std::vector<vectorN_type> Un;
    std::vector<vectorN_type> Undu;
    std::vector<vectorN_type> Unold;
    std::vector<vectorN_type> Unduold;
    auto o = lb( Nwn, mu, Un, Undu, Unold, Unduold  );
    double sn = o.template get<0>();

    int nb_element =Nwn/M_factor*( M_factor>0 ) + ( Nwn+M_factor )*( M_factor<0 && ( int )Nwn>( -M_factor ) ) + 1*( M_factor<0 && ( int )Nwn<=( -M_factor ) )  ;

    std::vector<vectorN_type> Un2;
    std::vector<vectorN_type> Undu2;//( nb_element );

    //double output_smaller_basis = lb(nb_element, mu, Un2, Undu2, Unold, Unduold);
    auto tuple = lb( nb_element, mu, Un2, Undu2, Unold, Unduold );
    double output_smaller_basis = tuple.template get<0>();

    double error_estimation = math::abs( sn-output_smaller_basis );

    return error_estimation;

}




template<typename TruthModelType>
typename CRB<TruthModelType>::value_type
CRB<TruthModelType>::initialDualResidual( int Ncur, parameter_type const& mu, vectorN_type const& Unduini, double time_step ) const
{
    beta_vector_type betaAqm;
    beta_vector_type betaMqm;
    beta_vector_type betaMFqm;
    std::vector<beta_vector_type> betaFqm;
    double time = M_model->timeFinal();
    boost::tie( betaMqm, betaAqm, betaFqm) = M_model->computeBetaQm( mu, time );

    int __QLhs = M_model->Qa();
    int __QOutput = M_model->Ql( M_output_index );
    int __Qm = M_model->Qm();
    int __N = Ncur;


    value_type __c0_du = 0.0;

    for ( int __q1 = 0; __q1 < __QOutput; ++__q1 )
    {
        for ( int __m1 = 0; __m1 < M_model->mMaxF(M_output_index,__q1); ++__m1 )
        {
            value_type fq1 = betaFqm[M_output_index][__q1][__m1];

            for ( int __q2 = 0; __q2 < __QOutput; ++__q2 )
            {
                for ( int __m2 = 0; __m2 < M_model->mMaxF(M_output_index,__q2); ++__m2 )
                {
                    value_type fq2 = betaFqm[M_output_index][__q2][__m2];
                    //__c0_du += 1./(time_step*time_step) * M_C0_du[__q1][__q2]*fq1*fq2;
                    //__c0_du += 1./(time_step*time_step) * M_C0_du[__q1][__q2]*fq1*fq2;
                    __c0_du +=  M_C0_du[__q1][__m1][__q2][__m2]*fq1*fq2;
                }//end of loop __m2
            }//end of loop __q2
        }//end of loop __m1
    }//end of loop __q1

#if 0
    value_type __Caf_du = 0.0;
    value_type __Caa_du = 0.0;

    for ( int __q1 = 0; __q1 < __QLhs; ++__q1 )
    {
        for ( int __m1 = 0; __m1 < M_model->mMaxA(__q1) ; ++__m1 )
        {

            value_type a_q1 = betaAqm[__q1][__m1];

            for ( int __q2 = 0; __q2 < __QOutput; ++__q2 )
            {

                for ( int __m2 = 0; __m2 < M_model->mMaxF(M_output_index,__q2); ++__m2 )
                {
                    value_type f_q2 = betaFqm[M_output_index][__q2][__m2];
                    __Caf_du += 1./time_step * a_q1*f_q2*M_Lambda_du[__q1][__m1][__q2][__m2].dot( Unduini );
                }
            }


            for ( int __q2 = 0; __q2 < __QLhs; ++__q2 )
            {
                for ( int __m2 = 0; __m2 < M_model->mMaxA(__q2); ++__m2 )
                {
                    value_type a_q2 = betaAqm[__q2][__m2];
                    auto m = M_Gamma_du[__q1][__m1][__q2][__m2].block( 0,0,__N,__N )*Unduini;
                    __Caa_du += a_q1 * a_q2 * Unduini.dot( m );
                }
            }
        }
    }


            for ( int __q2 = 0; __q2 < __QLhs; ++__q2 )
            {
                for ( int __m2=0 ; __m2< M_model->mMaxA(__q2); ++__m2 )
                {
                    value_type a_q2 = betaAqm[__q2][__m2];
                    auto m = M_Cma_du[__q1][__m1][__q2][__m2].block( 0,0,__N,__N )*Unduini;
                    //__Cma_du += 1./time_step * m_q1 * a_q2 * Unduini.dot(m);
                    __Cma_du +=  m_q1 * a_q2 * Unduini.dot( m );
                }//m2
            }//q2

#endif

    value_type __Cmf_du=0;
    value_type __Cmm_du=0;
    value_type __Cma_du=0;

    for ( int __q1=0 ; __q1<__Qm; ++__q1 )
    {
        for ( int __m1=0 ; __m1< M_model->mMaxM(__q1); ++__m1 )
        {

            value_type m_q1 = betaMqm[__q1][__m1];

            for ( int __q2=0 ; __q2<__QOutput; ++__q2 )
            {
                for ( int __m2=0 ; __m2< M_model->mMaxF(M_output_index,__q2); ++__m2 )
                {
                    value_type f_q2 = betaFqm[M_output_index][__q2][__m2];
                    //__Cmf_du +=  1./(time_step*time_step) * m_q1 * f_q2 * M_Cmf_du[__q1][__m1][__q2][__m2].head(__N).dot( Unduini );
                    __Cmf_du +=   m_q1 * f_q2 * M_Cmf_du_ini[__q1][__m1][__q2][__m2].head( __N ).dot( Unduini );
                }//m2
            }//q2

            for ( int __q2 = 0; __q2 < __Qm; ++__q2 )
            {
                for ( int __m2=0 ; __m2< M_model->mMaxM(__q2); ++__m2 )
                {
                    value_type m_q2 = betaMqm[__q2][__m2];
                    auto m = M_Cmm_du[__q1][__m1][__q2][__m2].block( 0,0,__N,__N )*Unduini;
                    //__Cmm_du += 1./(time_step*time_step) * m_q1 * m_q2 * Unduini.dot(m);
                    __Cmm_du += m_q1 * m_q2 * Unduini.dot( m );
                }//m2
            }// q2

        }//m1
    }//q1

    //return  math::abs(__c0_du+__Cmf_du+__Caf_du+__Cmm_du+__Cma_du+__Caa_du) ;
    return  math::abs( __c0_du+__Cmf_du+__Cmm_du ) ;

}




template<typename TruthModelType>
typename CRB<TruthModelType>::residual_error_type
CRB<TruthModelType>::transientPrimalResidual( int Ncur,parameter_type const& mu,  vectorN_type const& Un ,vectorN_type const& Unold , double time_step, double time ) const
{

    beta_vector_type betaAqm;
    beta_vector_type betaMqm;
    beta_vector_type betaMFqm;
    std::vector<beta_vector_type> betaFqm, betaLqm;

    boost::tie( betaMqm, betaAqm, betaFqm ) = M_model->computeBetaQm( mu, time );

    size_type __QLhs = M_model->Qa();
    size_type __QRhs = M_model->Ql( 0 );
    size_type __Qm = M_model->Qm();
    size_type __N = Ncur;



    residual_error_type steady_residual_contribution = steadyPrimalResidual( Ncur, mu, Un, time );
    std::vector<double> steady_coeff_vector = steady_residual_contribution.template get<1>();
    value_type __c0_pr     = steady_coeff_vector[0];
    value_type __lambda_pr = steady_coeff_vector[1];
    value_type __gamma_pr  = steady_coeff_vector[2];

    value_type __Cmf_pr=0;
    value_type __Cma_pr=0;
    value_type __Cmm_pr=0;


    if ( ! M_model->isSteady() )
    {

        for ( size_type __q1=0 ; __q1<__Qm; ++__q1 )
        {
            for ( size_type __m1=0 ; __m1< M_model->mMaxM(__q1); ++__m1 )
            {
                value_type m_q1 = betaMqm[__q1][__m1];

                for ( size_type __q2=0 ; __q2<__QRhs; ++__q2 )
                {
                    for ( size_type __m2=0 ; __m2< M_model->mMaxF(0,__q2); ++__m2 )
                    {
                        value_type f_q2 = betaFqm[0][__q2][__m2];
                        __Cmf_pr +=  1./time_step * m_q1 * f_q2  * M_Cmf_pr[__q1][__m1][__q2][__m2].head( __N ).dot( Un );
                        __Cmf_pr -=  1./time_step * m_q1 * f_q2  * M_Cmf_pr[__q1][__m1][__q2][__m2].head( __N ).dot( Unold );
                    }//m2
                }//q2

                for ( size_type __q2 = 0; __q2 < __QLhs; ++__q2 )
                {
                    for ( size_type __m2 = 0; __m2 < M_model->mMaxA(__q2); ++__m2 )
                    {
                        value_type a_q2 = betaAqm[__q2][__m2];
                        auto m = M_Cma_pr[__q1][__m1][__q2][__m2].block( 0,0,__N,__N )*Un;
                        __Cma_pr += 1./time_step * m_q1 * a_q2 * Un.dot( m );
                        __Cma_pr -= 1./time_step * m_q1 * a_q2 * Unold.dot( m );
                    }//m2
                }//q2

                for ( size_type __q2 = 0; __q2 < __Qm; ++__q2 )
                {
                    for ( size_type __m2 = 0; __m2 < M_model->mMaxM(__q2); ++__m2 )
                    {
                        value_type m_q2 = betaMqm[__q2][__m2];
                        auto m1 = M_Cmm_pr[__q1][__m1][__q2][__m2].block( 0,0,__N,__N )*Un;
                        auto m2 = M_Cmm_pr[__q1][__m1][__q2][__m2].block( 0,0,__N,__N )*Unold;
                        __Cmm_pr += 1./( time_step*time_step ) * m_q1 * m_q2 * Un.dot( m1 );
                        __Cmm_pr -= 1./( time_step*time_step ) * m_q1 * m_q2 * Un.dot( m2 );
                        __Cmm_pr -= 1./( time_step*time_step ) * m_q1 * m_q2 * Unold.dot( m1 );
                        __Cmm_pr += 1./( time_step*time_step ) * m_q1 * m_q2 * Unold.dot( m2 );
                    }//m2
                }//q2
            }//m1
        }//q1
    }//end of if(! M_model->isSteady() )

#if 0
    std::cout<<"[transientResidual]  time "<<time<<std::endl;
    std::cout<<"Un = \n"<<Un<<std::endl;
    std::cout<<"Unold = \n"<<Unold<<std::endl;
    std::cout<<"__C0_pr = "<<__c0_pr<<std::endl;
    std::cout<<"__lambda_pr = "<<__lambda_pr<<std::endl;
    std::cout<<"__gamma_pr = "<<__gamma_pr<<std::endl;
    std::cout<<"__Cmf_pr = "<<__Cmf_pr<<std::endl;
    std::cout<<"__Cma_pr = "<<__Cma_pr<<std::endl;
    std::cout<<"__Cmm_pr = "<<__Cmm_pr<<std::endl;
    std::cout<<"__N = "<<__N<<std::endl;
#endif



    value_type delta_pr =  math::abs( __c0_pr+__lambda_pr+__gamma_pr+__Cmf_pr+__Cma_pr+__Cmm_pr ) ;
    std::vector<double> transient_coeffs_vector;
    transient_coeffs_vector.push_back( __c0_pr );
    transient_coeffs_vector.push_back( __lambda_pr );
    transient_coeffs_vector.push_back( __gamma_pr );
    transient_coeffs_vector.push_back( __Cmf_pr );
    transient_coeffs_vector.push_back( __Cma_pr );
    transient_coeffs_vector.push_back( __Cmm_pr );

    return boost::make_tuple( delta_pr , transient_coeffs_vector ) ;
}



template<typename TruthModelType>
typename CRB<TruthModelType>::residual_error_type
CRB<TruthModelType>::steadyPrimalResidual( int Ncur,parameter_type const& mu, vectorN_type const& Un, double time ) const
{

    beta_vector_type betaAqm;
    std::vector<beta_vector_type> betaFqm, betaLqm;
    boost::tie( boost::tuples::ignore, betaAqm, betaFqm ) = M_model->computeBetaQm( mu , time );

    int __QLhs = M_model->Qa();
    int __QRhs = M_model->Ql( 0 );
    int __N = Ncur;

    // primal terms
    value_type __c0_pr = 0.0;

    for ( int __q1 = 0; __q1 < __QRhs; ++__q1 )
    {
        for ( int __m1 = 0; __m1 < M_model->mMaxF(0,__q1); ++__m1 )
        {
            value_type fq1 = betaFqm[0][__q1][__m1];
            for ( int __q2 = 0; __q2 < __QRhs; ++__q2 )
            {
                for ( int __m2 = 0; __m2 < M_model->mMaxF(0,__q2); ++__m2 )
                {
                    value_type fq2 = betaFqm[0][__q2][__m2];
                    __c0_pr += M_C0_pr[__q1][__m1][__q2][__m2]*fq1*fq2;
                }//m2
            }//q2
        }//m1
    }//q1

    value_type __lambda_pr = 0.0;
    value_type __gamma_pr = 0.0;


    for ( int __q1 = 0; __q1 < __QLhs; ++__q1 )
    {
        for ( int __m1 = 0; __m1 < M_model->mMaxA(__q1); ++__m1 )
        {
            value_type a_q1 = betaAqm[__q1][__m1];

            for ( int __q2 = 0; __q2 < __QRhs; ++__q2 )
            {
                for ( int __m2 = 0; __m2 < M_model->mMaxF(0,__q2); ++__m2 )
                {
                    value_type f_q2 = betaFqm[0][__q2][__m2];
                    __lambda_pr += a_q1*f_q2*M_Lambda_pr[__q1][__m1][__q2][__m2].head( __N ).dot( Un );
                }//m2
            }//q2

            for ( int __q2 = 0; __q2 < __QLhs; ++__q2 )
            {
                for ( int __m2 = 0; __m2 < M_model->mMaxA(__q2); ++__m2 )
                {
                    value_type a_q2 = betaAqm[__q2][__m2];
                    auto m = M_Gamma_pr[__q1][__m1][__q2][__m2].block( 0,0,__N,__N )*Un;
                    __gamma_pr += a_q1 * a_q2 * Un.dot( m );
                }//m2
            }//q2
        }//m1
    }//q1

    //value_type delta_pr = math::sqrt( math::abs(__c0_pr+__lambda_pr+__gamma_pr) );
    value_type delta_pr = math::abs( __c0_pr+__lambda_pr+__gamma_pr ) ;
#if(0)

    if ( !boost::math::isfinite( delta_pr ) )
    {
        std::cout << "delta_pr is not finite:\n"
                  << "  - c0_pr = " << __c0_pr << "\n"
                  << "  - lambda_pr = " << __lambda_pr << "\n"
                  << "  - gamma_pr = " << __gamma_pr << "\n";
        std::cout << " - betaAqm = " << betaAqm << "\n"
                  << " - betaFqm = " << betaFqm << "\n";
        std::cout << " - Un : " << Un << "\n";
    }

    //std::cout << "delta_pr=" << delta_pr << std::endl;
#endif

    std::vector<double> coeffs_vector;
    coeffs_vector.push_back( __c0_pr );
    coeffs_vector.push_back( __lambda_pr );
    coeffs_vector.push_back( __gamma_pr );

    return boost::make_tuple( delta_pr,coeffs_vector );
}


template<typename TruthModelType>
typename CRB<TruthModelType>::residual_error_type
CRB<TruthModelType>::transientDualResidual( int Ncur,parameter_type const& mu,  vectorN_type const& Undu ,vectorN_type const& Unduold , double time_step, double time ) const
{
    int __QLhs = M_model->Qa();
    int __QOutput = M_model->Ql( M_output_index );
    int __Qm = M_model->Qm();
    int __N = Ncur;

    beta_vector_type betaAqm;
    beta_vector_type betaMqm;
    beta_vector_type betaMFqm;
    std::vector<beta_vector_type> betaFqm;
    boost::tie( betaMqm, betaAqm, betaFqm) = M_model->computeBetaQm( mu, time );

    residual_error_type steady_residual_contribution = steadyDualResidual( Ncur, mu, Undu, time );
    std::vector<double> steady_coeff_vector = steady_residual_contribution.template get<1>();
    value_type __c0_du     = steady_coeff_vector[0];
    value_type __lambda_du = steady_coeff_vector[1];
    value_type __gamma_du  = steady_coeff_vector[2];


    value_type __Cmf_du=0;
    value_type __Cma_du=0;
    value_type __Cmm_du=0;

#if 1

    if ( ! M_model->isSteady() )
    {

        for ( int __q1=0 ; __q1<__Qm; ++__q1 )
        {
            for ( int __m1=0 ; __m1< M_model->mMaxM(__q1); ++__m1 )
            {
                value_type m_q1 = betaMqm[__q1][__m1];

                for ( int __q2 = 0; __q2 < __QLhs; ++__q2 )
                {
                    for ( int __m2 = 0; __m2 < M_model->mMaxA(__q2); ++__m2 )
                    {
                        value_type a_q2 = betaAqm[__q2][__m2];
                        auto m = M_Cma_du[__q1][__m1][__q2][__m2].block( 0,0,__N,__N )*Undu;
                        __Cma_du += 1./time_step * m_q1 * a_q2 * Undu.dot( m );
                        __Cma_du -= 1./time_step * m_q1 * a_q2 * Unduold.dot( m );
                    }//m2
                }//q2

                for ( int __q2 = 0; __q2 < __Qm; ++__q2 )
                {
                    for ( int __m2 = 0; __m2 < M_model->mMaxM(__q2); ++__m2 )
                    {
                        value_type m_q2 = betaMqm[__q2][__m2];
                        auto m1 = M_Cmm_du[__q1][__m1][__q2][__m2].block( 0,0,__N,__N )*Undu;
                        auto m2 = M_Cmm_du[__q1][__m1][__q2][__m2].block( 0,0,__N,__N )*Unduold;
                        __Cmm_du += 1./( time_step*time_step ) * m_q1 * m_q2 * Undu.dot( m1 );
                        __Cmm_du -= 1./( time_step*time_step ) * m_q1 * m_q2 * Undu.dot( m2 );
                        __Cmm_du -= 1./( time_step*time_step ) * m_q1 * m_q2 * Unduold.dot( m1 );
                        __Cmm_du += 1./( time_step*time_step ) * m_q1 * m_q2 * Unduold.dot( m2 );
                    } //m2
                } //q2

            } //m1
        }//end of loop over q1
    }//end of if(! M_model->isSteady() )

#else
#endif

    value_type delta_du;

    if ( M_model->isSteady() )
        delta_du =  math::abs( __c0_du+__lambda_du+__gamma_du ) ;
    else
        delta_du =  math::abs( __gamma_du+__Cma_du+__Cmm_du ) ;



#if 0
    std::cout<<"[dualN2Q2] time "<<time<<std::endl;
    std::cout<<"Undu = \n"<<Undu<<std::endl;
    std::cout<<"Unduold = \n"<<Unduold<<std::endl;
    std::cout<<"__c0_du = "<<__c0_du<<std::endl;
    std::cout<<"__lambda_du = "<<__lambda_du<<std::endl;
    std::cout<<"__gamma_du = "<<__gamma_du<<std::endl;
    std::cout<<"__Cma_du = "<<__Cma_du<<std::endl;
    std::cout<<"__Cmm_du = "<<__Cmm_du<<std::endl;
#endif


    std::vector<double> coeffs_vector;
    coeffs_vector.push_back( __c0_du );
    coeffs_vector.push_back( __lambda_du );
    coeffs_vector.push_back( __gamma_du );
    coeffs_vector.push_back( __Cmf_du );
    coeffs_vector.push_back( __Cma_du );
    coeffs_vector.push_back( __Cmm_du );

    return boost::make_tuple( delta_du,coeffs_vector );
}




template<typename TruthModelType>
typename CRB<TruthModelType>::residual_error_type
CRB<TruthModelType>::steadyDualResidual( int Ncur,parameter_type const& mu, vectorN_type const& Undu, double time ) const
{

    int __QLhs = M_model->Qa();
    int __QOutput = M_model->Ql( M_output_index );
    int __N = Ncur;

    beta_vector_type betaAqm;
    beta_vector_type betaMqm;
    std::vector<beta_vector_type> betaFqm, betaLqm;
    boost::tie( boost::tuples::ignore, betaAqm, betaFqm ) = M_model->computeBetaQm( mu , time );

    value_type __c0_du = 0.0;

    for ( int __q1 = 0; __q1 < __QOutput; ++__q1 )
    {
        for ( int __m1 = 0; __m1 < M_model->mMaxF(M_output_index,__q1); ++__m1 )
        {
            value_type fq1 = betaFqm[M_output_index][__q1][__m1];
            for ( int __q2 = 0; __q2 < __QOutput; ++__q2 )
            {
                for ( int __m2 = 0; __m2 < M_model->mMaxF(M_output_index,__q2); ++__m2 )
                {
                    value_type fq2 = betaFqm[M_output_index][__q2][__m2];
                    __c0_du += M_C0_du[__q1][__m1][__q2][__m2]*fq1*fq2;
                }//m2
            }//q2
        } //m1
    } //q1

    value_type __lambda_du = 0.0;
    value_type __gamma_du = 0.0;

    for ( int __q1 = 0; __q1 < __QLhs; ++__q1 )
    {
        for ( int __m1 = 0; __m1 < M_model->mMaxA(__q1); ++__m1 )
        {
            value_type a_q1 = betaAqm[__q1][__m1];
            for ( int __q2 = 0; __q2 < __QOutput; ++__q2 )
            {
                for ( int __m2 = 0; __m2 < M_model->mMaxF(M_output_index,__q2); ++__m2 )
                {
                    value_type a_q2 = betaFqm[M_output_index][__q2][__m2]*a_q1;
                    __lambda_du += a_q2 * M_Lambda_du[__q1][__m1][__q2][__m2].head( __N ).dot( Undu );
                }//m2
            }//q2

            for ( int __q2 = 0; __q2 < __QLhs; ++__q2 )
            {
                for ( int __m2 = 0; __m2 < M_model->mMaxA(__q2); ++__m2 )
                {
                    value_type a_q2 = betaAqm[__q2][__m2]*a_q1;
                    auto m = M_Gamma_du[ __q1][ __m1][ __q2][ __m2].block( 0,0,__N,__N )*Undu;
                    __gamma_du += a_q2*Undu.dot( m );
                }//m2
            }//q2
        }//m1
    }//q1

    value_type delta_du = math::abs( __c0_du+__lambda_du+__gamma_du );

#if(0)

    if ( !boost::math::isfinite( delta_du ) )
    {
        std::cout << "delta_du is not finite:\n"
                  << "  - c0_du = " << __c0_du << "\n"
                  << "  - lambda_du = " << __lambda_du << "\n"
                  << "  - gamma_du = " << __gamma_du << "\n";
        std::cout << " - betaAqm = " << betaAqm << "\n"
                  << " - betaFqm = " << betaFqm << "\n";
        std::cout << " - Undu : " << Undu << "\n";
    }

#endif

    std::vector<double> coeffs_vector;
    coeffs_vector.push_back( __c0_du );
    coeffs_vector.push_back( __lambda_du );
    coeffs_vector.push_back( __gamma_du );
    return boost::make_tuple( delta_du,coeffs_vector );
}

template<typename TruthModelType>
void
CRB<TruthModelType>::offlineResidual( int Ncur ,int number_of_added_elements )
{
    return offlineResidual( Ncur, mpl::bool_<model_type::is_time_dependent>(), number_of_added_elements );
}

template<typename TruthModelType>
void
CRB<TruthModelType>::offlineResidual( int Ncur, mpl::bool_<true>, int number_of_added_elements )
{

    boost::timer ti;
    int __QLhs = M_model->Qa();
    int __QRhs = M_model->Ql( 0 );
    int __QOutput = M_model->Ql( M_output_index );
    int __Qm = M_model->Qm();
    int __N = Ncur;

    if( Environment::worldComm().globalRank() == Environment::worldComm().masterRank() )
        std::cout << "     o N=" << Ncur << " QLhs=" << __QLhs
                  << " QRhs=" << __QRhs << " Qoutput=" << __QOutput
                  << " Qm=" << __Qm << "\n";

    vector_ptrtype __X( M_backend->newVector( M_model->functionSpace() ) );
    vector_ptrtype __Fdu( M_backend->newVector( M_model->functionSpace() ) );
    vector_ptrtype __Z1(  M_backend->newVector( M_model->functionSpace() ) );
    vector_ptrtype __Z2(  M_backend->newVector( M_model->functionSpace() ) );
    vector_ptrtype __W(  M_backend->newVector( M_model->functionSpace() ) );
    namespace ublas = boost::numeric::ublas;

    std::vector< std::vector<sparse_matrix_ptrtype> > Aqm;
    std::vector< std::vector<sparse_matrix_ptrtype> > Mqm;
    std::vector< std::vector<std::vector<vector_ptrtype> > > Fqm;
    std::vector<std::vector<vector_ptrtype> > MFqm;
    boost::tie( Mqm, Aqm, Fqm ) = M_model->computeAffineDecomposition();
    __X->zero();
    __X->add( 1.0 );
    //std::cout << "measure of domain= " << M_model->scalarProduct( __X, __X ) << "\n";
#if 0
    ublas::vector<value_type> mu( P );

    for ( int q = 0; q < P; ++q )
    {
        mu[q] = mut( 0.0 );
    }

#endif

    offlineResidual( Ncur, mpl::bool_<false>(), number_of_added_elements );


    for ( int __q1 = 0; __q1 < __Qm; ++__q1 )
    {
        for ( int __m1 = 0; __m1 < M_model->mMaxM(__q1); ++__m1 )
        {

            for ( int __q2 = 0; __q2 < __QRhs; ++__q2 )
            {
                for ( int __m2 = 0; __m2 < M_model->mMaxF(0,__q2); ++__m2 )
                {

                    M_Cmf_pr[__q1][__m1][__q2][__m2].conservativeResize( __N );

                    for ( int elem=__N-number_of_added_elements; elem<__N; elem++ )
                    {
                        *__X=M_WN[elem];
                        Mqm[__q1][__m1]->multVector(  __X, __W );
                        __W->scale( -1. );
                        M_model->l2solve( __Z1, __W );

                        M_model->l2solve( __Z2, Fqm[0][__q2][__m2] );

                        M_Cmf_pr[ __q1][ __m1][ __q2][ __m2]( elem ) = 2.0*M_model->scalarProduct( __Z1, __Z2 );
                    }// elem
                }//m2
            }//q2
        }//m1
    }//q1

    if( Environment::worldComm().globalRank() == Environment::worldComm().masterRank() )
        std::cout << "     o M_Cmf_pr updated in " << ti.elapsed() << "s\n";
    ti.restart();


    for ( int __q1 = 0; __q1 < __Qm; ++__q1 )
    {
        for ( int __m1 = 0; __m1 < M_model->mMaxM(__q1); ++__m1 )
        {

            for ( int __q2 = 0; __q2 < __QLhs; ++__q2 )
            {
                for ( int __m2 = 0; __m2 < M_model->mMaxA(__q2); ++__m2 )
                {

                    M_Cma_pr[__q1][__m1][__q2][__m2].conservativeResize( __N, __N );

                    for ( int elem=__N-number_of_added_elements; elem<__N; elem++ )
                    {
                        *__X=M_WN[elem];
                        Mqm[__q1][__m1]->multVector(  __X, __W );
                        __W->scale( -1. );
                        M_model->l2solve( __Z1, __W );

                        for ( int __l = 0; __l < ( int )__N; ++__l )
                        {
                            *__X=M_WN[__l];
                            Aqm[__q2][__m2]->multVector(  __X, __W );
                            __W->scale( -1. );
                            M_model->l2solve( __Z2, __W );

                            M_Cma_pr[ __q1][ __m1][ __q2][ __m2]( elem,__l ) = 2.0*M_model->scalarProduct( __Z1, __Z2 );
                        }// l
                    }//end of loop over elem

                    for ( int __j = 0; __j < ( int )__N; ++__j )
                    {
                        *__X=M_WN[__j];
                        Mqm[__q1][ __m1]->multVector(  __X, __W );
                        __W->scale( -1. );
                        M_model->l2solve( __Z1, __W );

                        for ( int elem=__N-number_of_added_elements; elem<__N; elem++ )
                        {
                            *__X=M_WN[elem];
                            Aqm[__q2][ __m2]->multVector(  __X, __W );
                            __W->scale( -1. );
                            M_model->l2solve( __Z2, __W );

                            M_Cma_pr[ __q1][ __m1][ __q2][ __m2]( __j,elem ) = 2.0*M_model->scalarProduct( __Z1, __Z2 );
                        }//elem
                    }// __j

                }// on m2
            }// on q2
        }// on m1
    } // on q1

    if( Environment::worldComm().globalRank() == Environment::worldComm().masterRank() )
        std::cout << "     o M_Cma_pr updated in " << ti.elapsed() << "s\n";
    ti.restart();

    for ( int __q1 = 0; __q1 < __Qm; ++__q1 )
    {
        for ( int __m1 = 0; __m1 < M_model->mMaxM(__q1); ++__m1 )
        {

            for ( int __q2 = 0; __q2 < __Qm; ++__q2 )
            {

                for ( int __m2 = 0; __m2 < M_model->mMaxM(__q2); ++__m2 )
                {

                    M_Cmm_pr[__q1][__m1][__q2][__m2].conservativeResize( __N, __N );

                    for ( int elem=__N-number_of_added_elements; elem<__N; elem++ )
                    {
                        *__X=M_WN[elem];
                        Mqm[__q1][__m1]->multVector(  __X, __W );
                        __W->scale( -1. );
                        M_model->l2solve( __Z1, __W );

                        for ( int __l = 0; __l < ( int )__N; ++__l )
                        {
                            *__X=M_WN[__l];
                            Mqm[__q2][__m2]->multVector(  __X, __W );
                            __W->scale( -1. );
                            M_model->l2solve( __Z2, __W );

                            M_Cmm_pr[ __q1][ __m1][ __q2][ __m2]( elem,__l ) = M_model->scalarProduct( __Z1, __Z2 );
                        }// l
                    }//end of loop over elem

                    for ( int __j = 0; __j < ( int )__N; ++__j )
                    {
                        *__X=M_WN[__j];
                        Mqm[__q1][ __m1]->multVector(  __X, __W );
                        __W->scale( -1. );
                        M_model->l2solve( __Z1, __W );

                        for ( int elem=__N-number_of_added_elements; elem<__N; elem++ )
                        {
                            *__X=M_WN[elem];
                            Mqm[__q2][ __m2]->multVector(  __X, __W );
                            __W->scale( -1. );
                            M_model->l2solve( __Z2, __W );

                            M_Cmm_pr[ __q1][ __m1][ __q2][ __m2]( __j,elem ) = M_model->scalarProduct( __Z1, __Z2 );

                        }//elem
                    }//j

                }// on m2
            } // on q2
        }// on m1
    } // on q1

    if( Environment::worldComm().globalRank() == Environment::worldComm().masterRank() )
        std::cout << "     o M_Cmm_pr updated in " << ti.elapsed() << "s\n";
    ti.restart();


    sparse_matrix_ptrtype Atq1 = M_model->newMatrix();
    sparse_matrix_ptrtype Atq2 = M_model->newMatrix();
    sparse_matrix_ptrtype Mtq1 = M_model->newMatrix();
    sparse_matrix_ptrtype Mtq2 = M_model->newMatrix();
    //
    // Dual
    //


    LOG(INFO) << "[offlineResidual] Cmf_du Cma_du Cmm_du\n";

    for ( int __q1 = 0; __q1 < __Qm; ++__q1 )
    {

        for ( int __m1 = 0; __m1 < M_model->mMaxM(__q1); ++__m1 )
        {

            if( option("crb.use-symmetric-matrix").template as<bool>() )
                Mtq1 = Mqm[__q1][__m1];
            else
                Mqm[__q1][__m1]->transpose( Mtq1 );

            for ( int __q2 = 0; __q2 < __QOutput; ++__q2 )
            {
                for ( int __m2 = 0; __m2 < M_model->mMaxF(M_output_index,__q2); ++__m2 )
                {

                    M_Cmf_du[__q1][__m1][__q2][__m2].conservativeResize( __N );
                    M_Cmf_du_ini[__q1][__m1][__q2][__m2].conservativeResize( __N );

                    for ( int elem=__N-number_of_added_elements; elem<__N; elem++ )
                    {
                        *__X=M_WNdu[elem];
                        Mtq1->multVector(  __X, __W );
                        __W->close();
                        __W->scale( -1. );
                        M_model->l2solve( __Z1, __W );

                        *__Fdu = *Fqm[M_output_index][__q2][__m2];
                        __Fdu->close();
                        __Fdu->scale( -1.0 );
                        M_model->l2solve( __Z2, __Fdu );

                        M_Cmf_du[ __q1][__m1][ __q2][__m2]( elem ) = 2.0*M_model->scalarProduct( __Z1, __Z2 );

                        *__Fdu = *Fqm[M_output_index][__q2][__m2];
                        __Fdu->close();
                        M_model->l2solve( __Z2, __Fdu );
                        M_Cmf_du_ini[ __q1][__m1][ __q2][__m2]( elem ) = 2.0*M_model->scalarProduct( __Z1, __Z2 );
                    }//elem
                } // m2
            } // q2
        } // m1
    } // q1


    for ( int __q1 = 0; __q1 < __Qm; ++__q1 )
    {
        for ( int __m1 = 0; __m1 < M_model->mMaxM(__q1); ++__m1 )
        {

            if( option("crb.use-symmetric-matrix").template as<bool>() )
                Mtq1 = Mqm[__q1][__m1];
            else
                Mqm[__q1][__m1]->transpose( Mtq1 );

            for ( int __q2 = 0; __q2 < __QLhs; ++__q2 )
            {
                for ( int __m2 = 0; __m2 < M_model->mMaxA(__q2); ++__m2 )
                {

                    if( option("crb.use-symmetric-matrix").template as<bool>() )
                        Atq2 = Aqm[__q2][__m2];
                    else
                        Aqm[__q2][__m2]->transpose( Atq2 );

                    M_Cma_du[__q1][__m1][__q2][__m2].conservativeResize( __N, __N );

                    for ( int elem=__N-number_of_added_elements; elem<__N; elem++ )
                    {

                        *__X=M_WNdu[elem];
                        Mtq1->multVector(  __X, __W );
                        __W->scale( -1. );
                        M_model->l2solve( __Z1, __W );

                        for ( int __l = 0; __l < ( int )__N; ++__l )
                        {
                            *__X=M_WNdu[__l];
                            Atq2->multVector(  __X, __W );
                            __W->scale( -1. );
                            M_model->l2solve( __Z2, __W );

                            M_Cma_du[ __q1][__m1][__q2][ __m2]( elem,__l ) = 2.0*M_model->scalarProduct( __Z1, __Z2 );
                        }//j
                    }//elem

                    for ( int __j = 0; __j < ( int )__N; ++__j )
                    {
                        *__X=M_WNdu[__j];
                        Mtq1->multVector(  __X, __W );
                        __W->scale( -1. );
                        M_model->l2solve( __Z1, __W );

                        for ( int elem=__N-number_of_added_elements; elem<__N; elem++ )
                        {
                            *__X=M_WNdu[elem];
                            Atq2->multVector(  __X, __W );
                            __W->scale( -1. );
                            M_model->l2solve( __Z2, __W );

                            M_Cma_du[ __q1][__m1][__q2][ __m2]( __j,elem ) = 2.0*M_model->scalarProduct( __Z1, __Z2 );
                        }//elem
                    }//j

                }// m2
            } // q2
        }// m1
    } // q1

    if( Environment::worldComm().globalRank() == Environment::worldComm().masterRank() )
        std::cout << "     o Cma_du updated in " << ti.elapsed() << "s\n";

    ti.restart();



    for ( int __q1 = 0; __q1 < __Qm; ++__q1 )
    {
        for ( int __m1 = 0; __m1 < M_model->mMaxM(__q1); ++__m1 )
        {
            if( option("crb.use-symmetric-matrix").template as<bool>() )
                Mtq1 = Mqm[__q1][__m1];
            else
                Mqm[__q1][__m1]->transpose( Mtq1 );

            for ( int __q2 = 0; __q2 < __Qm; ++__q2 )
            {
                for ( int __m2 = 0; __m2 < M_model->mMaxM(__q2); ++__m2 )
                {

                    if( option("crb.use-symmetric-matrix").template as<bool>() )
                        Mtq2 = Mqm[__q2][__m2];
                    else
                        Mqm[__q2][__m2]->transpose( Mtq2 );

                    M_Cmm_du[__q1][__m1][__q2][__m2].conservativeResize( __N, __N );

                    for ( int elem=__N-number_of_added_elements; elem<__N; elem++ )
                    {
                        *__X=M_WNdu[elem];
                        Mtq1->multVector(  __X, __W );
                        __W->scale( -1. );
                        M_model->l2solve( __Z1, __W );

                        for ( int __l = 0; __l < ( int )__N; ++__l )
                        {
                            *__X=M_WNdu[__l];
                            Mtq2->multVector(  __X, __W );
                            __W->scale( -1. );
                            M_model->l2solve( __Z2, __W );

                            M_Cmm_du[ __q1][ __m1][ __q2][ __m2]( elem,__l ) = M_model->scalarProduct( __Z1, __Z2 );
                        }//l
                    }//elem

                    for ( int __j = 0; __j < ( int )__N; ++__j )
                    {
                        *__X=M_WNdu[__j];
                        Mtq1->multVector(  __X, __W );
                        __W->scale( -1. );
                        M_model->l2solve( __Z1, __W );

                        for ( int elem=__N-number_of_added_elements; elem<__N; elem++ )
                        {
                            *__X=M_WNdu[elem];
                            Mtq2->multVector(  __X, __W );
                            __W->scale( -1. );
                            M_model->l2solve( __Z2, __W );

                            M_Cmm_du[ __q1][ __m1][ __q2][ __m2]( __j,elem ) = M_model->scalarProduct( __Z1, __Z2 );
                        }// elem
                    }// j

                }// m2
            } // q2
        }// m1
    } // q1

    if( Environment::worldComm().globalRank() == Environment::worldComm().masterRank() )
        std::cout << "     o Cmm_du updated in " << ti.elapsed() << "s\n";
    ti.restart();

    LOG(INFO) << "[offlineResidual] Done.\n";

}



template<typename TruthModelType>
void
CRB<TruthModelType>::offlineResidual( int Ncur, mpl::bool_<false> , int number_of_added_elements )
{
    boost::timer ti;
    int __QLhs = M_model->Qa();
    int __QRhs = M_model->Ql( 0 );
    int __QOutput = M_model->Ql( M_output_index );
    int __N = Ncur;

    if( Environment::worldComm().globalRank() == Environment::worldComm().masterRank() )
        std::cout << "     o N=" << Ncur << " QLhs=" << __QLhs
                  << " QRhs=" << __QRhs << " Qoutput=" << __QOutput << "\n";

    vector_ptrtype __X( M_backend->newVector( M_model->functionSpace() ) );
    vector_ptrtype __Fdu( M_backend->newVector( M_model->functionSpace() ) );
    vector_ptrtype __Z1(  M_backend->newVector( M_model->functionSpace() ) );
    vector_ptrtype __Z2(  M_backend->newVector( M_model->functionSpace() ) );
    vector_ptrtype __W(  M_backend->newVector( M_model->functionSpace() ) );
    namespace ublas = boost::numeric::ublas;

    std::vector< std::vector<sparse_matrix_ptrtype> > Aqm,Mqm;
    std::vector< std::vector<vector_ptrtype> > MFqm;
    std::vector< std::vector<std::vector<vector_ptrtype> > > Fqm,Lqm;
    boost::tie( Mqm, Aqm, Fqm ) = M_model->computeAffineDecomposition();
    __X->zero();
    __X->add( 1.0 );
    //std::cout << "measure of domain= " << M_model->scalarProduct( __X, __X ) << "\n";
#if 0
    ublas::vector<value_type> mu( P );

    for ( int q = 0; q < P; ++q )
    {
        mu[q] = mut( 0.0 );
    }

#endif

    // Primal
    // no need to recompute this term each time
    if ( Ncur == 1 )
    {
        LOG(INFO) << "[offlineResidual] Compute Primal residual data\n";
        LOG(INFO) << "[offlineResidual] C0_pr\n";

        // see above Z1 = C^-1 F and Z2 = F
        for ( int __q1 = 0; __q1 < __QRhs; ++__q1 )
        {
            for ( int __m1 = 0; __m1 < M_model->mMaxF(0,__q1); ++__m1 )
            {
                //LOG(INFO) << "__Fq->norm1=" << Fq[0][__q1][__m1]->l2Norm() << "\n";
                M_model->l2solve( __Z1, Fqm[0][__q1][__m1] );
                //for ( int __q2 = 0;__q2 < __q1;++__q2 )
                for ( int __q2 = 0; __q2 < __QRhs; ++__q2 )
                {
                    for ( int __m2 = 0; __m2 < M_model->mMaxF(0,__q2); ++__m2 )
                    {
                        //LOG(INFO) << "__Fq->norm 2=" << Fq[0][__q2][__m2]->l2Norm() << "\n";
                        M_model->l2solve( __Z2, Fqm[0][__q2][__m2] );
                        //M_C0_pr[__q1][__m1][__q2][__m2] = M_model->scalarProduct( __X, Fq[0][__q2][__m2] );
                        M_C0_pr[__q1][__m1][__q2][__m2] = M_model->scalarProduct( __Z1, __Z2 );
                        //M_C0_pr[__q2][__q1] = M_C0_pr[__q1][__q2];
                        //VLOG(1) <<"M_C0_pr[" << __q1 << "][" << __m1 << "]["<< __q2 << "][" << __m2 << "]=" << M_C0_pr[__q1][__m1][__q2][__m2] << "\n";
                        //LOG(INFO) << "M_C0_pr[" << __q1 << "][" << __m1 << "]["<< __q2 << "][" << __m2 << "]=" << M_C0_pr[__q1][__m1][__q2][__m2] << "\n";
                    }//end of loop __m2
                }//end of loop __q2
                //M_C0_pr[__q1][__q1] = M_model->scalarProduct( __X, __X );
            }//end of loop __m1
        }//end of loop __q1
    }// Ncur==1

    if( Environment::worldComm().globalRank() == Environment::worldComm().masterRank() )
        std::cout << "     o initialize offlineResidual in " << ti.elapsed() << "s\n";
    ti.restart();

#if 0
    parameter_type const& mu = M_WNmu->at( 0 );
    //std::cout << "[offlineResidual] mu=" << mu << "\n";
    beta_vector_type betaAqm;
    std::vector<beta_vector_type> betaFqm;
    boost::tie( boost::tuples::ignore, betaAqm, betaFqm ) = M_model->computeBetaQm( mu );
    value_type __c0_pr = 0.0;

    for ( int __q1 = 0; __q1 < __QRhs; ++__q1 )
    {
        for ( int __m1 = 0; __m1 < M_model->mMaxF(0,__q1); ++__m1 )
        {
            value_type fq1 = betaFqm[0][__q1][__m1];

            for ( int __q2 = 0; __q2 < __QRhs; ++__q2 )
            {
                for ( int __m2 = 0; __m2 < M_model->mMaxF(M_output_index,__q2); ++__m2 )
                {
                    value_type fq2 = betaFqm[0][__q2][__m2];
                    __c0_pr += M_C0_pr[__q1][__m1][__q2][__m2]*fq1*fq2;
                }
            }
        }//end of loop __m1
    }//end of loop __q1


    //std::cout << "c0=" << __c0_pr << std::endl;

    std::vector< sparse_matrix_ptrtype > A;
    std::vector< std::vector<vector_ptrtype> > F;
    boost::tie( A, F ) = M_model->update( mu );
    M_model->l2solve( __X, F[0] );
    //std::cout << "c0 2 = " << M_model->scalarProduct( __X, __X ) << "\n";;
#endif

    //
    //  Primal
    //
    LOG(INFO) << "[offlineResidual] Lambda_pr, Gamma_pr\n";

    for ( int elem=__N-number_of_added_elements; elem<__N; elem++ )
    {
        *__X=M_WN[elem];

        for ( int __q1 = 0; __q1 < __QLhs; ++__q1 )
        {
            for ( int __m1 = 0; __m1 < M_model->mMaxA(__q1) ; ++__m1 )
            {
                Aqm[__q1][__m1]->multVector(  __X, __W );
                __W->scale( -1. );
                //std::cout << "__W->norm=" << __W->l2Norm() << "\n";
                M_model->l2solve( __Z1, __W );
                for ( int __q2 = 0; __q2 < __QRhs; ++__q2 )
                {
                    for ( int __m2 = 0; __m2 < M_model->mMaxF(0, __q2); ++__m2 )
                    {
                        M_Lambda_pr[__q1][__m1][__q2][__m2].conservativeResize( __N );
                        //__Y = Fq[0][__q2];
                        //std::cout << "__Fq->norm=" << Fq[0][__q2]->l2Norm() << "\n";
                        M_model->l2solve( __Z2, Fqm[0][__q2][__m2] );
                        M_Lambda_pr[ __q1][ __m1][ __q2][ __m2]( elem ) = 2.0*M_model->scalarProduct( __Z1, __Z2 );
                        //VLOG(1) << "M_Lambda_pr[" << __q1 << "][" << __m1 << "][" << __q2 << "][" << __m2 << "][" << __j << "]=" <<  M_Lambda_pr[__q1][__m1][__q2][__m2][__j] << "\n";
                        //std::cout << "M_Lambda_pr[" << __q1 << "][" << __m1 << "][" << __q2 << "][" << __m2 << "][" << __j << "]=" << M_Lambda_pr[__q1][__m1][__q2][__m2][__j] << "\n";
                    }//m2
                }//end of __q2
            }//end of loop __m1
        }//end of __q1
    }//elem

    if( Environment::worldComm().globalRank() == Environment::worldComm().masterRank() )
        std::cout << "     o Lambda_pr updated in " << ti.elapsed() << "s\n";

    ti.restart();

    for ( int __q1 = 0; __q1 < __QLhs; ++__q1 )
    {
        for ( int __m1 = 0; __m1 < M_model->mMaxA(__q1); ++__m1 )
        {

            for ( int __q2 = 0; __q2 < __QLhs; ++__q2 )
            {

                for ( int __m2 = 0; __m2 < M_model->mMaxA(__q2); ++__m2 )
                {

                    M_Gamma_pr[__q1][__m1][__q2][__m2].conservativeResize( __N, __N );

                    for ( int elem=__N-number_of_added_elements; elem<__N; elem++ )
                    {
                        *__X=M_WN[elem];
                        Aqm[__q1][__m1]->multVector(  __X, __W );
                        __W->scale( -1. );
                        //std::cout << "__W->norm=" << __W->l2Norm() << "\n";
                        M_model->l2solve( __Z1, __W );

                        //Aq[__q2]->multVector(  __Z1, __W );
                        for ( int __l = 0; __l < ( int )__N; ++__l )
                        {
                            *__X=M_WN[__l];
                            Aqm[__q2][__m2]->multVector(  __X, __W );
                            __W->scale( -1. );
                            //std::cout << "__W2_pr->norm=" << __W->l2Norm() << "\n";
                            M_model->l2solve( __Z2, __W );
                            M_Gamma_pr[ __q1][ __m1][ __q2][ __m2]( elem,__l ) = M_model->scalarProduct( __Z1, __Z2 );
                            //M_Gamma_pr[ __q2][ __q1][ __j ][__l] = M_Gamma_pr[ __q1][ __q2][ __j ][__l];
                            //VLOG(1) << "M_Gamma_pr[" << __q1 << "][" << __m1 << "][" << __q2 << "][" << __m2 << "][" << __j << "][" << __l << "]=" << M_Gamma_pr[__q1][__m1][__q2][__m2][__j][__l] << "\n";
                            //std::cout << "M_Gamma_pr[" << __q1 << "][" << __m1 << "][" << __q2 << "][" << __m2 << "][" << __j << "][" << __l << "]=" << M_Gamma_pr[__q1][__m1][__q2][__m2][__j][__l] << "\n";
                        }//end of loop over l
                    }//end of loop over elem

                    for ( int __j = 0; __j < ( int )__N; ++__j )
                    {
                        *__X=M_WN[__j];
                        Aqm[__q1][__m1]->multVector(  __X, __W );
                        __W->scale( -1. );
                        //std::cout << "__W->norm=" << __W->l2Norm() << "\n";
                        M_model->l2solve( __Z1, __W );

                        //Aq[__q2]->multVector(  __Z1, __W );
                        //column N-1
                        //int __l = __N-1;
                        for ( int elem=__N-number_of_added_elements; elem<__N; elem++ )
                        {
                            *__X=M_WN[elem];
                            Aqm[__q2][__m2]->multVector(  __X, __W );
                            __W->scale( -1. );
                            //std::cout << "__W2_pr->norm=" << __W->l2Norm() << "\n";
                            M_model->l2solve( __Z2, __W );
                            M_Gamma_pr[ __q1][ __m1][ __q2][ __m2]( __j,elem ) = M_model->scalarProduct( __Z1, __Z2 );
                            //M_Gamma_pr[ __q2][ __q1][ __j ][__l] = M_Gamma_pr[ __q1][ __q2][ __j ][__l];
                            //VLOG(1) << "M_Gamma_pr[" << __q1 << "][" << __m1 << "][" << __q2 << "][" << __m2 << "][" << __j << "][" << __l << "]=" << M_Gamma_pr[__q1][__m1][__q2][__m2][__j][__l] << "\n";
                            //std::cout << "M_Gamma_pr[" << __q1 << "][" << __m1 << "][" << __q2 << "][" << __m2 << "][" << __j << "][" << __l << "]=" << M_Gamma_pr[__q1][__m1][__q2][__m2][__j][__l] << "\n";
                        }// end of loop elem
                    }// end of loop __j
                }// end of loop __m2
            }// end of loop __q2
        }// on m1
    } // on q1

    if( Environment::worldComm().globalRank() == Environment::worldComm().masterRank() )
        std::cout << "     o Gamma_pr updated in " << ti.elapsed() << "s\n";
    ti.restart();
    sparse_matrix_ptrtype Atq1 = M_model->newMatrix();
    sparse_matrix_ptrtype Atq2 = M_model->newMatrix();

    //
    // Dual
    //
    // compute this only once
    if( solve_dual_problem )
    {
        if ( Ncur == 1 )
        {
            LOG(INFO) << "[offlineResidual] Compute Dual residual data\n";
            LOG(INFO) << "[offlineResidual] C0_du\n";

            for ( int __q1 = 0; __q1 < __QOutput; ++__q1 )
            {
                for ( int __m1 = 0; __m1 < M_model->mMaxF(M_output_index,__q1); ++__m1 )
                {
                    *__Fdu = *Fqm[M_output_index][__q1][__m1];
                    __Fdu->close();
                    __Fdu->scale( -1.0 );
                    M_model->l2solve( __Z1, __Fdu );

                    for ( int __q2 = 0; __q2 < __QOutput; ++__q2 )
                    {
                        for ( int __m2 = 0; __m2 < M_model->mMaxF(M_output_index,__q2); ++__m2 )
                        {
                            *__Fdu = *Fqm[M_output_index][__q2][__m2];
                            __Fdu->close();
                            __Fdu->scale( -1.0 );
                            M_model->l2solve( __Z2, __Fdu );
                            M_C0_du[__q1][__m1][__q2][__m2] = M_model->scalarProduct( __Z1, __Z2 );
                            //M_C0_du[__q2][__q1] = M_C0_du[__q1][__q2];
                            //M_C0_du[__q1][__q1] = M_model->scalarProduct( __Xdu, __Xdu );
                        }//end of loop __m2
                    }//end of loop __q2
                }//end of loop __m1
            }//end of loop __q1

            if( Environment::worldComm().globalRank() == Environment::worldComm().masterRank() )
                std::cout << "     o C0_du updated in " << ti.elapsed() << "s\n";
            ti.restart();
        }

        LOG(INFO) << "[offlineResidual] Lambda_du, Gamma_du\n";

        for ( int elem=__N-number_of_added_elements; elem<__N; elem++ )
        {
            *__X=M_WNdu[elem];

            for ( int __q1 = 0; __q1 < __QLhs; ++__q1 )
            {
                for ( int __m1 = 0; __m1 < M_model->mMaxA(__q1); ++__m1 )
                {

                    if( option("crb.use-symmetric-matrix").template as<bool>() )
                        Atq1 = Aqm[__q1][__m1];
                    else
                        Aqm[__q1][__m1]->transpose( Atq1 );

                    Atq1->multVector(  __X, __W );
                    __W->close();
                    __W->scale( -1. );
                    //std::cout << "__W->norm=" << __W->l2Norm() << "\n";
                    M_model->l2solve( __Z1, __W );

                    for ( int __q2 = 0; __q2 < __QOutput; ++__q2 )
                    {
                        for ( int __m2 = 0; __m2 < M_model->mMaxF(M_output_index,__q2) ; ++__m2 )
                        {
                            M_Lambda_du[__q1][__m1][__q2][__m2].conservativeResize( __N );

                            *__Fdu = *Fqm[M_output_index][__q2][__m2];
                            __Fdu->close();
                            __Fdu->scale( -1.0 );
                            M_model->l2solve( __Z2, __Fdu );
                            M_Lambda_du[ __q1][ __m1][ __q2][ __m2]( elem ) = 2.0*M_model->scalarProduct( __Z2, __Z1 );
                            //VLOG(1) << "M_Lambda_pr[" << __q1 << "][" << __m1 << "][" << __q2 << "][" << __m2 << "][" << __j << "]=" << M_Lambda_pr[__q1][__m1][__q2][__m2][__j] << "\n";
                            //std::cout << "M_Lambda_pr[" << __q1 << "][" << __m1 << "][" << __q2 << "][" << __m2 << "][" << __j << "]=" << M_Lambda_pr[__q1][__m1][__q2][__m2][__j] << "\n";
                        } // m2
                    } // q2
                } // m1
            } // q1
        }//elem

        if( Environment::worldComm().globalRank() == Environment::worldComm().masterRank() )
            std::cout << "     o Lambda_du updated in " << ti.elapsed() << "s\n";
        ti.restart();

        for ( int __q1 = 0; __q1 < __QLhs; ++__q1 )
        {
            for ( int __m1 = 0; __m1 < M_model->mMaxA(__q1); ++__m1 )
            {

                if( option("crb.use-symmetric-matrix").template as<bool>() )
                    Atq1=Aqm[__q1][__m1];
                else
                    Aqm[__q1][__m1]->transpose( Atq1 );

                for ( int __q2 = 0; __q2 < __QLhs; ++__q2 )
                {
                    for ( int __m2 = 0; __m2 < M_model->mMaxA(__q2); ++__m2 )
                    {

                        if( option("crb.use-symmetric-matrix").template as<bool>() )
                            Atq2 = Aqm[__q2][__m2];
                        else
                            Aqm[__q2][__m2]->transpose( Atq2 );

                        M_Gamma_du[__q1][__m1][__q2][__m2].conservativeResize( __N, __N );

                        for ( int elem=__N-number_of_added_elements; elem<__N; elem++ )
                        {
                            //int __j = __N-1;
                            *__X=M_WNdu[elem];
                            Atq1->multVector(  __X, __W );
                            __W->scale( -1. );
                            //std::cout << "__W->norm=" << __W->l2Norm() << "\n";
                            M_model->l2solve( __Z1, __W );

                            //Aq[__q2][__m2]->multVector(  __Z1, __W );

                            for ( int __l = 0; __l < ( int )__N; ++__l )
                            {
                                *__X=M_WNdu[__l];
                                Atq2->multVector(  __X, __W );
                                __W->scale( -1. );
                                //std::cout << "__W2_pr->norm=" << __W->l2Norm() << "\n";
                                M_model->l2solve( __Z2, __W );
                                M_Gamma_du[ __q1][ __m1][ __q2][ __m2]( elem,__l ) = M_model->scalarProduct( __Z1, __Z2 );
                                //M_Gamma_pr[ __q2][ __q1][ __j ][__l] = M_Gamma_pr[ __q1][ __q2][ __j ][__l];
                                //VLOG(1) << "M_Gamma_du[" << __q1 << "][" << __m1 << "][" << __q2 << "][" << __m2 << "][" << __j << "][" << __l << "]=" << M_Gamma_du[__q1][__m1][__q2][__m2][__j][__l] << "\n";
                                //std::cout << "M_Gamma_du[" << __q1 << "][" << __m1 << "][" << __q2 << "][" << __m2 << "][" << __j << "][" << __l << "]=" << M_Gamma_du[__q1][__m1][__q2][__m2][__j][__l] << "\n";
                            } // __l
                        }// elem

                        // update column __N-1
                        for ( int __j = 0; __j < ( int )__N; ++__j )
                        {
                            *__X=M_WNdu[__j];
                            Atq1->multVector(  __X, __W );
                            __W->scale( -1. );
                            //std::cout << "__W->norm=" << __W->l2Norm() << "\n";
                            M_model->l2solve( __Z1, __W );

                            //Aq[__q2][__m2]->multVector(  __Z1, __W );
                            //int __l = __N-1;
                            for ( int elem=__N-number_of_added_elements; elem<__N; elem++ )
                            {
                                *__X=M_WNdu[elem];
                                Atq2->multVector(  __X, __W );
                                __W->scale( -1. );
                                //std::cout << "__W2_pr->norm=" << __W->l2Norm() << "\n";
                                M_model->l2solve( __Z2, __W );
                                M_Gamma_du[ __q1][ __m1][ __q2][ __m2]( __j,elem ) = M_model->scalarProduct( __Z1, __Z2 );
                                //M_Gamma_pr[ __q2][ __q1][ __j ][__l] = M_Gamma_pr[ __q1][ __q2][ __j ][__l];
                                //VLOG(1) << "M_Gamma_du[" << __q1 << "][" << __m1 << "][" << __q2 << "][" << __m2 << "][" << __j << "][" << __l << "]=" << M_Gamma_du[__q1][__m1][__q2][__m2][__j][__l] << "\n";
                                //std::cout << "M_Gamma_du[" << __q1 << "][" << __m1 << "][" << __q2 << "][" << __m2 << "][" << __j << "][" << __l << "]=" << M_Gamma_du[__q1][__m1][__q2][__m2][__j][__l] << "\n";
                            }//elem
                        }// __j
                    }// on m2
                }// on q2
            }// on m1
        } // on q1

        if( Environment::worldComm().globalRank() == Environment::worldComm().masterRank() )
            std::cout << "     o Gamma_du updated in " << ti.elapsed() << "s\n";
        ti.restart();
        LOG(INFO) << "[offlineResidual] Done.\n";

    }//end of if (solve_dual_problem)
}

template<typename TruthModelType>
void
CRB<TruthModelType>::printErrorsDuringRbConstruction( void )
{

    if( M_rbconv_contains_primal_and_dual_contributions )
    {
        typedef convergence_type::left_map::const_iterator iterator;
        LOG(INFO)<<"\nMax error during offline stage\n";
        for(iterator it = M_rbconv.left.begin(); it != M_rbconv.left.end(); ++it)
            LOG(INFO)<<"N : "<<it->first<<"  -  maxerror : "<<it->second.template get<0>()<<"\n";

        LOG(INFO)<<"\nPrimal contribution\n";
        for(iterator it = M_rbconv.left.begin(); it != M_rbconv.left.end(); ++it)
            LOG(INFO)<<"N : "<<it->first<<"  -  delta_pr : "<<it->second.template get<1>()<<"\n";

        LOG(INFO)<<"\nDual contribution\n";
        for(iterator it = M_rbconv.left.begin(); it != M_rbconv.left.end(); ++it)
            LOG(INFO)<<"N : "<<it->first<<"  -  delta_du : "<<it->second.template get<2>()<<"\n";
        LOG(INFO)<<"\n";
    }
    else
        throw std::logic_error( "[CRB::printErrorsDuringRbConstruction] ERROR, the database is too old to print the error during offline step, use the option rebuild-database = true" );

}



template<typename TruthModelType>
void
CRB<TruthModelType>::printMuSelection( void )
{
    LOG(INFO)<<" List of parameter selectionned during the offline algorithm \n";
    for(int k=0;k<M_WNmu->size();k++)
    {
        std::cout<<" mu "<<k<<" = [ ";
        LOG(INFO)<<" mu "<<k<<" = [ ";
        parameter_type const& _mu = M_WNmu->at( k );
        for( int i=0; i<_mu.size()-1; i++ )
        {
            LOG(INFO)<<_mu(i)<<" , ";
            std::cout<<_mu(i)<<" , ";
        }
        LOG(INFO)<<_mu( _mu.size()-1 )<<" ] \n";
        std::cout<<_mu( _mu.size()-1 )<<" ] "<<std::endl;
    }
}


template<typename TruthModelType>
typename CRB<TruthModelType>::element_type
CRB<TruthModelType>::expansion( parameter_type const& mu , int N , int time_index )
{
    int Nwn;

    if( N > 0 )
        Nwn = N;
    else
        Nwn = M_N;

    std::vector<vectorN_type> uN;
    std::vector<vectorN_type> uNdu;
    std::vector<vectorN_type> uNold;
    std::vector<vectorN_type> uNduold;

    auto o = lb( Nwn, mu, uN, uNdu , uNold, uNduold );
    int size = uN.size();
    FEELPP_ASSERT( N <= M_WN.size() )( N )( M_WN.size() ).error( "invalid expansion size ( N and M_WN ) ");
    element_type ucrb;
    if( time_index == -1 )
        ucrb = Feel::expansion( M_WN, uN[size-1] , Nwn);
    else
    {
        CHECK( time_index < size )<<" call crb::expansion with a wrong value of time index : "<<time_index<<" or size of uN vector is only "<<size;
        ucrb = Feel::expansion( M_WN, uN[time_index] , Nwn);
    }
    return ucrb;
}


template<typename TruthModelType>
typename CRB<TruthModelType>::element_type
CRB<TruthModelType>::expansion( vectorN_type const& u , int const N, wn_type const & WN ) const
{
    int Nwn;

    if( N > 0 )
        Nwn = N;
    else
        Nwn = M_N;

    //FEELPP_ASSERT( M_WN.size() == u.size() )( M_WN.size() )( u.size() ).error( "invalid expansion size");
    FEELPP_ASSERT( Nwn <= WN.size() )( Nwn )( WN.size() ).error( "invalid expansion size ( Nwn and WN ) ");
    FEELPP_ASSERT( Nwn <= u.size() )( Nwn )( WN.size() ).error( "invalid expansion size ( Nwn and u ) ");
    //FEELPP_ASSERT( Nwn == u.size() )( Nwn )( u.size() ).error( "invalid expansion size");
    return Feel::expansion( WN, u, N );
}


template<typename TruthModelType>
boost::tuple<double,double, typename CRB<TruthModelType>::solutions_tuple, double, double, double, boost::tuple<double,double,double> >
CRB<TruthModelType>::run( parameter_type const& mu, double eps , int N)
{

    M_compute_variance = this->vm()["crb.compute-variance"].template as<bool>();

    //int Nwn = M_N;
    int Nwn_max = vm()["crb.dimension-max"].template as<int>();
#if 0
    if (  M_error_type!=CRB_EMPIRICAL )
    {
        auto lo = M_rbconv.right.range( boost::bimaps::unbounded, boost::bimaps::_key <= eps );

        for ( auto it = lo.first; it != lo.second; ++it )
        {
            std::cout << "rbconv[" << it->first <<"]=" << it->second << "\n";
        }

        auto it = M_rbconv.project_left( lo.first );
        Nwn = it->first;
        std::cout << "Nwn = "<< Nwn << " error = "<< it->second.template get<0>() << " eps=" << eps << "\n";
    }
#endif

    std::vector<vectorN_type> uN;
    std::vector<vectorN_type> uNdu;
    std::vector<vectorN_type> uNold;
    std::vector<vectorN_type> uNduold;

    int Nwn;
    if( N > 0 )
    {
        //in this case we want to fix Nwn
        Nwn = N;
    }
    else
    {
        //Nwn = Nwn_max;
        //M_N may be different of dimension-max
        Nwn = M_N;
    }
    auto o = lb( Nwn, mu, uN, uNdu , uNold, uNduold );
    double output = o.template get<0>();

    auto error_estimation = delta( Nwn, mu, uN, uNdu , uNold, uNduold );
    double output_upper_bound = error_estimation.template get<0>();
    double condition_number = o.template get<1>();
    auto primal_coeffcients = error_estimation.template get<1>();
    auto dual_coefficients = error_estimation.template get<2>();
    if ( this->vm()["crb.check.residual-transient-problems"].template as<bool>() )
    {
        std::vector< std::vector<double> > primal_residual_coefficients = error_estimation.template get<1>();
        std::vector< std::vector<double> > dual_residual_coefficients = error_estimation.template get<2>();
        std::vector<element_ptrtype> Un,Unold,Undu,Unduold;
        buildFunctionFromRbCoefficients(Nwn, uN, M_WN, Un );
        buildFunctionFromRbCoefficients(Nwn, uNold, M_WN, Unold );
        buildFunctionFromRbCoefficients(Nwn, uNdu, M_WNdu, Undu );
        buildFunctionFromRbCoefficients(Nwn, uNduold, M_WNdu, Unduold );
        compareResidualsForTransientProblems(Nwn, mu , Un, Unold, Undu, Unduold, primal_residual_coefficients, dual_residual_coefficients );
    }

    //compute dual norm of primal/dual residual at final time
    int nb_dt = primal_coeffcients.size();
    int final_time_index = nb_dt-1;
    double primal_residual_norm = 0;
    double dual_residual_norm = 0;

    if ( M_error_type == CRB_RESIDUAL || M_error_type == CRB_RESIDUAL_SCM )
    {
        int nb_coeff = primal_coeffcients[final_time_index].size();
        for(int i=0 ; i<nb_coeff ; i++)
            primal_residual_norm += primal_coeffcients[final_time_index][i] ;

        bool solve_dual_problem = this->vm()["crb.solve-dual-problem"].template as<bool>() ;
        if( solve_dual_problem )
        {
            if ( M_model->isSteady() )
                dual_residual_norm =  math::abs( dual_coefficients[0][0]+dual_coefficients[0][1]+dual_coefficients[0][2] ) ;
            else
                dual_residual_norm =  math::abs( dual_coefficients[0][2]+dual_coefficients[0][4]+dual_coefficients[0][5] ) ;
        }
        primal_residual_norm = math::sqrt( math::abs(primal_residual_norm) );
        dual_residual_norm = math::sqrt( math::abs(dual_residual_norm) );
    }
    double delta_pr = error_estimation.template get<3>();
    double delta_du = error_estimation.template get<4>();
    int size = uN.size();

    auto upper_bounds = boost::make_tuple(output_upper_bound , delta_pr, delta_du );
    auto solutions = boost::make_tuple( uN[size-1] , uNdu[0]);
    return boost::make_tuple( output , Nwn , solutions, condition_number , primal_residual_norm , dual_residual_norm, upper_bounds );
}


template<typename TruthModelType>
void
CRB<TruthModelType>::run( const double * X, unsigned long N, double * Y, unsigned long P )
{
    return run( X, N, Y, P, mpl::bool_<model_type::is_time_dependent>() );
}



template<typename TruthModelType>
void
CRB<TruthModelType>::run( const double * X, unsigned long N, double * Y, unsigned long P, mpl::bool_<true> )
{
    //std::cout<<"N = "<<N<<" et P = "<<P<<std::endl;

    for ( unsigned long p= 0; p < N-5; ++p ) std::cout<<"mu["<<p<<"] = "<<X[p]<<std::endl;


    parameter_type mu( M_Dmu );

    // the last parameter is the max error
    for ( unsigned long p= 0; p < N-5; ++p )
        mu( p ) = X[p];


    //double meshSize  = X[N-4];
    //M_model->setMeshSize(meshSize);
    // setOutputIndex( ( int )X[N-3] );
    // std::cout<<"output index = "<<X[N-3]<<std::endl;
    // int Nwn =  X[N-2];
    // std::cout<<"Nwn : "<<X[N-2]<<std::endl;
    // size_type maxerror = X[N-1];
    // std::cout<<"maxerror = "<<X[N-1]<<std::endl;
    //CRBErrorType errorType =(CRBErrorType)X[N-1];
    //setCRBErrorType(errorType);

    setOutputIndex( ( int )X[N-5] );
    //std::cout<<"output_index = "<<X[N-5]<<std::endl;
    int Nwn =  X[N-4];
    //std::cout<<" Nwn = "<<Nwn<<std::endl;
    int maxerror = X[N-3];
    //std::cout<<" maxerror = "<<maxerror<<std::endl;
    CRBErrorType errorType =( CRBErrorType )X[N-2];
    //std::cout<<"errorType = "<<X[N-2]<<std::endl;
    setCRBErrorType( errorType );
    M_compute_variance = X[N-1];
    //std::cout<<"M_compute_variance = "<<M_compute_variance<<std::endl;


#if 0

    if (  M_error_type!=CRB_EMPIRICAL )
    {
        auto lo = M_rbconv.right.range( boost::bimaps::unbounded,boost::bimaps::_key <= maxerror );

        for ( auto it = lo.first; it != lo.second; ++it )
        {
            std::cout << "rbconv[" << it->first <<"]=" << it->second << "\n";
        }

        auto it = M_rbconv.project_left( lo.first );
        Nwn = it->first;
    }

#endif

    std::vector<vectorN_type> uN;
    std::vector<vectorN_type> uNdu;
    std::vector<vectorN_type> uNold ;
    std::vector<vectorN_type> uNduold;

    FEELPP_ASSERT( P == 2 )( P ).warn( "invalid number of outputs" );
    auto o = lb( Nwn, mu, uN, uNdu , uNold, uNduold );
    auto e = delta( Nwn, mu, uN, uNdu , uNold, uNduold );
    Y[0]  = o.template get<0>();
    Y[1]  = e.template get<0>();
}


template<typename TruthModelType>
void
CRB<TruthModelType>::run( const double * X, unsigned long N, double * Y, unsigned long P, mpl::bool_<false> )
{

    parameter_type mu( M_Dmu );

    for ( unsigned long p= 0; p < N-5; ++p )
      {
        mu( p ) = X[p];
        std::cout << "mu( " << p << " ) = " << mu( p ) << std::endl;
      }

    // std::cout<<" list of parameters : [";
    // for ( unsigned long i=0; i<N-1; i++ ) std::cout<<X[i]<<" , ";
    // std::cout<<X[N-1]<<" ] "<<std::endl;


    setOutputIndex( ( int )X[N-5] );
    //std::cout<<"output_index = "<<X[N-5]<<std::endl;
    int Nwn =  X[N-4];
    //std::cout<<" Nwn = "<<Nwn<<std::endl;
    int maxerror = X[N-3];
    //std::cout<<" maxerror = "<<maxerror<<std::endl;
    CRBErrorType errorType =( CRBErrorType )X[N-2];
    //std::cout<<"errorType = "<<X[N-2]<<std::endl;
    setCRBErrorType( errorType );
    M_compute_variance = X[N-1];
    //std::cout<<"M_compute_variance = "<<M_compute_variance<<std::endl;

#if 0
    auto lo = M_rbconv.right.range( boost::bimaps::unbounded,boost::bimaps::_key <= maxerror );


    for ( auto it = lo.first; it != lo.second; ++it )
    {
        std::cout << "rbconv[" << it->first <<"]=" << it->second << "\n";
    }

    auto it = M_rbconv.project_left( lo.first );
    Nwn = it->first;
    //std::cout << "Nwn = "<< Nwn << " error = "<< it->second << " maxerror=" << maxerror << " ErrorType = "<<errorType <<"\n";
#endif
    //int Nwn = M_WN.size();
    std::vector<vectorN_type> uN;
    std::vector<vectorN_type> uNdu;
    std::vector<vectorN_type> uNold ;
    std::vector<vectorN_type> uNduold;

    FEELPP_ASSERT( P == 2 )( P ).warn( "invalid number of outputs" );
    auto o = lb( Nwn, mu, uN, uNdu , uNold, uNduold );
    auto e = delta( Nwn, mu, uN, uNdu , uNold, uNduold );
    Y[0]  = o.template get<0>();
    Y[1]  = e.template get<0>();
}






template<typename TruthModelType>
void
CRB<TruthModelType>::projectionOnPodSpace( const element_type & u , element_ptrtype& projection, const std::string& name_of_space )
{

    projection->zero();

    if ( name_of_space=="dual" )
    {
        if ( orthonormalize_dual )
            //in this case we can simplify because elements of reduced basis are orthonormalized
        {
            BOOST_FOREACH( auto du, M_WNdu )
            {
                element_type e = du.functionSpace()->element();
                e = du;
                double k =  M_model->scalarProduct( u, e );
                e.scale( k );
                projection->add( 1 , e );
            }
        }//end of if orthonormalize_dual

        else
        {
            matrixN_type MN ( ( int )M_N, ( int )M_N ) ;
            vectorN_type FN ( ( int )M_N );

            for ( size_type i=0; i<M_N; i++ )
            {
                for ( size_type j=0; j<i; j++ )
                {
                    MN( i,j ) = M_model->scalarProduct( M_WNdu[j] , M_WNdu[i] );
                    MN( j,i ) = MN( i,j );
                }

                MN( i,i ) = M_model->scalarProduct( M_WNdu[i] , M_WNdu[i] );
                FN( i ) = M_model->scalarProduct( u,M_WNdu[i] );
            }

            vectorN_type projectionN ( ( int ) M_N );
            projectionN = MN.lu().solve( FN );
            int index=0;
            BOOST_FOREACH( auto du, M_WNdu )
            {
                element_type e = du.functionSpace()->element();
                e = du;
                double k =  projectionN( index );
                e.scale( k );
                projection->add( 1 , e );
                index++;
            }
        }//end of if( ! orthonormalize_dual )
    }//end of projection on dual POD space

    else
    {
        if ( orthonormalize_primal )
        {
            BOOST_FOREACH( auto pr, M_WN )
            {
                auto e = pr.functionSpace()->element();
                e = pr;
                double k =  M_model->scalarProduct( u, e );
                e.scale( k );
                projection->add( 1 , e );
            }
        }//end of if orthonormalize_primal

        else
        {
            matrixN_type MN ( ( int )M_N, ( int )M_N ) ;
            vectorN_type FN ( ( int )M_N );

            for ( size_type i=0; i<M_N; i++ )
            {
                for ( size_type j=0; j<i; j++ )
                {
                    MN( i,j ) = M_model->scalarProduct( M_WN[j] , M_WN[i] );
                    MN( j,i ) = MN( i,j );
                }

                MN( i,i ) = M_model->scalarProduct( M_WN[i] , M_WN[i] );
                FN( i ) = M_model->scalarProduct( u,M_WN[i] );
            }

            vectorN_type projectionN ( ( int ) M_N );
            projectionN = MN.lu().solve( FN );
            int index=0;
            BOOST_FOREACH( auto pr, M_WN )
            {
                element_type e = pr.functionSpace()->element();
                e = pr;
                double k =  projectionN( index );
                e.scale( k );
                projection->add( 1 , e );
                index++;
            }
        }//end of if( ! orthonormalize_primal )
    }//end of projection on primal POD space

}



template<typename TruthModelType>
void
CRB<TruthModelType>::computationalTimeStatistics(std::string appname)
{

    double min=0,max=0,mean=0,standard_deviation=0;

    int n_eval = option(_name="crb.computational-time-neval").template as<int>();

    Eigen::Matrix<double, Eigen::Dynamic, 1> time_crb;
    time_crb.resize( n_eval );

    sampling_ptrtype Sampling( new sampling_type( M_Dmu ) );
    Sampling->logEquidistribute( n_eval  );

    bool cvg = option(_name="crb.cvg-study").template as<bool>();
    int dimension = this->dimension();
    double tol = option(_name="crb.online-tolerance").template as<double>();

    int N=dimension;//by default we perform only one time statistics

    if( cvg ) //if we want to compute time statistics for every crb basis then we start at 1
        N=1;

    int proc_number =  Environment::worldComm().globalRank();
    int master =  Environment::worldComm().masterRank();

    //write on a file
    std::string file_name = "cvg-timing-crb.dat";

    std::ofstream conv;
    if( proc_number == master )
    {
        conv.open(file_name, std::ios::app);
        conv << "NbBasis" << "\t" << "min" <<"\t"<< "max" <<"\t"<< "mean"<<"\t"<<"standard_deviation" << "\n";
    }

    //loop over basis functions (if cvg option)
    for(; N<=dimension; N++)
    {

        int mu_number = 0;
        BOOST_FOREACH( auto mu, *Sampling )
        {
            boost::mpi::timer tcrb;
            auto o = this->run( mu, tol , N);
            time_crb( mu_number ) = tcrb.elapsed() ;
            mu_number++;
        }

        auto stat = M_model->computeStatistics( time_crb , appname );

        min=stat(0);
        max=stat(1);
        mean=stat(2);
        standard_deviation=stat(3);

        if( proc_number == master )
            conv << N << "\t" << min << "\t" << max<< "\t"<< mean<< "\t"<< standard_deviation<<"\n";
    }//loop over basis functions
    conv.close();
}


template<typename TruthModelType>
template<class Archive>
void
CRB<TruthModelType>::save( Archive & ar, const unsigned int version ) const
{
    int proc_number = this->worldComm().globalRank();

    LOG(INFO) <<"[CRB::save] version : "<<version<<std::endl;

    auto mesh = mesh_type::New();
    auto is_mesh_loaded = mesh->load( _name="mymesh",_path=this->dbLocalPath(),_type="binary" );

    if ( ! is_mesh_loaded )
    {
        auto first_element = M_WN[0];
        mesh = first_element.functionSpace()->mesh() ;
        mesh->save( _name="mymesh",_path=this->dbLocalPath(),_type="binary" );
    }


    ar & boost::serialization::base_object<super>( *this );
    ar & BOOST_SERIALIZATION_NVP( M_output_index );
    ar & BOOST_SERIALIZATION_NVP( M_N );
    ar & BOOST_SERIALIZATION_NVP( M_rbconv );
    ar & BOOST_SERIALIZATION_NVP( M_error_type );
    ar & BOOST_SERIALIZATION_NVP( M_Xi );
    ar & BOOST_SERIALIZATION_NVP( M_WNmu );
    ar & BOOST_SERIALIZATION_NVP( M_Aqm_pr );
    ar & BOOST_SERIALIZATION_NVP( M_Aqm_du );
    ar & BOOST_SERIALIZATION_NVP( M_Aqm_pr_du );
    ar & BOOST_SERIALIZATION_NVP( M_Fqm_pr );
    ar & BOOST_SERIALIZATION_NVP( M_Fqm_du );
    ar & BOOST_SERIALIZATION_NVP( M_Lqm_pr );
    ar & BOOST_SERIALIZATION_NVP( M_Lqm_du );

    ar & BOOST_SERIALIZATION_NVP( M_C0_pr );
    ar & BOOST_SERIALIZATION_NVP( M_C0_du );
    ar & BOOST_SERIALIZATION_NVP( M_Lambda_pr );
    ar & BOOST_SERIALIZATION_NVP( M_Lambda_du );
    ar & BOOST_SERIALIZATION_NVP( M_Gamma_pr );
    ar & BOOST_SERIALIZATION_NVP( M_Gamma_du );


    ar & BOOST_SERIALIZATION_NVP( M_Mqm_pr );
    ar & BOOST_SERIALIZATION_NVP( M_Mqm_du );
    ar & BOOST_SERIALIZATION_NVP( M_Mqm_pr_du );

    if ( model_type::is_time_dependent )
    {

            ar & BOOST_SERIALIZATION_NVP( M_coeff_pr_ini_online );
            ar & BOOST_SERIALIZATION_NVP( M_coeff_du_ini_online );
            ar & BOOST_SERIALIZATION_NVP( M_Cmf_pr );
            ar & BOOST_SERIALIZATION_NVP( M_Cma_pr );
            ar & BOOST_SERIALIZATION_NVP( M_Cmm_pr );
            ar & BOOST_SERIALIZATION_NVP( M_Cmf_du );
            ar & BOOST_SERIALIZATION_NVP( M_Cmf_du_ini );
            ar & BOOST_SERIALIZATION_NVP( M_Cma_du );
            ar & BOOST_SERIALIZATION_NVP( M_Cmm_du );
    }
    ar & BOOST_SERIALIZATION_NVP ( M_database_contains_variance_info );
    if( M_database_contains_variance_info )
        ar & BOOST_SERIALIZATION_NVP( M_variance_matrix_phi );

        ar & BOOST_SERIALIZATION_NVP( M_Fqm_pr );
        ar & BOOST_SERIALIZATION_NVP( M_InitialGuessV_pr );

        ar & BOOST_SERIALIZATION_NVP( M_current_mu );
        ar & BOOST_SERIALIZATION_NVP( M_no_residual_index );

#if 0
        for(int i=0; i<M_N; i++)
            ar & BOOST_SERIALIZATION_NVP( M_WN[i] );
        for(int i=0; i<M_N; i++)
            ar & BOOST_SERIALIZATION_NVP( M_WNdu[i] );
#endif

        ar & BOOST_SERIALIZATION_NVP( M_maxerror );
        ar & BOOST_SERIALIZATION_NVP( M_use_newton );
        ar & BOOST_SERIALIZATION_NVP( M_Jqm_pr );
        ar & BOOST_SERIALIZATION_NVP( M_Rqm_pr );
}


template<typename TruthModelType>
template<class Archive>
void
CRB<TruthModelType>::load( Archive & ar, const unsigned int version )
{

    //if( version <= 4 )
    //    throw std::logic_error( "[CRB::load] ERROR while loading the existing database, since version 5 there was many changes. Please use the option --crb.rebuild-database=true " );
    int proc_number = this->worldComm().globalRank();

    LOG(INFO) <<"[CRB::load] version"<< version <<"\n";

#if 0
    mesh_ptrtype mesh;
    space_ptrtype Xh;

    if ( !M_model )
    {
        LOG(INFO) << "[load] model not initialized, loading fdb files...\n";
        mesh = mesh_type::New();
        bool is_mesh_loaded = mesh->load( _name="mymesh",_path=this->dbLocalPath(),_type="binary" );
        Xh = space_type::New( mesh );
        LOG(INFO) << "[load] loading fdb files done.\n";
    }
    else
    {
        LOG(INFO) << "[load] get mesh/Xh from model...\n";
        mesh = M_model->functionSpace()->mesh();
        Xh = M_model->functionSpace();
        LOG(INFO) << "[load] get mesh/Xh from model done.\n";
    }
#endif

    typedef boost::bimap< int, double > old_convergence_type;
    ar & boost::serialization::base_object<super>( *this );
    ar & BOOST_SERIALIZATION_NVP( M_output_index );
    ar & BOOST_SERIALIZATION_NVP( M_N );

	ar & BOOST_SERIALIZATION_NVP( M_rbconv );

    ar & BOOST_SERIALIZATION_NVP( M_error_type );
    ar & BOOST_SERIALIZATION_NVP( M_Xi );
    ar & BOOST_SERIALIZATION_NVP( M_WNmu );
    ar & BOOST_SERIALIZATION_NVP( M_Aqm_pr );
    ar & BOOST_SERIALIZATION_NVP( M_Aqm_du );
    ar & BOOST_SERIALIZATION_NVP( M_Aqm_pr_du );
    ar & BOOST_SERIALIZATION_NVP( M_Fqm_pr );
    ar & BOOST_SERIALIZATION_NVP( M_Fqm_du );
    ar & BOOST_SERIALIZATION_NVP( M_Lqm_pr );
    ar & BOOST_SERIALIZATION_NVP( M_Lqm_du );
    ar & BOOST_SERIALIZATION_NVP( M_C0_pr );
    ar & BOOST_SERIALIZATION_NVP( M_C0_du );
    ar & BOOST_SERIALIZATION_NVP( M_Lambda_pr );
    ar & BOOST_SERIALIZATION_NVP( M_Lambda_du );
    ar & BOOST_SERIALIZATION_NVP( M_Gamma_pr );
    ar & BOOST_SERIALIZATION_NVP( M_Gamma_du );

    ar & BOOST_SERIALIZATION_NVP( M_Mqm_pr );
    ar & BOOST_SERIALIZATION_NVP( M_Mqm_du );
    ar & BOOST_SERIALIZATION_NVP( M_Mqm_pr_du );

    if ( model_type::is_time_dependent )
    {
            ar & BOOST_SERIALIZATION_NVP( M_coeff_pr_ini_online );
            ar & BOOST_SERIALIZATION_NVP( M_coeff_du_ini_online );
            ar & BOOST_SERIALIZATION_NVP( M_Cmf_pr );
            ar & BOOST_SERIALIZATION_NVP( M_Cma_pr );
            ar & BOOST_SERIALIZATION_NVP( M_Cmm_pr );
            ar & BOOST_SERIALIZATION_NVP( M_Cmf_du );
            ar & BOOST_SERIALIZATION_NVP( M_Cmf_du_ini );
            ar & BOOST_SERIALIZATION_NVP( M_Cma_du );
            ar & BOOST_SERIALIZATION_NVP( M_Cmm_du );
    }

    ar & BOOST_SERIALIZATION_NVP ( M_database_contains_variance_info );
    if( M_database_contains_variance_info )
        ar & BOOST_SERIALIZATION_NVP( M_variance_matrix_phi );
        ar & BOOST_SERIALIZATION_NVP( M_Fqm_pr );
        ar & BOOST_SERIALIZATION_NVP( M_InitialGuessV_pr );

        ar & BOOST_SERIALIZATION_NVP( M_current_mu );
        ar & BOOST_SERIALIZATION_NVP( M_no_residual_index );

        ar & BOOST_SERIALIZATION_NVP( M_maxerror );
        ar & BOOST_SERIALIZATION_NVP( M_use_newton );
        ar & BOOST_SERIALIZATION_NVP( M_Jqm_pr );
        ar & BOOST_SERIALIZATION_NVP( M_Rqm_pr );

        if( this->vm()["crb.use-newton"].template as<bool>() != M_use_newton  )
        {
            if( M_use_newton )
                throw std::logic_error( "[CRB::loadDB] ERROR in the database used the option use-newton=true and it's not the case in your option" );
            else
                throw std::logic_error( "[CRB::loadDB] ERROR in the database used the option use-newton=false and it's not the case in your option" );
        }

#if 0
    std::cout << "[loadDB] output index : " << M_output_index << "\n"
              << "[loadDB] N : " << M_N << "\n"
              << "[loadDB] error type : " << M_error_type << "\n";

    for ( auto it = M_rbconv.begin(), en = M_rbconv.end();it != en; ++it )
        std::cout << "[loadDB] convergence: (" << it->left << ","  << it->right  << ")\n";

        element_type temp = Xh->element();

        M_WN.resize( M_N );
        M_WNdu.resize( M_N );

        for( int i = 0 ; i < M_N ; i++ )
        {
            temp.setName( (boost::format( "fem-primal-%1%" ) % ( i ) ).str() );
            ar & BOOST_SERIALIZATION_NVP( temp );
            M_WN[i] = temp;
        }

        for( int i = 0 ; i < M_N ; i++ )
        {
            temp.setName( (boost::format( "fem-dual-%1%" ) % ( i ) ).str() );
            ar & BOOST_SERIALIZATION_NVP( temp );
            M_WNdu[i] = temp;
        }

#endif
    LOG(INFO) << "[CRB::load] end of load function" << std::endl;
}


template<typename TruthModelType>
bool
CRB<TruthModelType>::printErrorDuringOfflineStep()
{
  bool print = this->vm()["crb.print-error-during-rb-construction"].template as<bool>();
    return print;
}

template<typename TruthModelType>
bool
CRB<TruthModelType>::rebuildDB()
{
    bool rebuild = this->vm()["crb.rebuild-database"].template as<bool>();
    return rebuild;
}

template<typename TruthModelType>
bool
CRB<TruthModelType>::showMuSelection()
{
    bool show = this->vm()["crb.show-mu-selection"].template as<bool>();
    return show;
}


template<typename TruthModelType>
void
CRB<TruthModelType>::saveDB()
{
    fs::ofstream ofs( this->dbLocalPath() / this->dbFilename() );

    if ( ofs )
    {
        boost::archive::text_oarchive oa( ofs );
        // write class instance to archive
        oa << *this;
        // archive and stream closed when destructors are called
    }
}

template<typename TruthModelType>
bool
CRB<TruthModelType>::loadDB()
{
    if ( this->rebuildDB() )
        return false;

    if( this->isDBLoaded() )
        return true;

    fs::path db = this->lookForDB();

    if ( db.empty() )
        return false;

    if ( !fs::exists( db ) )
        return false;

    //std::cout << "Loading " << db << "...\n";
    fs::ifstream ifs( db );

    if ( ifs )
    {
        boost::archive::text_iarchive ia( ifs );
        // write class instance to archive
        ia >> *this;
        //std::cout << "Loading " << db << " done...\n";
        this->setIsLoaded( true );
        // archive and stream closed when destructors are called
        return true;
    }

    return false;
}

} // Feel

namespace boost
{
namespace serialization
{
template< typename T>
struct version< Feel::CRB<T> >
{
    // at the moment the version of the CRB DB is 0. if any changes is done
    // to the format it is mandatory to increase the version number below
    // and use the new version number of identify the new entries in the DB
    typedef mpl::int_<0> type;
    typedef mpl::integral_c_tag tag;
    static const unsigned int value = version::type::value;
};
template<typename T> const unsigned int version<Feel::CRB<T> >::value;
}
}
#endif /* __CRB_H */
