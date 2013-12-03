/* -*- mode: c++; coding: utf-8; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; show-trailing-whitespace: t -*- vim:fenc=utf-8:ft=tcl:et:sw=4:ts=4:sts=4

   This file is part of the Feel library

   Author(s): Christophe Prud'homme <christophe.prudhomme@feelpp.org>
   Date: 2013-04-07

   Copyright (C) 2013 Feel++ Consortium

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3.0 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
/**
   \file ReducedBasisSpace.hpp
   \author Christophe Prud'homme <christophe.prudhomme@feelpp.org>
   \author Stephane Veys <stephane.veys@imag.fr>
   \date 2013-04-07
*/
#ifndef __ReducedBasisSpace_H
#define __ReducedBasisSpace_H 1

#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

#include <feel/feel.hpp>
#include <feel/feelcrb/crb.hpp>
#include <feel/feelcrb/modelcrbbase.hpp>

#include <Eigen/Core>
#include <Eigen/LU>
#include <Eigen/Dense>
#include <boost/timer.hpp>


namespace Feel
{


typedef parameter::parameters<
    parameter::required<tag::model_type, boost::is_base_and_derived<ModelCrbBase<>,_> >
    , parameter::required<tag::mesh_type, boost::is_base_and_derived<MeshBase,_> >
    , parameter::optional<parameter::deduced<tag::bases_list>, boost::is_base_and_derived<Feel::detail::bases_base,_> >
    , parameter::optional<parameter::deduced<tag::value_type>, boost::is_floating_point<_> >
    , parameter::optional<parameter::deduced<tag::periodicity_type>, boost::is_base_and_derived<Feel::detail::periodicity_base,_> >
    > reducedbasisspace_signature;

/**
 * \class ReducedBasisSpace
 * \brief Reduced Basis Space class
 *
 * @author Christophe Prud'homme
 * @see
 */

template<typename ModelType,
         typename MeshType,
         typename A1 = parameter::void_,
         typename A2 = parameter::void_,
         typename A3 = parameter::void_,
         typename A4 = parameter::void_>
class ReducedBasisSpace : public FunctionSpace<MeshType,A1,A2,A3,A4>
{

    typedef typename reducedbasisspace_signature::bind<ModelType,MeshType,A1,A2,A3,A4>::type args;

    typedef FunctionSpace<MeshType,A1,A2,A3,A4> super;
    typedef boost::shared_ptr<super> super_ptrtype;

public :

    typedef double value_type;

    typedef ModelType model_type;
    typedef boost::shared_ptr<model_type> model_ptrtype;

    typedef MeshType mesh_type;
    typedef boost::shared_ptr<mesh_type> mesh_ptrtype;

    typedef super fespace_type;
    typedef super_ptrtype fespace_ptrtype;

    //typedef typename super::element_type space_element_type;
    typedef typename model_type::element_type space_element_type;
    typedef boost::shared_ptr<space_element_type> space_element_ptrtype;

    typedef std::vector< space_element_type > rb_basis_type;
    typedef boost::shared_ptr<rb_basis_type> rb_basis_ptrtype;

    typedef ReducedBasisSpace<ModelType, MeshType, A1, A2, A3, A4> this_type;
    typedef boost::shared_ptr<this_type> this_ptrtype;

    typedef Eigen::Matrix<value_type,Eigen::Dynamic,1> eigen_vector_type;
    typedef Eigen::Matrix<value_type,Eigen::Dynamic,Eigen::Dynamic> eigen_matrix_type;


    typedef typename parameter::binding<args, tag::bases_list, Feel::detail::bases<Lagrange<1,Scalar> > >::type bases_list; // FEM basis
    typedef typename parameter::binding<args, tag::mesh_type>::type meshes_list;


    //static const bool is_composite = ( mpl::size<bases_list>::type::value > 1 );
    static const bool is_composite = super::is_composite;
    static const bool is_product = super::is_product ;
    static const uint16_type nSpaces = super::nSpaces;


    struct nodim { static const int nDim = -1; static const int nRealDim = -1; };
    static const uint16_type nDim = mpl::if_<boost::is_base_of<MeshBase, meshes_list >,
                                             mpl::identity<meshes_list >,
                                             mpl::identity<nodim> >::type::type::nDim;
    static const uint16_type nRealDim = mpl::if_<boost::is_base_of<MeshBase, meshes_list >,
                                                 mpl::identity<meshes_list>,
                                                 mpl::identity<nodim> >::type::type::nRealDim;


    // geomap
    typedef typename mpl::if_<mpl::greater<mpl::int_<nDim>, mpl::int_<0> >,mpl::identity<typename mesh_type::gm_type>, mpl::identity<mpl::void_> >::type::type gm_type;
    typedef typename mpl::if_<mpl::greater<mpl::int_<nDim>, mpl::int_<0> >,mpl::identity<typename mesh_type::gm1_type>, mpl::identity<mpl::void_> >::type::type gm1_type;
    typedef typename mpl::if_<mpl::greater<mpl::int_<nDim>, mpl::int_<0> >,mpl::identity<typename mesh_type::element_type>, mpl::identity<mpl::void_> >::type::type geoelement_type;
    typedef boost::shared_ptr<gm_type> gm_ptrtype;
    typedef boost::shared_ptr<gm1_type> gm1_ptrtype;
    typedef typename mpl::if_<mpl::greater<mpl::int_<nDim>, mpl::int_<0> >,mpl::identity<typename gm_type::template Context<vm::POINT, geoelement_type> >,
                              mpl::identity<mpl::void_> >::type::type pts_gmc_type;
    typedef typename mpl::if_<mpl::greater<mpl::int_<nDim>, mpl::int_<0> >,mpl::identity<typename gm_type::template Context<vm::POINT|vm::JACOBIAN|vm::HESSIAN|vm::KB, geoelement_type> >,
                              mpl::identity<mpl::void_> >::type::type gmc_type;
    typedef boost::shared_ptr<gmc_type> gmc_ptrtype;
    typedef typename mpl::if_<mpl::greater<mpl::int_<nDim>, mpl::int_<0> >,mpl::identity<typename gm_type::precompute_ptrtype>, mpl::identity<mpl::void_> >::type::type geopc_ptrtype;
    typedef typename mpl::if_<mpl::greater<mpl::int_<nDim>, mpl::int_<0> >,mpl::identity<typename gm_type::precompute_type>, mpl::identity<mpl::void_> >::type::type geopc_type;


