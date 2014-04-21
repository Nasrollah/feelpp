/* -*- mode: c++ -*-

  This file is part of the Feel library

  Author(s): Christophe Prud'homme <christophe.prudhomme@feelpp.org>
  Author(s): Stephane Veys <stephane.veys@imag.fr>
       Date: 2014-01-19

  Copyright (C) 2011-2014 Feel++ Consortium

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
   \file benchmarkgrepl.hpp
   \author Christophe Prud'homme <christophe.prudhomme@feelpp.org>
   \author Stephane Veys <stephane.veys@imag.fr>

   date 2014-01-19
 */
#ifndef __BenchmarkGrepl_H
#define __BenchmarkGrepl_H 1

#include <boost/timer.hpp>
#include <boost/shared_ptr.hpp>

#include <feel/options.hpp>
#include <feel/feelcore/feel.hpp>

#include <feel/feelalg/backend.hpp>

#include <feel/feeldiscr/functionspace.hpp>
#include <feel/feeldiscr/region.hpp>
#include <feel/feelpoly/im.hpp>

#include <feel/feelfilters/gmsh.hpp>
#include <feel/feelpoly/polynomialset.hpp>
#include <feel/feelalg/solvereigen.hpp>

#include <feel/feelvf/vf.hpp>
#include <feel/feelcrb/parameterspace.hpp>
#include <feel/feelcrb/modelcrbbase.hpp>

#include <Eigen/Core>
#include <Eigen/LU>
#include <Eigen/Dense>

#include <feel/feeldiscr/reducedbasisspace.hpp>



namespace Feel
{

po::options_description
makeBenchmarkGreplOptions()
{
    po::options_description bgoptions( "BenchmarkGrepl options" );
    bgoptions.add_options()
        ( "mshfile", Feel::po::value<std::string>()->default_value( "" ), "name of the gmsh file input")
        ( "hsize", Feel::po::value<double>()->default_value( 1e-1 ), "hsize")
        ( "trainset-eim-size", Feel::po::value<int>()->default_value( 40 ), "EIM trainset is built using a equidistributed grid 40 * 40 by default")
        ( "gamma", Feel::po::value<double>()->default_value( 10 ), "penalisation parameter for the weak boundary Dirichlet formulation" )
        ;
    return bgoptions;
}
AboutData
makeBenchmarkGreplAbout( std::string const& str = "benchmarkGrepl" )
{
    Feel::AboutData about( str.c_str(),
                           str.c_str(),
                           "0.1",
                           "Benchmark Grepl",
                           Feel::AboutData::License_GPL,
                           "Copyright (C) 2011-2014 Feel++ Consortium" );

    about.addAuthor( "Christophe Prud'homme", "developer", "christophe.prudhomme@feelpp.org", "" );
    about.addAuthor( "Stephane Veys", "developer", "stephane.veys@imag.fr", "" );
    return about;
}


class ParameterDefinition
{
public :
    static const uint16_type ParameterSpaceDimension = 2;
    typedef ParameterSpace<ParameterSpaceDimension> parameterspace_type;
};

template<int Order>
class FunctionSpaceDefinition
{
public :
    //static const uint16_type Order = 1;
    typedef double value_type;

    /*mesh*/
    typedef Simplex<2,1> entity_type; /*dim,order*/
    typedef Mesh<entity_type> mesh_type;

    /*basis*/
    typedef bases<Lagrange<Order, Scalar> > basis_type;

    /*space*/
    typedef FunctionSpace<mesh_type, basis_type, value_type> space_type;

    static const bool is_time_dependent = false;
    static const bool is_linear = true;

};

template <typename ParameterDefinition, typename FunctionSpaceDefinition >
class EimDefinition
{
public :
    typedef typename ParameterDefinition::parameterspace_type parameterspace_type;
    typedef typename FunctionSpaceDefinition::space_type space_type;


