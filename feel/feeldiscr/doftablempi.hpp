/*
  This file is part of the Feel library

  Copyright (C) 2012 Université de Grenoble 1

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
 * \file doftablempi.hpp
 * \author Vincent Chabannes
 */
#if !defined(FEELPP_DOFTABLE_MPI_HPP)
#define FEELPP_DOFTABLE_MPI_HPP 1

namespace Feel
{

template<typename MeshType, typename FEType, typename PeriodicityType>
void
DofTable<MeshType, FEType, PeriodicityType>::buildGhostDofMap( mesh_type& mesh )
{
#if 0 //old

    // dofs map on interprocess faces : ( dofOnMyRank, tuple< otherProcRank, dofOnOtherRank> )
    std::map<size_type,boost::tuple<size_type,size_type> > mapInterProcessDof;
    // dofs no present on interprocess faces : ( dofOnMyRank, tuple< otherProcRank, dofOnOtherRank> )
    std::map<size_type,boost::tuple<size_type,size_type> > setInterProcessDofNotPresent;

    if (is_continuous)
    {
        DVLOG(2) << "[buildGhostDofMap] call buildGhostInterProcessDofMap() with god rank "<<  this->worldComm().godRank() << "\n";
        buildGhostInterProcessDofMap( mesh,mapInterProcessDof );

        DVLOG(2) << "[buildGhostDofMap] call buildDofNotPresent() with rank "<<  this->worldComm().rank() << "\n";
        buildDofNotPresent( mapInterProcessDof,setInterProcessDofNotPresent );
    }

    DVLOG(2) << "[buildGhostDofMap] call buildGlobalProcessToGlobalClusterDofMap() with rank "<<  this->worldComm().rank() << "\n";
    buildGlobalProcessToGlobalClusterDofMap( mesh,setInterProcessDofNotPresent );

#else

    if (is_continuous)
    {
        DVLOG(2) << "[buildGhostDofMap] call buildGlobalProcessToGlobalClusterDofMapContinuous() with god rank "<<  this->worldComm().godRank() << "\n";
        buildGlobalProcessToGlobalClusterDofMapContinuous( mesh );
    }
    else
    {
        DVLOG(2) << "[buildGhostDofMap] call buildGlobalProcessToGlobalClusterDofMapDiscontinuous() with rank "<<  this->worldComm().rank() << "\n";
        buildGlobalProcessToGlobalClusterDofMapDiscontinuous();
    }

#endif


    DVLOG(2) << "[buildGhostDofMap] call localtoglobalOnCluster() with rank "<<  this->worldComm().rank() << "\n";
    auto it_elt = mesh.beginElementWithProcessId( this->comm().rank() );
    auto en_elt = mesh.endElementWithProcessId( this->comm().rank() );

    for ( ; it_elt != en_elt; ++it_elt )
    {
        size_type elid= it_elt->id();

        for ( int i = 0; i < FEType::nLocalDof; ++i )
        {
            int nc1 = ( is_product?nComponents1:1 );

            for ( int c1 =0; c1 < nc1; ++c1 )
            {
                int ind = FEType::nLocalDof*c1+i;
                auto const& dof = localToGlobalOnCluster( elid, i, c1 );

                M_locglobOnCluster_indices[elid][ind] = dof.index();
                M_locglobOnCluster_signs[elid][ind] = dof.sign();
            }
        }
    }

    DVLOG(2) << "[buildGhostDofMap] finish () with god rank "<< this->worldComm().godRank() << "\n";

}



//--------------------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------------------//

template<typename MeshType, typename FEType, typename PeriodicityType>
void
DofTable<MeshType, FEType, PeriodicityType>::buildGlobalProcessToGlobalClusterDofMapContinuous( mesh_type& mesh )
{
    //------------------------------------------------------------------------------//
    size_type nbFaceDof = invalid_size_type_value;
    if ( !fe_type::is_modal )
        nbFaceDof = ( face_type::numVertices * fe_type::nDofPerVertex +
                      face_type::numEdges * fe_type::nDofPerEdge +
                      face_type::numFaces * fe_type::nDofPerFace );
    else
        nbFaceDof = face_type::numVertices * fe_type::nDofPerVertex;

    if ( nbFaceDof == 0 ) return;
    //------------------------------------------------------------------------------//
    // build GlobalProcessToGlobalClusterDofMap for actif dofs and prepare send for ghost dof
    std::vector< std::map<size_type,std::set<boost::tuple<size_type,uint16_type> > > > listToSend(this->worldComm().size());
    this->buildGlobalProcessToGlobalClusterDofMapContinuousActifDof(mesh,listToSend);
    //------------------------------------------------------------------------------//
    // update GlobalProcessToGlobalClusterDofMap for ghost dofs
    this->buildGlobalProcessToGlobalClusterDofMapContinuousGhostDof(mesh,listToSend);
    //------------------------------------------------------------------------------//
}

//--------------------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------------------//

namespace detail {

template <typename MeshType>
boost::tuple<int,size_type>
updateDofOnVertices( MeshType const& mesh, typename MeshType::face_type const& theface,
                     const int IdProcessOfGhost, const size_type idFaceInPartition, typename MeshType::element_type const& eltOnProc,
                     /*typename MeshType::node_type const& thedofnode,*/ const uint16_type locDof )
{
    int procMin = IdProcessOfGhost;
    size_type idFaceMin = idFaceInPartition;

    uint16_type iFaEl = ( theface.processId() == theface.proc_first() )? theface.pos_first():theface.pos_second();
    //local point number (in element)
    uint16_type iPtEl = MeshType::element_type::fToP(  iFaEl, locDof );

#if 0
    //typedef MeshType::face_type
    bool hasFind=false;
    for ( uint16_type p=0;p<MeshType::face_type::numVertices && !hasFind; ++p)
    {
        auto const& thept = theface.point(p);
        bool find=true;
        for (uint16_type d=0;d<MeshType::nRealDim;++d)
        {
                find = find && ( std::abs( thept(d)-thedofnode[d] )<1e-9 );
        }
        if (find)
        {
            hasFind=true;
            auto const theptId = thept.id();
            if (eltOnProc.point(iPtEl).id()!=theptId) std::cout << "COOOOYIOKLK"<< std::endl;

            auto iteltghost=thept.elementsGhost().begin();
            auto const eneltghost=thept.elementsGhost().end();
            //size_type idFaceDDD=0;
            for ( ; iteltghost!=eneltghost ; ++iteltghost)
                {
                    if (procMin>iteltghost->template get<0>())
                        {
                            procMin=iteltghost->template get<0>();
                            auto const& eltGhost = mesh.element(iteltghost->template get<1>(),procMin);
                            bool findFace=false;
                            for ( uint16_type f = 0; f < MeshType::element_type::numTopologicalFaces && !findFace; ++f )
                                {
                                    auto const& faceOnGhost = eltGhost.face(f);
                                    //auto const& verticefaceOnGhost =faceOnGhost.vertices();
                                    for ( uint16_type vv = 0; vv < MeshType::face_type::numVertices && !findFace ; ++vv )
                                        {
                                            if (faceOnGhost.point(vv).id()==theptId) {findFace=true;idFaceMin = faceOnGhost.idInOthersPartitions(procMin); }
                                        }
                                }
                            if (!findFace) std::cout << "YYYYYYY" << std::endl;

                        }
                }
        }
    }
    if (!hasFind) std::cout << "JKJKJKJKJKJKJ" << std::endl;
#else
    auto const& thept = eltOnProc.point(iPtEl);
    auto const theptId = thept.id();

    auto iteltghost=thept.elementsGhost().begin();
    auto const eneltghost=thept.elementsGhost().end();
    for ( ; iteltghost!=eneltghost ; ++iteltghost)
    {
        if (procMin>iteltghost->template get<0>())
        {
            procMin=iteltghost->template get<0>();
            auto const& eltGhost = mesh.element(iteltghost->template get<1>(),procMin);
            bool findFace=false;
            for ( uint16_type f = 0; f < MeshType::element_type::numTopologicalFaces && !findFace; ++f )
            {
                auto const& faceOnGhost = eltGhost.face(f);
                for ( uint16_type vv = 0; vv < MeshType::face_type::numVertices && !findFace ; ++vv )
                {
                    if (faceOnGhost.point(vv).id()==theptId) {findFace=true;idFaceMin = faceOnGhost.idInOthersPartitions(procMin); }
                }
            }
            CHECK( findFace ) << "\nPROBLEM with parallel dof table construction \n";
        }
    }
#endif
    return boost::make_tuple(procMin,idFaceMin);

}

//--------------------------------------------------------------------------------------------------------//

template <typename MeshType>
boost::tuple<int,size_type>
updateDofOnEdges( MeshType const& mesh, typename MeshType::face_type const& theface,
                  const int IdProcessOfGhost, const size_type idFaceInPartition, typename MeshType::element_type const& eltOnProc,
                  const uint16_type idEdgesInFace, mpl::int_<0> /**/ )
{
    return boost::make_tuple(0,0);
}
template <typename MeshType>
boost::tuple<int,size_type>
updateDofOnEdges( MeshType const& mesh, typename MeshType::face_type const& theface,
                  const int IdProcessOfGhost, const size_type idFaceInPartition, typename MeshType::element_type const& eltOnProc,
                  const uint16_type idEdgesInFace, mpl::int_<1> /**/ )
{
    return boost::make_tuple(0,0);
}
template <typename MeshType>
boost::tuple<int,size_type>
updateDofOnEdges( MeshType const& mesh, typename MeshType::face_type const& theface,
                  const int IdProcessOfGhost, const size_type idFaceInPartition, typename MeshType::element_type const& eltOnProc,
                  const uint16_type idEdgesInFace, mpl::int_<2> /**/ )
{
    return boost::make_tuple(0,0);
}
template <typename MeshType>
boost::tuple<int,size_type>
updateDofOnEdges( MeshType const& mesh, typename MeshType::face_type const& theface,
                  const int IdProcessOfGhost, const size_type idFaceInPartition, typename MeshType::element_type const& eltOnProc,
                  const uint16_type idEdgesInFace, mpl::int_<3> /**/ )
{
    int procMin = IdProcessOfGhost;
    size_type idFaceMin = idFaceInPartition;

    uint16_type iFaEl = ( theface.processId() == theface.proc_first() )? theface.pos_first():theface.pos_second();
    //local edge number (in element)
    uint16_type iEdEl = MeshType::element_type::fToE(  iFaEl, idEdgesInFace );


    auto const& theedge = eltOnProc.edge(iEdEl);
    auto const theedgeId = theedge.id();
    auto iteltghost=theedge.elementsGhost().begin();
    auto const eneltghost=theedge.elementsGhost().end();

    for ( ; iteltghost!=eneltghost ; ++iteltghost)
    {
        if (procMin>iteltghost->template get<0>())
        {
            procMin=iteltghost->template get<0>();
            auto const& eltGhost = mesh.element(iteltghost->template get<1>(),procMin);
            bool findFace=false;
            for ( uint16_type f = 0; f < MeshType::element_type::numTopologicalFaces && !findFace; ++f )
            {
                auto const& faceOnGhost = eltGhost.face(f);
                for ( uint16_type vv = 0; vv < MeshType::face_type::numEdges && !findFace ; ++vv )
                {
                    if (faceOnGhost.edge(vv).id()==theedgeId) { findFace=true;idFaceMin = faceOnGhost.idInOthersPartitions(procMin); }
                }
            }
            CHECK( findFace ) << "\nPROBLEM with parallel dof table construction \n";
        }
    }

    return boost::make_tuple(procMin,idFaceMin);
}

//--------------------------------------------------------------------------------------------------------//

template <typename MeshType>
boost::tuple<int,size_type>
updateDofOnEdges( MeshType const& mesh, typename MeshType::face_type const& theface,
                  const int IdProcessOfGhost, const size_type idFaceInPartition, typename MeshType::element_type const& eltOnProc,
                  const uint16_type idEdgesInFace )
{
    return updateDofOnEdges(mesh,theface,IdProcessOfGhost,idFaceInPartition,eltOnProc,idEdgesInFace,mpl::int_<MeshType::nDim>() );
}

} // namespace detail

//--------------------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------------------//

template<typename MeshType, typename FEType, typename PeriodicityType>
void
DofTable<MeshType, FEType, PeriodicityType>::buildGlobalProcessToGlobalClusterDofMapContinuousActifDof( mesh_type& mesh,
                                                                                                        std::vector< std::map<size_type,std::set<boost::tuple<size_type,uint16_type> > > > & listToSend )
{
    // goal init container listToSend
    // std::vector< std::map<size_type,std::set<size_type> > > ( proc,( idFace,(globDof,..)), ...   )) )
    const int myRank = this->worldComm().rank();
    //------------------------------------------------------------------------------//
    // get nbFaceDof
    size_type nbFaceDof = invalid_size_type_value;
    if ( !fe_type::is_modal )
        nbFaceDof = ( face_type::numVertices * fe_type::nDofPerVertex +
                      face_type::numEdges * fe_type::nDofPerEdge +
                      face_type::numFaces * fe_type::nDofPerFace );
    else
        nbFaceDof = face_type::numVertices * fe_type::nDofPerVertex;

    DVLOG(2) << "[buildGhostInterProcessDofMap] nbFaceDof " << nbFaceDof << "\n";

    if ( nbFaceDof == 0 ) return;

    const uint16_type ncdof = is_product?nComponents:1;
    DVLOG(2) << "[buildGhostInterProcessDofMap] ncdof " << ncdof << "\n";

    //------------------------------------------------------------------------------//
    std::vector<bool> dofdone(this->M_n_localWithGhost_df[myRank],false);
    size_type nDofNotPresent=0;

    // iteration on all interprocessfaces in order to send requests to the near proc
    auto face_it = mesh.interProcessFaces().first;
    auto const face_en = mesh.interProcessFaces().second;
    DVLOG(2) << "[buildGhostInterProcessDofMap] nb interprocess faces: " << std::distance( face_it, face_en ) << "\n";
    for ( ; face_it != face_en ; ++face_it )
    {
        DVLOG(2) << "[buildGhostInterProcessDofMap] face id: " << face_it->id() << "\n";
        auto const& elt0 = face_it->element0();
        auto const& elt1 = face_it->element1();
        const bool elt0isGhost = elt0.isGhostCell();
        auto const& eltOnProc = (elt0isGhost)?elt1:elt0;
        auto const& eltOffProc = (elt0isGhost)?elt0:elt1;
        DVLOG(2) << "[buildGhostInterProcessDofMap] (myRank:" <<  myRank << ") eltOnProc id: "  << eltOnProc.id()  << "G(): " << eltOnProc.G() << "\n";
        DVLOG(2) << "[buildGhostInterProcessDofMap] (myRank:" <<  myRank << ") eltOffProc id: " << eltOffProc.id() << "G(): " << eltOffProc.G() << "\n";

        //------------------------------------------------------------------------------//

        const int IdProcessOfGhostIP = eltOffProc.processId();
        const size_type idFaceInPartitionIP = face_it->idInOthersPartitions( IdProcessOfGhostIP );
        int IdProcessOfGhost = IdProcessOfGhostIP;
        size_type idFaceInPartition = idFaceInPartitionIP;

        //------------------------------------------------------------------------------//
        // for each dof in face
        for ( uint16_type locDof = 0; locDof < nbFaceDof; ++locDof )
        {
            IdProcessOfGhost = IdProcessOfGhostIP;
            idFaceInPartition = idFaceInPartitionIP;
            if ( locDof < face_type::numVertices*fe_type::nDofPerVertex)
            {
                const int nDofPerVertexTemp = mpl::if_<boost::is_same<mpl::int_<fe_type::nDofPerVertex>,mpl::int_<0> >,
                                                       mpl::int_<1>,
                                                       mpl::int_<fe_type::nDofPerVertex> >::type::value;
                int pointGetLocDof = locDof / nDofPerVertexTemp;

                boost::tie( IdProcessOfGhost, idFaceInPartition ) = detail::updateDofOnVertices<mesh_type>(mesh,*face_it, IdProcessOfGhost, idFaceInPartition, eltOnProc, /*thedofnode,*/ pointGetLocDof/*locDof*/ );
            }
            else if ( nDim == 3 && locDof < (face_type::numVertices*fe_type::nDofPerVertex + face_type::numEdges*fe_type::nDofPerEdge) )
            {
                int locDofInEgde = locDof - face_type::numVertices*fe_type::nDofPerVertex;
                const int nDofPerEdgeTemp = mpl::if_<boost::is_same<mpl::int_<fe_type::nDofPerEdge>,mpl::int_<0> >,
                                                     mpl::int_<1>,
                                                     mpl::int_<fe_type::nDofPerEdge> >::type::value;
                int edgeGetLocDof = locDofInEgde / nDofPerEdgeTemp;

                boost::tie( IdProcessOfGhost, idFaceInPartition ) = detail::updateDofOnEdges<mesh_type>(mesh,*face_it, IdProcessOfGhost, idFaceInPartition, eltOnProc, edgeGetLocDof );
            }

            if (IdProcessOfGhost<myRank)
            {
                for ( uint16_type c = 0; c < ncdof; ++c )
                {
                    // add dof in subcontainer
                    const size_type theglobdof = faceLocalToGlobal( face_it->id(),locDof,c ).template get<0>();
                    if (!dofdone[theglobdof])
                    {
                        listToSend[IdProcessOfGhost][idFaceInPartition].insert(boost::make_tuple(theglobdof,c));
                        dofdone[theglobdof]=true;
                        ++nDofNotPresent;
                    }
                }
            }

        } // for ( uint16_type locDof = 0; locDof < nbFaceDof; ++locDof )


    } // for ( ; face_it != face_en ; ++face_it )

    //------------------------------------------------------------------------------//
    //------------------------------------------------------------------------------//
    //------------------------------------------------------------------------------//
    // update datamap info
    //const size_type mynDofWithoutGhost = this->M_last_df[myRank] - this->M_first_df[myRank] + 1 - nDofNotPresent;
    const size_type mynDofWithoutGhost = this->M_n_localWithGhost_df[myRank] - nDofNotPresent;
    mpi::all_gather( this->worldComm(),
                     mynDofWithoutGhost,
                     this->M_n_localWithoutGhost_df );

    this->M_n_dofs=0;
    for ( int proc=0; proc<this->worldComm().size(); ++proc )
    {
        this->M_n_dofs+=this->M_n_localWithoutGhost_df[proc];
    }

    this->M_first_df_globalcluster=this->M_first_df;
    this->M_last_df_globalcluster=this->M_last_df;
    for ( int i=1; i<this->worldComm().size(); ++i )
    {
        if ( this->M_n_localWithoutGhost_df[i-1] >0 )
            this->M_first_df_globalcluster[i]=this->M_last_df_globalcluster[i-1]+1;
        else
            this->M_first_df_globalcluster[i]=this->M_last_df_globalcluster[i-1];

        if ( this->M_n_localWithoutGhost_df[i] >0 )
            this->M_last_df_globalcluster[i]=this->M_first_df_globalcluster[i]+this->M_n_localWithoutGhost_df[i]-1;
        else
            this->M_last_df_globalcluster[i]=this->M_first_df_globalcluster[i];
    }
    //------------------------------------------------------------------------------//
    // init map
    this->M_mapGlobalProcessToGlobalCluster.resize( this->M_n_localWithGhost_df[myRank],invalid_size_type_value );
    this->M_mapGlobalClusterToGlobalProcess.resize( this->M_n_localWithoutGhost_df[myRank],invalid_size_type_value );
    //------------------------------------------------------------------------------//
    // add in map the dofs presents
    size_type firstGlobIndex = this->M_first_df_globalcluster[myRank];
    size_type nextGlobIndex = firstGlobIndex;
    for ( size_type i=0; i< this->M_n_localWithGhost_df[myRank]; ++i )
    {
        //if ( setInterProcessDofNotPresent.find( i )==setInterProcessDofNotPresent.end() )
        if (!dofdone[i])
        {
            this->M_mapGlobalProcessToGlobalCluster[i]=nextGlobIndex;
            this->M_mapGlobalClusterToGlobalProcess[nextGlobIndex-firstGlobIndex]=i;
            ++nextGlobIndex;
        }
    }
   //------------------------------------------------------------------------------//
} // buildGlobalProcessToGlobalClusterDofMapContinuousActifDof

//--------------------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------------------//

template<typename MeshType, typename FEType, typename PeriodicityType>
void
DofTable<MeshType, FEType, PeriodicityType>::buildGlobalProcessToGlobalClusterDofMapContinuousGhostDof( mesh_type& mesh,
                                                                                     std::vector< std::map<size_type,std::set<boost::tuple<size_type,uint16_type> > > > const& listToSend )
{
    const int myRank = this->worldComm().rank();
    //--------------------------------------------------------------------------------------------------------//
    size_type nbFaceDof = invalid_size_type_value;
    if ( !fe_type::is_modal )
        nbFaceDof = ( face_type::numVertices * fe_type::nDofPerVertex +
                      face_type::numEdges * fe_type::nDofPerEdge +
                      face_type::numFaces * fe_type::nDofPerFace );

    else
        nbFaceDof = face_type::numVertices * fe_type::nDofPerVertex;
    //--------------------------------------------------------------------------------------------------------//

    std::vector<int> nbMsgToSend( this->worldComm().size(), 0 );

    std::vector< std::vector< std::vector<size_type> > > memoryInitialRequest( this->worldComm().size() );

    typedef std::vector< boost::tuple<uint16_type, ublas::vector<double> > > dofs_in_face_subcontainer_type;
    typedef boost::tuple<size_type, dofs_in_face_subcontainer_type > dofs_in_face_container_type;

    for ( int proc=0; proc<this->worldComm().size(); ++proc )
    {
        auto itFaces = listToSend[proc].begin();
        auto const enFaces = listToSend[proc].end();
        memoryInitialRequest[proc].resize(std::distance(itFaces,enFaces));
        for ( int cptFaces=0 ; itFaces!=enFaces ; ++itFaces, ++cptFaces)
        {
            auto itDof = itFaces->second.begin();
            auto const enDof = itFaces->second.end();
            auto const nDofsInFace = std::distance(itDof,enDof);
            dofs_in_face_subcontainer_type dofsInFaceContainer(nDofsInFace);
            memoryInitialRequest[proc][cptFaces].resize(nDofsInFace);
            for (int cptDof=0 ; itDof!=enDof ; ++itDof,++cptDof)
            {
                auto const theglobdof = itDof->get<0>();
                auto const comp = itDof->get<1>();
                // save the tag of mpi send
                memoryInitialRequest[proc][cptFaces][cptDof] = theglobdof;
                //------------------------------------------------------------------------------//
                // get info to send
                ublas::vector<double> nodeDofToSend( nRealDim );
                nodeDofToSend[0]=dofPoint( theglobdof ).template get<0>()[0];
                if ( nRealDim>1 )
                    nodeDofToSend[1]=dofPoint( theglobdof ).template get<0>()[1];
                if ( nRealDim>2 )
                    nodeDofToSend[2]=dofPoint( theglobdof ).template get<0>()[2];
                // up container
                dofsInFaceContainer[cptDof] = boost::make_tuple(comp,nodeDofToSend);
                //------------------------------------------------------------------------------//
            }

            this->worldComm().send( proc , nbMsgToSend[proc], boost::make_tuple(itFaces->first,dofsInFaceContainer) );
            ++nbMsgToSend[proc];
        } // for ( int cptFaces=0 ; itFaces!=enFaces ; ++itFaces, ++cptFaces)
    } // for ( int proc=0; proc<this->worldComm().size(); ++proc )

    //--------------------------------------------------------------------------------------------------------//
    // counter of msg received for each process
    std::vector<int> nbMsgToRecv;
    mpi::all_to_all( this->worldComm(),
                     nbMsgToSend,
                     nbMsgToRecv );
    //--------------------------------------------------------------------------------------------------------//
    // recv dof asked and re-send
    for ( int proc=0; proc<this->worldComm().size(); ++proc )
    {
        for ( int cpt=0; cpt<nbMsgToRecv[proc]; ++cpt )
        {
            dofs_in_face_container_type dataToRecvVec;
            this->worldComm().recv( proc, cpt, dataToRecvVec );

            auto const idFaceInMyPartition = dataToRecvVec.template get<0>();
            DVLOG(2) << "[buildGhostInterProcessDofMap] (myRank:" <<  myRank << ") "
                    << "idFaceInMyPartition: " << idFaceInMyPartition << "\n";

            auto itDofInFace = dataToRecvVec.template get<1>().begin();
            auto const enDofInFace = dataToRecvVec.template get<1>().end();
            std::vector< size_type > resAskedWithMultiProcess(std::distance(itDofInFace,enDofInFace));
            for ( int cptDofInFace=0 ; itDofInFace != enDofInFace ; ++itDofInFace,++cptDofInFace )
            {
                auto const comp = itDofInFace->template get<0>();
                auto const nodeDofRecv = itDofInFace->template get<1>();

                //------------------------------------------------------------------------------//

                auto const& theface = mesh.face( idFaceInMyPartition );
                auto const& elt0 = theface.element0();
                auto const& elt1 = theface.element1();

                const bool elt0isGhost = elt0.isGhostCell();
                auto const& eltOnProc = (elt0isGhost)?elt1:elt0;
                auto const& eltOffProc = (elt0isGhost)?elt0:elt1;

                //------------------------------------------------------------------------------//
                // search dof on face recv
                int locDof = nbFaceDof;
                bool find=false;

                if ( nDim==1 )
                {
                    auto itdofpt = this->dofPointBegin();
                    auto const endofpt = this->dofPointEnd();
                    for ( ; itdofpt!=endofpt && !find ; ++itdofpt )
                    {
                        const auto thedofPt = itdofpt->template get<0>();
                        if ( itdofpt->template get<2>() != comp ) continue;

                        DVLOG(3) << "[buildGhostInterProcessDofMap] (myRank:" <<  myRank << ") "
                                 << "thedofPt: " << thedofPt << "nodeDofRecv: " << nodeDofRecv << "\n";

                        // test equatlity of dofs point
                        bool find2=true;
                        for (uint16_type d=0;d<nRealDim;++d)
                        {
                            find2 = find2 && (std::abs( thedofPt[d]-nodeDofRecv[d] )<1e-9);
                        }
                        // if find else save local dof
                        if (find2) { locDof = itdofpt->template get<1>();find=true; }
                    }
                    // check
                    CHECK( find ) << "\nPROBLEM with parallel dof table construction : Dof point not find on interprocess face " << nodeDofRecv << "\n";
                    //------------------------------------------------------------------------------//
                    // get global dof
                    const auto dofGlobAsked = locDof;
                    // save response
                    resAskedWithMultiProcess[cptDofInFace] = this->M_mapGlobalProcessToGlobalCluster[dofGlobAsked];
                }
                else
                {
                for ( uint16_type l = 0; ( l < nbFaceDof && !find ) ; ++l )
                {
                    // dof point in face
                    const auto thedofPtInFace = dofPoint(faceLocalToGlobal( idFaceInMyPartition, l, comp ).template get<0>()).template get<0>();
                    DVLOG(3) << "[buildGhostInterProcessDofMap] (myRank:" <<  myRank << ") "
                            << "thedofPtInFace: " << thedofPtInFace << "nodeDofRecv: " << nodeDofRecv << "\n";

                    // test equatlity of dofs point
                    bool find2=true;
                    for (uint16_type d=0;d<nRealDim;++d)
                    {
                        find2 = find2 && (std::abs( thedofPtInFace[d]-nodeDofRecv[d] )<1e-9);
                    }
                    // if find else save local dof
                    if (find2)
                    {
                        locDof = l;
                        find=true;
                    }
                } // for ( uint16_type l = 0; ( l < nbFaceDof && !find ) ; ++l )
                //------------------------------------------------------------------------------//
                // check
                CHECK( find ) << "\nPROBLEM with parallel dof table construction : Dof point not find on interprocess face " << nodeDofRecv << "\n";
                //------------------------------------------------------------------------------//
                // get global dof
                const auto thedof = faceLocalToGlobal( idFaceInMyPartition, locDof, comp );
                const auto dofGlobAsked = thedof.template get<0>();
                // save response
                resAskedWithMultiProcess[cptDofInFace] = this->M_mapGlobalProcessToGlobalCluster[dofGlobAsked];
                }
                //------------------------------------------------------------------------------//
            }
            this->worldComm().send( proc, cpt, resAskedWithMultiProcess );

        } // for ( int cpt=0; cpt<nbMsgToRecv[proc]; ++cpt )
    } // for ( int proc=0; proc<this->worldComm().size(); ++proc )

    //--------------------------------------------------------------------------------------------------------//
    // get response to initial request and update Feel::Mesh::Faces data
    for ( int proc=0; proc<this->worldComm().size(); ++proc )
    {
        for ( int cpt=0; cpt<nbMsgToSend[proc]; ++cpt )
        {
            //------------------------------------------------------------------------------//
            // recv response
            std::vector< size_type > resultRecvWithMultiProcess;
            this->worldComm().recv( proc, cpt, resultRecvWithMultiProcess );
            //------------------------------------------------------------------------------//
            // iterate on dofs
            auto itDofRes = resultRecvWithMultiProcess.begin();
            auto const enDofRes = resultRecvWithMultiProcess.end();
            for ( int cptDofRes=0 ; itDofRes != enDofRes ; ++itDofRes,++cptDofRes )
            {
                auto const myGlobProcessDof = memoryInitialRequest[proc][cpt][cptDofRes];
                const auto dofGlobRecv = *itDofRes;
                //update data map
                this->M_mapGlobalProcessToGlobalCluster[myGlobProcessDof]=dofGlobRecv;
            }
        } // for ( int cpt=0; cpt<nbMsgToSend[proc]; ++cpt )
    } // for ( int proc=0; proc<this->worldComm().size(); ++proc )



} // buildGhostInterProcessDofMapRecursive

//--------------------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------------------//

template<typename MeshType, typename FEType, typename PeriodicityType>
void
DofTable<MeshType, FEType, PeriodicityType>::buildGlobalProcessToGlobalClusterDofMapDiscontinuous()
{
    const int myRank = this->worldComm().rank();
    //------------------------------------------------------------------------------//
    // update datamap info
    const size_type mynDofWithoutGhost = this->M_last_df[myRank] - this->M_first_df[myRank] + 1;// - setInterProcessDofNotPresent.size();
    mpi::all_gather( this->worldComm(),
                     mynDofWithoutGhost,
                     this->M_n_localWithoutGhost_df );

    this->M_n_dofs=0;
    for ( int proc=0; proc<this->worldComm().size(); ++proc )
    {
        this->M_n_dofs+=this->M_n_localWithoutGhost_df[proc];
    }

    this->M_first_df_globalcluster=this->M_first_df;
    this->M_last_df_globalcluster=this->M_last_df;
    for ( int i=1; i<this->worldComm().size(); ++i )
    {
        this->M_first_df_globalcluster[i]=this->M_last_df_globalcluster[i-1]+1;
        this->M_last_df_globalcluster[i]=this->M_first_df_globalcluster[i]+this->M_n_localWithoutGhost_df[i]-1;
    }
    //------------------------------------------------------------------------------//
    // init map
    this->M_mapGlobalProcessToGlobalCluster.resize( this->M_n_localWithGhost_df[myRank],invalid_size_type_value );
    this->M_mapGlobalClusterToGlobalProcess.resize( this->M_n_localWithoutGhost_df[myRank],invalid_size_type_value );
    //------------------------------------------------------------------------------//
    // add in map the dofs presents
    size_type firstGlobIndex = this->M_first_df_globalcluster[myRank];
    size_type nextGlobIndex = firstGlobIndex;
    for ( size_type i=0; i< this->M_n_localWithGhost_df[myRank]; ++i )
    {
        //if ( setInterProcessDofNotPresent.find( i )==setInterProcessDofNotPresent.end() )
        {
            this->M_mapGlobalProcessToGlobalCluster[i]=nextGlobIndex;
            this->M_mapGlobalClusterToGlobalProcess[nextGlobIndex-firstGlobIndex]=i;
            ++nextGlobIndex;
        }
    }
    //------------------------------------------------------------------------------//

} // buildGlobalProcessToGlobalClusterDofMap






//--------------------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------------------//

template<typename MeshType, typename FEType, typename PeriodicityType>
void
DofTable<MeshType, FEType, PeriodicityType>::buildGhostInterProcessDofMap( mesh_type& mesh,
        std::map<size_type,boost::tuple<size_type,size_type> > & mapInterProcessDof )
{
    //------------------------------------------------------------------------------//
    // if nbFaceDof == 0 else return
    size_type nbFaceDof = invalid_size_type_value;
    if ( !fe_type::is_modal )
        nbFaceDof = ( face_type::numVertices * fe_type::nDofPerVertex +
                      face_type::numEdges * fe_type::nDofPerEdge +
                      face_type::numFaces * fe_type::nDofPerFace );
    else
        nbFaceDof = face_type::numVertices * fe_type::nDofPerVertex;

    if ( nbFaceDof == 0 ) return;

    //------------------------------------------------------------------------------//
    // init dof list to search with interprocess dof
    std::vector< std::map<size_type,std::set<boost::tuple<size_type,uint16_type> > > > listToSend(this->worldComm().size());
    this->buildGhostInterProcessDofMapInit(mesh,listToSend);
    //------------------------------------------------------------------------------//
    // search real and ghost dofs and build mapping between them
    // the search is iterative beacause some dofs are connected over 2 proc
    // and we need to search all proc around these dofs
    std::vector< std::set<size_type > > memoryFace(this->worldComm().size());
    bool allDofAreFind =false;
    while (!allDofAreFind)
    {
        auto const& res = this->buildGhostInterProcessDofMapRecursive(mesh,listToSend,
                                                                      mapInterProcessDof,
                                                                      memoryFace);

        allDofAreFind = res.template get<0>();
        if (!allDofAreFind) listToSend = res.template get<1>();
    }
    //------------------------------------------------------------------------------//
}

//--------------------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------------------//


template<typename MeshType, typename FEType, typename PeriodicityType>
void
DofTable<MeshType, FEType, PeriodicityType>::buildGhostInterProcessDofMapInit( mesh_type& mesh,
                                                                               std::vector< std::map<size_type,std::set<boost::tuple<size_type,uint16_type> > > > & listToSend )
{

    // goal init container listToSend
    // std::vector< std::map<size_type,std::set<size_type> > > ( proc,( idFace,(globDof,..)), ...   )) )
    const int myRank = this->worldComm().rank();
    //------------------------------------------------------------------------------//
    // get nbFaceDof
    size_type nbFaceDof = invalid_size_type_value;
    if ( !fe_type::is_modal )
        nbFaceDof = ( face_type::numVertices * fe_type::nDofPerVertex +
                      face_type::numEdges * fe_type::nDofPerEdge +
                      face_type::numFaces * fe_type::nDofPerFace );
    else
        nbFaceDof = face_type::numVertices * fe_type::nDofPerVertex;

    if ( nbFaceDof == 0 ) return;
    //------------------------------------------------------------------------------//
    std::vector<bool> dofdone(this->M_n_localWithGhost_df[myRank],false);
    // iteration on all interprocessfaces in order to send requests to the near proc
    auto face_it = mesh.interProcessFaces().first;
    auto const face_en = mesh.interProcessFaces().second;
    DVLOG(2) << "[buildGhostInterProcessDofMap] nb interprocess faces: " << std::distance( face_it, face_en ) << "\n";
    for ( ; face_it != face_en ; ++face_it )
    {
        DVLOG(2) << "[buildGhostInterProcessDofMap] face id: " << face_it->id() << "\n";
        auto const& elt0 = face_it->element0();
        auto const& elt1 = face_it->element1();
        const bool elt0isGhost = elt0.isGhostCell();
        auto const& eltOnProc = (elt0isGhost)?elt1:elt0;
        auto const& eltOffProc = (elt0isGhost)?elt0:elt1;

#if 0
        element_type eltOnProc,eltOffProc;
        if ( elt0.processId()!=myRank )
        {
            eltOffProc = elt0;
            eltOnProc=elt1;
        }
        else if ( elt1.processId()!=myRank )
        {
            eltOffProc = elt1;
            eltOnProc=elt0;
        }
        else std::cout << "\n[buildGhostInterProcessDofMapInit] PROBLEME1!!!!" << std::endl;
#endif
        //------------------------------------------------------------------------------//

        const int IdProcessOfGhost = eltOffProc.processId();
        const size_type idFaceInPartition = face_it->idInOthersPartitions( IdProcessOfGhost );
        //------------------------------------------------------------------------------//
        // for each dof in face
        const uint16_type ncdof = is_product?nComponents:1;
        // subcontainer
        std::set<boost::tuple<size_type,uint16_type> > dofSetAdd;

        for ( uint16_type l = 0; l < nbFaceDof; ++l )
        {
            for ( uint16_type c = 0; c < ncdof; ++c )
            {
                // add dof in subcontainer
                const size_type theglobdof = faceLocalToGlobal( face_it->id(),l,c ).template get<0>();
                if (!dofdone[theglobdof])
                {
                    dofSetAdd.insert(boost::make_tuple(theglobdof,c));
                    dofdone[theglobdof]=true;
                }
            }
            // add composante dofs in result container
        }
        listToSend[IdProcessOfGhost].insert( std::make_pair( idFaceInPartition/*face_it->id()*/, dofSetAdd  ) );


    } // for ( ; face_it != face_en ; ++face_it )
}

//--------------------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------------------//

namespace detail {

template<typename element_type>
void
searchPartitionAroundNode( const int myRank, ublas::vector<double> const& nodeSearched,
                           element_type const& eltOnProc, std::set<size_type> & memoryIdsFaces,
                           std::set<boost::tuple<int,size_type> > & multiProcessRes )
{
    for ( uint16_type f = 0; f < element_type::numTopologicalFaces; ++f )
        {
            auto const& theFace = eltOnProc.face(f);
            size_type theFaceId = theFace.id();
            if (memoryIdsFaces.find(theFaceId) != memoryIdsFaces.end()) continue;

            // save face
            memoryIdsFaces.insert( theFaceId );
            // face contains node?
            bool find=false;
            auto const& facevertices = theFace.vertices();
            for (uint16_type v=0;v<element_type::face_type::numVertices && !find ;++v)
                {
                    bool find2=true;
                    for (uint16_type d=0;d<element_type::nDim;++d)
                        {
                            find2 = find2 && ( std::abs( facevertices(d,v)-nodeSearched[d] )<1e-9 );
                        }
                    if (find2) find=true;
                }

            if (find)
                {
                    if ( theFace.isConnectedTo0() )
                        {
                            auto const& eltOnProc0 = theFace.element0();
                            if (eltOnProc0.processId()!=myRank)
                                multiProcessRes.insert( boost::make_tuple(eltOnProc0.processId(),theFace.idInOthersPartitions(eltOnProc0.processId()) ));
                            else if (eltOnProc.id()!=eltOnProc0.id())
                                searchPartitionAroundNode(myRank,nodeSearched,eltOnProc0,memoryIdsFaces,multiProcessRes );
                        }
                    if ( theFace.isConnectedTo1() )
                        {
                            auto const& eltOnProc1 = theFace.element1();
                            if (eltOnProc1.processId()!=myRank)
                                multiProcessRes.insert( boost::make_tuple(eltOnProc1.processId(),theFace.idInOthersPartitions(eltOnProc1.processId()) ));
                            else if (eltOnProc.id()!=eltOnProc1.id())
                                searchPartitionAroundNode(myRank,nodeSearched,eltOnProc1,memoryIdsFaces,multiProcessRes );
                        }
                } // if (find)

        } // for ( uint16_type f = 0; f < element_type::numTopologicalFaces; ++f )

} // searchPartitionAroundNode


template<typename mesh_type>
void
searchPartitionAroundEdge( const int myRank,
                           typename mesh_type::face_type const& faceContainEdge,const uint16_type idEdgesInFace,
                           typename mesh_type::element_type const& eltOnProc, std::set<size_type> & memoryIdsFaces,
                           std::set<boost::tuple<int,size_type> > & multiProcessRes, bool firstExp, mpl::int_<0> /**/ )
{}

template<typename mesh_type>
void
searchPartitionAroundEdge( const int myRank,
                           typename mesh_type::face_type const& faceContainEdge,const uint16_type idEdgesInFace,
                           typename mesh_type::element_type const& eltOnProc, std::set<size_type> & memoryIdsFaces,
                           std::set<boost::tuple<int,size_type> > & multiProcessRes, bool firstExp, mpl::int_<1> /**/ )
{}

template<typename mesh_type>
void
searchPartitionAroundEdge( const int myRank,
                           typename mesh_type::face_type const& faceContainEdge,const uint16_type idEdgesInFace,
                           typename mesh_type::element_type const& eltOnProc, std::set<size_type> & memoryIdsFaces,
                           std::set<boost::tuple<int,size_type> > & multiProcessRes, bool firstExp, mpl::int_<2> /**/ )
{}

template<typename mesh_type>
void
searchPartitionAroundEdge( const int myRank,
                           typename mesh_type::face_type const& faceContainEdge,const uint16_type idEdgesInFace,
                           typename mesh_type::element_type const& eltOnProc, std::set<size_type> & memoryIdsFaces,
                           std::set<boost::tuple<int,size_type> > & multiProcessRes, bool firstExp, mpl::int_<3> /**/ )
{

    // only usefull whith first pass
    uint16_type iFaEl=0,iEdEl=0;
    if (firstExp)
    {
        iFaEl = ( faceContainEdge.processId() == faceContainEdge.proc_first() )? faceContainEdge.pos_first():faceContainEdge.pos_second();
        //local edge number (in element)
        iEdEl = mesh_type::element_type::fToE(  iFaEl, idEdgesInFace );
    }

    auto const& theedge = (firstExp)?eltOnProc.edge(iEdEl):faceContainEdge.edge(idEdgesInFace);
    //auto const& theEdgesVertices = theedge.vertices();

    for ( uint16_type f = 0; f < mesh_type::element_type::numTopologicalFaces; ++f )
    {
        auto const& theFace = eltOnProc.face(f);

        size_type theFaceId = theFace.id();
        //auto const& theneighborface = mesh.face( eltOnProc.face(f).id() );

        if (memoryIdsFaces.find(theFaceId) != memoryIdsFaces.end()) continue;

        // save face
        memoryIdsFaces.insert( theFaceId );

        // face contains edge?
        bool find=false;uint16_type newEdgeIdInFace=0;
        for (uint16_type ed=0;ed<mesh_type::element_type::face_type::numEdges && !find ;++ed)
        {
            //auto const& otheredges = theFace.edge(ed);
            if (theFace.edge(ed).id() == theedge.id())
            {
                find=true; newEdgeIdInFace=ed;
#if 0
                // check
                auto const& newEdgesVertices = theFace.edge(ed).vertices();
                //auto const& theEdgesVertices = theedge.vertices();
                bool find2=true;
                for (uint16_type d=0;d<mesh_type::element_type::nDim;++d)
                    {
                        find2 = find2 && ( std::abs( newEdgesVertices(d,0)-theEdgesVertices(d,0) )<1e-9 );
                    }
                if (!find2) std::cout << "\n IAOIOIOAIOIO"<<std::endl;
                //if (newEdgesVertices!=theEdgesVertices) std::cout << "\IAOIOIOAIOIO"<<std::endl;
#endif
            }
        }

        if (find)
        {
            if ( theFace.isConnectedTo0() )
            {
                auto const& eltOnProc0 = theFace.element0();
                if (eltOnProc0.processId()!=myRank)
                    multiProcessRes.insert( boost::make_tuple(eltOnProc0.processId(),theFace.idInOthersPartitions(eltOnProc0.processId()) ));
                else if (eltOnProc.id()!=eltOnProc0.id())
                    searchPartitionAroundEdge<mesh_type>(myRank,theFace,newEdgeIdInFace,eltOnProc0,memoryIdsFaces,multiProcessRes,false,mpl::int_<3>());
            }
            if ( theFace.isConnectedTo1() )
            {
                auto const& eltOnProc1 = theFace.element1();
                if (eltOnProc1.processId()!=myRank)
                    multiProcessRes.insert( boost::make_tuple(eltOnProc1.processId(),theFace.idInOthersPartitions(eltOnProc1.processId()) ));
                else if (eltOnProc.id()!=eltOnProc1.id())
                    searchPartitionAroundEdge<mesh_type>(myRank,theFace,newEdgeIdInFace,eltOnProc1,memoryIdsFaces,multiProcessRes,false,mpl::int_<3>());
            }
        } // if (find)

    } // for ( uint16_type f = 0; f < element_type::numTopologicalFaces; ++f )

}

template<typename mesh_type>
void
searchPartitionAroundEdge( const int myRank,
                           typename mesh_type::face_type const& faceContainEdge,const uint16_type idEdgesInFace,
                           typename mesh_type::element_type const& eltOnProc, std::set<size_type> & memoryIdsFaces,
                           std::set<boost::tuple<int,size_type> > & multiProcessRes )
{
    searchPartitionAroundEdge<mesh_type>(myRank,faceContainEdge,idEdgesInFace,eltOnProc,memoryIdsFaces,multiProcessRes,true,mpl::int_<mesh_type::nDim>());
}

} // namespace detail

template<typename MeshType, typename FEType, typename PeriodicityType>
boost::tuple<bool, std::vector< std::map<size_type,std::set<boost::tuple<size_type,uint16_type> > > > >
DofTable<MeshType, FEType, PeriodicityType>::buildGhostInterProcessDofMapRecursive( mesh_type& mesh,
                                                                                    std::vector< std::map<size_type,std::set<boost::tuple<size_type,uint16_type> > > > const& listToSend,
                                                                                    std::map<size_type,boost::tuple<size_type,size_type> > & mapInterProcessDof,
                                                                                    std::vector< std::set<size_type > > & memoryFace )
{
    const int myRank = this->worldComm().rank();
    //--------------------------------------------------------------------------------------------------------//
    size_type nbFaceDof = invalid_size_type_value;
    if ( !fe_type::is_modal )
        nbFaceDof = ( face_type::numVertices * fe_type::nDofPerVertex +
                      face_type::numEdges * fe_type::nDofPerEdge +
                      face_type::numFaces * fe_type::nDofPerFace );

    else
        nbFaceDof = face_type::numVertices * fe_type::nDofPerVertex;
    //--------------------------------------------------------------------------------------------------------//

    std::vector<int> nbMsgToSend( this->worldComm().size(), 0 );

    std::vector< std::map<size_type,std::set<boost::tuple<size_type,uint16_type> > > > listNeedMoreSearch(this->worldComm().size());
    int counterListNeedMoreSearch = 0, counterListNeedMoreSearchLoc = 0;

    std::vector< std::vector< std::vector<boost::tuple<uint16_type,int> > > > memoryInitialRequest( this->worldComm().size() );

    typedef std::vector< boost::tuple<uint16_type, ublas::vector<double> > > dofs_in_face_subcontainer_type;
    typedef boost::tuple<size_type, dofs_in_face_subcontainer_type > dofs_in_face_container_type;

    for ( int proc=0; proc<this->worldComm().size(); ++proc )
    {
        auto itFaces = listToSend[proc].begin();
        auto const enFaces = listToSend[proc].end();
        memoryInitialRequest[proc].resize(std::distance(itFaces,enFaces));
        for ( int cptFaces=0 ; itFaces!=enFaces ; ++itFaces, ++cptFaces)
        {
            auto itDof = itFaces->second.begin();
            auto const enDof = itFaces->second.end();
            auto const nDofsInFace = std::distance(itDof,enDof);
            dofs_in_face_subcontainer_type dofsInFaceContainer(nDofsInFace);
            memoryInitialRequest[proc][cptFaces].resize(nDofsInFace);
            for (int cptDof=0 ; itDof!=enDof ; ++itDof,++cptDof)
            {
                auto const theglobdof = itDof->get<0>();
                auto const comp = itDof->get<1>();
                // save the tag of mpi send
                memoryInitialRequest[proc][cptFaces][cptDof] = boost::make_tuple(comp,theglobdof);
                //------------------------------------------------------------------------------//
                // get info to send
                ublas::vector<double> nodeDofToSend( nDim );
                nodeDofToSend[0]=dofPoint( theglobdof ).template get<0>()[0];
                if ( nDim>1 )
                    nodeDofToSend[1]=dofPoint( theglobdof ).template get<0>()[1];
                if ( nDim>2 )
                    nodeDofToSend[2]=dofPoint( theglobdof ).template get<0>()[2];
                // up container
                dofsInFaceContainer[cptDof] = boost::make_tuple(comp,nodeDofToSend);
                //------------------------------------------------------------------------------//
            }

            this->worldComm().send( proc , nbMsgToSend[proc], boost::make_tuple(itFaces->first,dofsInFaceContainer) );
            ++nbMsgToSend[proc];
        } // for ( int cptFaces=0 ; itFaces!=enFaces ; ++itFaces, ++cptFaces)
    } // for ( int proc=0; proc<this->worldComm().size(); ++proc )

    //--------------------------------------------------------------------------------------------------------//
    // counter of msg received for each process
    std::vector<int> nbMsgToRecv;
    mpi::all_to_all( this->worldComm(),
                     nbMsgToSend,
                     nbMsgToRecv );
    //--------------------------------------------------------------------------------------------------------//
    // recv dof asked and re-send
    for ( int proc=0; proc<this->worldComm().size(); ++proc )
    {
        for ( int cpt=0; cpt<nbMsgToRecv[proc]; ++cpt )
        {
            dofs_in_face_container_type dataToRecvVec;
            this->worldComm().recv( proc, cpt, dataToRecvVec );

            auto const idFaceInMyPartition = dataToRecvVec.template get<0>();

            auto itDofInFace = dataToRecvVec.template get<1>().begin();
            auto const enDofInFace = dataToRecvVec.template get<1>().end();
            std::vector< boost::tuple<size_type,std::vector<boost::tuple<int,size_type> > > > resAskedWithMultiProcess(std::distance(itDofInFace,enDofInFace));
            for ( int cptDofInFace=0 ; itDofInFace != enDofInFace ; ++itDofInFace,++cptDofInFace )
            {
                auto const comp = itDofInFace->template get<0>();
                auto const nodeDofRecv = itDofInFace->template get<1>();

                //------------------------------------------------------------------------------//

                auto const& theface = mesh.face( idFaceInMyPartition );
                auto const& elt0 = theface.element0();
                auto const& elt1 = theface.element1();

                const bool elt0isGhost = elt0.isGhostCell();
                auto const& eltOnProc = (elt0isGhost)?elt1:elt0;
                auto const& eltOffProc = (elt0isGhost)?elt0:elt1;

#if 0
                element_type eltOnProc, eltOffProc;
                uint16_type faceIdInEltOnProc = invalid_uint16_type_value;
                if ( elt0.processId()!=myRank )
                {
                    eltOffProc = elt0;
                    eltOnProc=elt1;
                    faceIdInEltOnProc = theface.pos_second();
                }
                else if ( elt1.processId()!=myRank )
                {
                    eltOffProc = elt1;
                    eltOnProc=elt0;
                    faceIdInEltOnProc = theface.pos_first();
                }
                else
                {
                    CHECK( (elt0.processId()==myRank) ||
                           (elt1.processId()==myRank) )
                        << "\nPROBLEM with parallel dof table construction\n"
                        << " elt0.processId() " << elt0.processId()
                        << " elt1.processId() " << elt1.processId()
                        << "\n";
                }
#endif

                //------------------------------------------------------------------------------//
                // search dof on face recv
                int locDof = nbFaceDof;
                bool find=false;

                for ( uint16_type l = 0; ( l < nbFaceDof && !find ) ; ++l )
                {
                    // dof point in face
                    const auto thedofPtInFace = dofPoint(faceLocalToGlobal( idFaceInMyPartition, l, comp ).template get<0>()).template get<0>();
                    // test equatlity of dofs point
                    bool find2=true;
                    for (uint16_type d=0;d<nDim;++d)
                    {
                        find2 = find2 && (std::abs( thedofPtInFace[d]-nodeDofRecv[d] )<1e-9);
                    }
                    // if find else save local dof
                    if (find2)
                    {
                        locDof = l;
                        find=true;
                    }
                } // for ( uint16_type l = 0; ( l < nbFaceDof && !find ) ; ++l )
                //------------------------------------------------------------------------------//
                // check
                //if ( !find ) std::cout<<"\n [buildGhostInterProcessDofMap] : Dof point not find on interprocess face " << nodeDofRecv << std::endl;
                CHECK( find ) << "\nPROBLEM with parallel dof table construction : Dof point not find on interprocess face " << nodeDofRecv << "\n";
                //------------------------------------------------------------------------------//
                // get global dof
                const auto thedof = faceLocalToGlobal( idFaceInMyPartition, locDof, comp );
                const auto dofGlobAsked = thedof.template get<0>();
                //------------------------------------------------------------------------------//
                //------------------------------------------------------------------------------//
                //------------------------------------------------------------------------------//
                //------------------------------------------------------------------------------//
                // search maybe with neighboors elt
                std::set<size_type> memoryIdsFaces;
                std::set<boost::tuple<int,size_type> > multiProcessRes;
                if ( locDof < face_type::numVertices*fe_type::nDofPerVertex)
                {
                    detail::searchPartitionAroundNode( myRank, nodeDofRecv, eltOnProc, memoryIdsFaces, multiProcessRes );
                }
                else if ( nDim == 3 && locDof < (face_type::numVertices*fe_type::nDofPerVertex + face_type::numEdges*fe_type::nDofPerEdge) )
                {
                    int locDofInEgde = locDof - face_type::numVertices*fe_type::nDofPerVertex;

                    const int nDofPerEdgeTemp = mpl::if_<boost::is_same<mpl::int_<fe_type::nDofPerEdge>,mpl::int_<0> >,
                                                         mpl::int_<1>,
                                                         mpl::int_<fe_type::nDofPerEdge> >::type::value;

                    int edgeGetLocDof = locDofInEgde / nDofPerEdgeTemp;

                    detail::searchPartitionAroundEdge<mesh_type>( myRank, theface, edgeGetLocDof, eltOnProc, memoryIdsFaces, multiProcessRes );
                }
                //if (proc==0 ) std::cout << "myRank " << myRank << " multiProcessRes.size() " << multiProcessRes.size() << " " << nodeDofRecv << std::endl;
                //------------------------------------------------------------------------------//
                // convert in vector for send
                auto const nbMultiProcessRes = multiProcessRes.size();
                std::vector<boost::tuple<int,size_type> > multiProcessResVec( nbMultiProcessRes );
                std::copy( multiProcessRes.begin(),multiProcessRes.end(),
                           multiProcessResVec.begin() );
                //------------------------------------------------------------------------------//
                //------------------------------------------------------------------------------//
                //------------------------------------------------------------------------------//
                // send response
                resAskedWithMultiProcess[cptDofInFace] = boost::make_tuple(dofGlobAsked,multiProcessResVec);
                //------------------------------------------------------------------------------//
            }
            this->worldComm().send( proc, cpt, resAskedWithMultiProcess );

        } // for ( int cpt=0; cpt<nbMsgToRecv[proc]; ++cpt )
    } // for ( int proc=0; proc<this->worldComm().size(); ++proc )

    //--------------------------------------------------------------------------------------------------------//
    // get response to initial request and update Feel::Mesh::Faces data
    for ( int proc=0; proc<this->worldComm().size(); ++proc )
    {
        for ( int cpt=0; cpt<nbMsgToSend[proc]; ++cpt )
        {
            //------------------------------------------------------------------------------//
            // recv response
            std::vector< boost::tuple<size_type,std::vector<boost::tuple<int,size_type> > > > resultRecvWithMultiProcess;
            this->worldComm().recv( proc, cpt, resultRecvWithMultiProcess );
            //------------------------------------------------------------------------------//
            // iterate on dofs
            auto itDofRes = resultRecvWithMultiProcess.begin();
            auto const enDofRes = resultRecvWithMultiProcess.end();
            for ( int cptDofRes=0 ; itDofRes != enDofRes ; ++itDofRes,++cptDofRes )
            {

                auto const mycomp = memoryInitialRequest[proc][cpt][cptDofRes].template get<0>();
                auto const myGlobProcessDof = memoryInitialRequest[proc][cpt][cptDofRes].template get<1>();
                const auto dofGlobRecv = itDofRes->template get<0>();
                const auto multiProcessRes = itDofRes->template get<1>();

                //------------------------------------------------------------------------------//
                // update mapInterProcessDof data
                if ( mapInterProcessDof.find( myGlobProcessDof ) == mapInterProcessDof.end() )
                {
                    mapInterProcessDof.insert( std::make_pair( myGlobProcessDof, boost::make_tuple( proc,dofGlobRecv ) ) );
                }
                else if ( ( int )mapInterProcessDof.find( myGlobProcessDof )->second.template get<0>() > proc )
                {
                    mapInterProcessDof[ myGlobProcessDof ] = boost::make_tuple( proc,dofGlobRecv );
                }
                //------------------------------------------------------------------------------//
                // save global dof with search on proc
                memoryFace[proc].insert(myGlobProcessDof);
                //------------------------------------------------------------------------------//
                // add maybe more search
                auto itMP = multiProcessRes.begin();
                auto const enMP = multiProcessRes.end();
                for ( ; itMP!=enMP ;++itMP )
                {
                    auto const eltMultiProcessProcId = itMP->template get<0>();
                    auto const eltMultiProcessFaceId = itMP->template get<1>();
                    if ( memoryFace[eltMultiProcessProcId].find( myGlobProcessDof ) == memoryFace[eltMultiProcessProcId].end() )
                    {
                        listNeedMoreSearch[eltMultiProcessProcId][eltMultiProcessFaceId].insert(boost::make_tuple(myGlobProcessDof,mycomp));
                        ++counterListNeedMoreSearchLoc;
                    }
                }
            }
        } // for ( int cpt=0; cpt<nbMsgToSend[proc]; ++cpt )
    } // for ( int proc=0; proc<this->worldComm().size(); ++proc )

    //--------------------------------------------------------------------------------------------------------//
    // need to apply an all_reduce
    mpi::all_reduce( this->worldComm().globalComm(),
                     counterListNeedMoreSearchLoc,
                     counterListNeedMoreSearch,
                     std::plus<int>() );
    //std::cout<< " counterListNeedMoreSearch "<< counterListNeedMoreSearch << " counterListNeedMoreSearchLoc " << counterListNeedMoreSearchLoc << std::endl;
    //--------------------------------------------------------------------------------------------------------//
    return boost::make_tuple( (bool) (counterListNeedMoreSearch == 0),
                              listNeedMoreSearch);


} // buildGhostInterProcessDofMapRecursive

//--------------------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------------------//

template<typename MeshType, typename FEType, typename PeriodicityType>
void
DofTable<MeshType, FEType, PeriodicityType>::buildDofNotPresent( std::map<size_type,boost::tuple<size_type,size_type> > const & mapInterProcessDof,
                                                                 std::map<size_type,boost::tuple<size_type,size_type> > & setInterProcessDofNotPresent )

{
    const int myRank = this->worldComm().rank();

    auto it_mapIPDof = mapInterProcessDof.begin();
    auto const en_mapIPDof = mapInterProcessDof.end();
    for ( ; it_mapIPDof!=en_mapIPDof; ++it_mapIPDof )
    {
        if ( it_mapIPDof->second.template get<0>() < myRank )
            setInterProcessDofNotPresent[it_mapIPDof->first] = it_mapIPDof->second;
    }
}

//--------------------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------------------//

template<typename MeshType, typename FEType, typename PeriodicityType>
void
DofTable<MeshType, FEType, PeriodicityType>::buildGlobalProcessToGlobalClusterDofMap( mesh_type& mesh,
        std::map<size_type,boost::tuple<size_type,size_type> > const& setInterProcessDofNotPresent )
{
    const int myRank = this->worldComm().rank();

    //------------------------------------------------------------------------------//
    // update datamap info
    //const size_type mynDofWithoutGhost = this->M_last_df[myRank] - this->M_first_df[myRank] + 1 - setInterProcessDofNotPresent.size();
    const size_type mynDofWithoutGhost = this->M_n_localWithGhost_df[myRank] - setInterProcessDofNotPresent.size();
    mpi::all_gather( this->worldComm(),
                     mynDofWithoutGhost,
                     this->M_n_localWithoutGhost_df );

    this->M_n_dofs=0;
    for ( int proc=0; proc<this->worldComm().size(); ++proc )
    {
        this->M_n_dofs+=this->M_n_localWithoutGhost_df[proc];
    }

    this->M_first_df_globalcluster=this->M_first_df;
    this->M_last_df_globalcluster=this->M_last_df;
    for ( int i=1; i<this->worldComm().size(); ++i )
    {
        if ( this->M_n_localWithoutGhost_df[i-1] >0 )
            this->M_first_df_globalcluster[i]=this->M_last_df_globalcluster[i-1]+1;
        else
            this->M_first_df_globalcluster[i]=this->M_last_df_globalcluster[i-1];

        if ( this->M_n_localWithoutGhost_df[i] >0 )
            this->M_last_df_globalcluster[i]=this->M_first_df_globalcluster[i]+this->M_n_localWithoutGhost_df[i]-1;
        else
            this->M_last_df_globalcluster[i]=this->M_first_df_globalcluster[i];
    }

    //------------------------------------------------------------------------------//
    // init map
    this->M_mapGlobalProcessToGlobalCluster.resize( this->M_n_localWithGhost_df[myRank],invalid_size_type_value );
    this->M_mapGlobalClusterToGlobalProcess.resize( this->M_n_localWithoutGhost_df[myRank],invalid_size_type_value );

    //------------------------------------------------------------------------------//
    // add in map the dofs presents
    size_type firstGlobIndex = this->M_first_df_globalcluster[myRank];
    size_type nextGlobIndex = firstGlobIndex;
    for ( size_type i=0; i< this->M_n_localWithGhost_df[myRank]; ++i )
    {
        if ( setInterProcessDofNotPresent.find( i )==setInterProcessDofNotPresent.end() )
        {
            this->M_mapGlobalProcessToGlobalCluster[i]=nextGlobIndex;
            this->M_mapGlobalClusterToGlobalProcess[nextGlobIndex-firstGlobIndex]=i;
            ++nextGlobIndex;
        }
    }

    //------------------------------------------------------------------------------//
    // update datamap with dofs non presents (ghosts)
    this->updateGhostGlobalDof( setInterProcessDofNotPresent );

} // buildGlobalProcessToGlobalClusterDofMap

//--------------------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------------------//

template<typename MeshType, typename FEType, typename PeriodicityType>
void
DofTable<MeshType, FEType, PeriodicityType>::updateGhostGlobalDof( std::map<size_type,boost::tuple<size_type,size_type> > const& setInterProcessDofNotPresent )
{
    std::vector<int> nbMsgToSend2( this->worldComm().size(), 0 );
    std::vector< std::map<int,size_type> > mapMsg2( this->worldComm().size() );

    auto dofNP_it = setInterProcessDofNotPresent.begin();
    auto dofNP_en = setInterProcessDofNotPresent.end();
    for ( ; dofNP_it!=dofNP_en ; ++dofNP_it )
    {
        auto const globalProcessDof = dofNP_it->first;
        auto const IdProcessOfGhost = dofNP_it->second.template get<0>();
        auto const dofFaceInPartition = dofNP_it->second.template get<1>();

        this->worldComm().send( IdProcessOfGhost , nbMsgToSend2[IdProcessOfGhost], dofFaceInPartition );
        mapMsg2[IdProcessOfGhost].insert( std::make_pair( nbMsgToSend2[IdProcessOfGhost],globalProcessDof ) );
        ++nbMsgToSend2[IdProcessOfGhost];
    }

    // counter of msg received for each process
    std::vector<int> nbMsgToRecv2;
    mpi::all_to_all( this->worldComm(),
                     nbMsgToSend2,
                     nbMsgToRecv2 );

    // recv dof asked and re-send dof in this proc
    for ( int proc=0; proc<this->worldComm().size(); ++proc )
    {
        for ( int cpt=0; cpt<nbMsgToRecv2[proc]; ++cpt )
        {
            //recv
            size_type dofRecv;
            this->worldComm().recv( proc, cpt, dofRecv );
            // send
            size_type const dofToSend= this->M_mapGlobalProcessToGlobalCluster[dofRecv];
            this->worldComm().send( proc, cpt,dofToSend );
        }
    }

    // get response to initial request and update Feel::Mesh::Faces data
    for ( int proc=0; proc<this->worldComm().size(); ++proc )
    {
        for ( int cpt=0; cpt<nbMsgToSend2[proc]; ++cpt )
        {
            //recv
            size_type dofGlobClusterRecv;
            this->worldComm().recv( proc, cpt, dofGlobClusterRecv );
            //update data
            this->M_mapGlobalProcessToGlobalCluster[mapMsg2[proc][cpt]]=dofGlobClusterRecv;
        }
    }

}



} // namespace Feel

#endif /* FEELPP_DOFTABLE_MPI_HPP */