    static const uint16_type nComponents = super::nComponents;


    template<typename MeshListType,int N>
    struct GetMesh
    {
        typedef typename mpl::if_<mpl::or_<boost::is_base_of<MeshBase, MeshListType >,
                                           is_shared_ptr<MeshListType> >,
                                  mpl::identity<mpl::identity<MeshListType> >,
                                  mpl::identity<mpl::at_c<MeshListType,N> > >::type::type::type type;
    };



    typedef typename mpl::at_c<bases_list,0>::type::template apply<GetMesh<meshes_list,0>::type::nDim,
                                                                   GetMesh<meshes_list,0>::type::nRealDim,
                                                                   value_type,
                                                                   typename GetMesh<meshes_list,0>::type::element_type>::type basis_0_type;


    typedef typename mpl::if_<mpl::bool_<is_composite>,
                              mpl::identity<bases_list>,
                              mpl::identity<basis_0_type> >::type::type basis_type;


    typedef typename super::periodicity_type periodicity_type;
    typedef boost::shared_ptr<periodicity_type> periodicity_ptrtype;


    ReducedBasisSpace( model_ptrtype const& model,
                       mesh_ptrtype const& mesh,
                       size_type mesh_components = MESH_RENUMBER | MESH_CHECK,
                       periodicity_type  periodicity = periodicity_type(),
                       std::vector<WorldComm> const& _worldsComm = Environment::worldsComm(nSpaces) )
        :
        super( mesh, mesh_components, periodicity, _worldsComm ),
        M_mesh( mesh ),
        M_model( model )
        {
            this->init();
        }
    ReducedBasisSpace( boost::shared_ptr<ModelType> model , boost::shared_ptr<MeshType> mesh )
        :
        super ( mesh ),
        M_mesh( mesh ),
        M_model( model )
        {
            LOG( INFO ) <<" ReducedBasisSpace constructor " ;
            this->init();
        }


    ReducedBasisSpace( boost::shared_ptr<ModelType> model , boost::shared_ptr<MeshType> mesh,  periodicity_ptrtype & periodicity )
        :
        super ( mesh , MESH_RENUMBER | MESH_CHECK , periodicity ),
        M_mesh( mesh ),
        M_model( model )
        {
            LOG( INFO ) <<" ReducedBasisSpace constructor ( with periodicity )" ;
            this->init();
        }

    //copy constructor
    ReducedBasisSpace( ReducedBasisSpace const& rb )
    :
    super ( rb.M_mesh ),
    M_primal_rb_basis( rb.M_primal_rb_basis ),
    M_dual_rb_basis( rb.M_dual_rb_basis ),
    M_mesh( rb.M_mesh )
        {}

    ReducedBasisSpace( this_ptrtype const& rb )
    :
    super ( rb->M_mesh ),
    M_primal_rb_basis( rb->M_primal_rb_basis ),
    M_dual_rb_basis( rb->M_dual_rb_basis ),
    M_mesh( rb->M_mesh )
        {}

    void init()
        {
            //M_rbbasis = rbbasis_ptrtype( new rb_basis_type() );
        }


    /*
     * Get the mesh
     */
    mesh_ptrtype mesh()
        {
            return M_mesh;
        }


    /*
     * add a new basis
     */
    void addPrimalBasisElement( space_element_type const & e )
        {
            M_primal_rb_basis.push_back( e );
        }

    void addPrimalBasisElement( space_element_ptrtype const & e )
        {
            M_primal_rb_basis.push_back( *e );
        }

    space_element_type& primalBasisElement( int index )
        {
            int size = M_primal_rb_basis.size();
            CHECK( index < size ) << "bad index value, size of the RB "<<size<<" and index given : "<<index;
            return M_primal_rb_basis[index];
        }
    void addDualBasisElement( space_element_type const & e )
        {
            M_dual_rb_basis.push_back( e );
        }

    void addDualBasisElement( space_element_ptrtype const & e )
        {
            M_dual_rb_basis.push_back( *e );
        }

    space_element_type& dualBasisElement( int index )
        {
            int size = M_dual_rb_basis.size();
            CHECK( index < size ) << "bad index value, size of the RB "<<size<<" and index given : "<<index;
            return M_dual_rb_basis[index];
        }

    /*
     * Get basis of the reduced basis space
     */
    rb_basis_type & primalRB()
        {
            return M_primal_rb_basis;
        }
    rb_basis_type & dualRB()
        {
            return M_dual_rb_basis;
        }
    rb_basis_type & primalRB() const
        {
            return M_primal_rb_basis;
        }
    rb_basis_type & dualRB() const
        {
            return M_dual_rb_basis;
        }

    /*
     * Set basis of the reduced basis space
     */
    void setBasis( boost::tuple< rb_basis_type , rb_basis_type > & tuple )
        {
            auto primal = tuple.template get<0>();
            auto dual = tuple.template get<1>();
            M_primal_rb_basis = primal;
            M_dual_rb_basis = dual;
        }

    //basis of RB space are elements of FEM function space
    //return value of the N^th basis ( vector ) at index idx
    //idx is the global dof ( fem )
    double basisValue(int N, int idx) const
        {
            return M_primal_rb_basis[N].globalValue( idx );
        }

    /*
     * return true if the function space is composite
     */
    bool isComposite()
        {
            return super::is_composite;
        }
    /*
     * size of the reduced basis space : number of basis functions
     */
    int size()
        {
            return M_primal_rb_basis.size();
        }

    /*
     * visualize basis of rb space
     */
    void visualize ()
        {
        }

    BOOST_PARAMETER_MEMBER_FUNCTION( ( this_ptrtype ),
                                     static New,
                                     tag,
                                     ( required
                                       ( model, *)
                                       ( mesh,* )
                                         )
                                     ( optional
                                       ( worldscomm, *, Environment::worldsComm(nSpaces) )
                                       ( components, ( size_type ), MESH_RENUMBER | MESH_CHECK )
                                       ( periodicity,*,periodicity_type() )
                                         )
        )
        {
            //LOG( INFO ) << "ReducedBasis NEW (new impl)";
            return NewImpl( model, mesh, worldscomm, components, periodicity );
        }

    static this_ptrtype NewImpl( model_ptrtype const& model,
                                 mesh_ptrtype const& __m,
                                 std::vector<WorldComm> const& worldsComm = Environment::worldsComm(nSpaces),
                                 size_type mesh_components = MESH_RENUMBER | MESH_CHECK,
                                 periodicity_type periodicity = periodicity_type() )
        {

            return this_ptrtype( new this_type( model, __m, mesh_components, periodicity, worldsComm ) );
        }


