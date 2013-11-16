/* -*- mode: c++; coding: utf-8; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; show-trailing-whitespace: t -*- vim:fenc=utf-8:ft=tcl:et:sw=4:ts=4:sts=4
  This file is part of the Feel library

  Copyright (C) 2010 Université de Grenoble 1

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
 * \file doftable.hpp
 * \author Christophe Prud'homme
 */
#ifndef _DOFTABLE_HH
#define _DOFTABLE_HH

#include <set>
#include <map>
#include <unordered_map>
#include <vector>
#include <algorithm>

#include <boost/foreach.hpp>
#include <boost/multi_array.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>
#include <boost/tuple/tuple_io.hpp>
#include <boost/fusion/algorithm/iteration/accumulate.hpp>
#include <boost/bimap.hpp>
#include <boost/bimap/support/lambda.hpp>
#include <boost/bimap/unordered_set_of.hpp>
#include <boost/bimap/multiset_of.hpp>
#include <boost/bimap/set_of.hpp>

#include <Eigen/Core>
#include<Eigen/StdVector>

#include <feel/feelcore/feel.hpp>
#include <feel/feelmesh/filters.hpp>
#include <feel/feelalg/glas.hpp>
#include <feel/feelpoly/mapped.hpp>
#include <feel/feelalg/datamap.hpp>
#include <feel/feeldiscr/dof.hpp>

namespace Feel
{
// import fusion namespace in Feel
namespace fusion = boost::fusion;
namespace bimaps = boost::bimaps;
/**
 * \class DofTable
 * \ingroup SpaceTime
 * \brief Local-to-global Degree of Freedom table
 *
 * \author Christophe Prud'homme
 * \author Goncalo Pena
 */
template<typename MeshType,  typename FEType, typename PeriodicityType>
class DofTable : public DataMap
{
    typedef DataMap super;
public:

    /**
     * mesh type
     */
    typedef MeshType mesh_type;
    typedef FEType fe_type;
    typedef boost::shared_ptr<FEType> fe_ptrtype;


    typedef typename mesh_type::pid_element_const_iterator pid_element_const_iterator;
    typedef typename mesh_type::element_const_iterator element_const_iterator;
    typedef typename mesh_type::element_type element_type;
    typedef typename mesh_type::face_type face_type;
    typedef typename mesh_type::gm_ptrtype gm_ptrtype;
    typedef typename mesh_type::gm_type gm_type;

    typedef typename fe_type::matrix_type matrix_type;
    typedef typename fe_type::value_type value_type;
    typedef typename fe_type::reference_convex_type reference_convex_type;
    typedef typename fe_type::points_type points_type;

    typedef typename reference_convex_type::super convex_type;

    typedef typename element_type::edge_permutation_type edge_permutation_type;
    typedef typename element_type::face_permutation_type face_permutation_type;

    static const uint16_type nOrder = fe_type::nOrder;
    static const uint16_type nDim = mesh_type::nDim;
    static const uint16_type nRealDim = mesh_type::nRealDim;
    static const uint16_type Shape = mesh_type::Shape;
    static const uint16_type nComponents = fe_type::nComponents;
    static const uint16_type nComponents1 = fe_type::nComponents1;
    static const uint16_type nComponents2 = fe_type::nComponents2;


    static const bool is_continuous = fe_type::isContinuous;
    static const bool is_discontinuous_locally = fe_type::continuity_type::is_discontinuous_locally;
    static const bool is_discontinuous_totally = fe_type::continuity_type::is_discontinuous_totally;

    static const bool is_scalar = FEType::is_scalar;
    static const bool is_vectorial = FEType::is_vectorial;
    static const bool is_tensor2 = FEType::is_tensor2;
    static const bool is_modal = FEType::is_modal;
    static const bool is_product = FEType::is_product;

    static const bool is_p0_continuous = ( ( nOrder == 0 ) && is_continuous );

    static const uint16_type nDofPerElement = mpl::if_<mpl::bool_<is_product>, mpl::int_<FEType::nLocalDof*nComponents1>, mpl::int_<FEType::nLocalDof> >::type::value;

    typedef PeriodicityType periodicity_type;
    static const bool is_periodic = periodicity_type::is_periodic;

    //typedef ContinuityType continuity_type;
    typedef typename fe_type::continuity_type continuity_type;

    typedef DofTable<MeshType, FEType, PeriodicityType> self_type;


    /**
     * A global dof is defined by its index in the global table
     * and by its sign.
     **/

        //typedef boost::tuple<size_type, int16_type, bool> global_dof_type;
    typedef Dof global_dof_type;


    /**
     * A global dof from face is defined by
     * -its index in the global table
     * -its sign
     * -bool : periodicity
     * -local number in the element
     **/

    typedef boost::tuple<size_type, int16_type, bool, int16_type> global_dof_fromface_type;


    /**
     * Type for the localToGlobal table.
     */
    //typedef std::unordered_map<int,std::map<int,global_dof_type> > Container;
    //typedef typename std::map<int,global_dof_type>::iterator local_map_iterator;
    typedef boost::bimap<bimaps::unordered_set_of<LocalDof>, bimaps::multiset_of<Dof> > dof_table;
    typedef dof_table::value_type dof_relation;
    typedef std::unordered_map<int,std::map<int,global_dof_fromface_type> > Container_fromface;

    typedef typename std::map<int,global_dof_type> indices_per_element_type;

    typedef typename node<value_type>::type node_type;


    typedef boost::tuple<node_type, size_type, uint16_type > dof_point_type;
    typedef std::vector<dof_point_type> dof_points_type;
    typedef typename std::vector<dof_point_type>::iterator dof_points_iterator;
    typedef typename std::vector<dof_point_type>::const_iterator dof_points_const_iterator;

    /**
     * Tuple that holds a size_type \p elt 1 uint16_type \p l and 1
     * uint16_type ent
     * \p elt shall be an element index
     * \p l shall be the local index of the dof in the element
     * \p ent shall be the entity the dof belongs to (0: vertex, 1: edge, 2: face, 3: volume)
     */
    typedef boost::tuple<size_type, uint16_type, uint16_type, uint16_type> local_dof_type;
    typedef LocalDofSet local_dof_set_type;

    typedef boost::tuple<uint16_type&,size_type&> ref_shift_type;

    /**
     * Type that hold the map between a global dof and the elements
     */
    typedef std::map<size_type, std::list<local_dof_type> >  dof_element_type;

    typedef boost::bimap<size_type,boost::bimaps::multiset_of<size_type> > dof_marker_type;
    typedef typename dof_marker_type::value_type dof2marker;

    typedef typename dof_element_type::iterator dof_iterator;
    typedef typename dof_element_type::const_iterator dof_const_iterator;

    typedef std::list<local_dof_type>::const_iterator ldof_const_iterator;

    typedef boost::tuple<uint16_type,uint16_type,size_type> dof_type;
    typedef std::map<dof_type, size_type> dof_map_type;
    typedef std::map<dof_type, size_type>::iterator dof_map_iterator;
    typedef std::map<dof_type, size_type, size_type>::const_iterator dof_map_const_iterator;

    typedef std::map<size_type, std::set<size_type> > dof_procset_type;
    /**
     * This type is useful to construct the sign map in the modal case
     **/

    typedef ublas::vector<bool> face_sign_info_type;


    typedef Eigen::Matrix<int, nDofPerElement, 1> localglobal_indices_type;

    /**
     * Type for the permutations to be done in the faces
     **/

    typedef ublas::vector<uint16_type> permutation_vector_type;

    typedef boost::tuple<element_type const*, face_type const*> element_face_pair_type;
    typedef std::list<element_face_pair_type> periodic_element_list_type;
    typedef typename periodic_element_list_type::iterator periodic_element_list_iterator;
    typedef typename periodic_element_list_type::const_iterator periodic_element_list_const_iterator;
    typedef boost::tuple<size_type /*element id*/, uint16_type /*lid*/, uint16_type /*c*/, size_type /*gDof*/, uint16_type /*type*/> periodic_dof_type;
    typedef std::multimap<size_type /*gid*/, periodic_dof_type> periodic_dof_map_type;

    DofTable( WorldComm const& _worldComm )
        :
        super( _worldComm )
        {}

    /**
     * @brief The minimal constructor
     *
     * @param _fe reference element
     *
     */
    DofTable( fe_ptrtype const& _fe, periodicity_type const& periodicity, WorldComm const& _worldComm );

    /**
     * copy constructor
     *
     * @param dof2 a dof object instance
     */
    DofTable( const DofTable & dof2 );

    /**
     * @brief  Constructor accepting a mesh as parameter
     *
     *  @param mesh a RegionMesh3D
     *  @param _fe reference element
     */
    DofTable( mesh_type& mesh, fe_ptrtype const& _fe, periodicity_type const& periodicity, WorldComm const& _worldComm );

    ~DofTable()
    {
            M_el_l2g.clear();
            M_face_l2g.clear();
            M_dof_points.clear();
    }
    constexpr size_type nLocalDof( bool per_component = false ) const
    {
        return  (is_product&&!per_component)?(nComponents*(fe_type::nDofPerVolume * element_type::numVolumes +
                                                           fe_type::nDofPerFace * element_type::numGeometricFaces +
                                                           fe_type::nDofPerEdge * element_type::numEdges +
                                                           fe_type::nDofPerVertex * element_type::numVertices)):
            (fe_type::nDofPerVolume * element_type::numVolumes +
             fe_type::nDofPerFace * element_type::numGeometricFaces +
             fe_type::nDofPerEdge * element_type::numEdges +
             fe_type::nDofPerVertex * element_type::numVertices);
    }
    constexpr size_type nLocalDofOnFace( bool per_component = false ) const
    {
        return (is_product&&!per_component)?(nComponents*( face_type::numVertices * fe_type::nDofPerVertex +
                                                          face_type::numEdges * fe_type::nDofPerEdge +
                                                           face_type::numFaces * fe_type::nDofPerFace )):
            ( face_type::numVertices * fe_type::nDofPerVertex +
              face_type::numEdges * fe_type::nDofPerEdge +
              face_type::numFaces * fe_type::nDofPerFace );
    }
    local_dof_set_type const&
    localDofSet( size_type eid ) const
    {
        return M_local_dof_set.update( eid );
    }
    mesh_type* mesh() { return M_mesh; }
    mesh_type* mesh() const { return M_mesh; }

    /**
     * \return the number of dof for faces on the boundary
     */
    uint16_type nDofPerFaceOnBoundary() const
    {
        return M_n_dof_per_face_on_bdy;
    }

    indices_per_element_type  indices( size_type id_el ) const
    {
        indices_per_element_type ind;
        BOOST_FOREACH( LocalDof const& ldof, localDofSet( id_el ) )
        {
            auto it = M_el_l2g.left.find( ldof );
            DCHECK( it != M_el_l2g.left.end() ) << "Invalid element id " << id_el;
            ind[ldof.localDof()] = *it;
        }
        return ind;
    }

    constexpr size_type getIndicesSize() const
    {
        return nLocalDof();
    }
    std::vector<size_type> getIndices( size_type id_el ) const
    {
        std::vector<size_type> ind( nLocalDof() );
        getIndicesSet( id_el, ind );

        return ind;
    }

    std::vector<size_type> getIndices( size_type id_el, mpl::size_t<MESH_ELEMENTS> /**/ ) const
    {
        return getIndices( id_el );
    }

    void getIndicesSet( size_type id_el, std::vector<size_type>& ind ) const
    {
        BOOST_FOREACH( LocalDof const& ldof, this->localDofSet( id_el ) )
        {
            auto it = M_el_l2g.left.find( ldof );
            DCHECK(it != M_el_l2g.left.end() ) << "Invalid element id " << id_el;
            ind[ldof.localDof()] = it->second.index();
        }
    }

    std::vector<size_type> getIndices( size_type id_el, mpl::size_t<MESH_FACES> /**/ ) const
    {
        std::vector<size_type> ind( nLocalDofOnFace() );

        auto eit = M_face_l2g.find( id_el );
        DCHECK( eit != M_face_l2g.end() ) << "Invalid face id " << id_el;
        auto it = eit->second.begin();
        auto en = eit->second.end();
        for( ; it != en; ++ it )
            ind[it->first] = boost::get<0>( it->second );

        return ind;
    }


    void getIndicesSetOnGlobalCluster( size_type id_el, std::vector<size_type>& ind ) const
    {
        BOOST_FOREACH( LocalDof const& ldof, this->localDofSet( id_el ) )
        {
            auto it = M_el_l2g.left.find( ldof );
            DCHECK( it != M_el_l2g.left.end() ) << "Invalid element id " << id_el;
            ind[ldof.localDof()] =this->mapGlobalProcessToGlobalCluster()[ it->second.index() ];

        }
    }

    std::vector<size_type> getIndicesOnGlobalCluster( size_type id_el ) const
    {
        const size_type s = getIndicesSize();
        std::vector<size_type> ind( s );
        getIndicesSetOnGlobalCluster( id_el, ind );
        return ind;
    }


    /**
     * \return the number of processors \c dof belongs to
     */
    size_type dofNProc( size_type dof ) const
    {
        return M_dof_procset.find( dof )->second.size();
    }

    /**
     * \return the set of processors \c dof belongs to
     */
    std::set<size_type> dofProcSet( size_type dof ) const
    {
        return M_dof_procset.find( dof )->second;
    }

    /**
     * @return the coordinates of the nodal dofs associated with the
     * element \p el
     */
    const dof_point_type& dofPoint( size_type i ) const
    {
        return M_dof_points[i];
    }

    /**
     * @return an iterator at the beginning of dof points
     */
    dof_points_const_iterator dofPointBegin() const
    {
        return M_dof_points.begin();
    }

    /**
     * @return an iterator at the beginning of dof points
     */
    dof_points_iterator dofPointBegin()
    {
        return M_dof_points.begin();
    }

    /**
     * @return an iterator at the end of dof points
     */
    dof_points_const_iterator dofPointEnd() const
    {
        return M_dof_points.end();
    }

    /**
     * @return an iterator at the end of dof points
     */
    dof_points_iterator dofPointEnd()
    {
        return M_dof_points.end();
    }

    periodic_element_list_const_iterator beginPeriodicElements() const { return periodic_elements.begin(); }
    periodic_element_list_const_iterator endPeriodicElements() const { return periodic_elements.end(); }

    /**
     * insted of creating the dof indices on the fly, get them from a
     * vector. The situation typically arises when we want to have dof
     * correspondance between two spaces
     *
     * \see OperatorLagrangeP1
     */
    void setDofIndices( std::vector<Dof> const& dof )
    {
        M_dof_indices.resize( dof.size() );
        std::copy( dof.begin(), dof.end(), M_dof_indices.begin() );

        if ( dof.empty() )
            return ;

#if 1
        std::set<size_type> eltid;
        std::set<size_type> dofs;

        BOOST_FOREACH( Dof thedof,  dof )
        {
            eltid.insert( boost::get<0>( thedof ) );
            dofs.insert( boost::get<2>( thedof ) );
        }
#endif

        BOOST_FOREACH( Dof thedof,  dof )
        {
            M_el_l2g.insert( dof_relation( LocalDof( boost::get<0>( thedof ), boost::get<1>( thedof ) ),
                                            Dof( boost::get<2>( thedof ), 0, false ) ) );

        }
        int processor = this->worldComm().localRank();

        this->M_first_df[processor] = 0;
        this->M_last_df[processor] = dofs.size()-1;
        this->M_n_dofs = dofs.size();

        this->M_n_localWithGhost_df[processor] = this->M_last_df[processor] - this->M_first_df[processor] + 1;
        this->M_n_localWithoutGhost_df[processor]=this->M_n_localWithGhost_df[processor];
        this->M_first_df_globalcluster[processor]=this->M_first_df[processor];
        this->M_last_df_globalcluster[processor]=this->M_last_df[processor];
    }

    /**
     * \return the dof index
     */
    size_type dofIndex( size_type dof ) const
    {
        return dof;
#if 0

        if ( M_dof_indices.empty() )
            return dof;

        FEELPP_ASSERT( dof < M_dof_indices.size() )( dof )( M_dof_indices.size() ).warn( "invalid dof index" );
        return M_dof_indices[dof];
#endif
    }

    /**
     * \return the local to global indices
     */
    localglobal_indices_type const& localToGlobalIndices( size_type ElId )
    {
        return M_locglob_indices[ElId];
    }

    /**
     * \return the local to global indices on global cluster
     */
    localglobal_indices_type const& localToGlobalIndicesOnCluster( size_type ElId )
    {
        return M_locglobOnCluster_indices[ElId];
    }

    /**
     * \return the signs of the global dof (=1 in nodal case, +-1 in modal case)
     */
    localglobal_indices_type const& localToGlobalSigns( size_type ElId )
    {
        return M_locglob_signs[ElId];
    }

    /**
     * \return the specified entries of the localToGlobal table
     *
     * \param ElId the element ID
     * \param localNode the local DOF numbering (starting from 1)
     * \param c the component index, default is 0-th component
     *
     * \return the global numbering of a DOF, given an element and the local numbering
     */
    size_type localToGlobalId( const size_type ElId,
                               const uint16_type id ) const
    {
        auto it = M_el_l2g.left.find( LocalDof(ElId, id ) );
        DCHECK( it != M_el_l2g.left.end() ) << "Invalid dof entry ( " << ElId << ", " << id << ")";
        DCHECK( it->second.index() < nDof() ) << "Invalid Dof Entry: " << it->second.index() << " > " << this->nDof();
        return it->second.index();
    }

    /**
     * \return the specified entries of the globalToLocal table
     *
     * \param DofId the Dof ID
     *
     * \return the element id and local dof id
     */
    LocalDof const& globalToLocal( size_type dof )  const
    {
        auto it = M_el_l2g.right.find( Dof( dof ) );
        DCHECK( it != M_el_l2g.right.end() ) << "Invalid global dof entry ( " << dof << ")";
        return it->second;
    }

    uint16_type localDofId( uint16_type const lid, uint16_type const c = 0 ) const
        {
            return fe_type::nLocalDof * c  + lid;
        }
    global_dof_type const& localToGlobal( const size_type ElId,
                                   const uint16_type localNode,
                                   const uint16_type c = 0 ) const
    {
        auto it = M_el_l2g.left.find( LocalDof(ElId,fe_type::nLocalDof * c  + localNode ) );
        DCHECK( it != M_el_l2g.left.end() ) << "Invalid dof entry ( " << ElId << ", " << fe_type::nLocalDof * c  + localNode << ")";
        //DCHECK( it->second.index() < nDof() && nDof() > 0 ) << "Invalid Dof Entry: " << it->second.index() << " > " << this->nDof();
        return it->second;
    }