    /* EIM */
    // Scalar continuous //
    typedef EIMFunctionBase<space_type, space_type , parameterspace_type> fun_type;
    // Scalar Discontinuous //
    typedef EIMFunctionBase<space_type, space_type , parameterspace_type> fund_type;

};

/**
 * \class BenchmarkGrepl
 * \brief brief description
 *
 * This is from the paper
 * EFFICIENT REDUCED-BASIS TREATMENT OF NONAFFINE
 * AND NONLINEAR PARTIAL DIFFERENTIAL EQUATIONS
 *  authors :
 * Martin A. Grepl, Yvon Maday, Ngoc C. Nguyen and Anthony T. Patera
 * ESAIM: Mathematical Modelling and Numerical Analysis
 * --
 * 3. Nonaffine linear coercive elliptic equations
 *
 * @author Christophe Prud'homme
 * @author Stephane Veys
 * @see
 */
template<int Order>
class BenchmarkGrepl : public ModelCrbBase< ParameterDefinition, FunctionSpaceDefinition<Order> , EimDefinition<ParameterDefinition, FunctionSpaceDefinition<Order> > >,
                       public boost::enable_shared_from_this< BenchmarkGrepl<Order> >
{
public:

    typedef ModelCrbBase<ParameterDefinition, FunctionSpaceDefinition<Order>, EimDefinition<ParameterDefinition,FunctionSpaceDefinition<Order> > > super_type;
    typedef typename super_type::funs_type funs_type;
    typedef typename super_type::funsd_type funsd_type;


    /** @name Constants
     */
    //@{

    //static const uint16_type Order = 1;
    static const uint16_type ParameterSpaceDimension = 2;

    //@}

    /** @name Typedefs
     */
    //@{

    typedef double value_type;

    typedef Backend<value_type> backend_type;
    typedef boost::shared_ptr<backend_type> backend_ptrtype;

    /*matrix*/
    typedef backend_type::sparse_matrix_type sparse_matrix_type;
    typedef backend_type::sparse_matrix_ptrtype sparse_matrix_ptrtype;
    typedef backend_type::vector_type vector_type;
    typedef backend_type::vector_ptrtype vector_ptrtype;

    typedef Eigen::Matrix<double,Eigen::Dynamic,Eigen::Dynamic> eigen_matrix_type;
    typedef eigen_matrix_type ematrix_type;
    typedef boost::shared_ptr<eigen_matrix_type> eigen_matrix_ptrtype;

    /*mesh*/
    typedef Simplex<2,1> entity_type; /*dim,order*/
    typedef Mesh<entity_type> mesh_type;
    typedef boost::shared_ptr<mesh_type> mesh_ptrtype;

    /*basis*/
    typedef bases<Lagrange<Order, Scalar> > basis_type;

    /*space*/
    typedef FunctionSpace<mesh_type, basis_type, value_type> space_type;
    typedef boost::shared_ptr<space_type> space_ptrtype;
    typedef space_type functionspace_type;
    typedef space_ptrtype functionspace_ptrtype;
    typedef typename space_type::element_type element_type;
    typedef boost::shared_ptr<element_type> element_ptrtype;

    /*reduced basis space*/
    typedef ReducedBasisSpace<super_type, mesh_type, basis_type, value_type> rbfunctionspace_type;
    typedef boost::shared_ptr< rbfunctionspace_type > rbfunctionspace_ptrtype;


    /* parameter space */
    typedef ParameterSpace<ParameterSpaceDimension> parameterspace_type;
    typedef boost::shared_ptr<parameterspace_type> parameterspace_ptrtype;
    typedef typename parameterspace_type::element_type parameter_type;
    typedef typename parameterspace_type::element_ptrtype parameter_ptrtype;
    typedef typename parameterspace_type::sampling_type sampling_type;
    typedef typename parameterspace_type::sampling_ptrtype sampling_ptrtype;


    typedef Eigen::VectorXd vectorN_type;
    typedef std::vector< std::vector< double > > beta_vector_type;

    typedef boost::tuple<
        std::vector< std::vector<sparse_matrix_ptrtype> >,
        std::vector< std::vector<std::vector<vector_ptrtype> > >
        > affine_decomposition_type;

    typedef boost::tuple<
        std::map<int,double>,
        std::vector< std::map<int,double> >
        > eim_interpolation_error_type;

    typedef boost::tuple< sparse_matrix_ptrtype, std::vector<vector_ptrtype> > monolithic_type;

    typedef Preconditioner<double> preconditioner_type;
    typedef boost::shared_ptr<preconditioner_type> preconditioner_ptrtype;

    typedef std::vector< std::vector<sparse_matrix_ptrtype> > vector_sparse_matrix;
    //@}

    /** @name Constructors, destructor
     */
    //@{

    //! default constructor
    BenchmarkGrepl();

    //! constructor from command line
    BenchmarkGrepl( po::variables_map const& vm );


    //! copy constructor
    BenchmarkGrepl( BenchmarkGrepl const & );
    //! destructor
    ~BenchmarkGrepl() {}

    //! initialization of the model
    void initModel();
    //@}

    std::string modelName()
    {
        std::ostringstream ostr;
        ostr << "BenchMarkGrepl" <<  Order;
        return ostr.str();
    }

    /** @name Operator overloads
     */
    //@{

    //@}

    /** @name Accessors
     */
    //@{

    //\return the list of EIM objects
    virtual funs_type scalarContinuousEim() const
    {
        return M_funs;
    }

    int mMaxA( int q )
    {
        if ( q==0 )
            return 1;
        if( q==1 )
        {
            auto eim_g = M_funs[0];
            return eim_g->mMax();
        }
        else
            throw std::logic_error( "[Model Benchmark Grepl] ERROR : try to acces to mMaxA(q) with a bad value of q");
    }

    int mMaxF( int output_index, int q)
    {
        if ( q == 0 )
            return 1;
        if( q == 1 )
        {
            auto eim_g = M_funs[0];
            return eim_g->mMax();
        }
        else
            throw std::logic_error( "[Model Benchmark Grepl] ERROR : try to acces to mMaxF(output_index,q) with a bad value of q");
    }


    /**
     * \brief Returns the function space
     */
    space_ptrtype functionSpace()
    {
        return Xh;
    }

    /**
     * \brief Returns the reduced basis function space
     */
    rbfunctionspace_ptrtype rBFunctionSpace()
    {
        return RbXh;
    }

    //! return the parameter space
    parameterspace_ptrtype parameterSpace() const
    {
        return M_Dmu;
    }

    /**
     * \brief compute the theta coefficient for both bilinear and linear form
     * \param mu parameter to evaluate the coefficients
     */
    boost::tuple<beta_vector_type,  std::vector<beta_vector_type> >
    computeBetaQm( element_type const& T,parameter_type const& mu , double time=1e30 )
    {
        return computeBetaQm( mu , time );
    }

    boost::tuple<beta_vector_type,  std::vector<beta_vector_type>  >
    computeBetaQm( parameter_type const& mu , double time=1e30 )
    {
        double mu0   = mu( 0 );
        double mu1    = mu( 1 );
        this->M_betaAqm.resize( 2 );
        this->M_betaAqm[0].resize( 1 );
        this->M_betaAqm[0][0]=1;

        auto eim_g = M_funs[0];
        int M_g = eim_g->mMax();
        vectorN_type beta_g = eim_g->beta( mu );
        this->M_betaAqm[1].resize( M_g );
        for(int m=0; m<M_g; m++)
        {
            this->M_betaAqm[1][m] = beta_g(m);
        }

        this->M_betaFqm.resize( 2 );
        this->M_betaFqm[0].resize( 1 );
        this->M_betaFqm[0][0].resize( M_g );
        for(int m=0; m<M_g; m++)
        {
            this->M_betaFqm[0][0][m] = beta_g(m);
        }

        this->M_betaFqm[1].resize( 1 );
        this->M_betaFqm[1][0].resize( 1 );
        this->M_betaFqm[1][0][0] = 1;

        return boost::make_tuple( this->M_betaAqm, this->M_betaFqm );
    }

    beta_vector_type computeBetaLinearDecompositionA( parameter_type const& mu , double time=1e30 )
    {
        beta_vector_type beta;
        beta.resize(1);
        beta[0].resize(1);
        beta[0][0]=1;
        return beta;
    }


    //@}

    /** @name  Mutators
     */
    //@{


    //@}

    /** @name  Methods
     */
    //@{

    /**
     * \brief Returns the affine decomposition
     */
    affine_decomposition_type computeAffineDecomposition();
    std::vector< std::vector<sparse_matrix_ptrtype> > computeLinearDecompositionA();
    monolithic_type computeMonolithicFormulation( parameter_type const& mu );

    void assemble();


    //@}

    /**
     * inner product
     */
    sparse_matrix_ptrtype energyMatrix ( void )
    {
        return M;
    }

    /**
     * Given the output index \p output_index and the parameter \p mu, return
     * the value of the corresponding FEM output
     */
    value_type output( int output_index, parameter_type const& mu, element_type &T, bool need_to_solve=false, bool export_outputs=false );

    bool referenceParametersGivenByUser() { return true; }
    parameter_type refParameter()
    {
        return M_Dmu->min();
    }

    gmsh_ptrtype createGeo( double hsize );

    void checkEimExpansion();

    eim_interpolation_error_type eimInterpolationErrorEstimation()
    {
        std::map<int,double> eim_error_aq;
        std::vector< std::map<int,double> > eim_error_fq;

        //in that case we make an error on the eim approximation
        //M_Aqm[1] contains the eim approximation
        eim_error_aq.insert( std::pair<int,double>(1 , 0 ) );
        eim_error_fq.resize(2);
        //M_Fqm[0][0] contains the eim approximation
        eim_error_fq[0].insert( std::pair<int,double>(0 , 0 ) );
        return boost::make_tuple( eim_error_aq, eim_error_fq );
    }

    eim_interpolation_error_type eimInterpolationErrorEstimation( parameter_type const& mu , vectorN_type const& uN )
    {

        std::map<int,double> eim_error_aq;
        std::vector< std::map<int,double> > eim_error_fq;

        auto eim_g = M_funs[0];
        bool error;
        int max = eim_g->mMax(error);
        if( error )
        {
            //in that case we make an error on the eim approximation
            int size=uN.size();
            auto solution = Feel::expansion( RbXh->primalRB(), uN , size);
            double eim_error = eim_g->interpolationErrorEstimation(mu,solution,max).template get<0>();
            //M_Aqm[1] contains the eim approximation
            eim_error_aq.insert( std::pair<int,double>(1 , eim_error) );
            eim_error_fq.resize(2);
            //M_Fqm[0][0] contains the eim approximation
            eim_error_fq[0].insert( std::pair<int,double>(0 , eim_error) );
        }
        return boost::make_tuple( eim_error_aq, eim_error_fq );
    }


private:

    backend_ptrtype M_backend;

    parameterspace_ptrtype M_Dmu;

    /* mesh, pointers and spaces */
    mesh_ptrtype mesh;
    space_ptrtype Xh;
    rbfunctionspace_ptrtype RbXh;

    sparse_matrix_ptrtype D,M;
    vector_ptrtype F;

    element_ptrtype pT;

    sparse_matrix_ptrtype M_monoA;
    std::vector<vector_ptrtype> M_monoF;

    parameter_type M_mu;

    funs_type M_funs;
};

template<int Order>
BenchmarkGrepl<Order>::BenchmarkGrepl()
    :
    M_backend( backend_type::build( BACKEND_PETSC ) ),
    M_Dmu( new parameterspace_type ),
    M_mu( M_Dmu->element() )
{ }

template<int Order>
BenchmarkGrepl<Order>::BenchmarkGrepl( po::variables_map const& vm )
    :
    M_backend( backend_type::build( vm ) ),
    M_Dmu( new parameterspace_type ),
    M_mu( M_Dmu->element() )
{
}


template<int Order>
gmsh_ptrtype
BenchmarkGrepl<Order>::createGeo( double hsize )
{
    gmsh_ptrtype gmshp( new Gmsh );
    std::ostringstream ostr;
    double H = hsize;
    ostr <<"Point (1) = {0, 0, 0,"<< H <<"};\n"
         <<"Point (2) = {1, 0, 0,"<< H <<"};\n"
         <<"Point (3) = {1, 1, 0,"<< H <<"};\n"
         <<"Point (4) = {0, 1, 0,"<< H <<"};\n"
         <<"Line (11) = {1,2};\n"
         <<"Line (12) = {2,3};\n"
         <<"Line (13) = {3,4};\n"
         <<"Line (14) = {4,1};\n"
         <<"Line Loop (21) = {11, 12, 13, 14};\n"
         <<"Plane Surface (30) = {21};\n"
         <<"Physical Line (\"boundaries\") = {11,12,13,14};\n"
         <<"Physical Surface (\"Omega\") = {30};\n"
         ;
    std::ostringstream nameStr;
    nameStr.precision( 3 );
    nameStr << "benchmarkgrepl_geo";
    gmshp->setPrefix( nameStr.str() );
    gmshp->setDescription( ostr.str() );
    return gmshp;
}

template<int Order>
void BenchmarkGrepl<Order>::initModel()
{

    using namespace Feel::vf;

    std::string mshfile_name = option("mshfile").as<std::string>();

    /*
     * First we create the mesh or load it if already exist
     */

    if( mshfile_name=="" )
    {
        double hsize=option(_name="hsize").template as<double>();
        mesh = createGMSHMesh( _mesh=new mesh_type,
                               _update=MESH_CHECK|MESH_UPDATE_FACES|MESH_UPDATE_EDGES|MESH_RENUMBER,
                               _desc = createGeo( hsize ) );

        //mesh = unitSquare( hsize );
    }
    else
    {
        mesh = loadGMSHMesh( _mesh=new mesh_type,
                             _filename=option("mshfile").as<std::string>(),
                             _update=MESH_CHECK|MESH_UPDATE_FACES|MESH_UPDATE_EDGES|MESH_RENUMBER );
    }


    /*
     * The function space and some associate elements are then defined
     */
    Xh = space_type::New( mesh );
    RbXh = rbfunctionspace_type::New( _model=this->shared_from_this() , _mesh=mesh );
    if( Environment::worldComm().isMasterRank() )
    {
        std::cout << "Number of dof " << Xh->nDof() << std::endl;
        std::cout << "Number of local dof " << Xh->nLocalDof() << std::endl;
    }

    // allocate an element of Xh
    pT = element_ptrtype( new element_type( Xh ) );

    typename Feel::ParameterSpace<ParameterSpaceDimension>::Element mu_min( M_Dmu );
    mu_min <<  -1, -1;
    M_Dmu->setMin( mu_min );
    typename Feel::ParameterSpace<ParameterSpaceDimension>::Element mu_max( M_Dmu );
    mu_max << -0.01, -0.01;
    M_Dmu->setMax( mu_max );

    M_mu = M_Dmu->element();

    auto Pset = M_Dmu->sampling();
    //specify how many elements we take in each direction
    std::vector<int> N(2);
    int Ne = option(_name="trainset-eim-size").template as<int>();
    std::string supersamplingname =(boost::format("DmuEim-Ne%1%-generated-by-master-proc") %Ne ).str();

    // std::string file_name = ( boost::format("eim_trainset_Ne%1%-proc%2%on%3%") % Ne %proc_number %total_proc).str();
    std::ifstream file ( supersamplingname );

    //40 elements in each direction
    N[0]=Ne; N[1]=Ne;

    //interpolation points are located on different proc
    //so we can't distribute parameters on different proc as in crb case
    //else for a given mu we are not able to evaluate g at a node wich
    //is not on the same proc than mu (so it leads to wrong results !)
    bool all_proc_same_sampling=true;

    if( ! file )
    {
        //std::string supersamplingname =(boost::format("DmuEim-Ne%1%-generated-by-master-proc") %Ne ).str();
        Pset->equidistributeProduct( N , all_proc_same_sampling , supersamplingname );
        Pset->writeOnFile( supersamplingname );
    }
    else
    {
        Pset->clear();
        Pset->readFromFile(supersamplingname);
    }

    auto eim_g = eim( _model=eim_no_solve(this->shared_from_this()),
                      _element=*pT,
                      _space=Xh,
                      _parameter=M_mu,
                      _expr=1./sqrt( (Px()-cst_ref(M_mu(0)))*(Px()-cst_ref(M_mu(0))) + (Py()-cst_ref(M_mu(1)))*(Py()-cst_ref(M_mu(1))) ),
                      _sampling=Pset,
                      _name="eim_g" );

#if 0
    if( Environment::worldComm().isMasterRank() )
    {
        bool error;
        std::cout<<" eim g mMax : "<<eim_g->mMax(error)<<" error : "<<error<<std::endl;
    }
#endif

    M_funs.push_back( eim_g );

    //checkEimExpansion();

    assemble();

} // BenchmarkGrepl::init


template<int Order>
void BenchmarkGrepl<Order>::checkEimExpansion()
{
    auto Pset = M_Dmu->sampling();
    std::vector<int> N(2);
    int Ne = option(_name="trainset-eim-size").template as<int>();
    N[0]=Ne; N[1]=Ne;
    bool all_proc_same_sampling=true;
    std::string supersamplingname =(boost::format("PsetCheckEimExpansion-Ne%1%-generated-by-master-proc") %Ne ).str();
    std::ifstream file ( supersamplingname );
    if( ! file )
    {
        Pset->equidistributeProduct( N , all_proc_same_sampling , supersamplingname );
        Pset->writeOnFile( supersamplingname );
    }
    else
    {
        Pset->clear();
        Pset->readFromFile(supersamplingname);
    }

    auto eim_g = M_funs[0];

    //check that eim expansion of g is positive on each vertice
    int max = eim_g->mMax();
    auto e = exporter( _mesh=mesh );
    BOOST_FOREACH( auto mu, *Pset )
    {

        if( Environment::worldComm().isMasterRank() )
            std::cout<<"check gM for mu = ["<< mu(0)<<" , "<<mu(1)<<"]"<<std::endl;

        auto exprg = 1./sqrt( (Px()-mu(0))*(Px()-mu(0)) + (Py()-mu(1))*(Py()-mu(1)) );
        auto g = vf::project( _space=Xh, _expr=exprg );

        for(int m=1; m<max; m++)
        {
            vectorN_type beta_g = eim_g->beta( mu , m );

            auto gM = expansion( eim_g->q(), beta_g , m);
            auto px=vf::project(_space=Xh, _expr=Px() );
            auto py=vf::project(_space=Xh, _expr=Py() );

            int size=Xh->nLocalDof();
            for(int v=0; v<size; v++)
            {
                double x = px(v);
                double y = py(v);
                if( gM(v) < -1e-13 )
                    std::cout<<"gM("<<x<<","<<y<<") = "<<gM(v)<<" ! - proc  "<<Environment::worldComm().globalRank()<<" but g("<<x<<","<<y<<") =  "<<g(v)<<" === m : "<<m<<std::endl;
                if( g(v) < -1e-13  )
                    std::cout<<"g("<<x<<","<<y<<") = "<<g(v)<<" donc la projection de g est negative"<<std::endl;
            }
#if 0

            for(int i=0; i<beta_g.size(); i++)
            {
                if( beta_g(i) < - 1e-14 && Environment::worldComm().isMasterRank() )
                {
                    std::cout<<"beta("<<i<<") is negative : "<<beta_g(i)<<"  === m : "<<m<<std::endl;
                }
            }
            //std::string name =( boost::format("GM%1%") %m ).str();
            //e->add( name , gM );
            //if( m == max-1 )
            //e->add( "exact" , g );

                //auto basis = eim_g->q(i);
                //double basisnorm = basis.l2Norm();
                //if( basisnorm < 1e-14 && Environment::worldComm().isMasterRank() )
                    //{
                    //std::cout<<"basis "<<i<<" norm negative : "<<basisnorm<<std::endl;
                    //exit(0);
                    //}
                //for(int v=0; v<size; v++)
                //{
                //    if( basis(i) < 1e-14 )
                //        std::cout<<"basis("<<v<<") = "<<basis(v)<<" ! - proc  "<<Environment::worldComm().globalRank()<<std::endl;
                //}
            }
#endif
        }//loop over m
        //e->save();
    }
}

