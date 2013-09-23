/* -*- mode: c++; coding: utf-8; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; show-trailing-whitespace: t -*-

  This file is part of the Feel library

  Author(s): Christophe Prud'homme <christophe.prudhomme@feelpp.org>
       Date: 2011-06-18

  Copyright (C) 2011 Université Joseph Fourier (Grenoble I)

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
   \file opusapp.hpp
   \author Christophe Prud'homme <christophe.prudhomme@feelpp.org>
   \date 2011-06-18
 */
#ifndef __OpusApp_H
#define __OpusApp_H 1

#include <feel/feel.hpp>

#include <boost/assign/std/vector.hpp>
#include <feel/feelcrb/crb.hpp>
#include <feel/feelcrb/eim.hpp>
#include <feel/feelcrb/crbmodel.hpp>
#include <boost/serialization/version.hpp>
#include <boost/range/join.hpp>
#include <boost/regex.hpp>
#include <boost/assign/list_of.hpp>

#include <Eigen/Core>
#include <Eigen/LU>
#include <Eigen/Dense>


namespace Feel
{
po::options_description opusapp_options( std::string const& prefix );
std::string _o( std::string const& prefix, std::string const& opt )
{
    std::string o = prefix;

    if ( !o.empty() )
        o += ".";

    return o + opt;
}


enum class SamplingMode
{
    RANDOM = 0, EQUIDISTRIBUTED = 1, LOGEQUIDISTRIBUTED = 2
};

#define prec 4
#define Pdim 7
#define fill ' '
#define dmanip std::scientific << std::setprecision( prec )
#define hdrmanip(N) std::setw(N) << std::setfill(fill) << std::right
#define tabmanip(N) std::setw(N) << std::setfill(fill) << std::right << dmanip


/**
 * \class OpusApp
 * \brief Certified Reduced Basis application
 *
 * @author Christophe Prud'homme
 */
    template<typename ModelType,
             template < typename ReducedMethod > class RM=CRB,
             template < typename ModelInterface > class Model=CRBModel>
class OpusApp   : public Application
{
    typedef Application super;
public:

    typedef double value_type;

    //! model type
    typedef ModelType model_type;
    typedef boost::shared_ptr<ModelType> model_ptrtype;

    //! function space type
    typedef typename model_type::functionspace_type functionspace_type;
    typedef typename model_type::functionspace_ptrtype functionspace_ptrtype;

    typedef typename model_type::element_type element_type;

    typedef Eigen::VectorXd vectorN_type;

#if 0
    //old
    typedef CRBModel<ModelType> crbmodel_type;
    typedef boost::shared_ptr<crbmodel_type> crbmodel_ptrtype;
    typedef CRB<crbmodel_type> crb_type;
    typedef boost::shared_ptr<crb_type> crb_ptrtype;
#endif

    typedef Model<ModelType> crbmodel_type;
    typedef boost::shared_ptr<crbmodel_type> crbmodel_ptrtype;
    typedef RM<crbmodel_type> crb_type;
    typedef boost::shared_ptr<crb_type> crb_ptrtype;

    typedef CRBModel<ModelType> crbmodelbilinear_type;

    typedef typename ModelType::parameter_type parameter_type;
    typedef std::vector< parameter_type > vector_parameter_type;

    typedef typename crb_type::sampling_ptrtype sampling_ptrtype;

    OpusApp()
        :
        super(),
        M_mode( ( CRBModelMode )option(_name=_o( this->about().appName(),"run.mode" )).template as<int>() )
        {
            this->init();
        }

    OpusApp( AboutData const& ad, po::options_description const& od )
        :
        super( ad, opusapp_options( ad.appName() ).add( od ).add( crbOptions() ).add( feel_options() ).add( eimOptions() ) ),
        M_mode( ( CRBModelMode )option(_name=_o( this->about().appName(),"run.mode" )).template as<int>() )
        {
            this->init();
        }

    OpusApp( AboutData const& ad, po::options_description const& od, CRBModelMode mode )
        :
        super( ad, opusapp_options( ad.appName() ).add( od ).add( crbOptions() ).add( feel_options() ).add( eimOptions() ) ),
        M_mode( mode )
        {
            this->init();
        }

    OpusApp( int argc, char** argv, AboutData const& ad, po::options_description const& od )
        :
        super( argc, argv, ad, opusapp_options( ad.appName() ).add( od ).add( crbOptions() ).add( feel_options() ).add( eimOptions() ) ),
        M_mode( ( CRBModelMode )option(_name=_o( this->about().appName(),"run.mode" )).template as<int>() )
        {
            this->init();
        }
    OpusApp( int argc, char** argv, AboutData const& ad, po::options_description const& od, CRBModelMode mode )
        :
        super( argc, argv, ad, opusapp_options( ad.appName() ).add( od ).add( crbOptions() ).add( feel_options() ).add( eimOptions() ) ),
        M_mode( mode )
        {
            this->init();
        }
    void init()
        {
            try
            {
                M_current_path = fs::current_path();

                std::srand( static_cast<unsigned>( std::time( 0 ) ) );
                if( Environment::worldComm().globalRank() == Environment::worldComm().masterRank() )
                    std::cout << this->about().appName() << std::endl;
                LOG(INFO) << "[OpusApp] constructor " << this->about().appName()  << "\n";

                // Check if user have given a name for result files repo
                // Note : this name is also use for database storage
                std::string results_repo_name;
                if( this->vm().count("crb.results-repo-name") )
                    results_repo_name = option(_name="crb.results-repo-name").template as<std::string>();
                else
                    results_repo_name = "default_repo";

                LOG(INFO) << "Name for results repo : " << results_repo_name << "\n";
                this->changeRepository( boost::format( "%1%/%2%/" )
                                        % this->about().appName()
                                        % results_repo_name
                                        );

                LOG(INFO) << "[OpusApp] ch repo" << "\n";
                this->setLogs();
                LOG(INFO) << "[OpusApp] set Logs" << "\n";
                LOG(INFO) << "[OpusApp] mode:" << ( int )M_mode << "\n";

                model = crbmodel_ptrtype( new crbmodel_type( this->vm(),M_mode ) );
                LOG(INFO) << "[OpusApp] get model done" << "\n";

                crb = crb_ptrtype( new crb_type( this->about().appName(),
                                                 this->vm() ,
                                                 model ) );
                LOG(INFO) << "[OpusApp] get crb done" << "\n";

                //VLOG(1) << "[OpusApp] get crb done" << "\n";
                //crb->setTruthModel( model );
                //VLOG(1) << "[OpusApp] constructor done" << "\n";
            }

            catch ( boost::bad_any_cast const& e )
            {
                std::cout << "[OpusApp] a bad any cast occured, probably a nonexistant or invalid  command line/ config options\n";
                std::cout << "[OpusApp] exception reason: " << e.what() << "\n";
            }

        }

    void setMode( std::string const& mode )
        {
            if ( mode == "pfem" ) M_mode = CRBModelMode::PFEM;

            if ( mode == "crb" ) M_mode = CRBModelMode::CRB;

            if ( mode == "scm" ) M_mode = CRBModelMode::SCM;

            if ( mode == "scm_online" ) M_mode = CRBModelMode::SCM_ONLINE;

            if ( mode == "crb_online" ) M_mode = CRBModelMode::CRB_ONLINE;
        }
    void setMode( CRBModelMode mode )
        {
            M_mode = mode;
        }

    void loadDB()
        {
            bool crb_use_predefined = option(_name="crb.use-predefined-WNmu").template as<bool>();
            std::string file_name;
            int NlogEquidistributed = option(_name="crb.use-logEquidistributed-WNmu").template as<int>();
            int Nequidistributed = option(_name="crb.use-equidistributed-WNmu").template as<int>();
            int NlogEquidistributedScm = option(_name="crb.scm.use-logEquidistributed-C").template as<int>();
            int NequidistributedScm = option(_name="crb.scm.use-equidistributed-C").template as<int>();
            typename crb_type::sampling_ptrtype Sampling( new typename crb_type::sampling_type( model->parameterSpace() ) );
            if( NlogEquidistributed+Nequidistributed > 0 )
            {
                file_name = ( boost::format("SamplingWNmu") ).str();
                if( NlogEquidistributed > 0 )
                    Sampling->logEquidistribute( NlogEquidistributed );
                if( Nequidistributed > 0 )
                    Sampling->equidistribute( Nequidistributed );
                Sampling->writeOnFile(file_name);
            }
            if( NlogEquidistributedScm+NequidistributedScm > 0 )
            {
                file_name = ( boost::format("SamplingC") ).str();
                if( NlogEquidistributedScm > 0 )
                    Sampling->logEquidistribute( NlogEquidistributedScm );
                if( NequidistributedScm > 0 )
                    Sampling->equidistribute( NequidistributedScm );
                Sampling->writeOnFile(file_name);
            }

            if ( M_mode == CRBModelMode::PFEM )
                return;

            if ( !crb->scm()->isDBLoaded() || crb->scm()->rebuildDB() )
            {
                if ( M_mode == CRBModelMode::SCM )
                {
                    if( Environment::worldComm().globalRank() == Environment::worldComm().masterRank() )
                        std::cout << "No SCM DB available, do scm offline computations first...\n";
                    if( crb->scm()->doScmForMassMatrix() )
                        crb->scm()->setScmForMassMatrix( true );

                    crb->scm()->offline();
                }
            }

            if ( !crb->isDBLoaded() || crb->rebuildDB() )
            {
                if ( M_mode == CRBModelMode::CRB )
                    //|| M_mode == CRBModelMode::SCM )
                {
                    if( Environment::worldComm().globalRank() == Environment::worldComm().masterRank() )
                        std::cout << "No CRB DB available, do crb offline computations...\n";
                    crb->setOfflineStep( true );
                    crb->offline();
                }

                else if ( M_mode != CRBModelMode::SCM )
                    throw std::logic_error( "CRB/SCM Database could not be loaded" );
            }

            if( crb->isDBLoaded() )
            {
                bool do_offline = false;
                int current_dimension = crb->dimension();
                int dimension_max = option(_name="crb.dimension-max").template as<int>();
                int sampling_size = 0;
                if( crb_use_predefined )
                    sampling_size = Sampling->readFromFile(file_name);

                if( sampling_size > current_dimension )
                    do_offline = true;

                if( current_dimension < dimension_max && !crb_use_predefined )
                    do_offline=true;

                if( do_offline )
                {
                    crb->setOfflineStep( true );
                    crb->offline();
                }
            }
        }