    global_dof_type localToGlobalOnCluster( const size_type ElId,
                                            const uint16_type localNode,
                                            const uint16_type c = 0 ) const
    {
        Dof resloc = M_el_l2g.left.find( LocalDof( ElId, fe_type::nLocalDof * c  + localNode ) )->second;
        resloc.setIndex( this->mapGlobalProcessToGlobalCluster()[resloc.index()] );
        return resloc;
    }

    global_dof_fromface_type const& faceLocalToGlobal( const size_type ElId,
            const uint16_type localNode,
            const uint16_type c = 0 ) const
    {
        const size_type nDofF = nLocalDofOnFace( true );
        return M_face_l2g.find( ElId )->second.find( nDofF*c+localNode )->second;
    }

    struct element_access
    {
        element_access( DofTable const& __d )
            :
            M_d( __d )
        {}
        global_dof_type const& operator()( size_type __id, uint16_type __loc, uint16_type c = 0 ) const
        {
            return M_d.M_el_l2g.left.find( LocalDof(__id, M_d.nLocalDof(true) * c+ __loc ) )->second;
        }
        uint16_type localDofInElement( size_type __id, uint16_type __loc, uint16_type c = 0 ) const
        {
            return __loc;
        }
        DofTable const& M_d;
    };
    friend struct element_access;

    struct face_access
    {

        face_access( DofTable const& __d )
            :
            M_d( __d )
        {}
        global_dof_fromface_type operator()( size_type __id, uint16_type __loc, uint16_type c = 0 ) const
        {
            return M_d.M_face_l2g.find( __id)->second.find(M_d.nLocalDofOnFace( true )*c+__loc)->second;
        }

        uint16_type localDofInElement( size_type __id, uint16_type __loc, uint16_type c = 0 ) const
        {
            return boost::get<3>( M_d.M_face_l2g.find( __id )->second.find( M_d.nLocalDofOnFace( true )*c+__loc)->second );
        }

        DofTable const& M_d;
    };
    friend struct face_access;

    /**
     * @brief local to global mapping
     */
    template<typename Elem>
    typename mpl::if_<mpl::equal_to<mpl::int_<Elem::nDim>,mpl::int_<nDim> >,
             global_dof_type,
             global_dof_fromface_type >::type
             localToGlobal( Elem const& El, const uint16_type localNode, const uint16_type c = 0 ) const
    {
        typedef typename mpl::if_<mpl::equal_to<mpl::int_<Elem::nDim>,mpl::int_<nDim> >,mpl::identity<element_access>,mpl::identity<face_access> >::type::type access_type;
        //DVLOG(2) << "dof:(" << El.id() << ", " << localNode << ")= "
        //<< access_type(*this)( El.id(), localNode, c ) << "\n";
        return access_type( *this )( El.id(), localNode, c );
    }

    template<typename Elem>
    uint16_type
    localDofInElement( Elem const& El, const uint16_type localNode, const uint16_type c = 0 ) const
    {
        typedef typename mpl::if_<mpl::equal_to<mpl::int_<Elem::nDim>,mpl::int_<nDim> >,mpl::identity<element_access>,mpl::identity<face_access> >::type::type access_type;

        return ( access_type( *this ) ).localDofInElement( El.id(), localNode, c );
    }

    /**
     * Number of elements in mesh
     */
    size_type numElements() const
    {
        return M_n_el;
    }

    /**
     * Number of local vertices (in an element)
     */
    uint16_type numLocalVertices() const
    {
        return fe_type::numVertices;
    }

    /**
     * Number of local edges (in an element)
     */
    uint16_type numLocalEdges() const
    {
        return fe_type::numEdges;
    }

    /**
     * Number of local faces (in an element)
     */
    uint16_type numLocalFaces() const
    {
        return fe_type::numFaces;
    }

    /**
     * show some information about the dof instance
     */
    void showMe() const;

    void dump() const
    {
#if 0

        for ( size_type __i = 0; __i < M_face_l2g.nrows(); ++__i )
        {
            for ( size_type __l = 0; __l < M_face_l2g.ncols(); ++__l )
            {
                std::cout << "face " << __i << " local " << __l
                          << " to global " << M_face_l2g[ __i][ __l ] << "\n";
            }
        }

#endif // 0
    }

    /**
     * set element to done
     */
    void setElementDone( size_type elt )
    {
        M_elt_done[elt] = true;
    }
    bool isElementDone( size_type elt, int c = 0 )
    {
        bool done = true;
        BOOST_FOREACH( LocalDof const& local_dof, this->localDofSet( elt ) )
        {
            auto it = M_el_l2g.left.find( local_dof );
            if ( it == M_el_l2g.left.end() )
                return false;
        }
        return done;
    }
    void addDofFromElement( element_type const& __elt,
                            size_type& next_free_dof,
                            size_type processor = 0,
                            size_type shift = 0 )
    {

        size_type nldof = this->nLocalDof( true );

        if ( !this->isElementDone( __elt.id() ) )
        {
            DVLOG(3) << "adding dof from element " << __elt.id() << "\n";
            size_type gdofcount = shift;
            DVLOG(3) << "next_free_dof " << next_free_dof  << "\n";
            DVLOG(3) << "current dof " << dofIndex( next_free_dof ) << "\n";


            /*
             * Only in the continuous , we need to have the ordering [vertex,edge,face,volume]
             */
            if ( is_continuous || is_discontinuous_locally )
            {

                /* idem as above but for local element
                   numbering except that it is
                   reset to 0 after each element */
                uint16_type ldofcount = 0;

                /* pack the shifts into a tuple */
                boost::tuple<uint16_type&,size_type&> shifts = boost::make_tuple( boost::ref( ldofcount ),
                        boost::ref( gdofcount ) );

                /* \warning: the order of function calls is
                   crucial here we order the degrees of freedom
                   wrt the topological entities of the mesh
                   elements from lowest dimension (vertex) to
                   highest dimension (element)
                */
                addVertexDof( __elt, processor, next_free_dof, shifts  );
                addEdgeDof( __elt, processor, next_free_dof, shifts );
                addFaceDof( __elt, processor, next_free_dof, shifts );
                addVolumeDof( __elt, processor, next_free_dof, shifts );
            }

            else
            {

                size_type ie = __elt.id();

                const int ncdof = is_product?nComponents:1;

                for ( uint16_type l = 0; l < nldof; ++l )
                {
                    for ( int c = 0; c < ncdof; ++c, ++next_free_dof )
                    {
                        M_el_l2g.insert( dof_relation( LocalDof( ie, fe_type::nLocalDof*c + l ),
                                                        Dof( ( dofIndex( next_free_dof ) ) , 1, false ) ) );
                    }
                }
            }
        }

        else
        {
            DVLOG(3) << "element " << __elt.id() << "has already been taken care of\n";
        }
    }
    /**
     * \return the local to global map
     */
    std::map<size_type, std::pair<size_type, size_type> >
    localToGlobalMap() const
    {
        return M_local2global;
    }

    /**
     * local(on processor) dof to global dof
     */
    size_type localToGlobal( size_type l ) const
    {
        return M_local2global.find( l )->second.first;
    }

    /**
     * Add a new periodic dof to the dof map and to the list of periodic dof \p
     * periodic_dof for a given tag \p tag
     *
     * \arg __elt geometric element
     * \arg __face  face of the element that holds the periodic dof
     * \arg next_free_dof integer that will get possibilty incremented and holds the current number of dof
     * \arg periodic_dof table of periodic dof
     * \arg tag the boundary tag associated with the periodic dof
     */
    void addVertexPeriodicDof( element_type const& __elt,
                               face_type const& __face,
                               size_type& next_free_dof,
                               std::map<size_type,periodic_dof_map_type>& periodic_dof,
                               size_type tag )
    {
        addVertexPeriodicDof( __elt, __face, next_free_dof, periodic_dof, tag, mpl::bool_<(fe_type::nDofPerVertex>0)>() );
    }
    void addVertexPeriodicDof( element_type const& __elt,face_type const& __face,size_type& next_free_dof,std::map<size_type,periodic_dof_map_type>& periodic_dof,size_type tag, mpl::bool_<false> ) {}
    void addVertexPeriodicDof( element_type const& __elt,face_type const& __face,size_type& next_free_dof,std::map<size_type,periodic_dof_map_type>& periodic_dof,size_type tag, mpl::bool_<true> );

    void addEdgePeriodicDof( element_type const& __elt,
                             face_type const& __face,
                             size_type& next_free_dof,
                             std::map<size_type,periodic_dof_map_type>& periodic_dof,
                             size_type tag )
    {
        static const bool cond = fe_type::nDofPerEdge > 0;
        addEdgePeriodicDof( __elt, __face, next_free_dof, periodic_dof, tag , mpl::bool_<cond>(), mpl::int_<nDim>() );
    }
    void addEdgePeriodicDof( element_type const& __elt,face_type const& __face,size_type& next_free_dof,std::map<size_type,periodic_dof_map_type>& periodic_dof, size_type tag, mpl::bool_<false>, mpl::int_<1> ) {}
    void addEdgePeriodicDof( element_type const& __elt,face_type const& __face,size_type& next_free_dof,std::map<size_type,periodic_dof_map_type>& periodic_dof, size_type tag, mpl::bool_<false>, mpl::int_<2> ) {}
    void addEdgePeriodicDof( element_type const& __elt,face_type const& __face,size_type& next_free_dof,std::map<size_type,periodic_dof_map_type>& periodic_dof, size_type tag, mpl::bool_<false>, mpl::int_<3> ) {}
    void addEdgePeriodicDof( element_type const& __elt,face_type const& __face,size_type& next_free_dof,std::map<size_type,periodic_dof_map_type>& periodic_dof, size_type tag, mpl::bool_<true>, mpl::int_<1> ) {}
    void addEdgePeriodicDof( element_type const& __elt,face_type const& __face,size_type& next_free_dof,std::map<size_type,periodic_dof_map_type>& periodic_dof, size_type tag, mpl::bool_<true>, mpl::int_<2> );
    void addEdgePeriodicDof( element_type const& __elt,face_type const& __face,size_type& next_free_dof,std::map<size_type,periodic_dof_map_type>& periodic_dof, size_type tag, mpl::bool_<true>, mpl::int_<3> );


    void addFacePeriodicDof( element_type const& __elt,face_type const& __face,size_type& next_free_dof,std::map<size_type,periodic_dof_map_type>& periodic_dof,size_type tag )
    {
        static const bool cond = fe_type::nDofPerFace > 0;
        addFacePeriodicDof( __elt, __face, next_free_dof, periodic_dof, tag, mpl::bool_<cond>() );
    }
    void addFacePeriodicDof( element_type const& __elt,face_type const& __face,size_type& next_free_dof,std::map<size_type,periodic_dof_map_type>& periodic_dof,size_type tag, mpl::bool_<false> ) {}
    void addFacePeriodicDof( element_type const& __elt,face_type const& __face,size_type& next_free_dof,std::map<size_type,periodic_dof_map_type>& periodic_dof,size_type tag, mpl::bool_<true> );

    /**
     * Initialize the dof map table
     */
    void initDofMap( mesh_type& M );

    /**
     * build the dof map
     */
    void build( mesh_type* M )
    {
        this->build( *M );
    }

    /**
     * build the dof map
     */
    void build( boost::shared_ptr<mesh_type>& M )
    {
        this->build( *M );
    }

    /**
     * build the dof map
     */
    void build( mesh_type& M )
    {
        M_mesh = boost::addressof( M );

        M_elt_done.resize( M.numElements() );
        std::fill( M_elt_done.begin(), M_elt_done.end(), false );

        VLOG(2) << "[Dof::build] initDofMap\n";
        this->initDofMap( M );

        VLOG(2) << "[Dof::build] start building dof map\n";
        size_type start_next_free_dof = 0;
        VLOG(2) << "[Dof::build] start_next_free_dof = " << start_next_free_dof << "\n";

        if ( is_periodic )
        {
            VLOG(2) << "[build] call buildPeriodicDofMap()\n";
            start_next_free_dof = this->buildPeriodicDofMap( M );
            VLOG(2) << "[Dof::build] start_next_free_dof(after periodic) = " << start_next_free_dof << "\n";
        }

        if ( is_discontinuous_locally )
        {
            VLOG(2) << "[build] call buildLocallyDiscontinuousDofMap()\n";
            start_next_free_dof = this->buildLocallyDiscontinuousDofMap( M, start_next_free_dof );
            VLOG(2) << "[Dof::build] start_next_free_dof(after local discontinuities) = " << start_next_free_dof << "\n";
        }

        VLOG(2) << "[build] call buildDofMap()\n";
        this->buildDofMap( M, start_next_free_dof );
        //std::cout << "[build] callFINISH buildDofMap() with god rank " << this->worldComm().godRank() <<"\n";

#if !defined(NDEBUG)
        VLOG(2) << "[build] check that all elements dof were assigned()\n";
        element_const_iterator fit, fen;
        boost::tie( fit, fen ) = M.elementsRange();
        std::vector<boost::tuple<size_type,uint16_type,size_type> > em;

        for ( ; fit != fen; ++fit )
        {
            const int ncdof = is_product?nComponents:1;

            for ( uint16_type c = 0; c < ncdof; ++c )
                if ( !this->isElementDone( fit->id(), c ) )
                {
                    em.push_back( boost::make_tuple( fit->id(), c, fit->marker().value() ) );
                }
        }

        if ( !em.empty() )
        {
            VLOG(2) << "[build] some element dof were not assigned\n";

            for ( size_type i = 0; i < em.size(); ++i )
            {
                VLOG(3) << " - element " << boost::get<0>( em[i] ) << " c=" << boost::get<1>( em[i] )
                        << " m=" << boost::get<2>( em[i] ) << "\n";
            }
        }

        else
        {
            VLOG(2) << "[build] check that all elements dof were assigned: OK\n";
        }

#endif // NDEBUG
        VLOG(2) << "[Dof::build] n_dof = " << this->nLocalDofWithGhost() << "\n";

        VLOG(2) << "[build] call buildBoundaryDofMap()\n";
        this->buildBoundaryDofMap( M );


        // multi process
        if ( this->worldComm().localSize()>1 )
            {
                auto it_nlocDof = this->M_n_localWithGhost_df.begin();
                auto const en_nlocDof = this->M_n_localWithGhost_df.end();
                bool isP0continuous = true;
                for ( ; it_nlocDof!=en_nlocDof && isP0continuous ; ++it_nlocDof )
                    isP0continuous = isP0continuous && (*it_nlocDof==1);

                if ( !isP0continuous )//this->M_n_dofs>1 )
                    {
#if defined(FEELPP_ENABLE_MPI_MODE)
                        VLOG(2) << "[build] call buildGhostDofMap () with god rank " << this->worldComm().godRank()  << "\n";
                        this->buildGhostDofMap( M );
                        VLOG(2) << "[build] callFINISH buildGhostDofMap () with god rank " << this->worldComm().godRank()  << "\n";
#else
                        std::cerr << "ERROR : FEELPP_ENABLE_MPI_MODE is OFF" << std::endl;
                        //throw std::logic_error( "ERROR : FEELPP_ENABLE_MPI_MODE is OFF" );
#endif
                    }
                else
                    {
                        for ( int proc=0; proc<this->worldComm().globalSize(); ++proc )
                        {
                            if (proc==0)
                            {
                                this->M_n_localWithoutGhost_df[proc] = 1;
                                this->M_first_df_globalcluster[proc] = 0;
                                this->M_last_df_globalcluster[proc] = 0;
                            }
                            else
                            {
                                this->M_n_localWithoutGhost_df[proc] = 0;
                                this->M_first_df_globalcluster[proc] = 25;// 0;
                                this->M_last_df_globalcluster[proc] = 25; //0;
                            }
                            this->M_n_localWithGhost_df[proc] = 1;
                        }

                        if (this->worldComm().globalRank()==0)
                        {
                            this->M_mapGlobalClusterToGlobalProcess.resize( 1 );
                            this->M_mapGlobalClusterToGlobalProcess[0]=0;
                        }
                        else
                        {
                            this->M_mapGlobalClusterToGlobalProcess.resize( 0 );
                        }

                        this->M_mapGlobalProcessToGlobalCluster.resize( 1 );
                        this->M_mapGlobalProcessToGlobalCluster[0]=0;
                    }
        }

        else
        {
#if defined(FEELPP_ENABLE_MPI_MODE)
            // in sequential : identity map
            const size_type s = this->M_n_localWithGhost_df[this->comm().rank()];
            this->M_mapGlobalProcessToGlobalCluster.resize( s );
            this->M_mapGlobalClusterToGlobalProcess.resize( s );

            for ( size_type i=0; i<s ; ++i ) this->M_mapGlobalProcessToGlobalCluster[i]=i;

            for ( size_type i=0; i<s ; ++i ) this->M_mapGlobalClusterToGlobalProcess[i]=i;

#endif
        }

        VLOG(2) << "[Dof::build] done building the map\n";
    }

    /**
     * build dof map associated to the periodic dof, must be called
     * before buildDofMap
     */
    size_type buildPeriodicDofMap( mesh_type& M );

    /**
     * build dof associated to local discontinuities
     */
    size_type buildLocallyDiscontinuousDofMap( mesh_type& M, size_type start_next_free_dof );

    /**
     * @brief Build the localToGlobal table
     *
     *  \param mesh mesh
     */
    void buildDofMap( mesh_type& mesh, size_type start_next_free_dof = 0 );

    /**
     * @brief Build the localToGlobal table for the boundary
     *
     * \param mesh A mesh
     */
    void buildBoundaryDofMap( mesh_type& mesh );

#if defined(FEELPP_ENABLE_MPI_MODE)
    /**
     * build the GlobalProcessToGlobalClusterDof table
     */
    void buildGhostDofMap( mesh_type& mesh );

    /**
     * subroutines
     */
    void buildGhostInterProcessDofMap( mesh_type& mesh,
                                       std::map<size_type,boost::tuple<size_type,size_type> > & mapInterProcessDof );
    void buildGlobalProcessToGlobalClusterDofMapContinuous( mesh_type& mesh );
    void buildGlobalProcessToGlobalClusterDofMapContinuousActifDof( mesh_type& mesh,
                                                                    std::vector< std::map<size_type,std::set<std::vector<size_type> > > > & listToSend,
                                                                    std::set<int> & procRecvData );
    void buildGlobalProcessToGlobalClusterDofMapContinuousGhostDofBlockingComm( mesh_type& mesh,
                                                                                std::vector< std::map<size_type,std::set<std::vector<size_type> > > > const& listToSend,
                                                                                std::set<int> const& procRecvData );
    void buildGlobalProcessToGlobalClusterDofMapContinuousGhostDofNonBlockingComm( mesh_type& mesh,
                                                                                   std::vector< std::map<size_type,std::set<std::vector<size_type> > > > const& listToSend,
        std::set<int> const& procRecvData );
    void buildGlobalProcessToGlobalClusterDofMapDiscontinuous();