    model_ptrtype model()
        {
            return M_model;
        }

    /*
     * Get the FEM functionspace
     */
    super_ptrtype functionSpace()
        {
            return M_model->functionSpace();
        }

    class ContextRB
        :
        public fespace_type::basis_context_type,
        public boost::enable_shared_from_this<ContextRB>
    {
    public:
        typedef this_type reducedbasisspace_type;
        typedef this_ptrtype reducedbasisspace_ptrtype;
        typedef typename fespace_type::basis_context_type  super;
        typedef std::pair<int,super> ctx_type;
        typedef std::pair<int,boost::shared_ptr<super>> ctx_ptrtype;

        static const uint16_type nComponents = reducedbasisspace_type::nComponents;

        ContextRB( ctx_ptrtype const& ctx, reducedbasisspace_ptrtype W )
        :
        super( *ctx.second ),
        M_index( ctx.first ),
        M_rbspace( W )

            {
                CHECK( M_rbspace ) << "Invalid rb space";
                LOG( INFO ) << "size : "<< M_rbspace->size();;
                M_phi.resize( nComponents, M_rbspace->size() );
                M_grad.resize(nComponents);
                for(int c=0; c<nComponents; c++)
                {
                    M_grad[c].resize( nDim );
                    for(int d=0; d<nDim; d++)
                    {
                        M_grad[c][d].resize(M_rbspace->size());
                    }
                }//end of resize

            }

        int pointIndex() const
            {
                return M_index;
            }

        reducedbasisspace_ptrtype rbSpace() const { return M_rbspace; }

        void update()
            {
                int npts = this->nPoints();
                int N = M_rbspace->size();

                //
                // id
                //
                M_phi = M_rbspace->evaluateBasis( this->shared_from_this() );

                //
                // grad
                //
                for( int i=0; i<N; i++)
                {
                    //matrix containing grad at all points
                    auto evaluation = M_rbspace->evaluateGradBasis( i , this->shared_from_this() );
                    for(int c=0; c<nComponents; c++)
                    {
                        for(int d=0; d<nDim; d++)
                        {
                            //evaluation is a matrix
                            M_grad[c][d]( i ) = evaluation( d, c );
                        }//dimension
                    }//components
                }//rb functions

                for(int c=0; c<nComponents; c++)
                {
                    for(int d=0; d<nDim; d++)
                    {
                        DVLOG( 2 ) << "matrix M_grad of component "<<c<<"and dim "<<d<<" : \n"<<M_grad[c][d];
                    }
                }//components

            } // update


        eigen_vector_type id( eigen_vector_type const& coeffs, mpl::bool_<false> ) const
            {
                // this should work for scalar and vector fields we expect
                // coeffs to be (d,N) and M_phi (d,vN) where d is the number of
                // components and N the number of basis functions
                // TODO: check with vectorial functions
                //return (coeffs*M_phi.transpose()).diagonal();
                return M_phi*coeffs;

            }
        eigen_vector_type id( eigen_vector_type const& coeffs, mpl::bool_<true> ) const
            {
                // this should work for scalar and vector fields we expect
                // coeffs to be (d,N) and M_phi (d,vN) where d is the number of
                // components and N the number of basis functions
                // TODO: check with vectorial functions
                return (coeffs*M_phi.transpose()).diagonal();
            }
        //evaluation at only one node
        eigen_vector_type id( eigen_vector_type const& coeffs ) const
            {
                return id( coeffs, mpl::bool_<(nComponents>1)>() );
            }

        /*
         * for given element coefficients, gives the gradient of the element at node given in context_fem
         */
        eigen_vector_type grad( eigen_vector_type const& coeffs , int node_index=-1 ) const
            {
                int npts = super::nPoints();
                DCHECK(npts > node_index)<<"node_index "<<node_index<<" must be lower that npts "<<npts;
                if( node_index >=0 )
                    npts=1; //we study only the node at node_index

                eigen_vector_type result ;
                for(int d=0; d<nDim; d++)
                {
                    for(int c=0; c<nComponents; c++)
                    {
                        eigen_vector_type result_comp_dim( npts );
                        auto prod = coeffs.transpose()*M_grad[c][d];
                        result_comp_dim=prod.transpose();
                        //concatenate
                        int new_size = result.size() + result_comp_dim.size();
                        eigen_vector_type tmp( result );
                        result.resize( new_size );
                        result << tmp,result_comp_dim;
                    }
                }
#if 0
                //we now reorganize datas
                eigen_vector_type tmp( result );
                for(int p=0; p<npts; p++)
                {
                    for(int d=0; d<nDim; d++)
                    {
                        for(int c=0; c<nComponents; c++)
                            result(p*nComponents*nDim+c+d) = tmp(p+npts*c*d);
                    }
                }
#endif
                return result;

            }//grad


        /*
         * for given element coefficients, gives the dx,dy or dy of the element at node given in context_fem
         */
        eigen_vector_type d(int N, eigen_vector_type const& coeffs , int node_index=-1 ) const
            {
                int npts = super::nPoints();
                DCHECK(npts > node_index)<<"node_index "<<node_index<<" must be lower that npts "<<npts;
                if( node_index >=0 )
                    npts=1; //we study only the node at node_index

                eigen_vector_type result ;
                for(int c=0; c<nComponents; c++)
                {
                    eigen_vector_type result_comp_dim( npts );

                    auto prod = coeffs.transpose()*M_grad[c][N];
                    result_comp_dim=prod.transpose();
                    //concatenate
                    int new_size = result.size() + result_comp_dim.size();
                    eigen_vector_type tmp( result );
                    result.resize( new_size );
                    result << tmp,result_comp_dim;
                }
#if 0
                //we now reorganize datas
                eigen_vector_type tmp( result );
                for(int p=0; p<npts; p++)
                {
                    for(int c=0; c<nComponents; c++)
                        result(p*nComponents*nDim+c+N) = tmp(p+npts*c*N);
                }
#endif
                return result;
            }//d

        value_type id( int ldof, int c1, int c2, int q ) const
            {
                return super::id( ldof, c1, c2, q );
            }
        value_type grad( int ldof, int c1, int c2, int q ) const
            {
                return super::grad( ldof, c1, c2, q );
            }
        value_type d( int ldof, int c1, int c2, int q ) const
            {
                return super::d( ldof, c1, c2, q );
            }
    private :
        int M_index;
        reducedbasisspace_ptrtype M_rbspace;
        eigen_matrix_type M_phi;
        std::vector<std::vector< eigen_vector_type> > M_grad;

    };