    FEELPP_DONT_INLINE
    void run()
        {

            bool export_solution = option(_name=_o( this->about().appName(),"export-solution" )).template as<bool>();

            int proc_number =  Environment::worldComm().globalRank();

            if ( this->vm().count( "help" ) )
            {
                std::cout << this->optionsDescription() << "\n";
                return;
            }

            this->loadDB();
            int run_sampling_size = option(_name=_o( this->about().appName(),"run.sampling.size" )).template as<int>();
            SamplingMode run_sampling_type = ( SamplingMode )option(_name=_o( this->about().appName(),"run.sampling.mode" )).template as<int>();
            int output_index = option(_name="crb.output-index").template as<int>();
            //int output_index = option(_name=_o(this->about().appName(),"output.index")).template as<int>();

            typename crb_type::sampling_ptrtype Sampling( new typename crb_type::sampling_type( model->parameterSpace() ) );

            int n_eval_computational_time = option(_name="eim.computational-time-neval").template as<int>();
            bool compute_fem = option(_name="crb.compute-fem-during-online").template as<bool>();
            bool compute_stat =  option(_name="crb.compute-stat").template as<bool>();

            if( n_eval_computational_time > 0 )
            {
                compute_fem = false;
                auto eim_sc_vector = model->scalarContinuousEim();
                auto eim_sd_vector = model->scalarDiscontinuousEim();
                int size1 = eim_sc_vector.size();
                int size2 = eim_sd_vector.size();
                if( size1 + size2 == 0 )
                    throw std::logic_error( "[OpusApp] no eim object detected" );

                std::string appname = this->about().appName();
                for(int i=0; i<size1; i++)
                    eim_sc_vector[i]->computationalTimeStatistics(appname);
                for(int i=0; i<size2; i++)
                    eim_sd_vector[i]->computationalTimeStatistics(appname);

                run_sampling_size = 0;
            }
            n_eval_computational_time = option(_name="crb.computational-time-neval").template as<int>();
            if( n_eval_computational_time > 0 )
            {
                if( ! option(_name="crb.cvg-study").template as<bool>() )
                {
                    compute_fem = false;
                    run_sampling_size = 0;
                }
                std::string appname = this->about().appName();
                crb->computationalTimeStatistics( appname );
            }


            switch ( run_sampling_type )
            {
            default:
            case SamplingMode::RANDOM:
                Sampling->randomize( run_sampling_size  );
                break;

            case SamplingMode::EQUIDISTRIBUTED:
                Sampling->equidistribute( run_sampling_size  );
                break;

            case SamplingMode::LOGEQUIDISTRIBUTED:
                Sampling->logEquidistribute( run_sampling_size  );
                break;
            }


            std::map<CRBModelMode,std::vector<std::string> > hdrs;
            using namespace boost::assign;
            std::vector<std::string> pfemhdrs = boost::assign::list_of( "FEM Output" )( "FEM Time" );
            std::vector<std::string> crbhdrs = boost::assign::list_of( "FEM Output" )( "FEM Time" )( "RB Output" )( "Error Bounds" )( "CRB Time" )( "output error" )( "Conditionning" )( "l2_error" )( "h1_error" );
            std::vector<std::string> scmhdrs = boost::assign::list_of( "Lb" )( "Lb Time" )( "Ub" )( "Ub Time" )( "FEM" )( "FEM Time" )( "output error" );
            std::vector<std::string> crbonlinehdrs = boost::assign::list_of( "RB Output" )( "Error Bounds" )( "CRB Time" );
            std::vector<std::string> scmonlinehdrs = boost::assign::list_of( "Lb" )( "Lb Time" )( "Ub" )( "Ub Time" )( "Rel.(FEM-Lb)" );
            hdrs[CRBModelMode::PFEM] = pfemhdrs;
            hdrs[CRBModelMode::CRB] = crbhdrs;
            hdrs[CRBModelMode::SCM] = scmhdrs;
            hdrs[CRBModelMode::CRB_ONLINE] = crbonlinehdrs;
            hdrs[CRBModelMode::SCM_ONLINE] = scmonlinehdrs;
            std::ostringstream ostr;

            //if( boost::is_same<  crbmodel_type , crbmodelbilinear_type >::value )
            {
                if( crb->printErrorDuringOfflineStep() )
                    crb->printErrorsDuringRbConstruction();
                if ( crb->showMuSelection() )
                    crb->printMuSelection();
            }

            auto exporter = Exporter<typename crbmodel_type::mesh_type>::New( "ensight" );
            if( export_solution )
                exporter->step( 0 )->setMesh( model->functionSpace()->mesh() );

            printParameterHdr( ostr, model->parameterSpace()->dimension(), hdrs[M_mode] );

            int crb_error_type = option(_name="crb.error-type").template as<int>();

            int dim=0;
            if( M_mode==CRBModelMode::CRB )
            {
                dim=crb->dimension();
                if( crb->useWNmu() )
                    Sampling = crb->wnmu();

                if( option(_name="crb.run-on-scm-parameters").template as<bool>() )
                {
                    Sampling = crb->scm()->c();
                    if( crb_error_type!=1 )
                        throw std::logic_error( "[OpusApp] The SCM has not been launched, you can't use the option crb.run-on-scm-parameters. Run the SCM ( option crb.error-type=1 ) or comment this option line." );
                }
            }
            if( M_mode==CRBModelMode::SCM )
            {
                dim=crb->scm()->KMax();
                if( option(_name="crb.scm.run-on-C").template as<bool>() )
                    Sampling = crb->scm()->c();
            }

            std::ofstream file_summary_of_simulations( ( boost::format( "summary_of_simulations_%d" ) %dim ).str().c_str() ,std::ios::out | std::ios::app );

            int curpar = 0;

            /* Example of use of the setElements (but can use write in the file SamplingForTest)
            vector_parameter_type V;
            parameter_type UserMu( model->parameterSpace() );
            double j=0.1;
            //for(int i=1; i<101; i++)  { UserMu(0)=j;  UserMu(1)=1; UserMu(2)=1.5; UserMu(3)=2; UserMu(4)=3; UserMu(5)=4; UserMu(6)=4.5; UserMu(7)=5; UserMu(8)=6; V.push_back(UserMu ); j+=0.1;}
            //for(int i=10; i<100; i+=10)    { UserMu(0)=i;  UserMu(1)=1; V.push_back(UserMu );}
            //for(int i=1e2; i<1e3; i+=1e2)  { UserMu(0)=i;  UserMu(1)=1; V.push_back(UserMu );}
            //for(int i=1e3; i<1e4; i+=1e3)  { UserMu(0)=i;  UserMu(1)=1; V.push_back(UserMu );}
            //for(int i=1e4; i<1e5; i+=1e4)  { UserMu(0)=i;  UserMu(1)=1; V.push_back(UserMu );}
            //for(int i=1e5; i<1e6; i+=1e5)  { UserMu(0)=i;  UserMu(1)=1; V.push_back(UserMu );}
            //UserMu(0)=1e6;  UserMu(1)=1; V.push_back(UserMu );
            //Sampling->setElements( V );
            */

            // Script write current mu in cfg => need to write in SamplingForTest
            if( option(_name="crb.script-mode").template as<bool>() )
                {
                    // Sampling will be the parameter given by OT
                    if( proc_number == Environment::worldComm().masterRank() )
                            buildSamplingFromCfg();

                    Environment::worldComm().barrier();

                    compute_fem = false;
                    compute_stat = false;
                }

            /**
             * note that in the file SamplingForTest we expect :
             * mu_0= [ value0 , value1 , ... ]
             * mu_1= [ value0 , value1 , ... ]
             **/
            if( option(_name="crb.use-predefined-test-sampling").template as<bool>() || option(_name="crb.script-mode").template as<bool>() )
            {
                std::string file_name = ( boost::format("SamplingForTest") ).str();
                std::ifstream file ( file_name );
                if( file )
                {
                    Sampling->clear();
                    Sampling->readFromFile( file_name ) ;
                }
                else
                {
                    VLOG(2) << "proc number : " << proc_number << "can't find file \n";
                    throw std::logic_error( "[OpusApp] file SamplingForTest was not found" );
                }
            }

            //Statistics
            vectorN_type l2_error_vector;
            vectorN_type h1_error_vector;
            vectorN_type relative_error_vector;
            vectorN_type time_fem_vector;
            vectorN_type time_crb_vector;
            vectorN_type relative_estimated_error_vector;

            vectorN_type scm_relative_error;

            bool solve_dual_problem = option(_name="crb.solve-dual-problem").template as<bool>();

            if (option(_name="crb.cvg-study").template as<bool>() && compute_fem )
            {
                int Nmax = crb->dimension();
                vector_sampling_for_primal_efficiency_under_1.resize(Nmax);
                for(int N=0; N<Nmax; N++)
                {
                    sampling_ptrtype sampling_primal ( new typename crb_type::sampling_type( model->parameterSpace() ) );
                    vector_sampling_for_primal_efficiency_under_1[N]=sampling_primal;
                }
                if( solve_dual_problem )
                {
                    vector_sampling_for_dual_efficiency_under_1.resize(Nmax);
                    for(int N=0; N<Nmax; N++)
                    {
                        sampling_ptrtype sampling_dual ( new typename crb_type::sampling_type( model->parameterSpace() ) );
                        vector_sampling_for_dual_efficiency_under_1[N]=sampling_dual;
                    }
                }
            }

            if( M_mode==CRBModelMode::CRB )
            {

                l2_error_vector.resize( Sampling->size() );
                h1_error_vector.resize( Sampling->size() );
                relative_error_vector.resize( Sampling->size() );
                time_fem_vector.resize( Sampling->size() );
                time_crb_vector.resize( Sampling->size() );

                if( crb->errorType()!=2 )
                    relative_estimated_error_vector.resize( Sampling->size() );

                crb->setOfflineStep( false );

                if (option(_name="eim.cvg-study").template as<bool>() )
                {
                    this->initializeConvergenceEimMap( Sampling->size() );
                    compute_fem=false;
                }
                if (option(_name="crb.cvg-study").template as<bool>() )
                    this->initializeConvergenceCrbMap( Sampling->size() );

            }
            if( M_mode==CRBModelMode::SCM )
            {
                if (option(_name="crb.scm.cvg-study").template as<bool>() )
                    this->initializeConvergenceScmMap( Sampling->size() );

                scm_relative_error.resize( Sampling->size() );
            }

            int crb_dimension = option(_name="crb.dimension").template as<int>();
            int crb_dimension_max = option(_name="crb.dimension-max").template as<int>();
            double crb_online_tolerance = option(_name="crb.online-tolerance").template as<double>();
            bool crb_compute_variance  = option(_name="crb.compute-variance").template as<bool>();

            double output_fem = -1;


            //in the case we don't do the offline step, we need the affine decomposition
            model->computeAffineDecomposition();

            //compute beta coeff for reference parameters
            auto ref_mu = model->refParameter();
            double dt = model->timeStep();
            double ti = model->timeInitial();
            double tf = model->timeFinal();
            int K = ( tf - ti )/dt;
            std::vector< std::vector< std::vector< double > > > ref_betaAqm;
            for(int time_index=0; time_index<K; time_index++)
            {
                double time = time_index*dt;
                ref_betaAqm.push_back( model->computeBetaQm( ref_mu , time ).template get<1>() );
            }
            auto ref_betaMqm = model->computeBetaQm( ref_mu , tf ).template get<0>() ;


            BOOST_FOREACH( auto mu, *Sampling )
            {
                int size = mu.size();

                element_type u_crb; // expansion of reduced solution
                element_type u_crb_dual; // expansion of reduced solution ( dual )
#if !NDEBUG
                if( proc_number == Environment::worldComm().masterRank() )
                {
                    std::cout << "(" << curpar << "/" << Sampling->size() << ") mu = [ ";
                    for ( int i=0; i<size-1; i++ ) std::cout<< mu[i] <<" , ";
                    std::cout<< mu[size-1]<<" ]\n ";
                }
#endif
                curpar++;

                std::ostringstream mu_str;
                //if too many parameters, it will crash
                int sizemax=7;
                if( size < sizemax )
                    sizemax=size;
                for ( int i=0; i<sizemax-1; i++ ) mu_str << std::scientific << std::setprecision( 5 ) << mu[i] <<",";
                mu_str << std::scientific << std::setprecision( 5 ) << mu[size-1];

#if !NDEBUG
                LOG(INFO) << "mu=" << mu << "\n";
                mu.check();
#endif
                if( option(_name="crb.script-mode").template as<bool>() )
                {
                    unsigned long N = mu.size() + 5;
                    unsigned long P = 2;
                    std::vector<double> X( N );
                    std::vector<double> Y( P );
                    for(int i=0; i<mu.size(); i++)
                        X[i] = mu[i];

                    int N_dim = crb_dimension;
                    int N_dimMax = crb_dimension_max;
                    int Nwn;
                    if( N_dim > 0 )
                        Nwn = N_dim;
                    else
                        Nwn = N_dimMax;
                    X[N-5] = output_index;
                    X[N-4] = Nwn;
                    X[N-3] = crb_online_tolerance;
                    X[N-2] = crb_error_type;
                    //X[N-1] = option(_name="crb.compute-variance").template as<int>();
                    X[N-1] = 0;
                    bool compute_variance = crb_compute_variance;
                    if ( compute_variance )
                        X[N-1] = 1;

                    this->run( X.data(), X.size(), Y.data(), Y.size() );
                    //std::cout << "output = " << Y[0] << std::endl;

                    std::ofstream res(option(_name="result-file").template as<std::string>() );
                    res << "output="<< Y[0] << "\n";
                }
                else
                {
                    switch ( M_mode )
                        {
                        case  CRBModelMode::PFEM:
                            {
                                LOG(INFO) << "PFEM mode" << std::endl;
                                if( Environment::worldComm().globalRank() == Environment::worldComm().masterRank() )
                                    std::cout << "PFEM mode" << std::endl;
                                boost::mpi::timer ti;

                                model->computeAffineDecomposition();
                                auto u_fem =  model->solveFemUsingAffineDecompositionFixedPoint( mu );
                                std::ostringstream u_fem_str;
                                u_fem_str << "u_fem(" << mu_str.str() << ")";
                                u_fem.setName( u_fem_str.str()  );

                                LOG(INFO) << "compute output\n";
                                if( export_solution )
                                    exporter->step(0)->add( u_fem.name(), u_fem );
                                //model->solve( mu );
                                std::vector<double> o = boost::assign::list_of( model->output( output_index,mu , u_fem, true) )( ti.elapsed() );
                                if(proc_number == Environment::worldComm().masterRank() ) std::cout << "output=" << o[0] << "\n";
                                printEntry( ostr, mu, o );

                                std::ofstream res(option(_name="result-file").template as<std::string>() );
                                res << "output="<< o[0] << "\n";

                            }
                            break;

                        case  CRBModelMode::CRB:
                            {
                                LOG(INFO) << "CRB mode\n";
                                if( Environment::worldComm().globalRank() == Environment::worldComm().masterRank() )
                                    std::cout << "CRB mode\n";


                                boost::mpi::timer ti;

                                ti.restart();
                                LOG(INFO) << "solve crb\n";
                                google::FlushLogFiles(google::GLOG_INFO);

                                //dimension of the RB (not necessarily the max)
                                int N =  option(_name="crb.dimension").template as<int>();

                                auto o = crb->run( mu,  option(_name="crb.online-tolerance").template as<double>() , N);
                                double time_crb = ti.elapsed();

                                auto WN = crb->wn();
                                auto WNdu = crb->wndu();
                                //auto u_crb = crb->expansion( mu , N );
                                auto solutions=o.template get<2>();
                                auto uN = solutions.template get<0>();
                                auto uNdu = solutions.template get<1>();

                                //if( model->isSteady()) // Re-use uN given by lb in crb->run
                                u_crb = crb->expansion( uN , N , WN ); // Re-use uN given by lb in crb->run
                                if( solve_dual_problem )
                                    u_crb_dual = crb->expansion( uNdu , N , WNdu );
                                //else
                                //    u_crb = crb->expansion( mu , N , WN );

                                std::ostringstream u_crb_str;
                                u_crb_str << "u_crb(" << mu_str.str() << ")";
                                u_crb.setName( u_crb_str.str()  );
                                LOG(INFO) << "export u_crb \n";
                                if( export_solution )
                                    exporter->step(0)->add( u_crb.name(), u_crb );

                                double relative_error = -1;
                                double relative_estimated_error = -1;
                                double condition_number = o.template get<3>();
                                double l2_error = -1;
                                double h1_error = -1;
                                double l2_dual_error = -1;
                                double h1_dual_error = -1;
                                double time_fem = -1;

                                element_type u_fem ;
                                element_type u_dual_fem ;

                                auto all_upper_bounds = o.template get<6>();
                                double output_estimated_error = all_upper_bounds.template get<0>();
                                double solution_estimated_error = all_upper_bounds.template get<1>();
                                double dual_solution_estimated_error = all_upper_bounds.template get<2>();

                                if ( compute_fem )
                                {
									bool use_newton = option(_name="crb.use-newton").template as<bool>();

                                    ti.restart();
                                    LOG(INFO) << "solve u_fem\n";

                                    //auto u_fem = model->solveRB( mu );
                                    //auto u_fem = model->solveFemUsingOfflineEim( mu );

                                    if( boost::is_same<  crbmodel_type , crbmodelbilinear_type >::value && ! use_newton )
                                        //use affine decomposition
                                        u_fem = model->solveFemUsingAffineDecompositionFixedPoint( mu );
                                    else
                                        u_fem = model->solve( mu );

                                    std::ostringstream u_fem_str;
                                    u_fem_str << "u_fem(" << mu_str.str() << ")";
                                    u_fem.setName( u_fem_str.str()  );

                                    if( export_solution )
                                    {
                                        LOG(INFO) << "export u_fem \n";
                                        exporter->step(0)->add( u_fem.name(), u_fem );
                                    }
                                    std::vector<double> ofem = boost::assign::list_of( model->output( output_index,mu, u_fem ) )( ti.elapsed() );

                                    double ocrb = o.template get<0>();
                                    relative_error = std::abs( ofem[0]- ocrb) /ofem[0];
                                    relative_estimated_error = output_estimated_error / ofem[0];

                                    //compute || u_fem - u_crb||_L2
                                    LOG(INFO) << "compute error \n";
                                    auto u_error = model->functionSpace()->element();
                                    auto u_dual_error = model->functionSpace()->element();
                                    std::ostringstream u_error_str;
                                    u_error = (( u_fem - u_crb ).pow(2)).sqrt()  ;
                                    u_error_str << "u_error(" << mu_str.str() << ")";
                                    u_error.setName( u_error_str.str()  );
                                    if( export_solution )
                                        exporter->step(0)->add( u_error.name(), u_error );
                                    LOG(INFO) << "L2(fem)=" << l2Norm( u_fem )    << "\n";
                                    LOG(INFO) << "H1(fem)=" << h1Norm( u_fem )    << "\n";
                                    l2_error = l2Norm( u_error )/l2Norm( u_fem );
                                    h1_error = h1Norm( u_error )/h1Norm( u_fem );

                                    output_fem = ofem[0];
                                    time_fem = ofem[1];

                                    if( boost::is_same<  crbmodel_type , crbmodelbilinear_type >::value && ! use_newton )
                                    {
                                        if( solve_dual_problem )
                                        {
                                            u_dual_fem =  model->solveFemDualUsingAffineDecompositionFixedPoint( mu );

                                            u_dual_error = model->functionSpace()->element();
                                            u_dual_error = (( u_dual_fem - u_crb_dual ).pow(2)).sqrt() ;
                                            l2_dual_error = l2Norm( u_dual_error )/l2Norm( u_dual_fem );
                                            h1_dual_error = h1Norm( u_dual_error )/h1Norm( u_dual_fem );
                                        }
                                    }

                                }//compute-fem-during-online

                                if ( crb->errorType()==2 )
                                {
                                    double ocrb = o.template get<0>();
                                    std::vector<double> v = boost::assign::list_of( output_fem )( time_fem )( ocrb )( relative_estimated_error )( time_crb )( relative_error )( condition_number )( l2_error )( h1_error );
                                    if( proc_number == Environment::worldComm().masterRank() )
                                    {
                                        std::cout << "output=" << ocrb << " with " << o.template get<1>() << " basis functions\n";
                                        printEntry( file_summary_of_simulations, mu, v );
                                        printEntry( ostr, mu, v );
                                        //file_summary_of_simulations.close();

                                        if ( option(_name="crb.compute-stat").template as<bool>() && compute_fem )
                                        {
                                            relative_error_vector[curpar-1] = relative_error;
                                            l2_error_vector[curpar-1] = l2_error;
                                            h1_error_vector[curpar-1] = h1_error;
                                            time_fem_vector[curpar-1] = time_fem;
                                            time_crb_vector[curpar-1] = time_crb;
                                        }

                                        std::ofstream res(option(_name="result-file").template as<std::string>() );
                                        res << "output="<< ocrb << "\n";

                                    }

                                }//end of crb->errorType==2
                                else
                                {
                                    //if( ! boost::is_same<  crbmodel_type , crbmodelbilinear_type >::value )
                                    //    throw std::logic_error( "ERROR TYPE must be 2 when using CRBTrilinear (no error estimation)" );
                                    double ocrb = o.template get<0>();
                                    std::vector<double> v = boost::assign::list_of( output_fem )( time_fem )( ocrb )( relative_estimated_error )( ti.elapsed() ) ( relative_error )( condition_number )( l2_error )( h1_error ) ;
                                    if( proc_number == Environment::worldComm().masterRank() )
                                    {
                                        std::cout << "output=" << ocrb << " with " << o.template get<1>() << " basis functions  (relative error estimation on this output : " << relative_estimated_error<<") \n";
                                        //std::ofstream file_summary_of_simulations( ( boost::format( "summary_of_simulations_%d" ) % o.template get<2>() ).str().c_str() ,std::ios::out | std::ios::app );
                                        printEntry( file_summary_of_simulations, mu, v );
                                        printEntry( ostr, mu, v );
                                        //file_summary_of_simulations.close();

                                        if ( option(_name="crb.compute-stat").template as<bool>() && compute_fem )
                                        {
                                            relative_error_vector[curpar-1] = relative_error;
                                            l2_error_vector[curpar-1] = l2_error;
                                            h1_error_vector[curpar-1] = h1_error;
                                            time_fem_vector[curpar-1] = time_fem;
                                            time_crb_vector[curpar-1] = time_crb;
                                            relative_estimated_error_vector[curpar-1] = relative_estimated_error;
                                        }
                                        std::ofstream res(option(_name="result-file").template as<std::string>() );
                                        res << "output="<< ocrb << "\n";
                                    }//end of proc==master
                                }//end of else (errorType==2)

                                if (option(_name="eim.cvg-study").template as<bool>() )
                                {
                                    bool check_name = false;
                                    std::string how_compute_unknown = option(_name=_o( this->about().appName(),"how-compute-unkown-for-eim" )).template as<std::string>();
                                    if( how_compute_unknown == "CRB-with-ad")
                                    {
                                        LOG( INFO ) << "convergence eim with CRB-with-ad ";
                                        check_name = true;
                                        this->studyEimConvergence( mu , u_crb , curpar );
                                    }
                                    if( how_compute_unknown == "FEM-with-ad")
                                    {
                                        LOG( INFO ) << "convergence eim with FEM-with-ad ";
                                        check_name = true;
                                        //fem computed via solveFemUsingAffineDecomposition use the affine decomposition
                                        auto fem_with_ad = model->solveFemUsingAffineDecompositionFixedPoint( mu );
                                        this->studyEimConvergence( mu , fem_with_ad , curpar );
                                    }
                                    if( how_compute_unknown == "FEM-without-ad")
                                    {
                                        LOG( INFO ) << "convergence eim with FEM-without-ad ";
                                        check_name = true;
                                        auto fem_without_ad = model->solve( mu );
                                        this->studyEimConvergence( mu , fem_without_ad , curpar );
                                    }
                                    if( ! check_name )
                                        throw std::logic_error( "OpusApp error with option how-compute-unknown-for-eim, please use CRB-with-ad, FEM-with-ad or FEM-without-ad" );
                                }

                                if (option(_name="crb.cvg-study").template as<bool>() && compute_fem )
                                {

                                    LOG(INFO) << "start convergence study...\n";
                                    std::map<int, boost::tuple<double,double,double,double,double,double,double> > conver;

                                    int Nmax = crb->dimension();
                                    for( int N = 1; N <= Nmax ; N++ )
                                    {
                                        auto o= crb->run( mu,  option(_name="crb.online-tolerance").template as<double>() , N);
                                        auto ocrb = o.template get<0>();
                                        auto solutions=o.template get<2>();
                                        auto u_crb = solutions.template get<0>();
                                        auto u_crb_du = solutions.template get<1>();
                                        auto uN = crb->expansion( u_crb, N, WN );

                                        element_type uNdu;

                                        auto u_error = u_fem - uN;
                                        auto u_dual_error = model->functionSpace()->element();
                                        if( solve_dual_problem )
                                        {
                                            uNdu = crb->expansion( u_crb_du, N, WNdu );
                                            u_dual_error = u_dual_fem - uNdu;
                                        }

                                        auto all_upper_bounds = o.template get<6>();
                                        output_estimated_error = all_upper_bounds.template get<0>();
                                        solution_estimated_error = all_upper_bounds.template get<1>();
                                        dual_solution_estimated_error = all_upper_bounds.template get<2>();

                                        //auto o = crb->run( mu,  option(_name="crb.online-tolerance").template as<double>() , N);
                                        double rel_err = std::abs( output_fem-ocrb ) /output_fem;

                                        double output_relative_estimated_error = output_estimated_error / output_fem;

                                        double primal_residual_norm = o.template get<4>();
                                        double dual_residual_norm = o.template get<5>();

                                        double solution_error=0;
                                        double dual_solution_error=0;
                                        double ref_primal=0;
                                        double ref_dual=0;
                                        if( model->isSteady() )
                                        {
                                            //let ufem-ucrb = e
                                            //||| e |||_mu = sqrt( a( e , e ; mu ) ) = solution_error
                                            for(int q=0; q<model->Qa();q++)
                                            {
                                                for(int m=0; m<model->mMaxA(q); m++)
                                                {
                                                    solution_error +=  ref_betaAqm[0][q][m]*model->Aqm(q,m,u_error,u_error) ;
                                                    ref_primal +=  ref_betaAqm[0][q][m]*model->Aqm(q,m,u_fem,u_fem);
                                                }
                                            }

                                            if( solve_dual_problem )
                                            {
                                                for(int q=0; q<model->Qa();q++)
                                                {
                                                    for(int m=0; m<model->mMaxA(q); m++)
                                                    {
                                                        dual_solution_error += ref_betaAqm[0][q][m]*model->Aqm(q,m,u_dual_error,u_dual_error);
                                                        ref_dual += ref_betaAqm[0][q][m]*model->Aqm(q,m,u_dual_fem,u_dual_fem);
                                                    }
                                                }
                                                dual_solution_error = math::sqrt( dual_solution_error );
                                                ref_dual = math::sqrt( ref_dual );
                                            }
                                            solution_error = math::sqrt( solution_error );
                                            ref_primal = math::sqrt( ref_primal );
                                            //dual_solution_error = math::sqrt( model->scalarProduct( u_dual_error, u_dual_error ) );
                                        }
                                        else
                                        {
                                            double dt = model->timeStep();
                                            double ti = model->timeInitial();
                                            double tf = model->timeFinal();
                                            int K = ( tf - ti )/dt;

                                            for(int q=0; q<model->Qm();q++)
                                            {
                                                for(int m=0; m<model->mMaxM(q); m++)
                                                {
                                                    solution_error +=  ref_betaMqm[q][m]*model->Mqm(q,m,u_error,u_error);
                                                    ref_primal +=  ref_betaMqm[q][m]*model->Mqm(q,m,u_fem,u_fem);
                                                }
                                            }
                                            for(int time_index=0; time_index<K; time_index++)
                                            {
                                                double t=time_index*dt;
                                                for(int q=0; q<model->Qa();q++)
                                                {
                                                    for(int m=0; m<model->mMaxA(q); m++)
                                                    {
                                                        solution_error +=  ref_betaAqm[time_index][q][m]*model->Aqm(q,m,u_error,u_error) * dt;
                                                        ref_primal +=  ref_betaAqm[time_index][q][m]*model->Aqm(q,m,u_fem,u_fem) * dt;
                                                    }
                                                }
                                            }
                                            solution_error = math::sqrt( solution_error );
                                            ref_primal = math::sqrt( ref_primal );

                                            if( solve_dual_problem )
                                            {
                                                ti = model->timeFinal()+dt;
                                                tf = model->timeInitial()+dt;
                                                dt -= dt;

                                                for(int q=0; q<model->Qm();q++)
                                                {
                                                    for(int m=0; m<model->mMaxM(q); m++)
                                                    {
                                                        dual_solution_error +=  ref_betaMqm[q][m]*model->Mqm(q,m,u_dual_error,u_dual_error);
                                                        ref_dual +=  ref_betaMqm[q][m]*model->Mqm(q,m,u_dual_fem,u_dual_fem);
                                                    }
                                                }

                                                for(int time_index=0; time_index<K; time_index++)
                                                {
                                                    double t=time_index*dt;
                                                    for(int q=0; q<model->Qa();q++)
                                                    {
                                                        for(int m=0; m<model->mMaxA(q); m++)
                                                        {
                                                            dual_solution_error +=  ref_betaAqm[time_index][q][m]*model->Aqm(q,m,u_dual_error,u_dual_error) * dt;
                                                            ref_dual +=  ref_betaAqm[time_index][q][m]*model->Aqm(q,m,u_dual_fem,u_dual_fem) * dt;
                                                        }
                                                    }
                                                }

                                                dual_solution_error = math::sqrt( dual_solution_error );
                                                ref_dual = math::sqrt( ref_dual );

                                            }//if solve-dual

                                        }//transient case

                                        double l2_error = l2Norm( u_error )/l2Norm( u_fem );
                                        double h1_error = h1Norm( u_error )/h1Norm( u_fem );
                                        double condition_number = o.template get<3>();
                                        double output_error_bound_efficiency = output_relative_estimated_error / rel_err;

                                        double relative_primal_solution_error = solution_error / ref_primal ;
                                        double relative_primal_solution_estimated_error = solution_estimated_error / ref_primal;
                                        double relative_primal_solution_error_bound_efficiency = relative_primal_solution_estimated_error / relative_primal_solution_error;

                                        if( relative_primal_solution_error_bound_efficiency < 1 )
                                        {
                                            vector_sampling_for_primal_efficiency_under_1[N-1]->push_back( mu , 1);
                                        }

                                        double relative_dual_solution_error = 1;
                                        double relative_dual_solution_estimated_error = 1;
                                        double relative_dual_solution_error_bound_efficiency = 1;
                                        if( solve_dual_problem )
                                        {
                                            relative_dual_solution_error = dual_solution_error / ref_dual ;
                                            relative_dual_solution_estimated_error = dual_solution_estimated_error / ref_dual;
                                            relative_dual_solution_error_bound_efficiency = relative_dual_solution_estimated_error / relative_dual_solution_error;

                                            if( relative_dual_solution_error_bound_efficiency < 1 )
                                            {
                                                vector_sampling_for_dual_efficiency_under_1[N-1]->push_back( mu , 0);
                                            }

                                        }
                                        conver[N]=boost::make_tuple( rel_err, l2_error, h1_error , relative_estimated_error, condition_number , output_error_bound_efficiency , relative_primal_solution_error_bound_efficiency );

                                        LOG(INFO) << "N=" << N << " " << rel_err << " " << l2_error << " " << h1_error << " " <<condition_number<<"\n";
                                        if ( proc_number == Environment::worldComm().masterRank() )
                                            std::cout << "N=" << N << " " << rel_err << " " << l2_error << " " << h1_error << " " <<relative_estimated_error<<" "<<condition_number<<std::endl;
                                        M_mapConvCRB["L2"][N-1](curpar - 1) = l2_error;
                                        M_mapConvCRB["H1"][N-1](curpar - 1) = h1_error;
                                        M_mapConvCRB["OutputError"][N-1](curpar - 1) = rel_err;
                                        M_mapConvCRB["OutputEstimatedError"][N-1](curpar - 1) = output_relative_estimated_error;
                                        M_mapConvCRB["OutputErrorBoundEfficiency"][N-1](curpar - 1) =  output_error_bound_efficiency;
                                        M_mapConvCRB["SolutionErrorBoundEfficiency"][N-1](curpar - 1) =  relative_primal_solution_error_bound_efficiency;
                                        M_mapConvCRB["SolutionError"][N-1](curpar - 1) =  relative_primal_solution_error;
                                        M_mapConvCRB["SolutionErrorEstimated"][N-1](curpar - 1) =  relative_primal_solution_estimated_error;
                                        M_mapConvCRB["SolutionDualErrorBoundEfficiency"][N-1](curpar - 1) =  relative_dual_solution_error_bound_efficiency;
                                        M_mapConvCRB["SolutionDualError"][N-1](curpar - 1) =  relative_dual_solution_error;
                                        M_mapConvCRB["SolutionDualErrorEstimated"][N-1](curpar - 1) =  relative_dual_solution_estimated_error;
                                        M_mapConvCRB["PrimalResidualNorm"][N-1](curpar - 1) =  primal_residual_norm;
                                        M_mapConvCRB["DualResidualNorm"][N-1](curpar - 1) =  dual_residual_norm;
                                        LOG(INFO) << "N=" << N << " done.\n";
                                    }
                                    if( proc_number == Environment::worldComm().masterRank() )
                                    {
                                        LOG(INFO) << "save in logfile\n";
                                        std::string mu_str;
                                        for ( int i=0; i<mu.size(); i++ )
                                            mu_str= mu_str + ( boost::format( "_%1%" ) %mu[i] ).str() ;
                                        std::string file_name = "convergence"+mu_str+".dat";
                                        std::ofstream conv( file_name );
                                        BOOST_FOREACH( auto en, conver )
                                            conv << en.first << "\t" << en.second.get<0>()  << "\t" << en.second.get<1>() << "\t" << en.second.get<2>() <<
                                            "\t"<< en.second.get<3>() << "\t"<< en.second.get<4>()<< "\t" <<en.second.get<5>()<< "\n";
                                    }
                                }//end of cvg-study
                            }//case CRB
                            break;
                        case  CRBModelMode::CRB_ONLINE:
                            {
                                std::cout << "CRB Online mode\n";
                                boost::mpi::timer ti;
                                ti.restart();
                                auto o = crb->run( mu,  option(_name="crb.online-tolerance").template as<double>() );

                                if ( crb->errorType()==2 )
                                    {
                                        std::vector<double> v = boost::assign::list_of( o.template get<0>() )( ti.elapsed() );
                                        std::cout << "output=" << o.template get<0>() << " with " << o.template get<1>() << " basis functions\n";
                                        printEntry( ostr, mu, v );
                                    }

                                else
                                    {
                                        auto all_upper_bounds = o.template get<6>();
                                        double output_estimated_error = all_upper_bounds.template get<0>();
                                        double relative_estimated_error = output_estimated_error / output_fem;
                                        std::vector<double> v = boost::assign::list_of( o.template get<0>() )( output_estimated_error )( ti.elapsed() );
                                        std::cout << "output=" << o.template get<0>() << " with " << o.template get<1>() <<
                                            " basis functions  (relative error estimation on this output : " << relative_estimated_error<<") \n";
                                        printEntry( ostr, mu, v );
                                    }

                            }
                            break;

                        case  CRBModelMode::SCM:
                            {

                                if( Environment::worldComm().globalRank() == Environment::worldComm().masterRank() )
                                    std::cout << "SCM mode\n";
                                int kmax = crb->scm()->KMax();
                                auto o = crb->scm()->run( mu, kmax );
                                printEntry( file_summary_of_simulations, mu, o );
                                printEntry( ostr, mu, o );

                                scm_relative_error[curpar-1] = o[6];

                                if (option(_name="crb.scm.cvg-study").template as<bool>()  )
                                {
                                    LOG(INFO) << "start scm convergence study...\n";
                                    std::map<int, boost::tuple<double> > conver;
                                    for( int N = 1; N <= kmax; N++ )
                                    {
                                        auto o = crb->scm()->run( mu, N);
                                        double relative_error = o[6];
                                        conver[N]=boost::make_tuple( relative_error );
                                        if ( proc_number == Environment::worldComm().masterRank() )
                                            std::cout << "N=" << N << " " << relative_error <<std::endl;
                                        M_mapConvSCM["RelativeError"][N-1](curpar - 1) = relative_error;
                                    }
                                    if( proc_number == Environment::worldComm().masterRank() )
                                    {
                                        LOG(INFO) << "save in logfile\n";
                                        std::string mu_str;
                                        for ( int i=0; i<mu.size(); i++ )
                                            mu_str= mu_str + ( boost::format( "_%1%" ) %mu[i] ).str() ;
                                        std::string file_name = "convergence-scm-"+mu_str+".dat";
                                        std::ofstream conv( file_name );
                                        BOOST_FOREACH( auto en, conver )
                                            conv << en.first << "\t" << en.second.get<0>()  ;
                                    }
                                }//end of cvg-study

                            }
                            break;

                        case  CRBModelMode::SCM_ONLINE:
                            {
                                std::cout << "SCM online mode\n";
                                auto o = crb->scm()->run( mu, crb->scm()->KMax() );
                                printEntry( ostr, mu, o );
                            }
                            break;

                        }

                    LOG( INFO ) << "------------------------------------------------------------";
                }
            }

            //model->computationalTimeEimStatistics();
            if( export_solution )
                exporter->save();

            if( proc_number == Environment::worldComm().masterRank() ) std::cout << ostr.str() << "\n";

            if (option(_name="eim.cvg-study").template as<bool>() && M_mode==CRBModelMode::CRB)
                this->doTheEimConvergenceStat( Sampling->size() );

            if (option(_name="crb.cvg-study").template as<bool>() && compute_fem && M_mode==CRBModelMode::CRB )
                this->doTheCrbConvergenceStat( Sampling->size() );

            if (option(_name="crb.scm.cvg-study").template as<bool>() && M_mode==CRBModelMode::SCM )
                this->doTheScmConvergenceStat( Sampling->size() );

            if ( compute_stat && compute_fem && M_mode==CRBModelMode::CRB )
            {
                LOG( INFO ) << "compute statistics \n";
                Eigen::MatrixXf::Index index_max_l2;
                Eigen::MatrixXf::Index index_min_l2;
                Eigen::MatrixXf::Index index_max_h1;
                Eigen::MatrixXf::Index index_min_h1;
                Eigen::MatrixXf::Index index_max_time_fem;
                Eigen::MatrixXf::Index index_min_time_fem;
                Eigen::MatrixXf::Index index_max_time_crb;
                Eigen::MatrixXf::Index index_min_time_crb;
                Eigen::MatrixXf::Index index_max_estimated_error;
                Eigen::MatrixXf::Index index_min_estimated_error;
                Eigen::MatrixXf::Index index_max_output_error;
                Eigen::MatrixXf::Index index_min_output_error;

                double max_l2 = l2_error_vector.maxCoeff(&index_max_l2);
                double min_l2 = l2_error_vector.minCoeff(&index_min_l2);
                double mean_l2 = l2_error_vector.mean();
                double max_h1 = h1_error_vector.maxCoeff(&index_max_h1);
                double min_h1 = h1_error_vector.minCoeff(&index_min_h1);
                double mean_h1 = h1_error_vector.mean();
                double max_time_fem = time_fem_vector.maxCoeff(&index_max_time_fem);
                double min_time_fem = time_fem_vector.minCoeff(&index_min_time_fem);
                double mean_time_fem = time_fem_vector.mean();
                double max_time_crb = time_crb_vector.maxCoeff(&index_max_time_crb);
                double min_time_crb = time_crb_vector.minCoeff(&index_min_time_crb);
                double mean_time_crb = time_crb_vector.mean();
                double max_output_error = relative_error_vector.maxCoeff(&index_max_output_error);
                double min_output_error = relative_error_vector.minCoeff(&index_min_output_error);
                double mean_output_error = relative_error_vector.mean();
                double max_estimated_error = 0;
                double min_estimated_error = 0;
                double mean_estimated_error = 0;

                if( crb->errorType()!=2 )
                {
                    max_estimated_error = relative_estimated_error_vector.maxCoeff(&index_max_estimated_error);
                    min_estimated_error = relative_estimated_error_vector.minCoeff(&index_min_estimated_error);
                    mean_estimated_error = relative_estimated_error_vector.mean();
                }
                if( proc_number == Environment::worldComm().masterRank() )
                {
                    file_summary_of_simulations <<"\n\nStatistics\n";
                    file_summary_of_simulations <<"max of output error : "<<max_output_error<<" at the "<<index_max_output_error+1<<"^th simulation\n";
                    file_summary_of_simulations <<"min of output error : "<<min_output_error<<" at the "<<index_min_output_error+1<<"^th simulation\n";
                    file_summary_of_simulations <<"mean of output error : "<<mean_output_error<<"\n\n";
                    file_summary_of_simulations <<"max of estimated output error : "<<max_estimated_error<<" at the "<<index_max_estimated_error+1<<"^th simulation\n";
                    file_summary_of_simulations <<"min of estimated output error : "<<min_estimated_error<<" at the "<<index_min_estimated_error+1<<"^th simulation\n";
                    file_summary_of_simulations <<"mean of estimated output error : "<<mean_estimated_error<<"\n\n";
                    file_summary_of_simulations <<"max of L2 error : "<<max_l2<<" at the "<<index_max_l2+1<<"^th simulation\n";
                    file_summary_of_simulations <<"min of L2 error : "<<min_l2<<" at the "<<index_min_l2+1<<"^th simulation\n";
                    file_summary_of_simulations <<"mean of L2 error : "<<mean_l2<<"\n\n";
                    file_summary_of_simulations <<"max of H1 error : "<<max_h1<<" at the "<<index_max_h1+1<<"^th simulation\n";
                    file_summary_of_simulations <<"min of H1 error : "<<min_h1<<" at the "<<index_min_h1+1<<"^th simulation\n";
                    file_summary_of_simulations <<"mean of H1 error : "<<mean_h1<<"\n\n";
                    file_summary_of_simulations <<"max of time FEM : "<<max_time_fem<<" at the "<<index_max_time_fem+1<<"^th simulation\n";
                    file_summary_of_simulations <<"min of time FEM : "<<min_time_fem<<" at the "<<index_min_time_fem+1<<"^th simulation\n";
                    file_summary_of_simulations <<"mean of time FEM : "<<mean_time_fem<<"\n\n";
                    file_summary_of_simulations <<"max of time CRB : "<<max_time_crb<<" at the "<<index_max_time_crb+1<<"^th simulation\n";
                    file_summary_of_simulations <<"min of time CRB : "<<min_time_crb<<" at the "<<index_min_time_crb+1<<"^th simulation\n";
                    file_summary_of_simulations <<"mean of time CRB : "<<mean_time_crb<<"\n\n";
                }
            }//end of compute-stat CRB
            if( M_mode==CRBModelMode::SCM )
            {
                LOG( INFO ) << "compute statistics \n";
                Eigen::MatrixXf::Index index_max_error;
                Eigen::MatrixXf::Index index_min_error;
                double max_error = scm_relative_error.maxCoeff(&index_max_error);
                double min_error = scm_relative_error.minCoeff(&index_min_error);
                double mean_error = scm_relative_error.mean();
                if( proc_number == Environment::worldComm().masterRank() )
                {
                    file_summary_of_simulations <<"\n\nStatistics\n";
                    file_summary_of_simulations <<"max of relative error : "<<max_error<<" at the "<<index_max_error+1<<"^th simulation\n";
                    file_summary_of_simulations <<"min of relative error : "<<min_error<<" at the "<<index_min_error+1<<"^th simulation\n";
                    file_summary_of_simulations <<"mean of relative error : "<<mean_error<<"\n\n";
                }

            }//end of compute-stat SCM


        }
    void run( const double * X, unsigned long N,
              double * Y, unsigned long P )
        {

            switch ( M_mode )
            {
            case  CRBModelMode::PFEM:
            {
                model->run( X, N, Y, P );
            }
            break;

            case  CRBModelMode::SCM:
            case  CRBModelMode::SCM_ONLINE:
            {
                crb->scm()->run( X, N, Y, P );
            }
            break;

            case  CRBModelMode::CRB:
            case  CRBModelMode::CRB_ONLINE:
            {
                crb->run( X, N, Y, P );
            }
            break;
            }

            fs::current_path( M_current_path );
        }

private:
    int printParameterHdr( std::ostream& os, int N, std::vector<std::string> outputhdrs )
        {
            for ( int i = 0; i < N; ++i )
            {
                std::ostringstream s;
                s << "mu" << i;
                os  << hdrmanip( prec+7 ) << s.str();
            }

            BOOST_FOREACH( auto output, outputhdrs )
            {
                os << hdrmanip( 15 ) << output;
            }
            os << "\n";

            return N*( prec+7 )+outputhdrs.size()*15;
        }
    void printEntry( std::ostream& os,
                     typename ModelType::parameter_type const& mu,
                     std::vector<double> const& outputs )
        {
            for ( int i = 0; i < mu.size(); ++i )
                os  << std::right <<std::setw( prec+7 ) << dmanip << mu[i];

            BOOST_FOREACH( auto o, outputs )
            {
                os << tabmanip( 15 ) << o;
            }
            os << "\n";
        }