    void buildGhostInterProcessDofMapInit( mesh_type& mesh,
                                           std::vector< std::map<size_type,std::set<boost::tuple<size_type,uint16_type> > > > & listToSend );
    boost::tuple<bool, std::vector< std::map<size_type,std::set<boost::tuple<size_type,uint16_type> > > > >
    buildGhostInterProcessDofMapRecursive( mesh_type& mesh,
                                           std::vector< std::map<size_type,std::set<boost::tuple<size_type,uint16_type> > > > const& listToSend,
                                           std::map<size_type,boost::tuple<size_type,size_type> > & mapInterProcessDof,
                                           std::vector< std::set<size_type > > & memoryFace );

    void buildDofNotPresent( std::map<size_type,boost::tuple<size_type,size_type> > const & mapInterProcessDof,
                             std::map<size_type,boost::tuple<size_type,size_type> > & setInterProcessDofNotPresent );

    void buildGlobalProcessToGlobalClusterDofMap( mesh_type& mesh,
            std::map<size_type,boost::tuple<size_type,size_type> > const& setInterProcessDofNotPresent );
    void updateGhostGlobalDof( std::map<size_type,boost::tuple<size_type,size_type> > const& setInterProcessDofNotPresent );

#endif
    /**
     * \return the dictionnary for the global dof
     */
    dof_map_type const& mapGDof() const
    {
        return map_gdof;
    }

    /**
     * \return the dictionnary for the global dof
     */
    dof_map_type& mapGDof()
    {
        return map_gdof;
    }

    /**
     * clear the dictionnary
     */
    void clearMapGDof()
    {
        map_gdof.clear();
    }

    /**
     * set the dictionnary for the dictionary of the global dof
     */
    void setMapGDof( dof_map_type const& mapdof )
    {
        map_gdof = mapdof;
    }

    typename dof_marker_type::right_range_type
    markerToDof( boost::any const& marker )
        {
            using namespace boost::bimaps;
            int id = M_mesh->markerId( marker );
            return M_dof_marker.right.range( id <= _key, _key<id+1 );
        }

    typename dof_marker_type::right_range_type
    markerToDofLessThan( boost::any const& marker )
        {
            using namespace boost::bimaps;
            int id = M_mesh->markerId( marker );
            return M_dof_marker.right.range( unbounded, _key<id );
        }
    typename dof_marker_type::right_range_type
    markerToDofGreaterThan( boost::any const& marker )
        {
            using namespace boost::bimaps;
            int id = M_mesh->markerId( marker );
            return M_dof_marker.right.range( id<_key, unbounded );
        }

    void printDofMarker(std::string const& filename )
        {
            // std::ofstream ofs( filename.c_str() );
            // BOOST_FOREACH( auto dof, _M_dof_marker )
            // {
            //     //ofs << dof.first << " " << dof.second << "\n";
            // }
            std::ofstream ofs( filename.c_str() );
            BOOST_FOREACH( auto dofleft, M_dof_marker.left )
            {
                ofs << dofleft.first << " " << dofleft.second << "\n";
            }
        }
    /**
     * The dof are ordered such that they are contiguous per element
     * and components. This way an extraction of the dof indices in
     * one element allows to extract a view of the corresponding
     * coefficient in a given basis which is then very helpful for
     * interpolation for example.
     *
     * \param ie index of the element
     * \param lc_dof index of the dof in the element
     * \param lc local index of the entity associated with the dof in the element
     * \param gDof global dof index
     * \param pDof dof index in the processor
     *
     * \return the index of the next free dof in the processor
     */
    bool insertDof( size_type ie,
                    uint16_type l_dof,
                    uint16_type lc,
                    dof_type gDof,
                    uint16_type processor,
                    size_type& pDof,
                    int32_type sign = 1,
                    bool is_dof_periodic = false,
                    size_type shift = 0,
                    Marker1 const& marker = Marker1( 0 ) )
        {
            bool res = true;
            const int ncdof = is_product?nComponents:1;

            for ( int c = 0; c < ncdof; ++c )
            {
                gDof.template get<1>() = c;
                uint16_type lc_dof = fe_type::nLocalDof*c+l_dof;
                Feel::detail::ignore_unused_variable_warning( lc );
                dof_map_iterator itdof = map_gdof.find( gDof );
                dof_map_iterator endof = map_gdof.end();
                bool __inserted = false;

                if ( itdof == endof )
                {
                    DVLOG(4) << "[dof] dof (" << gDof.template get<0>() << "," << gDof.template get<1>() << "," << gDof.template get<2>() << ") not yet inserted in map\n";
                    boost::tie( itdof, __inserted ) = map_gdof.insert( std::make_pair( gDof, dofIndex( pDof ) ) );

                    pDof += 1;

                    FEELPP_ASSERT( __inserted == true )( ie )( lc_dof )
                        ( gDof.template get<0>() )( gDof.template get<1>() )( gDof.template get<2>() )
                        ( processor )( itdof->second ).error( "dof should have been inserted" );
                }

                else
                {
                    DVLOG(4) << "[dof] dof (" << gDof.template get<0>() << ","
                             << gDof.template get<1>()
                             << "," << gDof.template get<2>()
                             << ") already inserted in map with dof_id = " << itdof->second << "\n";
                }

#if !defined( NDEBUG )
                DVLOG(4) << "global dof = " << itdof->second
                         << " local dof = " << fe_type::nLocalDof*itdof->first.template get<1>() + lc_dof
                         << " element = " << ie
                         << " entity = " << itdof->first.template get<0>()
                         << " component = " << itdof->first.template get<1>()
                         << " index = " << itdof->first.template get<2>() << "\n";
#endif
                auto eit = M_el_l2g.left.find(  LocalDof(ie,lc_dof) );
                // make sure that no already created dof is overwritten here (may be done alsewhere)
                if ( eit == M_el_l2g.left.end() )
                {
                    DCHECK( itdof->first == gDof ) << "very bad logical error in insertDof";

                    DCHECK( lc_dof >= fe_type::nLocalDof*itdof->first.template get<1>() &&
                            lc_dof < fe_type::nLocalDof*( itdof->first.template get<1>()+1 ) )
                        << "invalid local dof index"
                        <<  lc_dof << ", " << fe_type::nLocalDof*itdof->first.template get<1>();


                    // add processor to the set of processors this dof belongs to
                    M_dof_procset[ itdof->second+shift ].insert( processor );

                    auto res = M_el_l2g.insert( dof_relation( LocalDof( ie, lc_dof ),
                                                               Dof( itdof->second+shift, sign, is_dof_periodic, 0, 0, marker.value() ) ) );
                    DCHECK( res.second ) << "global dof " << itdof->second+shift << " not inserted in local dof (" <<
                        ie << "," << lc_dof << ")";
                    M_dof_marker.insert( dof2marker( itdof->second+shift,  marker.value() ) );


#if !defined(NDEBUG)
                    M_dof2elt[itdof->second+shift].push_back( boost::make_tuple( ie, lc_dof, lc, itdof->first.template get<0>() ) );
#endif
#if 0
                    M_dof_view.insert( Dof( itdof->second+shift,      // global index
                                            sign,                     // sign
                                            itdof->first.template get<0>(),    // entity type
                                            false,                    // is on boundary ?
                                            0                         // marker
                                           ) );
#endif
#if 0
                    for ( index i2 = 0; i2 < nLocalDof(); ++i2 )
                        VLOG(1) << "dof table( " << ie << ", " << lc  << ")=" << M_el_l2g.left.find(LocalDof(ie,i2))->second.index() << "\n";

#endif
                }
                else
                {
                    size_type _dof = M_el_l2g.left.find(LocalDof(ie,lc_dof))->second.index();
#if 0
                    CHECK(  M_dof_marker[_dof] == marker.value() ) << "Invalid dof marker, element id: " <<  ie
                                                                    << ", local dof id: " << lc_dof
                                                                    << ", global dof id: "<< _dof
                                                                    << ", dof marker: " <<  M_dof_marker[_dof]
                                                                    << ", marker: " << marker.value() << "\n";
#endif
                }

                res = res && ( __inserted || ( ( M_el_l2g.left.find(LocalDof(ie,lc_dof)) != M_el_l2g.left.end() ) && shift ) );
            }

            return res;
        }

    /**
     * rebuild dof points
     */
    void rebuildDofPoints( mesh_type& M )
    {
        M_dof_points.clear();
        this->generateDofPoints(M);
    }

    /**
     * build point id to dof id relationship
     */
    std::pair<std::map<size_type,size_type>,std::map<size_type,size_type> >
    pointIdToDofRelation(std::string fname="") const;
private:

    void addVertexDof( element_type const& __elt, uint16_type processor, size_type& next_free_dof,
                       ref_shift_type& shifts  )
    {
        addVertexDof( __elt, processor, next_free_dof, shifts, mpl::bool_<(fe_type::nDofPerVertex>0)>() );
    }
    void addVertexDof( element_type const& /*M*/, uint16_type /*processor*/,  size_type& /*next_free_dof*/,
                       ref_shift_type& /*shifts*/, mpl::bool_<false> )
    {}
    void addVertexDof( element_type const& __elt, uint16_type processor, size_type& next_free_dof,
                       ref_shift_type& shifts, mpl::bool_<true> )
    {
        uint16_type local_shift;
        size_type global_shift;
        boost::tie( local_shift, global_shift ) = shifts;


        size_type ie = __elt.id();

        uint16_type lc = local_shift;

        for ( uint16_type i = 0; i < element_type::numVertices; ++i )
        {
            for ( uint16_type l = 0; l < fe_type::nDofPerVertex; ++l, ++lc )
            {
                //const size_type gDof = global_shift + ( __elt.point( i ).id() ) * fe_type::nDofPerVertex + l;
                const size_type gDof = ( __elt.point( i ).id() ) * fe_type::nDofPerVertex + l;
                this->insertDof( ie, lc, i, boost::make_tuple( 0, 0, gDof ),
                                 processor, next_free_dof, 1, false, global_shift, __elt.point( i ).marker() );
            }
        }

        // update shifts
        shifts.template get<0>() = lc;

#if !defined(NDEBUG)
        DVLOG(4) << "[Dof::updateVolumeDof(addVertexDof] vertex proc" << processor << " next_free_dof = " << next_free_dof << "\n";
#endif
    }
    void addEdgeDof( element_type const& __elt, uint16_type processor, size_type& next_free_dof,
                     ref_shift_type& shifts )
    {
        static const bool cond = fe_type::nDofPerEdge > 0;
        return addEdgeDof( __elt,
                           processor,
                           next_free_dof,
                           shifts,
                           mpl::int_<fe_type::nDim>(),
                           mpl::bool_<cond>() );
    }
    void addEdgeDof( element_type const& /*M*/, uint16_type /*processor*/, size_type& /*next_free_dof*/,
                     ref_shift_type& /*shifts*/, mpl::int_<1>, mpl::bool_<false> )
    {}

    void addEdgeDof( element_type const& __elt, uint16_type processor, size_type& next_free_dof,
                     ref_shift_type& shifts, mpl::int_<1>, mpl::bool_<true> )
    {
        uint16_type local_shift;
        size_type global_shift;
        boost::tie( local_shift, global_shift ) = shifts;

        size_type ie = __elt.id();
        uint16_type lc = local_shift;

        for ( uint16_type l = 0; l < fe_type::nDofPerEdge; ++l, ++lc )
        {
            const size_type gDof = is_p0_continuous? l:ie * fe_type::nDofPerEdge + l;
            this->insertDof( ie, lc, l, boost::make_tuple( 1, 0, gDof ), processor, next_free_dof, 1, false, global_shift, __elt.marker() );
        }

        // update shifts
        shifts.template get<0>() = lc;
#if !defined(NDEBUG)
        DVLOG(4) << "[Dof::addEdgeDof(1)] element proc" << processor << " next_free_dof = " << next_free_dof << "\n";
#endif
    }
    void addEdgeDof( element_type const& /*__elt*/, uint16_type /*processor*/, size_type& /*next_free_dof*/,
                     ref_shift_type& /*shifts*/, mpl::int_<2>, mpl::bool_<false> )
    {}
    void addEdgeDof( element_type const& __elt, uint16_type processor, size_type& next_free_dof,
                     ref_shift_type& shifts, mpl::int_<2>, mpl::bool_<true> )
    {
        uint16_type local_shift;
        size_type global_shift;
        boost::tie( local_shift, global_shift ) = shifts;

        size_type ie = __elt.id();
        uint16_type lc = local_shift;

        /** The boundary dofs are constructed in the same way if the basis is modal **/

        for ( uint16_type i = 0; i < element_type::numEdges; ++i )
        {
            for ( uint16_type l = 0; l < fe_type::nDofPerEdge; ++l, ++lc )
            {
                size_type gDof = __elt.edge( i ).id() * fe_type::nDofPerEdge;
                int32_type sign = 1;


                if ( __elt.edgePermutation( i ).value()  == edge_permutation_type::IDENTITY )
                {
                    gDof += l ; // both nodal and modal case
                }

                else if ( __elt.edgePermutation( i ).value()  == edge_permutation_type::REVERSE_PERMUTATION )
                {

                    if ( fe_type::is_modal )
                    {
                        //only half of the modes (odd polynomial order) are negative.
                        sign = ( l%2 )?( -1 ):( 1 );
                        gDof += l;
                    }

                    else
                        gDof += fe_type::nDofPerEdge - 1 - l ;
                }

                else
                    FEELPP_ASSERT( 0 ).error ( "invalid edge permutation" );

                this->insertDof( ie, lc, i, boost::make_tuple( 1, 0, gDof ), processor, next_free_dof, sign, false, global_shift, __elt.edge( i ).marker() );
            }
        }

        // update shifts
        shifts.template get<0>() = lc;
#if !defined(NDEBUG)
        DVLOG(4) << "[Dof::addEdgeDof] edge proc" << processor << " next_free_dof = " << next_free_dof << "\n";
#endif
    }

    void addEdgeDof( element_type const& /*__elt*/, uint16_type /*processor*/, size_type& /*next_free_dof*/,
                     ref_shift_type& /*shifts*/, mpl::int_<3>, mpl::bool_<false> )
    {}

    void addEdgeDof( element_type const& __elt, uint16_type processor, size_type& next_free_dof,
                     ref_shift_type& shifts, mpl::int_<3>, mpl::bool_<true> )
    {
        uint16_type local_shift;
        size_type global_shift;
        boost::tie( local_shift, global_shift ) = shifts;

        size_type ie = __elt.id();
        uint16_type lc = local_shift;

        for ( uint16_type i = 0; i < element_type::numEdges; ++i )
        {
            for ( uint16_type l = 0; l < fe_type::nDofPerEdge; ++l, ++lc )
            {
                size_type gDof = __elt.edge( i ).id() * fe_type::nDofPerEdge;

                int32_type sign = 1;

                if ( __elt.edgePermutation( i ).value()  == edge_permutation_type::IDENTITY )
                {
                    gDof += l ; // both nodal and modal case
                }

                else if ( __elt.edgePermutation( i ).value()  == edge_permutation_type::REVERSE_PERMUTATION )
                {

                    if ( fe_type::is_modal )
                    {
                        //only half of the modes (odd polynomial order) are negative.
                        sign = ( l%2 )?( -1 ):( 1 );
                        gDof += l;
                    }

                    else
                        gDof += fe_type::nDofPerEdge - 1 - l ;
                }

                else
                    FEELPP_ASSERT( 0 ).error ( "invalid edge permutation" );

                this->insertDof( ie, lc, i, boost::make_tuple( 1, 0, gDof ), processor, next_free_dof, sign, false, global_shift, __elt.edge( i ).marker() );
            }
        }

        // update shifts
        shifts.template get<0>() = lc;
#if !defined(NDEBUG)
        DVLOG(4) << "[Dof::addEdgeDof] edge proc" << processor << " next_free_dof = " << next_free_dof << "\n";
#endif
    }