    typedef ContextRB ctxrb_type;
    typedef boost::shared_ptr<ctxrb_type> ctxrb_ptrtype;



    /*
     * store the evaluation of basis functions in given points
     */
    class ContextRBSet
        : public std::map<int,boost::shared_ptr<ContextRB> >
    {
        typedef typename std::map<int,boost::shared_ptr<ContextRB>> super;

    public :
        static const bool is_rb_context = true;


        typedef ReducedBasisSpace<ModelType,MeshType,A1,A2,A3,A4> rbspace_type;
        typedef boost::shared_ptr<rbspace_type> rbspace_ptrtype;

        typedef typename rbspace_type::super_ptrtype functionspace_ptrtype;
        typedef typename rbspace_type::super functionspace_type;
        typedef typename functionspace_type::Context fe_context_type;
        typedef boost::shared_ptr<fe_context_type> fe_context_ptrtype;

        typedef typename super::iterator iterator;

        typedef rbspace_type::eigen_vector_type eigen_vector_type;

        typedef rbspace_type::rb_basis_type rb_basis_type;
        typedef boost::shared_ptr<rb_basis_type> basis_ptrtype;

        typedef Eigen::MatrixXd eigen_matrix_type;

        typedef rbspace_type::mesh_type mesh_type;

        //static const bool is_function_space_scalar = functionspace_type::is_scalar;
        //static const bool is_function_space_vectorial = functionspace_type::is_vectorial;
        //static const uint16_type nComponents = functionspace_type::nComponents;
        //static const uint16_type nDim = rbspace_type::nDim;

        ~ContextRBSet() {}

        ContextRBSet( super const& s )
        :
        super( s ),
        M_ctx_fespace(s.begin()->second->rbSpace()->functionSpace()),
        M_rbspace(s.begin()->second->rbSpace())
            {
                for( auto& m : s )
                {
                    M_ctx_fespace.addCtx( m.second, Environment::worldComm().globalRank() );
                }
            }

        ContextRBSet( rbspace_ptrtype rbspace )
        :
        super(),
        M_ctx_fespace( rbspace->functionSpace() ),
        M_rbspace( rbspace )
            {
                update();
            }

        //only because the function functionSpace()
        //returns an object : rb_functionspaceptrtype
        ContextRBSet( functionspace_ptrtype functionspace )
        :
        super(),
        M_ctx_fespace( functionspace )
            {
                update();
            }

        void update ( )
            {
                for( auto& c : *this )
                    c.second->update();
            }

        rbspace_ptrtype rbFunctionSpace() const
            {
                return M_rbspace;
            }

        rbspace_type* ptrFunctionSpace() const
            {
                return M_rbspace.get();
            }

        std::pair<iterator,bool>
        add( node_type t )
        {
            return add( t, mpl::bool_<is_composite>() );
        }
        std::pair<iterator,bool>
        add( node_type t, mpl::bool_<true> )
        {
            return std::make_pair( this->end(), false );
        }
        std::pair<iterator,bool>
        add( node_type t, mpl::bool_<false> )
        {
            // we suppose here that the point will be added which means that it
            // has been located in the underlying mesh
            auto ret = M_ctx_fespace.add( t );
            // if the current processor handles the point previously located
            // add it to the ContextRB set
            if ( ret.second )
            {
                return this->insert( std::make_pair( ret.first->first, boost::make_shared<ContextRB>( *ret.first, M_rbspace ) ) );
            }
            return std::make_pair( this->end(), false );
        }
        int nPoints() const
        {
            return M_ctx_fespace.nPoints();
        }

        fe_context_type const& feContext() { return M_ctx_fespace; }

    private :
        fe_context_type M_ctx_fespace;
        rbspace_ptrtype M_rbspace;
    };
    typedef ContextRBSet ctxrbset_type;
    typedef boost::shared_ptr<ctxrbset_type> ctxrbset_ptrtype;

    /**
     * \return function space context
     */
    ContextRBSet context() { return ContextRBSet( boost::dynamic_pointer_cast< ReducedBasisSpace<ModelType,MeshType,A1,A2,A3,A4> >( this->shared_from_this() ) ); }

    ctxrb_ptrtype
    contextBasis( std::pair<int,boost::shared_ptr<ContextRB>> const& p, ContextRBSet const& c )
        {
            return p.second;
        }

    /**
     * evaluate the i^th basis function at nodes
     * given by the context ctx
     */
    eigen_matrix_type evaluateBasis( boost::shared_ptr<ContextRB> const& ctx )
        {
            eigen_matrix_type m( nComponents, M_primal_rb_basis.size() );
            std::map<int,ctxrb_ptrtype> mc{{0,ctx}};
            ContextRBSet s( mc );
            int c = 0;
            for( auto& b : M_primal_rb_basis )
            {
                m.col(c++) = evaluateFromContext( _context=s , _expr=idv(b) );
            }
            return m;
        }

    eigen_matrix_type evaluateGradBasis( int i , boost::shared_ptr<ContextRB> const& ctxs )
        {
            std::map<int,ctxrb_ptrtype> mc{{0,ctxs}};
            ContextRBSet ctx( mc );
            int npts = ctx.nPoints();
            eigen_matrix_type evaluation;
            evaluation.resize( nDim , npts*nComponents );
            if( nDim >= 1 )
            {
                auto evalx = evaluateFromContext( _context=ctx , _expr= dxv( M_primal_rb_basis[i] ) );
                for(int p=0; p<npts; p++)
                {
                    for(int c=0; c<nComponents;c++)
                        evaluation( 0 , p*nComponents+c ) = evalx( p*nComponents+c );
                }
            }
            if( nDim >= 2 )
            {
                auto evaly = evaluateFromContext( _context=ctx , _expr= dyv( M_primal_rb_basis[i] ) );
                for(int p=0; p<npts; p++)
                {
                    for(int c=0; c<nComponents;c++)
                        evaluation( 1 , p*nComponents+c ) = evaly( p*nComponents+c );
                }
            }
            if( nDim == 3 )
            {
                auto evalz = evaluateFromContext( _context=ctx , _expr= dzv( M_primal_rb_basis[i] ) );
                for(int p=0; p<npts; p++)
                {
                    for(int c=0; c<nComponents;c++)
                        evaluation( 2 , p*nComponents+c ) = evalz( p*nComponents+c );
                }
            }
            return evaluation;
        }