    //double l2Norm( typename ModelType::parameter_type const& mu, int N )
    double l2Norm( element_type const& u )
    {
        static const bool is_composite = functionspace_type::is_composite;
        return l2Norm( u, mpl::bool_< is_composite >() );
    }
    double l2Norm( element_type const& u, mpl::bool_<false> )
    {
        auto mesh = model->functionSpace()->mesh();
        return math::sqrt( integrate( elements(mesh), (vf::idv(u))*(vf::idv(u)) ).evaluate()(0,0) );
    }
    double l2Norm( element_type const& u, mpl::bool_<true>)
    {
        auto mesh = model->functionSpace()->mesh();
        auto uT = u.template element<1>();
        return math::sqrt( integrate( elements(mesh), (vf::idv(uT))*(vf::idv(uT)) ).evaluate()(0,0) );
    }
    //double h1Norm( typename ModelType::parameter_type const& mu, int N )
    double h1Norm( element_type const& u )
    {
        static const bool is_composite = functionspace_type::is_composite;
        return h1Norm( u, mpl::bool_< is_composite >() );
    }
    double h1Norm( element_type const& u, mpl::bool_<false> )
    {
        auto mesh = model->functionSpace()->mesh();
        double l22 = integrate( elements(mesh), (vf::idv(u))*(vf::idv(u)) ).evaluate()(0,0);
        double semih12 = integrate( elements(mesh), (vf::gradv(u))*trans(vf::gradv(u)) ).evaluate()(0,0);
        return math::sqrt( l22+semih12 );
    }
    double h1Norm( element_type const& u, mpl::bool_<true>)
    {
        auto mesh = model->functionSpace()->mesh();
        auto u_femT = u.template element<1>();
        double l22 = integrate( elements(mesh), (vf::idv(u_femT))*(vf::idv(u_femT)) ).evaluate()(0,0);
        double semih12 = integrate( elements(mesh), (vf::gradv(u_femT))*trans(vf::gradv(u_femT))).evaluate()(0,0);
        return math::sqrt( l22+semih12 );
    }