    void addFaceDof( element_type const& __elt, uint16_type processor, size_type& next_free_dof,
                     ref_shift_type& shifts )
    {
        return addFaceDof( __elt, processor, next_free_dof, shifts, mpl::int_<fe_type::nDim>(), mpl::bool_<(fe_type::nDofPerFace > 0)>() );
    }
    void addFaceDof( element_type const& /*M*/, uint16_type /*processor*/, size_type& /*next_free_dof*/,
                     ref_shift_type& /*shifts*/, mpl::int_<1>, mpl::bool_<false> )
    {}
    void addFaceDof( element_type const& /*M*/, uint16_type /*processor*/, size_type& /*next_free_dof*/,
                     ref_shift_type& /*shifts*/, mpl::int_<2>, mpl::bool_<false> )
    {}
    void addFaceDof( element_type const& __elt, uint16_type processor, size_type& next_free_dof,
                     ref_shift_type& shifts, mpl::int_<2>, mpl::bool_<true> )
    {
        uint16_type local_shift;
        size_type global_shift;
        boost::tie( local_shift, global_shift ) = shifts;

        size_type ie = __elt.id();
        uint16_type lc = local_shift;

        for ( uint16_type l = 0; l < fe_type::nDofPerFace; ++l, ++lc )
        {
            const size_type gDof = is_p0_continuous? l:ie * fe_type::nDofPerFace + l;
            this->insertDof( ie, lc, l, boost::make_tuple( 2, 0, gDof ), processor, next_free_dof, 1, false, global_shift, __elt.marker() );
        }

        // update shifts
        shifts.template get<0>() = lc;
#if !defined(NDEBUG)
        DVLOG(4) << "[Dof::addFaceDof(2,true)] face proc" << processor << " next_free_dof = " << next_free_dof << "\n";
#endif
    }
    void addFaceDof( element_type const& /*M*/, uint16_type /*processor*/, size_type& /*next_free_dof*/,
                     ref_shift_type& /*shifts*/, mpl::int_<3>, mpl::bool_<false> )
    {}
    void addFaceDof( element_type const& __elt, uint16_type processor, size_type& next_free_dof,
                     ref_shift_type& shifts, mpl::int_<3>, mpl::bool_<true> )
    {
        uint16_type local_shift;
        size_type global_shift;
        boost::tie( local_shift, global_shift ) = shifts;

        size_type ie = __elt.id();

        uint16_type lc = local_shift;

        for ( uint16_type i = 0; i < element_type::numFaces; ++i )
        {
            face_permutation_type permutation = __elt.facePermutation( i );
            FEELPP_ASSERT( permutation != face_permutation_type( 0 ) ).error ( "invalid face permutation" );

            // Polynomial order in each direction
            uint16_type p=1;
            uint16_type q=0;

            // MaxOrder = Order - 2
            int MaxOrder = int( ( 3 + std::sqrt( 1+8*fe_type::nDofPerFace ) )/2 ) - 2;

            for ( uint16_type l = 0; l < fe_type::nDofPerFace; ++l, ++lc )
            {

                // TODO: orient the dof indices such
                // that they match properly the faces
                // dof of the connected faces. There
                // are a priori many permutations of
                // the dof face indices
                size_type gDof = __elt.face( i ).id() * fe_type::nDofPerFace;
                int32_type sign = 1;

                q=q+1;

                if ( q > MaxOrder )
                {
                    q = 1;
                    p = p+1;
                    MaxOrder = MaxOrder-1;
                }

                if ( !fe_type::is_modal )
                {
                    // no need of permutation is identity or only one dof on face
                    if ( permutation  == face_permutation_type( 1 ) || fe_type::nDofPerFace == 1 )
                        gDof += l;

                    else
                        gDof += vector_permutation[permutation][l];
                }

                else
                {
                    gDof += l;

                    if ( permutation == face_permutation_type( 2 ) )
                    {
                        // Reverse sign if polynomial order in
                        // eta_1 direction is odd

                        if ( p%2 == 0 )
                            sign = -1;

                    }
                }

                this->insertDof( ie, lc, i, boost::make_tuple( 2, 0, gDof ), processor, next_free_dof, sign, false, global_shift,__elt.face( i ).marker() );

            }
        }

        // update shifts
        shifts.template get<0>() = lc;
#if !defined(NDEBUG)
        DVLOG(4) << "[Dof::addFaceDof<3>] face proc" << processor << " next_free_dof = " << next_free_dof << "\n";
#endif
    }
    void addVolumeDof( element_type const& __elt, uint16_type processor, size_type& next_free_dof,
                       ref_shift_type& shifts )
    {
        return addVolumeDof( __elt, processor, next_free_dof, shifts, mpl::bool_<(fe_type::nDofPerVolume>0)>() );
    }
    void addVolumeDof( element_type const& /*M*/, uint16_type /*processor*/, size_type& /*next_free_dof*/,
                       ref_shift_type& /*shifts*/, mpl::bool_<false> )
    {}
    void addVolumeDof( element_type const& __elt, uint16_type processor, size_type& next_free_dof,
                       ref_shift_type& shifts, mpl::bool_<true> )
    {
        BOOST_STATIC_ASSERT( element_type::numVolumes );
        uint16_type local_shift;
        size_type global_shift;
        boost::tie( local_shift, global_shift ) = shifts;

        size_type ie = __elt.id();
        uint16_type lc = local_shift;

        for ( uint16_type l = 0; l < fe_type::nDofPerVolume; ++l, ++lc )
        {
            const size_type gDof = is_p0_continuous? l:ie * fe_type::nDofPerVolume + l;
            this->insertDof( ie, lc, l, boost::make_tuple( 3, 0, gDof ), processor, next_free_dof, 1, false, global_shift, __elt.marker() );
        }

        // update shifts
        shifts.template get<0>() = lc;
#if !defined(NDEBUG)
        DVLOG(4) << "[Dof::updateVolumeDof(<2>)] element proc" << processor << " next_free_dof = " << next_free_dof << "\n";
#endif
    }

    template<typename FaceIterator>
    void addVertexBoundaryDof( FaceIterator __face_it, uint16_type& lc )
    {
        addVertexBoundaryDof( __face_it, lc, mpl::bool_<(fe_type::nDofPerVertex>0)>(), mpl::int_<nDim>() );
    }
    template<typename FaceIterator> void addVertexBoundaryDof( FaceIterator /*__face_it*/, uint16_type& /*lc*/, mpl::bool_<false>, mpl::int_<1> ) {}
    template<typename FaceIterator> void addVertexBoundaryDof( FaceIterator /*__face_it*/, uint16_type& /*lc*/, mpl::bool_<false>, mpl::int_<2> ) {}
    template<typename FaceIterator> void addVertexBoundaryDof( FaceIterator /*__face_it*/, uint16_type& /*lc*/, mpl::bool_<false>, mpl::int_<3> ) {}
    template<typename FaceIterator>
    void addVertexBoundaryDof( FaceIterator __face_it, uint16_type& lc, mpl::bool_<true>, mpl::int_<1>  )
    {
        BOOST_STATIC_ASSERT( face_type::numVertices );
#if !defined(FEELPP_ENABLE_MPI_MODE)
        // id of the element adjacent to the face
        // \warning NEED TO INVESTIGATE THIS
        size_type iElAd = __face_it->ad_first();
        FEELPP_ASSERT( iElAd != invalid_size_type_value )( __face_it->id() ).error( "[Dof::buildBoundaryDof] invalid face/element in face" );

        // local id of the face in its adjacent element
        uint16_type iFaEl = __face_it->pos_first();
        FEELPP_ASSERT( iFaEl != invalid_uint16_type_value ).error ( "invalid element index in face" );
#else // MPI
        uint16_type iFaEl;
        size_type iElAd;

        if ( __face_it->processId() == __face_it->proc_first() )
        {
            iElAd = __face_it->ad_first();
            FEELPP_ASSERT( iElAd != invalid_size_type_value )( __face_it->id() ).error( "[Dof::buildBoundaryDof] invalid face/element in face" );

            // local id of the face in its adjacent element
            iFaEl = __face_it->pos_first();
            FEELPP_ASSERT( iFaEl != invalid_uint16_type_value ).error ( "invalid element index in face" );
        }

        else
        {
            iElAd = __face_it->ad_second();
            FEELPP_ASSERT( iElAd != invalid_size_type_value )( __face_it->id() ).error( "[Dof::buildBoundaryDof] invalid face/element in face" );

            // local id of the face in its adjacent element
            iFaEl = __face_it->pos_second();
            FEELPP_ASSERT( iFaEl != invalid_uint16_type_value ).error ( "invalid element index in face" );
        }

#endif
        // Loop number of Dof per vertex
        const int ncdof = is_product?nComponents:1;

        for ( int c = 0; c < ncdof; ++c )
        {
            for ( uint16_type l = 0; l < fe_type::nDofPerVertex; ++l )
            {
                auto temp= this->localToGlobal( iElAd,
                                                iFaEl * fe_type::nDofPerVertex + l,
                                                c );
                M_face_l2g[ __face_it->id()][ lc++ ] = boost::make_tuple( boost::get<0>( temp ),boost::get<1>( temp ),boost::get<2>( temp ),
                                                        iFaEl * fe_type::nDofPerVertex + l );
            }
        }
    }
    template<typename FaceIterator>
    void addVertexBoundaryDof( FaceIterator __face_it, uint16_type& lc, mpl::bool_<true>, mpl::int_<2>  )
    {
        BOOST_STATIC_ASSERT( face_type::numVertices );
#if !defined(FEELPP_ENABLE_MPI_MODE)
        // id of the element adjacent to the face
        // \warning NEED TO INVESTIGATE THIS
        size_type iElAd = __face_it->ad_first();
        FEELPP_ASSERT( iElAd != invalid_size_type_value )( __face_it->id() ).error( "[Dof::buildBoundaryDof] invalid face/element in face" );

        // local id of the face in its adjacent element
        uint16_type iFaEl = __face_it->pos_first();
        FEELPP_ASSERT( iFaEl != invalid_uint16_type_value ).error ( "invalid element index in face" );
#else // MPI
        uint16_type iFaEl;
        size_type iElAd;

        if ( __face_it->processId() == __face_it->proc_first() )
        {
            iElAd = __face_it->ad_first();
            FEELPP_ASSERT( iElAd != invalid_size_type_value )( __face_it->id() ).error( "[Dof::buildBoundaryDof] invalid face/element in face" );

            // local id of the face in its adjacent element
            iFaEl = __face_it->pos_first();
            FEELPP_ASSERT( iFaEl != invalid_uint16_type_value ).error ( "invalid element index in face" );
        }

        else
        {
            iElAd = __face_it->ad_second();
            FEELPP_ASSERT( iElAd != invalid_size_type_value )( __face_it->id() ).error( "[Dof::buildBoundaryDof] invalid face/element in face" );

            // local id of the face in its adjacent element
            iFaEl = __face_it->pos_second();
            FEELPP_ASSERT( iFaEl != invalid_uint16_type_value ).error ( "invalid element index in face" );
        }

#endif

        size_type ndofF = ( face_type::numVertices * fe_type::nDofPerVertex +
                            face_type::numEdges * fe_type::nDofPerEdge +
                            face_type::numFaces * fe_type::nDofPerFace );


        //M_dof2elt[gDof].push_back( boost::make_tuple( iElAd, lc-1, 48, 0 ) );
        // loop on face vertices
        const int ncdof = is_product?nComponents:1;

        for ( int c = 0; c < ncdof; ++c )
        {
            uint16_type lcc=c*ndofF;

            for ( uint16_type iVeFa = 0; iVeFa < face_type::numVertices; ++iVeFa )
            {
                // local vertex number (in element)
                uint16_type iVeEl = element_type::fToP( iFaEl, iVeFa );

                FEELPP_ASSERT( iVeEl != invalid_uint16_type_value ).error( "invalid local dof" );

                // Loop number of Dof per vertex
                for ( uint16_type l = 0; l < fe_type::nDofPerVertex; ++l )
                {
                    auto temp = this->localToGlobal( iElAd,
                                                     iVeEl * fe_type::nDofPerVertex + l,
                                                     c );
                    M_face_l2g[ __face_it->id()][ lcc++ ] = boost::make_tuple( boost::get<0>( temp ),boost::get<1>( temp ),boost::get<2>( temp ),
                            iVeEl * fe_type::nDofPerVertex + l );
                }
            }
        }
    }
    template<typename FaceIterator>
    void addVertexBoundaryDof( FaceIterator __face_it, uint16_type& lc, mpl::bool_<true>, mpl::int_<3>  )
    {
        addVertexBoundaryDof( __face_it, lc, mpl::bool_<true>(), mpl::int_<2>() );
    }
    template<typename FaceIterator>
    void addEdgeBoundaryDof( FaceIterator __face_it, uint16_type& lc )
    {
        static const bool cond = fe_type::nDofPerEdge*face_type::numEdges > 0;
        addEdgeBoundaryDof( __face_it, lc, mpl::bool_<cond>(), mpl::int_<nDim>() );
    }
    template<typename FaceIterator>
    void addEdgeBoundaryDof( FaceIterator /*__face_it*/, uint16_type& /*lc*/, mpl::bool_<false>, mpl::int_<1> ) {}
    template<typename FaceIterator>
    void addEdgeBoundaryDof( FaceIterator /*__face_it*/, uint16_type& /*lc*/, mpl::bool_<false>, mpl::int_<2> ) {}
    template<typename FaceIterator>
    void addEdgeBoundaryDof( FaceIterator /*__face_it*/, uint16_type& /*lc*/, mpl::bool_<false>, mpl::int_<3> ) {}
    template<typename FaceIterator>
    void addEdgeBoundaryDof( FaceIterator __face_it, uint16_type& lc, mpl::bool_<true>, mpl::int_<2> )
    {
#if !defined(FEELPP_ENABLE_MPI_MODE)
        // id of the element adjacent to the face
        // \warning NEED TO INVESTIGATE THIS
        size_type iElAd = __face_it->ad_first();
        FEELPP_ASSERT( iElAd != invalid_size_type_value )
        ( __face_it->id() ).error( "[Dof::buildBoundaryDof] invalid face/element in face" );

        // local id of the face in its adjacent element
        uint16_type iFaEl = __face_it->pos_first();
#else // MPI
        uint16_type iFaEl;
        size_type iElAd;

        if ( __face_it->processId() == __face_it->proc_first() )
        {
            iElAd = __face_it->ad_first();
            FEELPP_ASSERT( iElAd != invalid_size_type_value )
            ( __face_it->id() ).error( "[Dof::buildBoundaryDof] invalid face/element in face" );

            // local id of the face in its adjacent element
            iFaEl = __face_it->pos_first();
        }

        else
        {
            iElAd = __face_it->ad_second();
            FEELPP_ASSERT( iElAd != invalid_size_type_value )
            ( __face_it->id() ).error( "[Dof::buildBoundaryDof] invalid face/element in face" );

            // local id of the face in its adjacent element
            iFaEl = __face_it->pos_second();
        }

#endif


        FEELPP_ASSERT( iFaEl != invalid_uint16_type_value ).error ( "invalid element index in face" );
#if !defined(NDEBUG)
        DVLOG(4) << " local face id : " << iFaEl << "\n";
#endif
        size_type nVerticesF = face_type::numVertices * fe_type::nDofPerVertex;
        size_type ndofF = ( face_type::numVertices * fe_type::nDofPerVertex +
                            face_type::numEdges * fe_type::nDofPerEdge +
                            face_type::numFaces * fe_type::nDofPerFace );


        const int ncdof = is_product?nComponents:1;

        for ( int c = 0; c < ncdof; ++c )
        {
            uint16_type lcc=nVerticesF+c*ndofF;

            // Loop number of Dof per edge
            for ( uint16_type l = 0; l < fe_type::nDofPerEdge; ++l )
            {
                auto temp = this->localToGlobal( iElAd,
                                                 element_type::numVertices*fe_type::nDofPerVertex +
                                                 iFaEl * fe_type::nDofPerEdge + l,
                                                 c );
                M_face_l2g[ __face_it->id()][ lcc++ ] = boost::make_tuple( boost::get<0>( temp ),boost::get<1>( temp ),boost::get<2>( temp ),
                        element_type::numVertices*fe_type::nDofPerVertex +
                        iFaEl * fe_type::nDofPerEdge + l );
            }
        }
    }
    template<typename FaceIterator>
    void addEdgeBoundaryDof( FaceIterator __face_it, uint16_type& lc, mpl::bool_<true>, mpl::int_<3> )
    {
        //BOOST_STATIC_ASSERT( face_type::numEdges );
#if !defined(FEELPP_ENABLE_MPI_MODE)
        // id of the element adjacent to the face
        // \warning NEED TO INVESTIGATE THIS
        size_type iElAd = __face_it->ad_first();
        FEELPP_ASSERT( iElAd != invalid_size_type_value )( __face_it->id() ).error( "[Dof::buildBoundaryDof] invalid face/element in face" );

        // local id of the face in its adjacent element
        uint16_type iFaEl = __face_it->pos_first();
        FEELPP_ASSERT( iFaEl != invalid_uint16_type_value ).error ( "invalid element index in face" );
#else // MPI
        uint16_type iFaEl;
        size_type iElAd;

        if ( __face_it->processId() == __face_it->proc_first() )
        {
            iElAd = __face_it->ad_first();
            FEELPP_ASSERT( iElAd != invalid_size_type_value )
            ( __face_it->id() ).error( "[Dof::buildBoundaryDof] invalid face/element in face" );

            // local id of the face in its adjacent element
            iFaEl = __face_it->pos_first();
        }

        else
        {
            iElAd = __face_it->ad_second();
            FEELPP_ASSERT( iElAd != invalid_size_type_value )
            ( __face_it->id() ).error( "[Dof::buildBoundaryDof] invalid face/element in face" );

            // local id of the face in its adjacent element
            iFaEl = __face_it->pos_second();
        }

#endif

#if !defined(NDEBUG)
        DVLOG(4) << " local face id : " << iFaEl << "\n";
#endif
        size_type nVerticesF = face_type::numVertices * fe_type::nDofPerVertex;
        size_type ndofF = ( face_type::numVertices * fe_type::nDofPerVertex +
                            face_type::numEdges * fe_type::nDofPerEdge +
                            face_type::numFaces * fe_type::nDofPerFace );

        const int ncdof = is_product?nComponents:1;

        for ( int c = 0; c < ncdof; ++c )
        {
            uint16_type lcc=nVerticesF+c*ndofF;

            // loop on face vertices
            for ( uint16_type iEdFa = 0; iEdFa < face_type::numEdges; ++iEdFa )
            {
                // local edge number (in element)
                uint16_type iEdEl = element_type::fToE( iFaEl, iEdFa );

                FEELPP_ASSERT( iEdEl != invalid_uint16_type_value ).error( "invalid local dof" );

                // Loop number of Dof per edge
                for ( uint16_type l = 0; l < fe_type::nDofPerEdge; ++l )
                {
                    auto temp = this->localToGlobal( iElAd,
                                                     element_type::numVertices*fe_type::nDofPerVertex +
                                                     iEdEl * fe_type::nDofPerEdge + l,
                                                     c );
                    M_face_l2g[ __face_it->id()][ lcc++ ] = boost::make_tuple( boost::get<0>( temp ),boost::get<1>( temp ),boost::get<2>( temp ),
                            element_type::numVertices*fe_type::nDofPerVertex +
                            iEdEl * fe_type::nDofPerEdge + l );
                }
            }
        }
    }