    /*
     *  contains coefficients in the RB expression
     *  i.e. contains coefficients u_i in
     *  u_N ( \mu ) = \sum_i^N u_i ( \mu ) \xi_i( x )
     *  where \xi_i( x ) are basis function
     */
    template<typename T = double,  typename Cont = Eigen::Matrix<T,Eigen::Dynamic,1> >
    class Element
        :
        public Cont,boost::addable<Element<T,Cont> >, boost::subtractable<Element<T,Cont> >
    {
    public:
        typedef ReducedBasisSpace<ModelType,MeshType,A1,A2,A3,A4> rbspace_type;
        typedef boost::shared_ptr<rbspace_type> rbspace_ptrtype;

        typedef rbspace_type::super functionspace_type;
        typedef boost::shared_ptr<functionspace_type> functionspace_ptrtype;

        typedef Eigen::MatrixXd eigen_matrix_type;
        typedef rbspace_type::eigen_vector_type eigen_vector_type;

        //element of the FEM function space
        typedef rbspace_type::space_element_type space_element_type;

        //RB context
        typedef rbspace_type::ContextRBSet ctxrbset_type;
        typedef rbspace_type::ContextRB ctxrb_type;
        typedef boost::shared_ptr<rbspace_type::ContextRB> ctxrb_ptrtype;

        typedef T value_type;

        typedef Cont super;
        typedef Cont container_type;


        typedef typename functionspace_type::gm_type gm_type;
        typedef boost::shared_ptr<gm_type> gm_ptrtype;

        /**
         * geometry typedef
         */
        typedef typename mesh_type::element_type geoelement_type;
        typedef typename gm_type::template Context<vm::POINT|vm::JACOBIAN|vm::HESSIAN|vm::KB, geoelement_type> gmc_type;
        typedef boost::shared_ptr<gmc_type> gmc_ptrtype;
        typedef typename gm_type::precompute_ptrtype geopc_ptrtype;
        typedef typename gm_type::precompute_type geopc_type;


        static const uint16_type nRealDim = mesh_type::nRealDim;
        static const bool is_composite = functionspace_type::is_composite;

        typedef typename mpl::if_<mpl::bool_<is_composite>,
                                  mpl::identity<boost::none_t>,
                                  mpl::identity<typename basis_0_type::polyset_type> >::type::type polyset_type;

        typedef typename mpl::if_<mpl::bool_<is_composite>,
                                  mpl::identity<boost::none_t>,
                                  mpl::identity<typename basis_0_type::PreCompute> >::type::type pc_type;
        typedef boost::shared_ptr<pc_type> pc_ptrtype;

        static const uint16_type nComponents1 = functionspace_type::nComponents1;
        static const uint16_type nComponents2 = functionspace_type::nComponents2;

        typedef boost::multi_array<value_type,3> array_type;
        typedef Eigen::Matrix<value_type,nComponents1,1> _id_type;
        typedef Eigen::Matrix<value_type,nComponents1,nRealDim> _grad_type;
        typedef boost::multi_array<_id_type,1> id_array_type;
        typedef boost::multi_array<_grad_type,1> grad_array_type;

        typedef typename matrix_node<value_type>::type matrix_node_type;

        friend class ReducedBasisSpace<ModelType,MeshType,A1,A2,A3,A4>;

        Element()
            {}

        Element( rbspace_ptrtype const& rbspace , std::string const& name="u")
            :
            M_femfunctionspace( rbspace->functionSpace() ),
            M_rbspace( rbspace ),
            M_name( name )
            {
                this->resize( M_rbspace.size() );
            }

        /**
         * \return the mesh associated to the function
         */
        mesh_ptrtype mesh()
            {
                return M_femfunctionspace->mesh();
            }
        /**
         * \return the mesh associated to the function
         */
        mesh_ptrtype mesh() const
            {
                return M_femfunctionspace->mesh();
            }

        /*
         * return the reduced basis space associated
         */
        void setReducedBasisSpace( rbspace_ptrtype const& rbspace )
            {
                M_rbspace = rbspace;
            }

        /*
         * return the size of the element
         */
        int size() const
            {
                return super::size();
            }

        /**
         * \return the container read-only
         */
        super const& container() const
            {
                return *this;
            }

        /**
         * \return the container read-write
         */
        super & container()
            {
                return *this;
            }


        value_type globalValue( size_type i ) const
            {
                return this->operator()( i );
            }

        void setCoefficient( int index , double value )
            {
                int size=super::size();
                FEELPP_ASSERT( index < size )(index)(size).error("invalid index");
                this->operator()( index ) = value;
            }

        value_type operator()( size_t i ) const
            {
                int size=super::size();
                FEELPP_ASSERT( i < size )(i)(size).error("invalid index");
                return super::operator()( i );
            }


        int localSize() const
            {
                return super::size();
            }

        value_type& operator()( size_t i )
            {
                int size = super::size();
                FEELPP_ASSERT( i < size )(i)(size).error("invalid index");
                return super::operator()( i );
            }

        Element& operator+=( Element const& _e )
            {
                int size1=super::size();
                int size2=_e.size();
                FEELPP_ASSERT( size1 == size2 )(size1)(size2).error("invalid size");
                for ( int i=0; i <size1; ++i )
                    this->operator()( i ) += _e( i );
                return *this;
            }

        Element& operator-=( Element const& _e )
            {
                int size1=super::size();
                int size2=_e.size();
                FEELPP_ASSERT( size1 == size2 )(size1)(size2).error("invalid size");
                for ( int i=0; i <size2; ++i )
                    this->operator()( i ) -= _e( i );
                return *this;
            }

        space_element_type expansion( int  N=-1)
            {
                return M_rbspace.expansion( *this, N );
            }

        //basis of RB space are elements of FEM function space
        //return value of the N^th basis ( vector ) at index idx
        double basisValue( int N, int idx) const
            {
                return M_rbspace.basisValue( N , idx );
            }

        eigen_vector_type evaluate(  ctxrb_type const& context_rb )
            {
                return context_rb.id( *this );
            }

        /*
         * evaluate the element to nodes in context
         *
         * for given element coefficients, evaluate the element at node given in context_fem
         * in order to use matrix-vector multiplication, for np points, the result is given as follows :
         * np first elements of the vector : evaluation of the first component of field
         * np others : evaluation of second component
         * and in 3D case ( and vector field ) last np elements : evaluation of the third components
         *[ comp0 , ... , comp0 , comp1 , ... , comp1 , ... , comp2 , ... , comp2 ]
         * then we reorganize datas to have
         * [ comp0, comp1, comp2 ,... ,comp0, comp1, comp2 ]
         */
        eigen_vector_type evaluate(  ctxrbset_type const& context_rb )
            {
                int npts = context_rb.nPoints();
                Eigen::Matrix<value_type, Eigen::Dynamic, 1> result( npts*nComponents );
                int p = 0;
                for( auto const& ctx : context_rb )
                {
                    result.segment( nComponents*p, nComponents )  = this->evaluate( *ctx.second );
                    p++;
                }
                return result;
            }


        void updateGlobalValues() const
            {
                //nothing to do
            }
        bool areGlobalValuesUpdated() const
            {
                return true;
            }


        typedef Feel::detail::ID<value_type,nComponents1,nComponents2> id_type;

        /**
         * \return the extents of the interpolation of the function at
         * a set of points
         */
        template<typename ContextType>
        boost::array<typename array_type::index, 1>
        idExtents( ContextType const & context ) const
            {
                boost::array<typename array_type::index, 1> shape;
                shape[0] = context.xRefs().size2();
                return shape;
            }

        template<typename Context_t>
        id_type
        id( Context_t const & context ) const
            {
                return id_type( *this, context );
            }

        //the function id_ (called by idv, idt or id) will do not the same things
        //if it has a FEM context ( mpl::bool_<false> )
        //or if it has a RB context ( mpl::bool<true> )
        //with FEM context we use pre-computations of FEM basis functions whereas with RB context we use precomputations of RB basis functions
        template<typename Context_t> void  id_( Context_t const & context, id_array_type& v ) const;
        template<typename Context_t> void  id_( Context_t const & context, id_array_type& v , mpl::bool_<true> ) const;
        template<typename Context_t> void  id_( Context_t const & context, id_array_type& v , mpl::bool_<false>) const;

        template<typename Context_t>
        void
        id( Context_t const & context, id_array_type& v ) const
            {
                id_( context, v );
            }


        template<typename ContextType>
        boost::array<typename array_type::index, 1>
        gradExtents( ContextType const & context ) const
            {
                boost::array<typename array_type::index, 1> shape;
                shape[0] = context.xRefs().size2();
                return shape;
            }

        template<typename Context_t> void  grad_( Context_t const & context, grad_array_type& v ) const;
        template<typename Context_t> void  grad_( Context_t const & context, grad_array_type& v , mpl::bool_<true> ) const;
        template<typename Context_t> void  grad_( Context_t const & context, grad_array_type& v , mpl::bool_<false>) const;

        template<typename Context_t>
        void
        grad( Context_t const & context, grad_array_type& v ) const
            {
                grad_( context , v);
            }


        template<typename ContextType>
        boost::array<typename array_type::index, 1>
        dxExtents( ContextType const & context ) const
            {
                boost::array<typename array_type::index, 1> shape;
                shape[0] = context.xRefs().size2();
                return shape;
            }
        template<typename ContextType>
        boost::array<typename array_type::index, 1>
        dyExtents( ContextType const & context ) const
            {
                return dxExtents( context );
            }
        template<typename ContextType>
        boost::array<typename array_type::index, 1>
        dzExtents( ContextType const & context ) const
            {
                return dxExtents( context );
            }
        template<typename ContextType>
        boost::array<typename array_type::index, 1>
        dExtents( ContextType const & context ) const
            {
                return dxExtents( context );
            }

        template<typename Context_t> void  d_( int N, Context_t const & context, id_array_type& v ) const;
        template<typename Context_t> void  d_( int N, Context_t const & context, id_array_type& v , mpl::bool_<true> ) const;
        template<typename Context_t> void  d_( int N, Context_t const & context, id_array_type& v , mpl::bool_<false>) const;

        template<typename ContextType>
        void dx( ContextType const & context, id_array_type& v ) const
            {
                d_( 0, context, v );
            }
        template<typename ContextType>
        void dy( ContextType const & context, id_array_type& v ) const
            {
                d_( 1, context, v );
            }
        template<typename ContextType>
        void dz( ContextType const & context, id_array_type& v ) const
            {
                d_( 2, context, v );
            }


        void
        idInterpolate( matrix_node_type __ptsReal, id_array_type& v ) const;


        /*
         * Get the reals points matrix in a context
         * 1 : Element
         *
         * \todo store a geometric mapping context to evaluate the real points
         * from a set of point in the referene element, should probably done in
         * the real element (geond)
         */
        template<typename Context_t>
        matrix_node_type
        ptsInContext( Context_t const & context, mpl::int_<1> ) const
            {
                //new context for evaluate the points
                typedef typename Context_t::gm_type::template Context< Context_t::context|vm::POINT, typename Context_t::element_type> gmc_interp_type;
                typedef boost::shared_ptr<gmc_interp_type> gmc_interp_ptrtype;

                gmc_interp_ptrtype __c_interp( new gmc_interp_type( context.geometricMapping(), context.element_c(),  context.pc() ) );

                return __c_interp->xReal();
            }

        /*
         * Get the real point matrix in a context
         * 2 : Face
         * \todo see above
         */
        template<typename Context_t>
        matrix_node_type
        ptsInContext( Context_t const & context,  mpl::int_<2> ) const
            {
                //new context for the interpolation
                typedef typename Context_t::gm_type::template Context< Context_t::context|vm::POINT, typename Context_t::element_type> gmc_interp_type;
                typedef boost::shared_ptr<gmc_interp_type> gmc_interp_ptrtype;

                typedef typename Context_t::gm_type::template Context<Context_t::context,typename Context_t::element_type>::permutation_type permutation_type;
                typedef typename Context_t::gm_type::template Context<Context_t::context,typename Context_t::element_type>::precompute_ptrtype precompute_ptrtype;

                std::vector<std::map<permutation_type, precompute_ptrtype> > __geo_pcfaces = context.pcFaces();
                gmc_interp_ptrtype __c_interp( new gmc_interp_type( context.geometricMapping(), context.element_c(), __geo_pcfaces , context.faceId() ) );

                return __c_interp->xReal();
            }


        /**
           \return the finite element space ( not reduced basis space )
        */
        functionspace_ptrtype const& functionSpace() const
            {
                return M_femfunctionspace;
            }

    private:
        functionspace_ptrtype M_femfunctionspace;
        rbspace_type M_rbspace;
        std::string M_name;
    };//Element of the reduced basis space