    void initializeConvergenceEimMap( int sampling_size )
    {
        auto eim_sc_vector = model->scalarContinuousEim();
        auto eim_sd_vector = model->scalarDiscontinuousEim();

        for(int i=0; i<eim_sc_vector.size(); i++)
        {
            auto eim = eim_sc_vector[i];
            M_mapConvEIM[eim->name()] = std::vector<vectorN_type>(eim->mMax());
            for(int j=0; j<eim->mMax(); j++)
                M_mapConvEIM[eim->name()][j].resize(sampling_size);
        }

        for(int i=0; i<eim_sd_vector.size(); i++)
        {
            auto eim = eim_sd_vector[i];
            M_mapConvEIM[eim->name()] = std::vector<vectorN_type>(eim->mMax());
            for(int j=0; j<eim->mMax(); j++)
                M_mapConvEIM[eim->name()][j].resize(sampling_size);
        }
    }

    void initializeConvergenceCrbMap( int sampling_size )
    {
        auto N = crb->dimension();
        M_mapConvCRB["L2"] = std::vector<vectorN_type>(N);
        M_mapConvCRB["H1"] = std::vector<vectorN_type>(N);
        M_mapConvCRB["OutputError"] = std::vector<vectorN_type>(N);//true error
        M_mapConvCRB["OutputEstimatedError"] = std::vector<vectorN_type>(N);//estimated error
        M_mapConvCRB["OutputErrorBoundEfficiency"] = std::vector<vectorN_type>(N);
        M_mapConvCRB["SolutionErrorBoundEfficiency"] = std::vector<vectorN_type>(N);
        M_mapConvCRB["SolutionError"] = std::vector<vectorN_type>(N);
        M_mapConvCRB["SolutionErrorEstimated"] = std::vector<vectorN_type>(N);
        M_mapConvCRB["SolutionDualErrorBoundEfficiency"] = std::vector<vectorN_type>(N);
        M_mapConvCRB["SolutionDualError"] = std::vector<vectorN_type>(N);
        M_mapConvCRB["SolutionDualErrorEstimated"] = std::vector<vectorN_type>(N);
        M_mapConvCRB["PrimalResidualNorm"] = std::vector<vectorN_type>(N);
        M_mapConvCRB["DualResidualNorm"] = std::vector<vectorN_type>(N);

        for(int j=0; j<N; j++)
            {
                M_mapConvCRB["L2"][j].resize(sampling_size);
                M_mapConvCRB["H1"][j].resize(sampling_size);
                M_mapConvCRB["OutputError"][j].resize(sampling_size);
                M_mapConvCRB["OutputEstimatedError"][j].resize(sampling_size);
                M_mapConvCRB["OutputErrorBoundEfficiency"][j].resize(sampling_size);
                M_mapConvCRB["SolutionErrorBoundEfficiency"][j].resize(sampling_size);
                M_mapConvCRB["SolutionError"][j].resize(sampling_size);
                M_mapConvCRB["SolutionErrorEstimated"][j].resize(sampling_size);
                M_mapConvCRB["SolutionDualErrorBoundEfficiency"][j].resize(sampling_size);
                M_mapConvCRB["SolutionDualError"][j].resize(sampling_size);
                M_mapConvCRB["SolutionDualErrorEstimated"][j].resize(sampling_size);
                M_mapConvCRB["PrimalResidualNorm"][j].resize( sampling_size );
                M_mapConvCRB["DualResidualNorm"][j].resize( sampling_size );
            }
    }