    template<typename FaceIterator>
    void addFaceBoundaryDof( FaceIterator __face_it, uint16_type& lc )
    {
        addFaceBoundaryDof( __face_it, lc, mpl::bool_<(face_type::numFaces*fe_type::nDofPerFace > 0)>() );
    }
    template<typename FaceIterator>
    void addFaceBoundaryDof( FaceIterator /*__face_it*/, uint16_type& /*lc*/, mpl::bool_<false> )
    {
    }
    template<typename FaceIterator>
    void addFaceBoundaryDof( FaceIterator __face_it, uint16_type& lc, mpl::bool_<true> )
    {
#if !defined(FEELPP_ENABLE_MPI_MODE)
        // id of the element adjacent to the face
        // \warning NEED TO INVESTIGATE THIS
        size_type iElAd = __face_it->ad_first();
        FEELPP_ASSERT( iElAd != invalid_size_type_value )( __face_it->id() ).error( "[Dof::buildBoundaryDof] invalid face/element in face" );

        // local id of the face in its adjacent element
        uint16_type iFaEl = __face_it->pos_first();
        FEELPP_ASSERT( iFaEl != invalid_uint16_type_value ).error ( "invalid element index in face" );
#else // MPI
        uint16_type iFaEl;
        size_type iElAd;

        if ( __face_it->processId() == __face_it->proc_first() )
        {
            iElAd = __face_it->ad_first();
            FEELPP_ASSERT( iElAd != invalid_size_type_value )
            ( __face_it->id() ).error( "[Dof::buildBoundaryDof] invalid face/element in face" );

            // local id of the face in its adjacent element
            iFaEl = __face_it->pos_first();
        }

        else
        {
            iElAd = __face_it->ad_second();
            FEELPP_ASSERT( iElAd != invalid_size_type_value )
            ( __face_it->id() ).error( "[Dof::buildBoundaryDof] invalid face/element in face" );

            // local id of the face in its adjacent element
            iFaEl = __face_it->pos_second();
        }

#endif

#if !defined(NDEBUG)
        DVLOG(4) << " local face id : " << iFaEl << "\n";
#endif
        size_type nVerticesAndEdgeF = ( face_type::numVertices * fe_type::nDofPerVertex +
                                        face_type::numEdges * fe_type::nDofPerEdge );
        size_type ndofF = ( face_type::numVertices * fe_type::nDofPerVertex +
                            face_type::numEdges * fe_type::nDofPerEdge +
                            face_type::numFaces * fe_type::nDofPerFace );

        const int ncdof = is_product?nComponents:1;

        for ( int c = 0; c < ncdof; ++c )
        {
            uint16_type lcc=nVerticesAndEdgeF+c*ndofF;

            // Loop on number of Dof per face
            for ( uint16_type l = 0; l < fe_type::nDofPerFace; ++l )
            {
                auto temp = this->localToGlobal( iElAd,
                                                 element_type::numVertices*fe_type::nDofPerVertex +
                                                 element_type::numEdges*fe_type::nDofPerEdge +
                                                 iFaEl * fe_type::nDofPerFace + l,
                                                 c );
                M_face_l2g[ __face_it->id()][ lcc++ ] = boost::make_tuple( boost::get<0>( temp ),boost::get<1>( temp ),boost::get<2>( temp ),
                        element_type::numVertices*fe_type::nDofPerVertex +
                        element_type::numEdges*fe_type::nDofPerEdge +
                        iFaEl * fe_type::nDofPerFace + l );
            }
        }
    }

    void addSubstructuringDofMap( mesh_type const& M, size_type next_free_dof );
    void addSubstructuringDofVertex( mesh_type const& M, size_type next_free_dof );
    void addSubstructuringDofEdge( mesh_type const& M, size_type next_free_dof, mpl::int_<1> );
    void addSubstructuringDofEdge( mesh_type const& M, size_type next_free_dof, mpl::int_<2> );
    void addSubstructuringDofEdge( mesh_type const& M, size_type next_free_dof, mpl::int_<3> );
    void addSubstructuringDofFace( mesh_type const& M, size_type next_free_dof, mpl::int_<1> );
    void addSubstructuringDofFace( mesh_type const& M, size_type next_free_dof, mpl::int_<2> );
    void addSubstructuringDofFace( mesh_type const& M, size_type next_free_dof, mpl::int_<3> );

    /**
     * @brief Checks if the dofs associated with entity_id N are continuous
     *
     * \param M A mesh
     */
    template<int N>
    void checkDofEntity ( mesh_type& M )
    {
        Feel::detail::ignore_unused_variable_warning( M );

        if ( !is_scalar )
            return;

#if !defined(NDEBUG)

        using namespace Feel;

        gm_ptrtype M_gm_ptr = M.gm();

        fe_type M_basis;

        //value_type tol = value_type(100.0)*type_traits<value_type>::epsilon();
        value_type tol = value_type( 10.0 )*type_traits<double>::epsilon();

        bool global_signs_good = 1;

        std::vector<size_type> bad_dof;

        for ( uint16_type gDof = 0; gDof < nDof(); ++gDof )
        {
            uint16_type _numEntities = M_dof2elt[gDof].size();
            uint16_type _ent = M_dof2elt[gDof].begin()->template get<3>();

            if ( _numEntities > 1 && _ent == mpl::int_<N>() )
            {
                bool signs_good = 1;

                std::vector< ublas::vector<value_type> > basis_eval;

                std::vector< points_type > real_coordinates;

                ldof_const_iterator __ldofit = M_dof2elt[gDof].begin();
                ldof_const_iterator __ldofen = M_dof2elt[gDof].end();

                while ( __ldofit != __ldofen )
                {
                    size_type entity_element_id = __ldofit->template get<0>();
                    uint16_type entity_local_dof_id = __ldofit->template get<1>();
                    uint16_type entity_local_id = __ldofit->template get<2>();

                    PointSetMapped<element_type, convex_type, nOrder> test( M.element( entity_element_id ) );

                    points_type Gt = test.pointsBySubEntity( N, entity_local_id );

                    real_coordinates.push_back( test.pointsBySubEntity( N, entity_local_id, 0, 1 ) );

                    int sign = boost::get<1>( localToGlobal( entity_element_id, entity_local_dof_id ) );

                    basis_eval.push_back( value_type( sign )*ublas::row( M_basis.evaluate( Gt ), entity_local_dof_id ) );

                    ++__ldofit;
                }

                for ( uint16_type i=1; i < _numEntities; i++ )
                {

                    FEELPP_ASSERT( ublas::norm_inf( real_coordinates[i] - real_coordinates[0] ) < tol  )
                    ( gDof )
                    ( real_coordinates[0] )
                    ( real_coordinates[i] ).error( "Reference points aren't being mapped to the same real one's" );

                    if ( ublas::norm_inf( basis_eval[i] - basis_eval[0] ) > tol )
                    {
                        signs_good = 0;
                        global_signs_good = 0;
                    }
                }

                basis_eval.empty();
                real_coordinates.empty();

                if ( signs_good == 0 )
                    bad_dof.push_back( gDof );
            }
        }

        if ( !bad_dof.empty() )
        {
            for ( uint16_type i = 0; i < bad_dof.size(); ++i )
                Warning() << bad_dof[i] << "\n";

            if ( mpl::int_<N>() == 1 )
                Warning() << "Edges: ";

            else
                Warning() << "Faces: ";

            Warning() << "Bad dof signs. \n";
        }

#endif
    }

    void checkDofContinuity( mesh_type& /*mesh*/, mpl::int_<1> ) {}

    void checkDofContinuity( mesh_type& mesh, mpl::int_<2> )
    {
        checkDofEntity<1>( mesh );
    }

    void checkDofContinuity( mesh_type& mesh, mpl::int_<3> )
    {
        checkDofContinuity( mesh, mpl::int_<2>() );
        checkDofEntity<2>( mesh );
    }

    void generateFacePermutations ( mesh_type& /*mesh*/, mpl::bool_<false> ) {}

    void generateFacePermutations ( mesh_type& mesh, mpl::bool_<true> )
    {
        element_type _elt = *mesh.beginElement();
        PointSetMapped<element_type, convex_type, nOrder> pts( _elt );

        for ( uint16_type i = 2; i < face_permutation_type::N_PERMUTATIONS; i++ )
            vector_permutation[face_permutation_type( i )] = pts.getVectorPermutation( face_permutation_type( i ) );
    }
    void generateDofPoints( mesh_type& M );
    void generatePeriodicDofPoints( mesh_type& M, periodic_element_list_type const& periodic_elements, dof_points_type& periodic_dof_points );



private:

    mesh_type* M_mesh;
    fe_ptrtype M_fe;

    reference_convex_type M_convex_ref;

    size_type M_n_el;
    std::vector<bool> M_elt_done;

    uint16_type M_n_dof_per_face_on_bdy;
    uint16_type M_n_dof_per_face;

    dof_table M_el_l2g;
    Container_fromface M_face_l2g;

    mutable local_dof_set_type M_local_dof_set;
    dof_element_type M_dof2elt;
    dof_marker_type M_dof_marker;

    dof_map_type map_gdof;

    std::map<face_permutation_type, permutation_vector_type> vector_permutation;

    /**
     * map between local(on processor) dof and global dofs
     */
    std::map<size_type, std::pair<size_type, size_type> > M_local2global;


    face_sign_info_type M_face_sign;

    /**
     * for each dof, store the set of processor it belongs to
     */
    dof_procset_type M_dof_procset;

    /**
     * coordinates of the nodal dofs
     */
    dof_points_type M_dof_points;

    std::vector<Dof> M_dof_indices;

    periodicity_type M_periodicity;
    //! list of elements which have a periodic face Tag2
    periodic_element_list_type periodic_elements;

    /// a view of the dof container
    //dof_container_type M_dof_view;

    typedef typename std::vector<localglobal_indices_type,Eigen::aligned_allocator<localglobal_indices_type> > vector_indices_type;


    vector_indices_type M_locglob_indices;
    vector_indices_type M_locglob_signs;