    typedef Element<value_type> element_type;
    typedef boost::shared_ptr<element_type> element_ptrtype;

    //Access to an element of the reduced basis space
    element_type element( std::string const& name="u" )
        {
            element_type u(boost::dynamic_pointer_cast<ReducedBasisSpace<ModelType,MeshType,A1,A2,A3,A4> >(this->shared_from_this() ), name );
            u.setZero();
            return u;
        }

    element_ptrtype elementPtr( std::string const& name="u" )
        {
            element_ptrtype u( new element_type( boost::dynamic_pointer_cast<ReducedBasisSpace<ModelType,MeshType,A1,A2,A3,A4> >(this->shared_from_this() ) , name ) );
            u->setZero();
            return u;
        }

    space_element_type expansion( element_type const& unknown, int  N=-1)
        {
            int number_of_coeff;
            int basis_size = M_primal_rb_basis.size();
            if ( N == -1 )
                number_of_coeff = basis_size;
            else
                number_of_coeff = N;
            FEELPP_ASSERT( number_of_coeff <= basis_size )( number_of_coeff )( basis_size ).error("invalid size");
            return Feel::expansion( M_primal_rb_basis, unknown , number_of_coeff );
        }

private :
    rb_basis_type M_primal_rb_basis;
    rb_basis_type M_dual_rb_basis;
    mesh_ptrtype M_mesh;
    model_ptrtype M_model;

};//ReducedBasisSpace


template<typename ModelType, typename A0, typename A1, typename A2, typename A3, typename A4>
const uint16_type ReducedBasisSpace<ModelType,A0,A1,A2,A3,A4>::nComponents;

template<typename ModelType, typename A0, typename A1, typename A2, typename A3, typename A4>
const uint16_type ReducedBasisSpace<ModelType,A0,A1,A2,A3,A4>::ContextRB::nComponents;



template<typename ModelType, typename A0, typename A1, typename A2, typename A3, typename A4>
template<typename Y,  typename Cont>
template<typename Context_t>
void
ReducedBasisSpace<ModelType,A0, A1, A2, A3, A4>::Element<Y,Cont>::id_( Context_t const & context, id_array_type& v ) const
{
    return id_( context, v, boost::is_same<Context_t,ContextRB>() );
}


template<typename ModelType, typename A0, typename A1, typename A2, typename A3, typename A4>
template<typename Y,  typename Cont>
template<typename Context_t>
void
ReducedBasisSpace<ModelType,A0, A1, A2, A3, A4>::Element<Y,Cont>::grad_( Context_t const & context, grad_array_type& v ) const
{
    return grad_( context, v, boost::is_same<Context_t,ContextRB>() );
}

template<typename ModelType, typename A0, typename A1, typename A2, typename A3, typename A4>
template<typename Y,  typename Cont>
template<typename Context_t>
void
ReducedBasisSpace<ModelType,A0, A1, A2, A3, A4>::Element<Y,Cont>::d_( int N, Context_t const & context, id_array_type& v ) const
{
    return d_( N, context, v, boost::is_same<Context_t,ContextRB>() );
}


//warning :
//if we need to evaluate element at nodes in context
//use u.evaluate( rb_context ) should go faster
//because here we do the same thing ( context.id(*this) )
//but we add a loop over points
template<typename ModelType, typename A0, typename A1, typename A2, typename A3, typename A4>
template<typename Y,  typename Cont>
template<typename Context_t>
void
ReducedBasisSpace<ModelType,A0, A1, A2, A3, A4>::Element<Y,Cont>::id_( Context_t const & context, id_array_type& v , mpl::bool_<true> ) const
{
    v[0] = context.id( *this );
}

template<typename ModelType, typename A0, typename A1, typename A2, typename A3, typename A4>
template<typename Y,  typename Cont>
template<typename Context_t>
void
ReducedBasisSpace<ModelType,A0, A1, A2, A3, A4>::Element<Y,Cont>::grad_( Context_t const & context, grad_array_type& v , mpl::bool_<true> ) const
{
    ctxrb_type const& rb_context = dynamic_cast< ctxrb_type const& >( context );
    //LOG( INFO ) << " grad_ with a RB context";

    int index = rb_context.pointIndex();
    //evaluate the gradient at a specific point (called by evaluateFromContext)
    auto evaluation = rb_context.grad( *this );
    for(int d=0;d<nDim;d++)
    {
        for(int c=0; c<nComponents; c++)
        {
            v[0]( c,d ) += evaluation(c+d*nComponents);
        }
    }
}

template<typename ModelType, typename A0, typename A1, typename A2, typename A3, typename A4>
template<typename Y,  typename Cont>
template<typename Context_t>
void
ReducedBasisSpace<ModelType,A0, A1, A2, A3, A4>::Element<Y,Cont>::d_( int N, Context_t const & context, id_array_type& v , mpl::bool_<true> ) const
{
    ctxrb_type const& rb_context = dynamic_cast< ctxrb_type const& >( context );
    //LOG( INFO ) << " d_ with a RB context";

    auto evaluation = rb_context.d(N, *this );

    for(int c=0; c<nComponents; c++)
    {
        v[0]( c,0 ) = evaluation(c);
    }

}

template<typename ModelType, typename A0, typename A1, typename A2, typename A3, typename A4>
template<typename Y,  typename Cont>
template<typename Context_t>
void
ReducedBasisSpace<ModelType,A0, A1, A2, A3, A4>::Element<Y,Cont>::id_( Context_t const & context, id_array_type& v , mpl::bool_<false> ) const
{
    //LOG( INFO ) << "id_ with a FEM context";
    if ( !this->areGlobalValuesUpdated() )
        this->updateGlobalValues();

    size_type elt_id = context.eId();
    if ( context.gmContext()->element().mesh()->isSubMeshFrom( this->mesh() ) )
        elt_id = context.gmContext()->element().mesh()->subMeshToMesh( context.eId() );
    if ( context.gmContext()->element().mesh()->isParentMeshOf( this->mesh() ) )
        elt_id = this->mesh()->meshToSubMesh( context.eId() );
    if ( elt_id == invalid_size_type_value )
        return;

    const uint16_type nq = context.xRefs().size2();

    int rb_size = this->size();
    //loop on RB dof
    for(int N=0; N<rb_size; N++)
    {

        // the RB unknown can be written as
        // u^N = \sum_i^N u_i^N \PHI_i where \PHI_i are RB basis functions (i.e. elements of FEM function space)
        // u^N = \sum_i^N u_i^N \sum_j \PHI_ij \phi_j where PHI_ij is a scalar and phi_j is the j^th fem basis function
        value_type u_i = this->operator()( N );

        for ( int l = 0; l < basis_type::nDof; ++l )
        {
            const int ncdof = is_product?nComponents1:1;

            for ( typename array_type::index c1 = 0; c1 < ncdof; ++c1 )
            {
                typename array_type::index ldof = basis_type::nDof*c1+l;
                size_type gdof = boost::get<0>( M_femfunctionspace->dof()->localToGlobal( elt_id, l, c1 ) );

                //N is the index of the RB basis function (i.e fem element)
                //FEM coefficient associated to the global dof "gdof" of the N^th RB element in the basis
                value_type rb_basisij = this->basisValue( N , gdof );

                //coefficient u_i^N * \PHI_ij
                value_type coefficient = u_i*rb_basisij;

                for ( uint16_type q = 0; q < nq; ++q )
                {
                    for ( typename array_type::index i = 0; i < nComponents1; ++i )
                    {
                        //context contains evaluation of FEM basis functions
                        //context.id give access to M_phi in PolynomialSet
                        v[q]( i,0 ) += coefficient*context.id( ldof, i, 0, q );
                    }
                }
            }
        }
    }//end of loop on RB dof

}


template<typename ModelType, typename A0, typename A1, typename A2, typename A3, typename A4>
template<typename Y,  typename Cont>
template<typename Context_t>
void
ReducedBasisSpace<ModelType,A0, A1, A2, A3, A4>::Element<Y,Cont>::grad_( Context_t const & context, grad_array_type& v , mpl::bool_<false> ) const
{

    //LOG( INFO ) << "grad_ with a FEM context";
    if ( !this->areGlobalValuesUpdated() )
        this->updateGlobalValues();

    size_type elt_id = context.eId();
    if ( context.gmContext()->element().mesh()->isSubMeshFrom( this->mesh() ) )
        elt_id = context.gmContext()->element().mesh()->subMeshToMesh( context.eId() );
    if ( context.gmContext()->element().mesh()->isParentMeshOf( this->mesh() ) )
        elt_id = this->mesh()->meshToSubMesh( context.eId() );
    if ( elt_id == invalid_size_type_value )
        return;

    int rb_size = this->size();

    //loop on RB dof
    for(int N=0; N<rb_size; N++)
    {

        value_type u_i = this->operator()( N );

        for ( int l = 0; l < basis_type::nDof; ++l )
        {
            const int ncdof = is_product?nComponents1:1;

            for ( int c1 = 0; c1 < ncdof; ++c1 )
            {
                int ldof = c1*basis_type::nDof+l;
                size_type gdof = boost::get<0>( M_femfunctionspace->dof()->localToGlobal( elt_id, l, c1 ) );

                //N is the index of the RB basis function (i.e fem element)
                value_type rb_basisij = this->basisValue( N , gdof );

                //coefficient u_i^N * \PHI_ij
                value_type coefficient = u_i*rb_basisij;

                for ( size_type q = 0; q < context.xRefs().size2(); ++q )
                {
                    for ( int k = 0; k < nComponents1; ++k )
                    {
                        for ( int j = 0; j < nRealDim; ++j )
                        {
                            v[q]( k,j ) += coefficient*context.grad( ldof, k, j, q );
                        }//j
                    }//k
                }//q
            }//c1
        }//l

    }//N


}


template<typename ModelType, typename A0, typename A1, typename A2, typename A3, typename A4>
template<typename Y,  typename Cont>
template<typename Context_t>
void
ReducedBasisSpace<ModelType,A0, A1, A2, A3, A4>::Element<Y,Cont>::d_( int N, Context_t const & context, id_array_type& v , mpl::bool_<false> ) const
{

    int rb_size = this->size();

    for(int rbN=0; rbN<rb_size; rbN++)
    {
        value_type u_i = this->operator()( rbN );

        for ( int i = 0; i < basis_type::nDof; ++i )
        {
            const int ncdof = is_product?nComponents1:1;

            for ( int c1 = 0; c1 < ncdof; ++c1 )
            {
                size_type ldof = basis_type::nDof*c1 + i;
                size_type gdof = boost::get<0>( M_femfunctionspace->dof()->localToGlobal( context.eId(), i, c1 ) );

                //N is the index of the RB basis function (i.e fem element)
                value_type rb_basisij = this->basisValue( rbN , gdof );

                //coefficient u_i^N * \PHI_ij
                value_type coefficient = u_i*rb_basisij;

                for ( size_type q = 0; q < context.xRefs().size2(); ++q )
                {
                    for ( typename array_type::index i = 0; i < nComponents1; ++i )
                    {
                        v[q]( i,0 ) += coefficient*context.d( ldof, i, N, q );
                    }
                }//q
            }//c1
        }//i

    }//rbN

}




template<typename ModelType, typename A0, typename A1, typename A2, typename A3, typename A4>
template<typename Y,  typename Cont>
void
ReducedBasisSpace<ModelType,A0, A1, A2, A3, A4>::Element<Y,Cont>::idInterpolate( matrix_node_type __ptsReal, id_array_type& v ) const
{
    LOG( INFO ) << "not yet implemented";
}

template<int Order, typename ModelType , typename MeshType>
inline
boost::shared_ptr<ReducedBasisSpace<ModelType,MeshType,bases<Lagrange<Order,Scalar,Continuous>>, Periodicity <NoPeriodicity> >>
                   RbSpacePch(  boost::shared_ptr<ModelType> const& model , boost::shared_ptr<MeshType> const& mesh  )
{
    return ReducedBasisSpace<ModelType,MeshType,bases<Lagrange<Order,Scalar,Continuous>>, Periodicity <NoPeriodicity> >::New( model , mesh );
}

template<int Order, typename ModelType , typename MeshType>
inline
boost::shared_ptr<ReducedBasisSpace<ModelType,MeshType,bases<Lagrange<Order,Vectorial,Continuous>>>>
                   RbSpacePchv(  boost::shared_ptr<ModelType> const& model , boost::shared_ptr<MeshType> const& mesh  )
{
    return ReducedBasisSpace<ModelType,MeshType,bases<Lagrange<Order,Vectorial,Continuous>>>::New( model , mesh );
}


}//namespace Feel

#endif