    void initializeConvergenceScmMap( int sampling_size )
    {
        auto N = crb->scm()->KMax();
        M_mapConvSCM["RelativeError"] = std::vector<vectorN_type>(N);

        for(int j=0; j<N; j++)
        {
            M_mapConvSCM["RelativeError"][j].resize(sampling_size);
        }
    }

    void studyEimConvergence( typename ModelType::parameter_type const& mu , element_type & model_solution , int mu_number)
    {
        auto eim_sc_vector = model->scalarContinuousEim();
        auto eim_sd_vector = model->scalarDiscontinuousEim();

        for(int i=0; i<eim_sc_vector.size(); i++)
        {
            std::vector<double> l2error;
            auto eim = eim_sc_vector[i];
            //take two parameters : the mu and the solution of the model
            l2error = eim->studyConvergence( mu , model_solution );

            for(int j=0; j<l2error.size(); j++)
                M_mapConvEIM[eim->name()][j][mu_number-1] = l2error[j];
        }

        for(int i=0; i<eim_sd_vector.size(); i++)
        {
            std::vector<double> l2error;
            auto eim = eim_sd_vector[i];
            l2error = eim->studyConvergence( mu , model_solution );

            for(int j=0; j<l2error.size(); j++)
                M_mapConvEIM[eim->name()][j][mu_number-1] = l2error[j];
        }
    }