template<int Order>
typename BenchmarkGrepl<Order>::monolithic_type
BenchmarkGrepl<Order>::computeMonolithicFormulation( parameter_type const& mu )
{
    auto u=Xh->element();
    auto v=Xh->element();
    double gamma_dir = option(_name="gamma").template as<double>();

    auto exprg = 1./sqrt( (Px()-mu(0))*(Px()-mu(0)) + (Py()-mu(1))*(Py()-mu(1)) );
    auto g = vf::project( _space=Xh, _expr=exprg );
    M_monoA = M_backend->newMatrix( Xh, Xh );
    M_monoF.resize(2);
    M_monoF[0] = M_backend->newVector( Xh );
    M_monoF[1] = M_backend->newVector( Xh );
    form2( Xh, Xh, M_monoA ) = integrate( _range=elements( mesh ), _expr=gradt( u )*trans( grad( v ) ) + idt( u )*id( v )*idv(g) ) +
        integrate( markedfaces( mesh, "boundaries" ), gamma_dir*idt( u )*id( v )/h()
                   -gradt( u )*vf::N()*id( v )
                   -grad( v )*vf::N()*idt( u )
                   );
    form1( Xh, M_monoF[0] ) = integrate( _range=elements(mesh) , _expr=id( v ) * idv(g) );
    form1( Xh, M_monoF[1] ) = integrate( _range=elements(mesh) , _expr=id( v ) );

    return boost::make_tuple( M_monoA, M_monoF );

}