    // multi process
    vector_indices_type M_locglobOnCluster_indices;
    vector_indices_type M_locglobOnCluster_signs;

public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
};

template<typename MeshType, typename FEType, typename PeriodicityType>
const uint16_type DofTable<MeshType, FEType, PeriodicityType>::nComponents;

template<typename MeshType, typename FEType, typename PeriodicityType>
DofTable<MeshType, FEType, PeriodicityType>::DofTable( mesh_type& mesh, fe_ptrtype const& _fe, periodicity_type const& periodicity, WorldComm const& _worldComm )
    :
    super( _worldComm ),
    M_fe( _fe ),
    M_n_el( invalid_size_type_value ),
    M_n_dof_per_face_on_bdy( invalid_uint16_type_value ),
    M_n_dof_per_face( invalid_uint16_type_value ),
    M_el_l2g(),
    M_face_l2g(),
    M_local_dof_set( 0, nLocalDof() ),
    map_gdof(),
    M_face_sign(),
    M_dof_indices(),
    M_periodicity( periodicity )
{
    VLOG(2) << "[dof] is_periodic = " << is_periodic << "\n";
    size_type start_next_free_dof = 0;

    if ( is_periodic )
        start_next_free_dof = buildPeriodicDofMap( mesh );

    buildDofMap( mesh, start_next_free_dof );
    buildBoundaryDofMap( mesh );
}

template<typename MeshType, typename FEType, typename PeriodicityType>
DofTable<MeshType, FEType, PeriodicityType>::DofTable( fe_ptrtype const& _fe, periodicity_type const& periodicity, WorldComm const& _worldComm )
    :
    super( _worldComm ),
    M_fe( _fe ),
    M_n_el( 0 ),
    M_n_dof_per_face_on_bdy( invalid_uint16_type_value ),
    M_n_dof_per_face( invalid_uint16_type_value ),
    M_el_l2g(),
    M_face_l2g(),
    M_local_dof_set( 0, nLocalDof() ),
    map_gdof(),
    M_face_sign(),
    M_dof_indices(),
    M_periodicity( periodicity )
{
}

template<typename MeshType, typename FEType, typename PeriodicityType>
DofTable<MeshType, FEType, PeriodicityType>::DofTable( const DofTable<MeshType, FEType, PeriodicityType> & dof2 )
    :
    super( dof2 ),
    M_fe( dof2.M_fe ),
    M_n_el( dof2.M_n_el ),
    M_n_dof_per_face_on_bdy( dof2.M_n_dof_per_face_on_bdy ),
    M_n_dof_per_face( dof2.M_n_dof_per_face ),
    M_el_l2g( dof2.M_el_l2g ),
    M_face_l2g( dof2.M_face_l2g ),
    M_local_dof_set( dof2.M_local_dof_set),
    map_gdof( dof2.map_gdof ),
    M_face_sign( dof2.M_face_sign ),
    M_dof_indices( dof2.M_dof_indices ),
    M_periodicity( dof2.M_periodicity )
{
}

template<typename MeshType, typename FEType, typename PeriodicityType>
void
DofTable<MeshType, FEType, PeriodicityType>::showMe() const
{
    LOG(INFO)  << " Degree of Freedom (DofTable) Object" << "\n";
    //if ( verbose )
    {
        LOG(INFO) <<  "* nDof = " << this->nLocalDof() << "\n";
        LOG(INFO)  << "************************************************************" << "\n";
        LOG(INFO)  << "           Local to Global DOF table" << "\n";
        LOG(INFO)  << "************************************************************" << "\n";
        LOG(INFO)  << "Element Id    Loc. N.    Global N.   Sign#    Element Id   Loc. N.  Global N.  Sign" << "\n";

        for ( size_type i = 0; i < M_n_el; ++i )
        {

            for ( size_type j = 0; j < nDofPerElement; ++j )
            {

                LOG(INFO)<< "elt id " << i << " : "
                     << "(local/global/sign dof : " << j << " : "
                     << boost::get<0>( localToGlobal( i  , j ) ) << " : "
                     << boost::get<1>( localToGlobal( i  , j ) ) << "\n";
            }

        }

        LOG(INFO)  << "\n";

        LOG(INFO)  << "************************************************************" << "\n";
        LOG(INFO)  << " Boundary  Local to Global DOF table" << "\n";
        LOG(INFO)  << "************************************************************" << "\n";

        auto it = M_face_l2g.begin();
        auto en = M_face_l2g.end();

        for ( size_type f = 0; it!=en; ++it,++f )
        {
            std::ostringstream ostr;
            ostr  << "face id " << it->first << " : ";

            auto it2 = it->second.begin();
            auto en2 = it->second.end();

            for ( size_type l = 0; it2!=en2; ++it2,++l )
            {
                ostr << "(local/global/sign dof : " << it2->first << " : "
                     << boost::get<0>( it2->second )<< " : "
                     << boost::get<1>( it2->second ) << "\n";
            }

            LOG(INFO) << ostr.str() << "\n";
        }
    }

}

template<typename MeshType, typename FEType, typename PeriodicityType>
void
DofTable<MeshType, FEType, PeriodicityType>::addVertexPeriodicDof( element_type const& __elt,
        face_type const& __face,
        size_type& next_free_dof,
        std::map<size_type,periodic_dof_map_type>& periodic_dof,
        size_type tag,
        mpl::bool_<true> )
{
    // store the element and local dof id for further
    // reference when inserting the associated global dof
    // id of the element adjacent to the face

    size_type iElAd = __face.ad_first();
    FEELPP_ASSERT( iElAd != invalid_size_type_value )( __face.id() ).error( "[periodic]invalid face/element in face" );
    Feel::detail::ignore_unused_variable_warning( iElAd );

    // local id of the face in its adjacent element
    uint16_type iFaEl = __face.pos_first();
    FEELPP_ASSERT( iFaEl != invalid_uint16_type_value ).error ( "invalid element index in face" );

    // loop on face vertices
    for ( uint16_type iVeFa = 0; iVeFa < face_type::numVertices; ++iVeFa )
    {
        // local vertex number (in element)
        uint16_type iVeEl = element_type::fToP( iFaEl, iVeFa );
        Feel::detail::ignore_unused_variable_warning( iVeEl );

        FEELPP_ASSERT( iVeEl != invalid_uint16_type_value ).error( "invalid local dof" );

        // Loop number of Dof per vertex
        for ( uint16_type l = 0; l < fe_type::nDofPerVertex; ++l )
        {
            uint16_type lid = iVeEl * fe_type::nDofPerVertex + l;
            //const size_type gDof = global_shift + ( __elt.point( i ).id() ) * fe_type::nDofPerVertex + l;
            const size_type gDof = ( __elt.point( iVeEl ).id() ) * fe_type::nDofPerVertex + l;

            VLOG(2) << "add vertex periodic doc " << next_free_dof << " in element " << __elt.id() << " lid = " << lid << "\n";
            size_type dof_id = next_free_dof;
            // next_free_dof might be incremented if a new dof is created
            bool inserted = this->insertDof( __elt.id(), lid, iVeEl, boost::make_tuple( 0, 0, gDof ), 0, next_free_dof, 1, true, 0, __elt.point( iVeEl ).marker()  );
            VLOG(2) << "vertex periodic dof inserted : " << inserted << "\n";

            const int ncdof = is_product?nComponents:1;
            for ( int c1 = 0; c1 < ncdof; ++c1 )
            {
                // add the pair (elt, lid) to the map associated
                // with dof_id, one dof can be shared by several
                // elements
                dof_id = localToGlobal( __elt.id(), lid, c1 ).index();
                periodic_dof[tag].insert( std::make_pair( dof_id, boost::make_tuple( __elt.id(), lid, c1, gDof, 0 ) ) );

                VLOG(2) << "added vertex periodic dof " <<  __elt.id() << ", " <<  lid << ", " << boost::get<0>( localToGlobal( __elt.id(), lid, c1 ) ) << "\n";
            }
        }

    }

}

template<typename MeshType, typename FEType, typename PeriodicityType>
void
DofTable<MeshType, FEType, PeriodicityType>::addEdgePeriodicDof( element_type const& __elt,
        face_type const& __face,
        size_type& next_free_dof,
        std::map<size_type,periodic_dof_map_type>& periodic_dof,
        size_type tag,
        mpl::bool_<true>,
        mpl::int_<2> )
{
#if 0
    // id of the element adjacent to the face
    // \warning NEED TO INVESTIGATE THIS
    size_type iElAd = __face.ad_first();
    FEELPP_ASSERT( iElAd != invalid_size_type_value )
    ( __face.id() ).error( "[DofTable::buildBoundaryDof] invalid face/element in face" );
#endif // 0

    // local id of the face in its adjacent element
    uint16_type iFaEl = __face.pos_first();
    FEELPP_ASSERT( iFaEl != invalid_uint16_type_value ).error ( "invalid element index in face" );
#if !defined(NDEBUG)
    DVLOG(4) << " local face id : " << iFaEl << "\n";
#endif

    // Loop number of DofTable per edge
    for ( uint16_type l = 0; l < fe_type::nDofPerEdge; ++l )
    {
        uint16_type lid = element_type::numVertices*fe_type::nDofPerVertex + iFaEl * fe_type::nDofPerEdge + l;
        //const size_type gDof = global_shift + ( __elt.point( i ).id() ) * fe_type::nDofPerVertex + l;
        const size_type gDof = ( __elt.edge( iFaEl ).id() ) * fe_type::nDofPerEdge + l;

        DVLOG(4) << "add edge periodic dof " << next_free_dof << " in element " << __elt.id() << " lid = " << lid << "\n";
        size_type dof_id = next_free_dof;
        // next_free_dof might be incremented if a new dof is created
        bool inserted = this->insertDof( __elt.id(), lid, iFaEl, boost::make_tuple( 1, 0, gDof ), 0, next_free_dof, 1, true, 0, __elt.edge( iFaEl ).marker() );
        DVLOG(4) << "edge periodic dof inserted (1 or 0) : " << inserted << "\n";

        const int ncdof = is_product?nComponents:1;
        for ( int c1 = 0; c1 < ncdof; ++c1 )
        {
            // add the pair (elt, lid) to the map associated
            // with dof_id, one dof can be shared by several
            // elements
            dof_id = boost::get<0>( localToGlobal( __elt.id(), lid, c1 ) );
            periodic_dof[tag].insert( std::make_pair( dof_id, boost::make_tuple( __elt.id(), lid, c1, gDof, 1 ) ) );

            DVLOG(4) << "added edge periodic dof " <<  __elt.id() << ", " <<  lid << ", " << boost::get<0>( localToGlobal( __elt.id(), lid, c1 ) ) << "\n";
        }

    }
}
template<typename MeshType, typename FEType, typename PeriodicityType>
void
DofTable<MeshType, FEType, PeriodicityType>::addEdgePeriodicDof( element_type const& __elt,
        face_type const& __face,
        size_type& next_free_dof,
        std::map<size_type,periodic_dof_map_type>& periodic_dof,
        size_type tag,
        mpl::bool_<true>,
        mpl::int_<3> )
{
    //BOOST_STATIC_ASSERT( face_type::numEdges );

    // id of the element adjacent to the face
    // \warning NEED TO INVESTIGATE THIS
    size_type iElAd = __face.ad_first();
    Feel::detail::ignore_unused_variable_warning( iElAd );
    FEELPP_ASSERT( iElAd != invalid_size_type_value )( __face.id() ).error( "[Dof::buildBoundaryDof] invalid face/element in face" );

    // local id of the face in its adjacent element
    uint16_type iFaEl = __face.pos_first();
    FEELPP_ASSERT( iFaEl != invalid_uint16_type_value ).error ( "invalid element index in face" );
#if !defined(NDEBUG)
    DVLOG(4) << " local face id : " << iFaEl << "\n";
#endif

    // loop on face vertices
    for ( uint16_type iEdFa = 0; iEdFa < face_type::numEdges; ++iEdFa )
    {
        // local edge number (in element)
        uint16_type iEdEl = element_type::fToE( iFaEl, iEdFa );
        Feel::detail::ignore_unused_variable_warning( iEdEl );
        FEELPP_ASSERT( iEdEl != invalid_uint16_type_value ).error( "invalid local dof" );

        // Loop number of Dof per edge
        for ( uint16_type l = 0; l < fe_type::nDofPerEdge; ++l )
        {

            uint16_type lid = element_type::numVertices*fe_type::nDofPerVertex + iFaEl * fe_type::nDofPerEdge + l;
            //const size_type gDof = global_shift + ( __elt.point( i ).id() ) * fe_type::nDofPerVertex + l;
            size_type gDof = __elt.edge( l ).id() * fe_type::nDofPerEdge;

            if ( __elt.edgePermutation( l ).value()  == edge_permutation_type::IDENTITY )
            {
                gDof += l ; // both nodal and modal case
            }

            else if ( __elt.edgePermutation( l ).value()  == edge_permutation_type::REVERSE_PERMUTATION )
            {
                gDof += fe_type::nDofPerEdge - 1 - l ;
            }

            DVLOG(4) << "add periodic doc " << next_free_dof << " in element " << __elt.id() << " lid = " << lid << "\n";
            size_type dof_id = next_free_dof;
            // next_free_dof might be incremented if a new dof is created
            bool inserted = this->insertDof( __elt.id(), lid, l, boost::make_tuple( 1, 0, gDof ), 0, next_free_dof, 1, true, 0, __elt.edge( l ).marker() );
            DVLOG(4) << "periodic dof inserted : " << inserted << "\n";

            const int ncdof = is_product?nComponents:1;
            for ( int c1 = 0; c1 < ncdof; ++c1 )
            {
                // add the pair (elt, lid) to the map associated
                // with dof_id, one dof can be shared by several
                // elements
                dof_id = boost::get<0>( localToGlobal( __elt.id(), lid, c1 ) );
                periodic_dof[tag].insert( std::make_pair( dof_id, boost::make_tuple( __elt.id(),  lid, c1, gDof, 1 ) ) );

                DVLOG(4) << "added " <<  __elt.id() << ", " <<  lid << ", " << boost::get<0>( localToGlobal( __elt.id(), lid, c1 ) ) << "\n";
            }
        }
    }

}
template<typename MeshType, typename FEType, typename PeriodicityType>
void
DofTable<MeshType, FEType, PeriodicityType>::addFacePeriodicDof( element_type const& __elt,
        face_type const& __face,
        size_type& next_free_dof,
        std::map<size_type,periodic_dof_map_type>& periodic_dof,
        size_type tag,
        mpl::bool_<true> )
{
#if 0
    // id of the element adjacent to the face
    // \warning NEED TO INVESTIGATE THIS
    size_type iElAd = __face.ad_first();
    FEELPP_ASSERT( iElAd != invalid_size_type_value )( __face.id() ).error( "[Dof::buildBoundaryDof] invalid face/element in face" );

    // local id of the face in its adjacent element
    uint16_type iFaEl = __face.pos_first();
    FEELPP_ASSERT( iFaEl != invalid_uint16_type_value ).error ( "invalid element index in face" );
#if !defined(NDEBUG)
    DVLOG(2) << " local face id : " << iFaEl << "\n";
#endif

    // Loop on number of Dof per face
    for ( uint16_type l = 0; l < fe_type::nDofPerFace; ++l )
    {
        auto temp = this->localToGlobal( iElAd,
                                         element_type::numVertices*fe_type::nDofPerVertex +
                                         element_type::numEdges*fe_type::nDofPerEdge +
                                         iFaEl * fe_type::nDofPerFace + l,
                                         c );
        M_face_l2g[ __face_it->id()][ lc++ ] = boost::make_tuple( boost::get<0>( temp ),boost::get<1>( temp ),boost::get<2>( temp ),
                                                element_type::numVertices*fe_type::nDofPerVertex +
                                                element_type::numEdges*fe_type::nDofPerEdge +
                                                iFaEl * fe_type::nDofPerFace + l );
    }

#endif // 0
}
template<typename MeshType, typename FEType, typename PeriodicityType>
void
DofTable<MeshType, FEType, PeriodicityType>::initDofMap( mesh_type& M )
{
    M_n_el = M.numElements();

    size_type nldof =
        fe_type::nDofPerVolume * element_type::numVolumes +
        fe_type::nDofPerFace * element_type::numGeometricFaces +
        fe_type::nDofPerEdge * element_type::numEdges +
        fe_type::nDofPerVertex * element_type::numVertices;

    VLOG(2) << "==============================\n";
    VLOG(2) << "[initDofMap]\n";
    VLOG(2) << "nldof                   = "  << int( nldof ) << "\n";
    VLOG(2) << "fe_type::nLocalDof     = "  << int( fe_type::nLocalDof ) << "\n";
    VLOG(2) << "fe_type::nDofPerVolume = "  << int( fe_type::nDofPerVolume ) << "\n";
    VLOG(2) << "fe_type::nDofPerFace   = "  << int( fe_type::nDofPerFace ) << "\n";
    VLOG(2) << "fe_type::nDofPerEdge   = "  << int( fe_type::nDofPerEdge ) << "\n";
    VLOG(2) << "fe_type::nDofPerVertex = "  << int( fe_type::nDofPerVertex ) << "\n";
    VLOG(2) << "element_type::numVolumes= "  << int( element_type::numVolumes ) << "\n";
    VLOG(2) << "element_type::numFaces= "    << int( element_type::numFaces ) << "\n";
    VLOG(2) << "element_type::numEdges= "    << int( element_type::numEdges ) << "\n";
    VLOG(2) << "element_type::numVertices= " << int( element_type::numVertices ) << "\n";
    VLOG(2) << "==============================\n";

    FEELPP_ASSERT( nldof == fe_type::nLocalDof )
    ( nldof )
    ( fe_type::nLocalDof ).error( "Something wrong in FE specification" ) ;

    // initialize the local to global map and fill it with invalid
    // values that will allow to check whether we have a new dof or
    // not when building the table
    const size_type nV = M.numElements();
    int ntldof = is_product?nComponents*nldof:nldof;//this->getIndicesSize();
    M_locglob_indices.resize( nV );
    M_locglob_signs.resize( nV );
#if defined(FEELPP_ENABLE_MPI_MODE)
    M_locglobOnCluster_indices.resize( nV );
    M_locglobOnCluster_signs.resize( nV );
#endif

    M_face_sign = ublas::scalar_vector<bool>( M.numFaces(), false );

    const bool doperm = ( ( ( Shape == SHAPE_TETRA ) && ( nOrder > 2 ) ) || ( ( Shape == SHAPE_HEXA ) && ( nOrder > 1 ) ) );
    DVLOG(2) << "generateFacePermutations: " << doperm << "\n";
    generateFacePermutations( M, mpl::bool_<doperm>() );
}
template<typename MeshType, typename FEType, typename PeriodicityType>
size_type
DofTable<MeshType, FEType, PeriodicityType>::buildPeriodicDofMap( mesh_type& M )
{
    size_type nldof =
        fe_type::nDofPerVolume * element_type::numVolumes +
        fe_type::nDofPerFace * element_type::numGeometricFaces +
        fe_type::nDofPerEdge * element_type::numEdges +
        fe_type::nDofPerVertex * element_type::numVertices;

    FEELPP_ASSERT( nldof == fe_type::nLocalDof )
    ( nldof )
    ( fe_type::nLocalDof ).error( "Something wrong in FE specification" ) ;

    const size_type n_proc  = M.worldComm().localSize();


    for ( size_type processor=0; processor<n_proc; processor++ )
    {
        // compute the number of dof on current processor
        element_const_iterator it_elt = M.beginElementWithProcessId( processor );
        element_const_iterator en_elt = M.endElementWithProcessId( processor );
        size_type n_elts = std::distance( it_elt, en_elt );
        VLOG(2) << "[buildDofMap] n_elts =  " << n_elts << " on processor " << processor << "\n";
        //this->M_first_df[processor] = next_free_dof;

        it_elt = M.beginElementWithProcessId( processor );

        it_elt = M.beginElementWithProcessId( processor );
        VLOG(2) << "[buildDofMap] starting with elt " << it_elt->id() << "\n";

        for ( ; it_elt!=en_elt; ++it_elt )
        {
            element_type const& __elt = *it_elt;
            //VLOG(2) << "next_free_dof " << next_free_dof  << "\n";
            //VLOG(2) << "current dof " << dofIndex( next_free_dof ) << "\n";

            typename element_type::face_const_iterator it, en;
            boost::tie( it, en ) = it_elt->faces();

            //bool found_periodic_face_in_element = false;
            for ( ; it != en; ++it )
            {
                if ( ( *it )->marker().value() == M_periodicity.tag2() ||
                     ( *it )->marker().value() == M_periodicity.tag1() )
                {
                    // store the element reference for the end, the associated
                    // dof on the periodic face is in fact already taken care of.
                    // the "internal" dof or on not periodic face will be added
                    periodic_elements.push_back( boost::make_tuple( boost::addressof( __elt ), *it ) );
                    //found_periodic_face_in_element = true;
                    break;
                }
            }
        }
    }

    VLOG(2) << "[buildPeriodicDofMap] built periodic_elements " << periodic_elements.size() << "\n";
    std::map<size_type,periodic_dof_map_type> periodic_dof;
    /*
     * Generate the periodic dof, assign a gid to the tag1 dof and set
     * the tag2 dof to invalid_size_type_value for now.
     */
    periodic_element_list_iterator it_periodic = periodic_elements.begin();
    periodic_element_list_iterator en_periodic = periodic_elements.end();
    size_type next_free_dof = 0;

    while ( it_periodic != en_periodic )
    {
        element_type const& __elt = *it_periodic->template get<0>();
        face_type const& __face = *it_periodic->template get<1>();

        if ( __face.marker().value() == M_periodicity.tag1() )
        {
            addVertexPeriodicDof( __elt, __face, next_free_dof, periodic_dof, __face.marker().value() );
            addEdgePeriodicDof( __elt, __face, next_free_dof, periodic_dof, __face.marker().value() );
            addFacePeriodicDof( __elt, __face, next_free_dof, periodic_dof, __face.marker().value() );
        }

        ++it_periodic;
    }

    it_periodic = periodic_elements.begin();

    while ( it_periodic != en_periodic )
    {
        element_type const& __elt = *it_periodic->template get<0>();
        face_type const& __face = *it_periodic->template get<1>();

        if ( __face.marker().value() == M_periodicity.tag2() )
        {
            addVertexPeriodicDof( __elt, __face, next_free_dof, periodic_dof, __face.marker().value() );
            addEdgePeriodicDof( __elt, __face, next_free_dof, periodic_dof, __face.marker().value() );
            addFacePeriodicDof( __elt, __face, next_free_dof, periodic_dof, __face.marker().value() );
        }

        ++it_periodic;
    }

    VLOG(2) << "[periodic dof table] next_free_dof : " << next_free_dof << "\n";
    VLOG(2) << "[periodic dof table] number of periodic dof : " << periodic_dof[M_periodicity.tag1()].size() << "\n";

    dof_points_type periodic_dof_points( next_free_dof );
    generatePeriodicDofPoints( M, periodic_elements, periodic_dof_points );

    VLOG(2) << "[periodic dof table] generated dof points\n";
    VLOG(2) << "[periodic dof table] start matching the dof points\n";

    size_type max_gid = 0;
    std::pair<size_type,periodic_dof_type> dof;
    BOOST_FOREACH( dof, periodic_dof[M_periodicity.tag1()] )
    {
        size_type gid = dof.first;
        max_gid = ( max_gid > gid )?max_gid:gid;
    }

    size_type max_gid2 = 0;
    std::pair<size_type,periodic_dof_type> dof2;
    BOOST_FOREACH( dof2, periodic_dof[M_periodicity.tag2()] )
    {
        size_type gid2 = dof2.first;
        FEELPP_ASSERT( gid2 > max_gid )( gid2 )( max_gid ).error( "invalid dof index" );
        max_gid2 = ( max_gid2 > gid2 )?max_gid2:gid2;
    }
    CHECK( ( max_gid+1 ) == ( max_gid2+1-( max_gid+1 ) ) )
        << "[periodic] invalid periodic setup"
        << "  max_gid+1  = " <<  max_gid+1
        << ", ( max_gid2+1-( max_gid+1 ) =" << ( max_gid2+1-( max_gid+1 ) )
        << ", max gid = " << max_gid
        << ", max_gid2 = " << max_gid2 << "\n";

    std::vector<bool> periodic_dof_done( max_gid+1 );
    std::fill( periodic_dof_done.begin(), periodic_dof_done.end(), false );

    BOOST_FOREACH( dof, periodic_dof[M_periodicity.tag1()] )
    {
        size_type gid = dof.first;

        if ( periodic_dof_done[gid] )
            continue;

        node_type x1 = periodic_dof_points[gid].template get<0>();
        bool match = false;
        typename periodic_dof_map_type::iterator it_dof2 = periodic_dof[M_periodicity.tag2()].begin();
        typename periodic_dof_map_type::iterator en_dof2 = periodic_dof[M_periodicity.tag2()].end();
#if 0
        for ( ; it_dof2 != en_dof2; ++ it_dof2 )
        {
            size_type gid2 = it_dof2->first;
            FEELPP_ASSERT( gid2 < next_free_dof )( gid )( gid2 )( next_free_dof ).error( "[periodic] invalid dof id" );
            node_type x2 = periodic_dof_points[gid2].template get<0>();
            //FEELPP_ASSERT( math::abs( x2[0]-M_periodicity.translation()[0]) < 1e-10 )
            //( x1 )( x2 )( M_periodicity.translation() ).error( "[periodic] invalid periodic setup");
        }
#endif
        it_dof2 = periodic_dof[M_periodicity.tag2()].begin();
        size_type corresponding_gid = invalid_size_type_value;

        for ( ; it_dof2 != en_dof2; ++ it_dof2 )
        {
            // make sure that we iterate over dof belonging to the same function
            // component (e.g. in vectorial)
            if ( it_dof2->second.template get<2>() != dof.second.template get<2>() )
                continue;
            size_type gid2 = it_dof2->first;
            FEELPP_ASSERT( gid2 < next_free_dof )( gid )( gid2 )( next_free_dof ).error( "[periodic] invalid dof id" );
            node_type x2 = periodic_dof_points[gid2].template get<0>();
            FEELPP_ASSERT( ( x1.size() == x2.size() ) &&
                           ( x1.size() == M_periodicity.translation().size() ) )
            ( gid )( dof.second.template get<0>() )( dof.second.template get<1>() )
            ( gid2 )( it_dof2->second.template get<0>() )( it_dof2->second.template get<1>() )
            ( x1 )( x2 )( M_periodicity.translation() ).error( "invalid point size" );

            if ( ublas::norm_2( x1-( x2-M_periodicity.translation() ) ) < 1e-10 )
            {
                // loop on each pair (element, lid) which
                // has a global id gid2 and set it to gid
                corresponding_gid = gid2;
                match = true;
                break;
            }
        }

        // if we have --- actually we must have one --- a match, remove the
        // iterator from dof2 to quicken the search for the next dof1 match
        if ( match )
        {
            size_type ie1 = dof.second.template get<0>();
            size_type lid1 = dof.second.template get<1>();
            size_type c1 = dof.second.template get<2>();
            size_type gDof1 = dof.second.template get<3>();
            uint16_type dof1_type = dof.second.template get<4>();

            VLOG(2) << "matching dof id " << gid << " with dof id=" << corresponding_gid << "\n";

            it_dof2 = periodic_dof[M_periodicity.tag2()].lower_bound( corresponding_gid );
            en_dof2 = periodic_dof[M_periodicity.tag2()].upper_bound( corresponding_gid );
            VLOG(2) << "distance = " << std::distance( it_dof2, en_dof2 ) << "\n";

            while ( it_dof2 != en_dof2 )
            {

                size_type ie = it_dof2->second.template get<0>();
                size_type lid = it_dof2->second.template get<1>();
                size_type c2 = it_dof2->second.template get<2>();
                CHECK( c1 == c2 ) << "[periodic] invalid dof component, c1 = " << c1 << ", c2 = " << c2 << "\n";
                size_type gDof = it_dof2->second.template get<3>();
                uint16_type dof2_type = it_dof2->second.template get<4>();
                uint16_type dof1_type = dof.second.template get<4>();

                FEELPP_ASSERT( dof1_type == dof2_type )
                    ( gid )( it_dof2->first )( gDof )( lid )( c2) ( ie )
                ( dof1_type )( dof2_type ).error ( "invalid dof" );

                VLOG(2) << "link " <<  M_el_l2g.left.find( LocalDof( ie, localDofId(lid,c2) ) )->second.index()  << " -> " << gid << "\n"
                        << "element id1: " << ie1 << ", lid1: " << lid1 << ", c1: " << c1 << ",  gDof1: " << gDof1 << ", type1: " << dof1_type << "\n"
                        << "element id2: " << ie << ", lid2: " << lid << ", c2: " << c2 << ",  gDof2: " << gDof << ", type: " << dof2_type << "\n";

                // gid is given by dof1
                auto it = M_el_l2g.left.find(LocalDof( ie, localDofId(lid,c2) ));
                bool successful_modify = M_el_l2g.left.modify_data( it, bimaps::_data = Dof( boost::make_tuple( gid, 1, true ) ) );

                CHECK( successful_modify ) << "modify periodic dof table failed: element id "
                                           << ie << " local dof id " << lid << " component " << c2;
                // warning: must modify the data structure that allows to
                // generate unique global dof ids
                CHECK( ( map_gdof[  boost::make_tuple( dof2_type, c2, gDof ) ] == corresponding_gid ) ||
                       ( map_gdof[ boost::make_tuple( dof2_type, c2, gDof ) ] == gid ) )
                    << "[periodic] invalid matching periodic gid, "
                    << "corresponding_gid = " << corresponding_gid << ", dof2_type = " <<  dof2_type
                    << ", gDof = " << gDof << ", gid=" << gid
                    << ", c2 = " << c2
                    << ", map_gdof[ boost::make_tuple( dof2_type, c2, gDof ) ]= "
                    << map_gdof[ boost::make_tuple( dof2_type, c2, gDof ) ] << "\n";

                VLOG(2) << "link mapgdof " <<   map_gdof[ boost::make_tuple( dof2_type, c2, gDof ) ]  << " -> " << gid << "\n";
                map_gdof[ boost::make_tuple( dof2_type, c2, gDof ) ] = gid;

                FEELPP_ASSERT( map_gdof[ boost::make_tuple( dof2_type, c2, gDof ) ] == gid )
                ( corresponding_gid )( dof2_type )( gDof )( gid )
                ( map_gdof[ boost::make_tuple( dof2_type, c2, gDof ) ] ) .error ( "invalid gid" );

                ++it_dof2;
            }

            it_dof2 = periodic_dof[M_periodicity.tag2()].lower_bound( corresponding_gid );
            periodic_dof[M_periodicity.tag2()].erase( it_dof2, en_dof2 );
            periodic_dof_done[gid] =  true;
        }

        else
        {
            // we have a problem, no match was found, this should not happen
            VLOG(2) << "[periodic] invalid point/dof matching\n";
            VLOG(2) << "[periodic] n = " << x1 << "\n";
        }

    }
    VLOG(2) << "[periodic dof table] done matching the dof points\n";
    VLOG(2) << "[periodic dof table] is empty : " << periodic_dof[M_periodicity.tag2()].empty() << "\n";

    // ensure that periodic_dof[M_periodicity.tag2()] is empty
    if ( !periodic_dof[M_periodicity.tag2()].empty() )
    {
        VLOG(2) << "[periodic] periodic conditions not set properly, some periodic dof were not assigned\n";
        typename periodic_dof_map_type::iterator it_dof2 = periodic_dof[M_periodicity.tag2()].begin();
        typename periodic_dof_map_type::iterator en_dof2 = periodic_dof[M_periodicity.tag2()].end();

        while ( it_dof2 != en_dof2 )
        {

            size_type ie = it_dof2->second.template get<0>();
            size_type lid = it_dof2->second.template get<1>();

            VLOG(2) << "[periodic] dof " << it_dof2->first << " not assigned, "
                          << "x = " << periodic_dof_points[it_dof2->first].template get<0>() << " "
                          << "elt = " << ie << ", lid= " << lid << "\n";



            ++it_dof2;
        }
    }

    else
    {
        VLOG(2) << "[periodic] periodic condition done\n";
    }

    return max_gid+1;
}

template<typename MeshType, typename FEType, typename PeriodicityType>
size_type
DofTable<MeshType, FEType, PeriodicityType>::buildLocallyDiscontinuousDofMap( mesh_type& M, size_type start_next_free_dof )
{
    typedef typename continuity_type::template apply<MeshType, self_type> builder;
    return fusion::accumulate( typename continuity_type::discontinuity_markers_type(), start_next_free_dof,  builder( M, *this ) );
}
template<typename MeshType, typename FEType, typename PeriodicityType>
void
DofTable<MeshType, FEType, PeriodicityType>::buildDofMap( mesh_type& M, size_type start_next_free_dof )
{
    if ( !M_dof_indices.empty() )
    {
        generateDofPoints( M );
        return;
    }

    size_type nldof =
        fe_type::nDofPerVolume * element_type::numVolumes +
        fe_type::nDofPerFace * element_type::numGeometricFaces +
        fe_type::nDofPerEdge * element_type::numEdges +
        fe_type::nDofPerVertex * element_type::numVertices;

    FEELPP_ASSERT( nldof == fe_type::nLocalDof )
    ( nldof )
    ( fe_type::nLocalDof ).error( "Something wrong in FE specification" ) ;

#if !defined(FEELPP_ENABLE_MPI_MODE) // sequential if (this->worldComm().size()==1)

    const size_type n_proc  = M.worldComm().localSize();

    //! list of elements which have a periodic face Tag2
    std::list<std::pair<element_type const*, face_type const*> > periodic_elements;

    size_type next_free_dof = start_next_free_dof;

    for ( size_type processor=0; processor<n_proc; processor++ )
    {
        // compute the number of dof on current processor
        element_const_iterator it_elt = M.beginElementWithProcessId( processor );
        element_const_iterator en_elt = M.endElementWithProcessId( processor );
        size_type n_elts = std::distance( it_elt, en_elt );
        DVLOG(2) << "[buildDofMap] n_elts =  " << n_elts << " on processor " << processor << "\n";
        this->M_first_df[processor] = next_free_dof;

        if ( is_periodic || is_discontinuous_locally )
            this->M_first_df[processor] =  0;

        it_elt = M.beginElementWithProcessId( processor );

        DVLOG(2) << "[buildDofMap] starting with elt " << it_elt->id() << "\n";

        for ( ; it_elt!=en_elt; ++it_elt )
        {
            this->addDofFromElement( *it_elt, next_free_dof, processor );
        } // elements loop
        // printing Dof table only in debug mode
#if !defined( NDEBUG )
        const int ncdof = is_product?nComponents:1;

        for ( int c = 0; c < ncdof; ++c )
        {
            it_elt = M.beginElementWithProcessId( processor );

            for ( ; it_elt!=en_elt; ++it_elt )
            {
                element_type const& __elt = *it_elt;
                std::ostringstream ostr;
                ostr << "element " << __elt.id() << " : ";

                for ( uint16_type l = 0; l < nldof; ++l )
                {
                    ostr << M_el_l2g.left.find( LocalDof( __elt.id(), fe_type::nLocalDof*c + l ) )->second.index() << " ";
                }

                DVLOG(2) << ostr.str() << "\n";
            }
        }

#endif
        this->M_last_df[processor] = next_free_dof-1;

        // update info
        this->M_n_localWithGhost_df[processor] = this->M_last_df[processor] - this->M_first_df[processor] + 1;
        this->M_n_localWithoutGhost_df[processor]=this->M_n_localWithGhost_df[processor];
        this->M_first_df_globalcluster[processor]=this->M_first_df[processor];
        this->M_last_df_globalcluster[processor]=this->M_last_df[processor];
    }

    this->M_n_dofs = next_free_dof;

#if !defined(NDEBUG)
    DVLOG(2) << " n global dof " << nDof() << "\n";
    DVLOG(2) << " n local dof " << nLocalDof() << "\n";
#endif

    for ( size_type processor=0; processor<M.worldComm().localSize(); processor++ )
    {
        DVLOG(2) << "o processor " << processor << "\n";
        DVLOG(2) << "  - n dof on proc " << nDofOnProcessor( processor ) << "\n";
        DVLOG(2) << "  - first dof " << firstDof( processor ) << "\n";
        DVLOG(2) << "  - last dof " << lastDof( processor ) << "\n";
    }

    //if ( is_continuous )
    //checkDofContinuity( M, mpl::int_<fe_type::nDim>() );

    for ( size_type processor=0; processor<n_proc; processor++ )
    {
        auto it_elt = M.beginElementWithProcessId( processor );
        auto en_elt = M.endElementWithProcessId( processor );

        for ( ; it_elt != en_elt; ++it_elt )
        {
            size_type elid= it_elt->id();

            for ( int i = 0; i < FEType::nLocalDof; ++i )
            {
                int nc1 = ( is_product?nComponents1:1 );

                for ( int c1 =0; c1 < nc1; ++c1 )
                {
                    int ind = FEType::nLocalDof*c1+i;
                    boost::tie( M_locglob_indices[elid][ind],
                                M_locglob_signs[elid][ind], boost::tuples::ignore ) =
                                    localToGlobal( elid, i, c1 );
                }
            }
        }
    }

#else // MPI_MODE
    // compute the number of dof on current processor
    auto it_elt = M.beginElementWithProcessId();
    auto en_elt = M.endElementWithProcessId();
    bool hasNoElt = ( it_elt == en_elt );

    //size_type n_elts = std::distance( it_elt, en_elt);
    //DVLOG(2) << "[buildDofMap] n_elts =  " << n_elts << " on processor " << processor << "\n";

    size_type theFirstDf = start_next_free_dof;

    if ( is_periodic || is_discontinuous_locally )
        theFirstDf = 0;

    //if ( is_periodic || is_discontinuous_locally )
    //    this->M_first_df[processor] =  0;

    size_type next_free_dof = start_next_free_dof;

    for ( ; it_elt!=en_elt; ++it_elt )
    {
        this->addDofFromElement( *it_elt, next_free_dof, this->worldComm().localRank() );
    } // elements loop

#if 0
        for ( auto mit = M_dof_marker.right.begin(), men = M_dof_marker.right.end() ; mit != men ; ++mit )
        {
            LOG(INFO) << "marker " << mit->first << " dof id " << mit->second;
        }
#endif

#if 0
    LOG(INFO) << "local to global view";
    for( auto it = M_el_l2g.left.begin(), en = M_el_l2g.left.end();
         it != en; ++it )
    {
        LOG(INFO) << "local dof (" << it->first.elementId()<< ","<< it->first.localDof() << ") --> global dof " << it->second.index();
    }
    LOG(INFO) << "global to local  view";
    for( auto it = M_el_l2g.right.begin(), en = M_el_l2g.right.end();
         it != en; ++it )
    {
        LOG(INFO) << "global dof " << it->first.index() << " --> local dof (" << it->second.elementId()<< ","<< it->second.localDof() << ")";
    }
#endif
    const size_type thelastDof = ( !hasNoElt )?next_free_dof-1:0;

    std::vector<boost::tuple<bool,size_type,size_type> > dataRecvFromGather;
    auto dataSendToGather = boost::make_tuple(hasNoElt,theFirstDf,thelastDof);
    mpi::all_gather( this->worldComm().localComm(),
                     dataSendToGather,
                     dataRecvFromGather );

    for (int p=0;p<this->worldComm().localSize();++p)
    {
        bool procHasNoElt = dataRecvFromGather[p].template get<0>();
        this->M_first_df[p] = dataRecvFromGather[p].template get<1>();
        this->M_last_df[p] = dataRecvFromGather[p].template get<2>();

        size_type mynDofWithGhost = ( !procHasNoElt )?
            this->M_last_df[p] - this->M_first_df[p] + 1 :
            this->M_first_df[p];
        this->M_n_localWithGhost_df[p] = mynDofWithGhost;
    }

#if 0
    std::cout << "\n build Dof Map --2---with god rank " << this->worldComm().godRank()
              << " local rank DofT " << this->worldComm().localRank()
              << " local rank mesh " << M.worldComm().localRank()
              << std::endl;
#endif

    // only true in sequential, redefine in buildDofGhostMap
    this->M_n_localWithoutGhost_df[this->worldComm().localRank()]=this->M_n_localWithGhost_df[this->worldComm().localRank()];
    this->M_first_df_globalcluster[this->worldComm().localRank()]=this->M_first_df[this->worldComm().localRank()];
    this->M_last_df_globalcluster[this->worldComm().localRank()]=this->M_last_df[this->worldComm().localRank()];
    this->M_n_dofs = next_free_dof;

    it_elt = M.beginElementWithProcessId();

    for ( ; it_elt != en_elt; ++it_elt )
    {
        size_type elid= it_elt->id();

        for ( int i = 0; i < FEType::nLocalDof; ++i )
        {
            int nc1 = ( is_product?nComponents1:1 );

            for ( int c1 =0; c1 < nc1; ++c1 )
            {
                int ind = FEType::nLocalDof*c1+i;
                auto const& dof = localToGlobal( elid, i, c1 );
                M_locglob_indices[elid][ind] = dof.index();
                M_locglob_signs[elid][ind] = dof.sign();
            }
        }
    }

#endif // MPI_MODE
    generateDofPoints( M );

}

template<typename MeshType, typename FEType, typename PeriodicityType>
void
DofTable<MeshType, FEType, PeriodicityType>::buildBoundaryDofMap( mesh_type& M )
{
    size_type nDofF = nLocalDofOnFace(true);
    M_n_dof_per_face_on_bdy = nDofF;
    DVLOG(2) << "vertex dof : " <<  face_type::numVertices * fe_type::nDofPerVertex << "\n";
    DVLOG(2) << "edge dof : " <<  face_type::numEdges * fe_type::nDofPerEdge << "\n";
    DVLOG(2) << "face dof : " << face_type::numFaces * fe_type::nDofPerFace  << "\n";
    DVLOG(2) << "number of Dof on an Element Face : " << nDofF << "\n";

    if ( nDofF == 0 ) return;

    //
    // Face dof
    //
#if defined(FEELPP_ENABLE_MPI_MODE)
    auto __face_it = M.facesWithProcessId( M.worldComm().localRank() ).first;
    auto __face_en = M.facesWithProcessId( M.worldComm().localRank() ).second;
#else
    typename mesh_type::face_const_iterator __face_it = M.beginFace();
    typename mesh_type::face_const_iterator __face_en = M.endFace();
#endif

    const size_type nF = M.faces().size();
    int ntldof = nLocalDofOnFace();

    DVLOG(2) << "[buildBoundaryDofMap] nb faces : " << nF << "\n";
    DVLOG(2) << "[buildBoundaryDofMap] nb dof faces : " << nDofF*nComponents << "\n";

    for ( size_type nf = 0; __face_it != __face_en; ++__face_it, ++nf )
    {
        LOG_IF(WARNING, !__face_it->isConnectedTo0() )
            << "face " << __face_it->id() << " not connected"
            << " marker : " << __face_it->marker()
            << " connectedTo0 : " << __face_it->isConnectedTo0()
            << " connectedTo1 : " << __face_it->isConnectedTo1();

        if ( !__face_it->isConnectedTo0() ) continue;

#if !defined(NDEBUG)

        if (  __face_it->isOnBoundary() )
            DVLOG(4) << "[buildBoundaryDofMap] boundary global face id : " << __face_it->id()
                     << " marker: " << __face_it->marker()<< "\n";

        else
            DVLOG(4) << "[buildBoundaryDofMap] global face id : " << __face_it->id() << "\n";

#endif
        uint16_type lcVertex = 0;
        uint16_type lcEdge = 0;
        uint16_type lcFace = 0;


        addVertexBoundaryDof( __face_it, lcVertex );
        addEdgeBoundaryDof( __face_it, lcEdge );
        addFaceBoundaryDof( __face_it, lcFace );

    }

#if !defined(NDEBUG)
    __face_it = M.facesWithProcessId( M.worldComm().localRank() ).first;
    __face_en = M.facesWithProcessId( M.worldComm().localRank() ).second;
    for ( ; __face_it != __face_en; ++__face_it )
        for ( int face_dof_id = 0; face_dof_id < int( ntldof ); ++face_dof_id )
            FEELPP_ASSERT( boost::get<0>( M_face_l2g[__face_it->id()][face_dof_id] ) != invalid_size_type_value )( __face_it->id() )( face_dof_id ).warn( "invalid dof table: initialized dof entries" );

#endif
}    // updateBoundaryDof

template<typename MeshType, typename FEType, typename PeriodicityType>
void
DofTable<MeshType, FEType, PeriodicityType>::generateDofPoints(  mesh_type& M )
{
    if ( !M_dof_points.empty() )
        return;

    if ( fe_type::is_modal )
        return;

    DVLOG(2) << "[Dof::generateDofPoints] generating dof coordinates\n";
    typedef typename gm_type::template Context<vm::POINT, element_type> gm_context_type;
    typedef boost::shared_ptr<gm_context_type> gm_context_ptrtype;

    typedef typename fe_type::template Context<vm::POINT, fe_type, gm_type, element_type> fecontext_type;

    gm_ptrtype gm( new gm_type );
    fe_type fe;
    //
    // Precompute some data in the reference element for
    // geometric mapping and reference finite element
    //
    typename gm_type::precompute_ptrtype __geopc( new typename gm_type::precompute_type( gm, fe.points() ) );

    //const uint16_type ndofv = fe_type::nDof;

    element_const_iterator it_elt = M.beginElementWithProcessId( M.worldComm().localRank() );
    element_const_iterator en_elt = M.endElementWithProcessId( M.worldComm().localRank() );

    if ( it_elt == en_elt )
        return;

    gm_context_ptrtype __c( new gm_context_type( gm, *it_elt, __geopc ) );

    std::vector<bool> dof_done( nLocalDofWithGhost() );
    M_dof_points.resize( nLocalDofWithGhost() );
    std::fill( dof_done.begin(), dof_done.end(), false );

    for ( size_type dof_id = 0; it_elt!=en_elt ; ++it_elt )
    {
        __c->update( *it_elt );

        for ( uint16_type l =0; l < fe_type::nLocalDof; ++l )
        {
            int ncdof  = is_product?nComponents:1;

            for ( uint16_type c1 = 0; c1 < ncdof; ++c1 )
            {
                size_type thedof = boost::get<0>( localToGlobal( it_elt->id(), l, c1 ) );

                if ( ( thedof >= firstDof() ) && ( thedof <= lastDof() ) )
                {
                    // get only the local dof
                    //size_type thedofonproc = thedof - firstDof();
                    thedof -= firstDof();
                    DCHECK( thedof < nLocalDofWithGhost() )
                        << "invalid local dof index "
                        <<  thedof << ", " << nLocalDofWithGhost() << "," << firstDof()  << ","
                        <<  lastDof() << "," << it_elt->id() << "," << l << "," <<  c1;

                    if ( dof_done[ thedof ] == false )
                    {
                        std::set<uint16_type> lid;
                        lid.insert( l );
                        //M_dof_points[dof_id] = boost::make_tuple( thedof, __c->xReal( l ) );
                        M_dof_points[thedof] = boost::make_tuple( __c->xReal( l ), firstDof()+thedof, c1 );
                        dof_done[thedof] = true;
                        ++dof_id;
                    }
                }
            }
        }
    }

    for ( size_type dof_id = 0; dof_id < nLocalDofWithGhost() ; ++dof_id )
    {
        CHECK( boost::get<1>( M_dof_points[dof_id] ) >= firstDof() &&
               boost::get<1>( M_dof_points[dof_id] ) <= lastDof() )
            <<  "invalid dof point "
            <<  dof_id << ", " <<  firstDof() << ", " <<  lastDof() << ", " <<  nLocalDofWithGhost()
            << ", " << boost::get<1>( M_dof_points[dof_id] )
            << ", " <<  boost::get<0>( M_dof_points[dof_id] ) ;
        CHECK( dof_done[dof_id] == true )
            << "invalid dof point"
            << dof_id << ", " <<  nLocalDofWithGhost() << ", " <<  firstDof() << ", "
            <<  lastDof() << ", " <<  fe_type::nDim << ", " <<  fe_type::nLocalDof;
    }

    DVLOG(2) << "[Dof::generateDofPoints] generating dof coordinates done\n";
}
template<typename MeshType, typename FEType, typename PeriodicityType>
void
DofTable<MeshType, FEType, PeriodicityType>::generatePeriodicDofPoints(  mesh_type& M,
        periodic_element_list_type const& periodic_elements,
        dof_points_type& periodic_dof_points )
{
    if ( fe_type::is_modal )
        return;

    DVLOG(2) << "[Dof::generateDofPoints] generating dof coordinates\n";
    typedef typename gm_type::template Context<vm::POINT, element_type> gm_context_type;
    typedef boost::shared_ptr<gm_context_type> gm_context_ptrtype;

    typedef typename fe_type::template Context<vm::POINT, fe_type, gm_type, element_type> fecontext_type;

    gm_ptrtype gm( new gm_type );
    fe_type fe;
    //
    // Precompute some data in the reference element for
    // geometric mapping and reference finite element
    //
    typename gm_type::precompute_ptrtype __geopc( new typename gm_type::precompute_type( gm, fe.points() ) );

    //const uint16_type ndofv = fe_type::nDof;

    periodic_element_list_const_iterator it_elt = periodic_elements.begin();
    periodic_element_list_const_iterator en_elt = periodic_elements.end();

    if ( it_elt == en_elt )
        return;

    gm_context_ptrtype __c( new gm_context_type( gm, *it_elt->template get<0>(), __geopc ) );

    std::vector<bool> dof_done( periodic_dof_points.size() );
    std::fill( dof_done.begin(), dof_done.end(), false );

    for ( size_type dof_id = 0; it_elt!=en_elt ; ++it_elt )
    {
        __c->update( *it_elt->template get<0>() );

        face_type const& __face = *it_elt->template get<1>();

        size_type iElAd = __face.ad_first();
        FEELPP_ASSERT( iElAd != invalid_size_type_value )( __face.id() ).error( "[periodic]invalid face/element in face" );
        Feel::detail::ignore_unused_variable_warning( iElAd );

        // local id of the face in its adjacent element
        uint16_type iFaEl = __face.pos_first();
        FEELPP_ASSERT( iFaEl != invalid_uint16_type_value ).error ( "invalid element index in face" );

        int ncdof  = is_product?nComponents:1;

        for ( uint16_type c1 = 0; c1 < ncdof; ++c1 )
        {
            // loop on face vertices
            for ( uint16_type iVeFa = 0; iVeFa < face_type::numVertices; ++iVeFa )
            {
                // local vertex number (in element)
                uint16_type iVeEl = element_type::fToP( iFaEl, iVeFa );
                Feel::detail::ignore_unused_variable_warning( iVeEl );

                FEELPP_ASSERT( iVeEl != invalid_uint16_type_value ).error( "invalid local dof" );

                // Loop number of Dof per vertex
                for ( uint16_type l = 0; l < fe_type::nDofPerVertex; ++l )
                {
                    uint16_type lid = iVeEl * fe_type::nDofPerVertex + l;

                    size_type thedof = boost::get<0>( localToGlobal( it_elt->template get<0>()->id(), lid, c1 ) );
                    FEELPP_ASSERT( thedof < dof_done.size() )
                        ( thedof )
                        ( dof_done.size() )
                        ( c1 )
                        ( it_elt->template get<0>()->id() )
                        ( lid ).error ( "[generatePeriodicDofPoints] invalid dof id" );

                    if ( dof_done[ thedof ] == false )
                    {
                        periodic_dof_points[thedof] = boost::make_tuple( __c->xReal( lid ), thedof, c1 );
                        // these tests are problem specific x=0 and x=translation
#if 0

                        if ( __face.marker().value() == M_periodicity.tag1() )
                            FEELPP_ASSERT( math::abs( __c->xReal( lid )[0] ) < 1e-10 )( __c->xReal( lid ) ).warn( "[periodic] invalid p[eriodic point tag1" );

                        if ( __face.marker().value() == M_periodicity.tag2() )
                            FEELPP_ASSERT( math::abs( __c->xReal( lid )[0] - M_periodicity.translation()[0] ) < 1e-10 )
                                ( __c->xReal( lid ) )( M_periodicity.translation() ).warn( "[periodic] invalid p[eriodic point tag1" );

#endif
                        dof_done[thedof] = true;
                        ++dof_id;
                    }
                }
                // loop on edge
                for ( uint16_type l = 0; l < fe_type::nDofPerEdge; ++l )
                {
                    uint16_type lid = element_type::numVertices*fe_type::nDofPerVertex + iFaEl * fe_type::nDofPerEdge + l;
                    size_type thedof = boost::get<0>( localToGlobal( it_elt->template get<0>()->id(), lid, c1 ) );
                    FEELPP_ASSERT( thedof < dof_done.size() )
                        ( thedof )
                        ( dof_done.size() )
                        ( c1 )
                        ( it_elt->template get<0>()->id() )
                        ( lid ).error ( "[generatePeriodicDofPoints] invalid dof id" );

                    if ( dof_done[ thedof ] == false )
                    {
                        periodic_dof_points[thedof] = boost::make_tuple( __c->xReal( lid ), thedof, c1 );
                        // these tests are problem specific x=0 and x=translation
#if 0

                        if ( __face.marker().value() == M_periodicity.tag1() )
                            FEELPP_ASSERT( math::abs( __c->xReal( lid )[1] +1 ) < 1e-10 )( __c->xReal( lid ) ).warn( "[periodic] invalid p[eriodic point tag1" );

                        if ( __face.marker().value() == M_periodicity.tag2() )
                            FEELPP_ASSERT( math::abs( __c->xReal( lid )[1] - ( M_periodicity.translation()[1]-1 ) ) < 1e-10 )
                                ( __c->xReal( lid ) )( M_periodicity.translation() ).warn( "[periodic] invalid p[eriodic point tag1" );

#endif
                        dof_done[thedof] = true;
                        ++dof_id;
                    }
                }
            }
        }
    }
    for ( size_type dof_id = 0; dof_id < periodic_dof_points.size() ; ++dof_id )
    {
        FEELPP_ASSERT( boost::get<1>( periodic_dof_points[dof_id] ) >= 0 &&
                       boost::get<1>( periodic_dof_points[dof_id] ) < periodic_dof_points.size() )
        ( dof_id )( periodic_dof_points.size() )
        ( boost::get<1>( periodic_dof_points[dof_id] ) )
        ( boost::get<0>( periodic_dof_points[dof_id] ) ).error( "invalid dof point" );
        FEELPP_ASSERT( dof_done[dof_id] == true )( dof_id ).error( "invalid dof point" );
    }
}


template<typename MeshType, typename FEType, typename PeriodicityType>
void
DofTable<MeshType, FEType, PeriodicityType>::addSubstructuringDofMap( mesh_type const& M, size_type next_free_dof )
{
    addSubstructuringDofVertex( M, next_free_dof );
    addSubstructuringDofEdge( M, next_free_dof, mpl::int_<nDim>() );
    addSubstructuringDofFace( M, next_free_dof, mpl::int_<nDim>() );
}

template<typename MeshType, typename FEType, typename PeriodicityType>
void
DofTable<MeshType, FEType, PeriodicityType>::addSubstructuringDofVertex(mesh_type const& M,
                                                                        size_type next_free_dof )
{
        std::cout << "found CrossPoints and WireBasket\n";
        std::cout << "n cp: " << std::distance( M.beginPointWithMarker( M.markerName("CrossPoints") ), M.endPointWithMarker( M.markerName("CrossPoints") ) ) << "\n";
#if 0
        std::cout << "n wb: " << std::distance( M.beginEdgeWithMarker( M.markerName("WireBasket") ), M.endEdgeWithMarker( M.markerName("WireBasket") ) ) << "\n";
#endif
        // go through all the crosspoints and add them to the dof table

        for( auto pit = M.beginPointWithMarker( M.markerName("CrossPoints") ),
                 pen = M.endPointWithMarker( M.markerName("CrossPoints") );
             pit!=pen; ++pit )
        {
            // get one element
            auto __elt = M.element( *pit->elements().begin() );
            size_type ie = __elt.id();
            int lc = 0;
            for ( uint16_type i = 0; i < element_type::numVertices; ++i )
            {
                for ( uint16_type l = 0; l < fe_type::nDofPerVertex; ++l, ++lc )
                {
                    if (__elt.point( i ).id()==pit->id() )
                    {
                        const size_type gDof = ( __elt.point( i ).id() ) * fe_type::nDofPerVertex + l;
                        this->insertDof( ie, lc, i, boost::make_tuple( 0, 0, gDof ),
                                         M.worldComm().localRank(), next_free_dof, 1, false, 0 );
                        std::cout << "Adding crosspoint " << pit->id() << " with dof " << next_free_dof << "\n";
                    }
                }
            }
        }
}

template<typename MeshType, typename FEType, typename PeriodicityType>
void
DofTable<MeshType, FEType, PeriodicityType>::addSubstructuringDofEdge( mesh_type const& M,
                                                                       size_type next_free_dof,
                                                                       mpl::int_<1> )
{}

template<typename MeshType, typename FEType, typename PeriodicityType>
void
DofTable<MeshType, FEType, PeriodicityType>::addSubstructuringDofEdge( mesh_type const& M,
                                                                       size_type next_free_dof,
                                                                       mpl::int_<2> )
{}

template<typename MeshType, typename FEType, typename PeriodicityType>
void
DofTable<MeshType, FEType, PeriodicityType>::addSubstructuringDofEdge( mesh_type const& M,
                                                                       size_type next_free_dof,
                                                                       mpl::int_<3> )
{
    // go through all Wirebasket edges
    for( auto pit = M.beginEdgeWithMarker( M.markerName("WireBasket") ),
             pen = M.endEdgeWithMarker( M.markerName("WireBasket") );
         pit!=pen; ++pit )
    {
        auto __elt = M.element( *pit->elements().begin() );
        std::cout << "Adding wirebasket edge " << pit->id() << " using element "  << __elt.id() << "\n";
        size_type ie = __elt.id();
        uint16_type lc = 0;

        for ( uint16_type i = 0; i < element_type::numEdges; ++i )
        {
            for ( uint16_type l = 0; l < fe_type::nDofPerEdge; ++l, ++lc )
            {
                if (__elt.edge( i ).id()==pit->id() )
                {
                    size_type gDof = __elt.edge( i ).id() * fe_type::nDofPerEdge;
                    int32_type sign = 1;

                    if ( __elt.edgePermutation( i ).value()  == edge_permutation_type::IDENTITY )
                    {
                        gDof += l ; // both nodal and modal case
                    }
                    else if ( __elt.edgePermutation( i ).value()  == edge_permutation_type::REVERSE_PERMUTATION )
                    {

                        if ( fe_type::is_modal )
                        {
                            //only half of the modes (odd polynomial order) are negative.
                            sign = ( l%2 )?( -1 ):( 1 );
                            gDof += l;
                        }

                        else
                            gDof += fe_type::nDofPerEdge - 1 - l ;
                    }
                    else
                        FEELPP_ASSERT( 0 ).error ( "invalid edge permutation" );

                    this->insertDof( ie, lc, i, boost::make_tuple( 1, 0, gDof ), M.worldComm().localRank(), next_free_dof, sign, false, 0 );
                    std::cout << "Adding wirebasket edge " << pit->id() << " with dof " << next_free_dof << "\n";
                }
            }
        }

    }
}
template<typename MeshType, typename FEType, typename PeriodicityType>
void
DofTable<MeshType, FEType, PeriodicityType>::addSubstructuringDofFace( mesh_type const& M,
                                                                       size_type next_free_dof,
                                                                       mpl::int_<1> )
{}

template<typename MeshType, typename FEType, typename PeriodicityType>
void
DofTable<MeshType, FEType, PeriodicityType>::addSubstructuringDofFace( mesh_type const& M,
                                                                       size_type next_free_dof,
                                                                       mpl::int_<2> )
{}

template<typename MeshType, typename FEType, typename PeriodicityType>
void
DofTable<MeshType, FEType, PeriodicityType>::addSubstructuringDofFace( mesh_type const& M,
                                                                       size_type next_free_dof,
                                                                       mpl::int_<3> )
{
    std::vector<std::string> faces = assign::list_of("TOP")("BOTTOM")("NORTH")("EAST")("WEST")("SOUTH");
    BOOST_FOREACH( auto face, faces )
    {
        auto faces = markedfaces( &M, face );

        for( auto pit = faces.template get<1>(), pen = faces.template get<2>(); pit!=pen; ++pit )
        {
            auto __elt = M.element( *pit->elements().begin() );
            std::cout << "Adding face " << pit->id() << " with marker " << face << " using element "  << __elt.id() << "\n";
            size_type ie = __elt.id();
            uint16_type lc = 0;

            for ( uint16_type i = 0; i < element_type::numFaces; ++i )
            {
                face_permutation_type permutation = __elt.facePermutation( i );
                FEELPP_ASSERT( permutation != face_permutation_type( 0 ) ).error ( "invalid face permutation" );

                // Polynomial order in each direction
                uint16_type p=1;
                uint16_type q=0;

                // MaxOrder = Order - 2
                int MaxOrder = int( ( 3 + std::sqrt( 1+8*fe_type::nDofPerFace ) )/2 ) - 2;

                for ( uint16_type l = 0; l < fe_type::nDofPerFace; ++l, ++lc )
                {
                    if (__elt.face( i ).id()==pit->id() )
                    {
                        // TODO: orient the dof indices such
                        // that they match properly the faces
                        // dof of the connected faces. There
                        // are a priori many permutations of
                        // the dof face indices
                        size_type gDof = __elt.face( i ).id() * fe_type::nDofPerFace;
                        int32_type sign = 1;

                        q=q+1;

                        if ( q > MaxOrder )
                        {
                            q = 1;
                            p = p+1;
                            MaxOrder = MaxOrder-1;
                        }

                        if ( !fe_type::is_modal )
                        {
                            // no need of permutation is identity or only one dof on face
                            if ( permutation  == face_permutation_type( 1 ) || fe_type::nDofPerFace == 1 )
                                gDof += l;

                            else
                                gDof += vector_permutation[permutation][l];
                        }

                        else
                        {
                            gDof += l;

                            if ( permutation == face_permutation_type( 2 ) )
                            {
                                // Reverse sign if polynomial order in
                                // eta_1 direction is odd

                                if ( p%2 == 0 )
                                    sign = -1;

                            }
                        }

                        this->insertDof( ie, lc, i, boost::make_tuple( 2, 0, gDof ), M.worldComm().localRank(), next_free_dof, sign, false, 0 );
                        std::cout << "Adding face " << pit->id() << " with dof " << next_free_dof << "\n";
                    }
                }
            }

        }
    }

}

template<typename MeshType, typename FEType, typename PeriodicityType>
std::pair<std::map<size_type,size_type>,std::map<size_type,size_type> >
DofTable<MeshType, FEType, PeriodicityType>::pointIdToDofRelation(std::string fname) const
{
    std::map<size_type,size_type> pidtodof,doftopid;
    element_const_iterator it_elt = M_mesh->beginElementWithProcessId( M_mesh->worldComm().localRank() );
    element_const_iterator en_elt = M_mesh->endElementWithProcessId( M_mesh->worldComm().localRank() );

    if ( it_elt == en_elt )
        return std::make_pair(doftopid,pidtodof);

    for ( size_type dof_id = 0; it_elt!=en_elt ; ++it_elt )
    {
        for ( uint16_type i = 0; i < M_mesh->numLocalVertices(); ++i )
        {
                int ncdof  = is_product?nComponents:1;
                for ( uint16_type c1 = 0; c1 < ncdof; ++c1 )
                {
                    const size_type gDof = ( it_elt->point( i ).id() );
                    size_type thedof = boost::get<0>( localToGlobal( it_elt->id(), i, c1 ) );
                    //pidtodof[ncdof*it_elt->point(l).id()+c1] = thedof;
                    pidtodof[ncdof*gDof+c1] = thedof;
                    doftopid[thedof] = ncdof*gDof+c1;

                }
        }
    }
    if ( !fname.empty() )
    {
        std::ostringstream os1,os2;
        os1 << fs::path( fname ).stem().string() << "_pidtodof" << fs::path( fname ).extension().string();
        os2 << fs::path( fname ).stem().string() << "_doftopid" << fs::path( fname ).extension().string();
        std::ofstream ofs( os1.str().c_str() );
        auto it = pidtodof.begin();
        auto en = pidtodof.end();
        std::for_each( it, en,
                       [&ofs]( std::pair<size_type, size_type> const& p )
                       {
                           ofs << p.first << " " << p.second << "\n";
                       });
        std::ofstream ofs2( os2.str().c_str() );
        it = doftopid.begin();
        en = doftopid.end();
        std::for_each( it, en,
                       [&ofs2]( std::pair<size_type, size_type> const& p )
                       {
                           ofs2 << p.first << " " << p.second << "\n";
                       });

    }
    return std::make_pair(doftopid,pidtodof);
}
} // namespace Feel



#if defined(FEELPP_ENABLE_MPI_MODE)
#include <feel/feeldiscr/doftablempi.hpp>
#endif

#endif //_DOFTABLE_HH