    void doTheEimConvergenceStat( int sampling_size )
    {
        auto eim_sc_vector = model->scalarContinuousEim();
        auto eim_sd_vector = model->scalarDiscontinuousEim();

        for(int i=0; i<eim_sc_vector.size(); i++)
        {
            auto eim = eim_sc_vector[i];

            std::ofstream conv;
            std::string file_name = "cvg-eim-"+eim->name()+"-stats.dat";

            if( Environment::worldComm().globalRank() == Environment::worldComm().masterRank() )
            {
                conv.open(file_name, std::ios::app);
                conv << "NbBasis" << "\t" << "Min" << "\t" << "Max" << "\t" << "Mean" << "\t" << "Variance" << "\n";
            }

            for(int j=0; j<eim->mMax(); j++)
            {
                double mean = M_mapConvEIM[eim->name()][j].mean();
                double variance = 0.0;
                for( int k=0; k < sampling_size ; k++)
                {
                    variance += (M_mapConvEIM[eim->name()][j](k) - mean)*(M_mapConvEIM[eim->name()][j](k) - mean)/sampling_size;
                }

                if( Environment::worldComm().globalRank() == Environment::worldComm().masterRank() )
                {
                    conv << j+1 << "\t"
                         << M_mapConvEIM[eim->name()][j].minCoeff() << "\t"
                         << M_mapConvEIM[eim->name()][j].maxCoeff() << "\t"
                         << mean << "\t" << variance << "\n";
                }
            }
            conv.close();
        }

        for(int i=0; i<eim_sd_vector.size(); i++)
        {
            auto eim = eim_sd_vector[i];

            std::ofstream conv;
            std::string file_name = "cvg-eim-"+eim->name()+"-stats.dat";

            if( Environment::worldComm().globalRank() == Environment::worldComm().masterRank() )
            {
                conv.open(file_name, std::ios::app);
                conv << "NbBasis" << "\t" << "Min" << "\t" << "Max" << "\t" << "Mean" << "\t" << "Variance" << "\n";
            }

            for(int j=0; j<eim->mMax(); j++)
            {
                double mean = M_mapConvEIM[eim->name()][j].mean();
                double variance = 0.0;
                for( int k=0; k < sampling_size; k++)
                    variance += (M_mapConvEIM[eim->name()][j](k) - mean)*(M_mapConvEIM[eim->name()][j](k) - mean)/sampling_size;

                if( Environment::worldComm().globalRank()  == Environment::worldComm().masterRank() )
                {
                    conv << j+1 << "\t"
                         << M_mapConvEIM[eim->name()][j].minCoeff() << "\t"
                         << M_mapConvEIM[eim->name()][j].maxCoeff() << "\t"
                         << mean << "\t" << variance << "\n";
                }
            }
            conv.close();
        }
    }