template<int Order>
void BenchmarkGrepl<Order>::assemble()
{
    auto u = Xh->element();
    auto v = Xh->element();

    double gamma_dir = option(_name="gamma").template as<double>();
    auto eim_g = M_funs[0];

    this->M_Aqm.resize( 2 );
    this->M_Aqm[0].resize( 1 );
    this->M_Aqm[0][0] = M_backend->newMatrix( Xh, Xh );
    form2( _test=Xh, _trial=Xh, _matrix=this->M_Aqm[0][0] ) =
        integrate( elements( mesh ), gradt( u )*trans( grad( v ) ) ) +
        integrate( markedfaces( mesh, "boundaries" ), gamma_dir*idt( u )*id( v )/h()
                   -gradt( u )*vf::N()*id( v )
                   -grad( v )*vf::N()*idt( u )
                   );

    bool error;
    int M_g = eim_g->mMax(error);
    if( error ) M_g++;
    this->M_Aqm[1].resize( M_g );
    for(int m=0; m<M_g; m++)
    {
        this->M_Aqm[1][m] = M_backend->newMatrix( Xh, Xh );
        form2( _test=Xh, _trial=Xh, _matrix=this->M_Aqm[1][m] ) =
            integrate( elements( mesh ), idt( u )* id( v ) * idv( eim_g->q(m) ) );
    }

    this->M_Fqm.resize( 2 );
    this->M_Fqm[0].resize( 1 );
    this->M_Fqm[1].resize( 1 );

    this->M_Fqm[0][0].resize(M_g);
    for(int m=0; m<M_g; m++)
    {
        this->M_Fqm[0][0][m] = M_backend->newVector( Xh );
        form1( Xh, this->M_Fqm[0][0][m] ) = integrate( elements( mesh ), id( v ) * idv( eim_g->q(m) ) );
    }
    this->M_Fqm[1][0].resize(1);
    this->M_Fqm[1][0][0] = M_backend->newVector( Xh );
    form1( Xh, this->M_Fqm[1][0][0] ) = integrate( elements( mesh ), id( v ) );



    //for scalarProduct
    auto mu = refParameter();

    M = M_backend->newMatrix( _test=Xh, _trial=Xh );
    form2( Xh, Xh, M ) =
        integrate( _range=elements( mesh ), _expr=gradt( u )*trans( grad( v ) ) )+
        integrate( markedfaces( mesh, "boundaries" ), gamma_dir*idt( u )*id( v )/h()
                   -gradt( u )*vf::N()*id( v )
                   -grad( v )*vf::N()*idt( u ) );

#if 0
    vectorN_type beta_g = eim_g->beta( mu );
    for(int m=0; m<M_g; m++)
    {
        auto q = eim_g->q(m);
        q.scale( beta_g(m) );
        form2( Xh, Xh, M ) +=  integrate( _range=elements( mesh ), _expr= idt( u )*id( v ) * idv( q ) );
    }
#endif

}