    void doTheCrbConvergenceStat( int sampling_size )
    {
        auto N = crb->dimension();
        //std::list<std::string> list_error_type;
        std::list<std::string> list_error_type = boost::assign::list_of("L2")("H1")("OutputError")("OutputEstimatedError")
            ("SolutionError")("SolutionDualError")("PrimalResidualNorm")("DualResidualNorm")("SolutionErrorEstimated")
            ("SolutionDualErrorEstimated")("SolutionErrorBoundEfficiency")("SolutionDualErrorBoundEfficiency")("OutputErrorBoundEfficiency");


        std::vector< Eigen::MatrixXf::Index > index_max_vector_solution_primal;
        std::vector< Eigen::MatrixXf::Index > index_max_vector_solution_dual;
        std::vector< Eigen::MatrixXf::Index > index_max_vector_output;
        std::vector< Eigen::MatrixXf::Index > index_min_vector_solution_primal;
        std::vector< Eigen::MatrixXf::Index > index_min_vector_solution_dual;
        std::vector< Eigen::MatrixXf::Index > index_min_vector_output;
        Eigen::MatrixXf::Index index_max;
        Eigen::MatrixXf::Index index_min;

        BOOST_FOREACH( auto error_name, list_error_type)
        {
            std::ofstream conv;
            std::string file_name = "cvg-crb-"+ error_name +"-stats.dat";

            if( Environment::worldComm().globalRank() == Environment::worldComm().masterRank() )
            {
                if( error_name=="SolutionErrorEstimated" || error_name=="SolutionDualErrorEstimated" || error_name=="OutputEstimatedError" ||
                    error_name=="SolutionErrorBoundEfficiency" || error_name=="SolutionDualErrorBoundEfficiency" || error_name=="OutputErrorBoundEfficiency")
                {
                    conv.open(file_name, std::ios::app);
                    conv << "NbBasis" << "\t" << "Min2" << "\t" << "Max2" << "\t" << "Min" << "\t" << "Max\t" <<"Mean" << "\t"<< "Variance" << "\n";
                }
                else
                {
                    conv.open(file_name, std::ios::app);
                    conv << "NbBasis" << "\t" << "Min" << "\t" << "Max" << "\t" << "Mean" << "\t" << "Variance" << "\n";
                }
            }

            for(int j=0; j<N; j++)
            {
                double mean = M_mapConvCRB[error_name][j].mean();
                double variance = 0.0;
                for( int k=0; k < sampling_size; k++)
                    variance += (M_mapConvCRB[error_name][j](k) - mean)*(M_mapConvCRB[error_name][j](k) - mean)/sampling_size;

                if( Environment::worldComm().globalRank()  == Environment::worldComm().masterRank() )
                {

                    if( error_name=="SolutionErrorEstimated" || error_name=="SolutionDualErrorEstimated" || error_name=="OutputEstimatedError" ||
                        error_name=="SolutionErrorBoundEfficiency" || error_name=="SolutionDualErrorBoundEfficiency" || error_name=="OutputErrorBoundEfficiency")
                    {
                        double max2=0;
                        double min2=0;
                        if( error_name=="SolutionErrorEstimated" )
                        {
                            index_max = index_max_vector_solution_primal[j];
                            index_min = index_min_vector_solution_primal[j];
                            max2 = M_mapConvCRB[error_name][j]( index_max );
                            min2 = M_mapConvCRB[error_name][j]( index_min );
                        }
                        if( error_name=="SolutionErrorBoundEfficiency" )
                        {
                            index_max = index_max_vector_solution_primal[j];
                            index_min = index_min_vector_solution_primal[j];
                            double max_estimated = M_mapConvCRB["SolutionErrorEstimated"][j]( index_max );
                            double min_estimated = M_mapConvCRB["SolutionErrorEstimated"][j]( index_min );
                            double max = M_mapConvCRB["SolutionError"][j]( index_max );
                            double min = M_mapConvCRB["SolutionError"][j]( index_min );
                            max2 = max_estimated / max;
                            min2 = min_estimated / min;
                        }
                        if( error_name=="SolutionDualErrorEstimated" )
                        {
                            index_max = index_max_vector_solution_dual[j];
                            index_min = index_min_vector_solution_dual[j];
                            max2 = M_mapConvCRB[error_name][j]( index_max );
                            min2 = M_mapConvCRB[error_name][j]( index_min );
                        }
                        if( error_name=="SolutionDualErrorBoundEfficiency" )
                        {
                            index_max = index_max_vector_solution_dual[j];
                            index_min = index_min_vector_solution_dual[j];
                            double max_estimated = M_mapConvCRB["SolutionDualErrorEstimated"][j]( index_max );
                            double min_estimated = M_mapConvCRB["SolutionDualErrorEstimated"][j]( index_min );
                            double max = M_mapConvCRB["SolutionDualError"][j]( index_max );
                            double min = M_mapConvCRB["SolutionDualError"][j]( index_min );
                            max2 = max_estimated / max;
                            min2 = min_estimated / min;
                        }

                        if( error_name=="OutputEstimatedError" )
                        {
                            index_max = index_max_vector_output[j];
                            index_min = index_min_vector_output[j];
                            max2 = M_mapConvCRB[error_name][j]( index_max );
                            min2 = M_mapConvCRB[error_name][j]( index_min );
                        }
                        if( error_name=="OutputErrorBoundEfficiency" )
                        {
                            index_max = index_max_vector_output[j];
                            index_min = index_min_vector_output[j];
                            double max_estimated = M_mapConvCRB["OutputEstimatedError"][j]( index_max );
                            double min_estimated = M_mapConvCRB["OutputEstimatedError"][j]( index_min );
                            double max = M_mapConvCRB["OutputError"][j]( index_max );
                            double min = M_mapConvCRB["OutputError"][j]( index_min );
                            max2 = max_estimated / max;
                            min2 = min_estimated / min;
                        }
                        double min = M_mapConvCRB[error_name][j].minCoeff(&index_min) ;
                        double max = M_mapConvCRB[error_name][j].maxCoeff(&index_max) ;
                        conv << j+1 << "\t"
                             << min2 << "\t" << max2 << "\t" << min << "\t" << max << "\t" << mean << "\t" << variance << "\n";

                    }
                    else
                    {
                        conv << j+1 << "\t"
                             << M_mapConvCRB[error_name][j].minCoeff(&index_min) << "\t"
                             << M_mapConvCRB[error_name][j].maxCoeff(&index_max) << "\t"
                             << mean << "\t" << variance << "\n";
                        if( error_name=="OutputError" )
                        {
                            index_max_vector_output.push_back( index_max );
                            index_min_vector_output.push_back( index_min );
                        }
                        if( error_name=="SolutionError")
                        {
                            index_max_vector_solution_primal.push_back( index_max );
                            index_min_vector_solution_primal.push_back( index_min );
                        }
                        if( error_name=="SolutionDualError")
                        {
                            index_max_vector_solution_dual.push_back( index_max );
                            index_min_vector_solution_dual.push_back( index_min );
                        }
                    }
                }//master proc
            }//loop over number of RB elements

            conv.close();
        }

        int Nmax = vector_sampling_for_primal_efficiency_under_1.size();
        for(int N=0; N<Nmax; N++)
        {
            std::string file_name_primal = (boost::format("Sampling-Primal-Problem-Bad-Efficiency-N=%1%") %(N+1) ).str();
            if( vector_sampling_for_primal_efficiency_under_1[N]->size() > 1 )
                vector_sampling_for_primal_efficiency_under_1[N]->writeOnFile(file_name_primal);
        }
        Nmax = vector_sampling_for_dual_efficiency_under_1.size();
        for(int N=0; N<Nmax; N++)
        {
            std::string file_name_dual = (boost::format("Sampling-Dual-Problem-Bad-Efficiency-N=%1%") %(N+1) ).str();
            if( vector_sampling_for_dual_efficiency_under_1[N]->size() > 1 )
                vector_sampling_for_dual_efficiency_under_1[N]->writeOnFile(file_name_dual);
        }


    }

    void doTheScmConvergenceStat( int sampling_size )
    {
        auto N = crb->scm()->KMax();
        std::list<std::string> list_error_type = boost::assign::list_of("RelativeError");
        BOOST_FOREACH( auto error_name, list_error_type)
        {
            std::ofstream conv;
            std::string file_name = "cvg-scm-"+ error_name +"-stats.dat";

            if( Environment::worldComm().globalRank() == Environment::worldComm().masterRank() )
            {
                conv.open(file_name, std::ios::app);
                conv << "Nb_basis" << "\t" << "Min" << "\t" << "Max" << "\t" << "Mean" << "\t" << "Variance" << "\n";
            }

            for(int j=0; j<N; j++)
            {
                double mean = M_mapConvSCM[error_name][j].mean();
                double variance = 0.0;
                for( int k=0; k < sampling_size; k++)
                    variance += (M_mapConvSCM[error_name][j](k) - mean)*(M_mapConvSCM[error_name][j](k) - mean)/sampling_size;

                if( Environment::worldComm().globalRank()  == Environment::worldComm().masterRank() )
                {
                    conv << j+1 << "\t"
                         << M_mapConvSCM[error_name][j].minCoeff() << "\t"
                         << M_mapConvSCM[error_name][j].maxCoeff() << "\t"
                         << mean << "\t" << variance << "\n";
                }
            }
            conv.close();
        }
    }


    // Script write current mu in cfg => need to write it in SamplingForTest
    void buildSamplingFromCfg()
    {
        // Size of mu
        int mu_size = model->parameterSpace()->dimension();

        // Clear SamplingForTest is exists, and open a new one
        fs::path input_file ("SamplingForTest");
        if( fs::exists(input_file) )
            std::remove( "SamplingForTest" );

        std::ofstream input( "SamplingForTest" );
        input << "mu= [ ";

        // Check if cfg file is readable
        std::ifstream cfg_file( option(_name="config-file").template as<std::string>() );
        if(!cfg_file)
            std::cout << "[Script-mode] Config file cannot be read" << std::endl;

        // OT writes values of mu in config file => read it and copy in SamplingForTest with specific syntax
        for(int i=1; i<=mu_size; i++)
            {
                // convert i into string
                std::ostringstream oss;
                oss << i;
                std::string is = oss.str();

                // Read cfg file, collect line with current mu_i
                std::string cfg_line_mu, tmp_content;
                std::ifstream cfg_file( option(_name="config-file").template as<std::string>() );
                while(cfg_file)
                    {
                        std::getline(cfg_file, tmp_content);
                        if(tmp_content.compare(0,2+is.size(),"mu"+is) == 0)
                            cfg_line_mu += tmp_content;
                    }

                //Regular expression : corresponds to one set in xml file (mu<i>=<value>)
                std::string expr_s = "mu"+is+"[[:space:]]*=[[:space:]]*([0-9]+(.?)[0-9]*(e(\\+|-)[0-9]+)?)[[:space:]]*";
                boost::regex expression( expr_s );

                //Match mu<i>=<value> in cfg file and copy to SamplingForTest
                boost::smatch what;
                auto is_match = boost::regex_match(cfg_line_mu, what, expression);
                //std::cout << "is match ?" << is_match << std::endl;
                if(is_match)
                    {
                        // what[0] is the complete string mu<i>=<value>
                        // what[1] is the submatch <value>
                        //std::cout << "what 1 = " << what[1] << std::endl;
                        if( i!=mu_size )
                            input << what[1] << " , ";
                        else
                            input << what[1] << " ]";
                    }
            }
    }

private:
    CRBModelMode M_mode;
    crbmodel_ptrtype model;
    crb_ptrtype crb;

    // For EIM convergence study
    std::map<std::string, std::vector<vectorN_type> > M_mapConvEIM;
    // For CRB convergence study
    std::map<std::string, std::vector<vectorN_type> > M_mapConvCRB;
    // For SCM convergence study
    std::map<std::string, std::vector<vectorN_type> > M_mapConvSCM;

    //vector of sampling to stock parameters for which the efficiency is under 1
    std::vector< sampling_ptrtype > vector_sampling_for_primal_efficiency_under_1;
    std::vector< sampling_ptrtype > vector_sampling_for_dual_efficiency_under_1;

    fs::path M_current_path;
}; // OpusApp

} // Feel

#endif /* __OpusApp_H */