template<int Order>
typename BenchmarkGrepl<Order>::vector_sparse_matrix
BenchmarkGrepl<Order>::computeLinearDecompositionA()
{
    auto muref = refParameter();
    auto u=Xh->element();
    auto v=Xh->element();
    double gamma_dir = option(_name="gamma").template as<double>();

    auto exprg = 1./sqrt( (Px()-muref(0))*(Px()-muref(0)) + (Py()-muref(1))* (Py()-muref(1)) );
    auto g = vf::project( _space=Xh, _expr=exprg );
    vector_sparse_matrix A;
    A.resize(1);
    A[0].resize(1);
    A[0][0] = M_backend->newMatrix( Xh, Xh );
    form2( Xh, Xh, A[0][0] ) = integrate( _range=elements( mesh ), _expr=gradt( u )*trans( grad( v ) ) + idt( u )*id( v )*idv(g) ) +
        integrate( markedfaces( mesh, "boundaries" ), gamma_dir*idt( u )*id( v )/h()
                   -gradt( u )*vf::N()*id( v )
                   -grad( v )*vf::N()*idt( u )
                   );
    return A;

}

template<int Order>
typename BenchmarkGrepl<Order>::affine_decomposition_type
BenchmarkGrepl<Order>::computeAffineDecomposition()
{
    return boost::make_tuple( this->M_Aqm, this->M_Fqm );
}


template<int Order>
double BenchmarkGrepl<Order>::output( int output_index, parameter_type const& mu, element_type &solution, bool need_to_solve , bool export_outputs )
{

    CHECK( ! need_to_solve ) << "The model need to have the solution to compute the output\n";

    //solve the problem without affine decomposition
    //solution=this->solve(mu);

    double s=0;
    if ( output_index==0 )
    {
        for ( int q=0; q<1; q++ )
        {
            for ( int m=0; m<mMaxF(output_index,q); m++ )
            {
                s += this->M_betaFqm[output_index][q][m]*dot( *this->M_Fqm[output_index][q][m] , solution );
            }
        }
    }
    if( output_index==1 )
    {
        s = integrate( elements( mesh ), idv( solution ) ).evaluate()( 0,0 );
    }

    return s ;
}

}

#endif /* __BenchmarkGrepl_H */


