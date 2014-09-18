/* -*- mode: c++; coding: utf-8; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; show-trailing-whitespace: t -*- vim:fenc=utf-8:ft=tcl:et:sw=4:ts=4:sts=4

  This file is part of the Feel library

  Author(s): Christophe Prud'homme <christophe.prudhomme@feelpp.org>
       Date: 2007-07-21

  Copyright (C) 2007 Université Joseph Fourier (Grenoble I)
  Copyright (C) 2011 Feel++ Consortium

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
#ifndef FEELPP_EXPORTERENSIGHTGOLD_CPP
#define FEELPP_EXPORTERENSIGHTGOLD_CPP 1

#include <feel/feelcore/feel.hpp>

#include <feel/feeldiscr/mesh.hpp>
#include <feel/feeldiscr/functionspace.hpp>
#include <feel/feeldiscr/timeset.hpp>
#include <feel/feelfilters/exporterensightgold.hpp>
#include <feel/feelfilters/detail/fileindex.hpp>

namespace Feel
{
template<typename MeshType, int N>
ExporterEnsightGold<MeshType,N>::ExporterEnsightGold( WorldComm const& worldComm )
:
super( worldComm ),
M_element_type()
{
    init();
}
template<typename MeshType, int N>
ExporterEnsightGold<MeshType,N>::ExporterEnsightGold( std::string const& __p, int freq, WorldComm const& worldComm )
    :
    super( "ensightgold", __p, freq, worldComm ),
    M_element_type()
{
    init();
}
template<typename MeshType, int N>
ExporterEnsightGold<MeshType,N>::ExporterEnsightGold( po::variables_map const& vm, std::string const& exp_prefix, WorldComm const& worldComm )
    :
    super( vm, exp_prefix, worldComm )
{
    init();
}

template<typename MeshType, int N>
ExporterEnsightGold<MeshType,N>::ExporterEnsightGold( ExporterEnsightGold const & __ex )
    :
    super( __ex ),
    M_element_type( __ex.M_element_type )
{
}

template<typename MeshType, int N>
ExporterEnsightGold<MeshType,N>::~ExporterEnsightGold()
{}

template<typename MeshType, int N>
void
ExporterEnsightGold<MeshType,N>::init()
{
    /* define ensight named constant for the different faces/elements */
    if ( mesh_type::nDim == 1 )
        if ( mesh_type::Shape == SHAPE_LINE )
        {
            M_element_type = ( mesh_type::nOrder == 1 )?"bar2":"bar3";
            M_face_type = "point";
        }

    if ( mesh_type::nDim == 2 )
    {
        if ( mesh_type::Shape == SHAPE_TRIANGLE )
            M_element_type = ( mesh_type::nOrder == 1 )?"tria3":"tria6";

        else if ( mesh_type::Shape == SHAPE_QUAD )
            M_element_type = ( mesh_type::nOrder == 1 )?"quad4":"quad8";

        M_face_type = ( mesh_type::nOrder == 1 )?"bar2":"bar3";
    }

    if ( mesh_type::nDim == 3 )
    {
        if ( mesh_type::Shape == SHAPE_TETRA )
        {
            M_element_type = ( mesh_type::nOrder == 1 )?"tetra4":"tetra10";
            M_face_type = ( mesh_type::nOrder == 1 )?"tria3":"tria6";
        }

        else if ( mesh_type::Shape == SHAPE_HEXA )
        {
            M_element_type = ( mesh_type::nOrder == 1 )?"hexa8":"hexa20";
            M_face_type = ( mesh_type::nOrder == 1 )?"quad4":"quad8";
        }
    }

    /* TODO Do a cleanup of previous stored files */
    /* to avoid conflicts */
    /* example case: where a new simulation has fewer timesteps */
    /* it can be confusing if files with higher timesteps remain */

    /* Init number of digit for maximum time step */
    M_timeExponent = 4;

    /* if we do not want to merge the results from the different processes */
    /* we isolate each process by using worldCommSeq() from Environment */
    /* Each process will be in a group seeing only itself and not every */
    /* process as worldComm() */
    if( ! boption( _name = "exporter.merge.markers" ) )
    {
        this->setWorldComm(Environment::worldCommSeq());
    }
}

template<typename MeshType, int N>
void
ExporterEnsightGold<MeshType,N>::save() const
{
    if ( !this->worldComm().isActive() ) return;

    //static int freq = 0;
    //

    /* Check that we have steps to save */
    /* Ensures that we do not end up in a segfault */
    timeset_const_iterator __ts_it = this->beginTimeSet();
    timeset_const_iterator __ts_en = this->endTimeSet();
    bool hasSteps = true;
    while ( __ts_it != __ts_en )
    {
        timeset_ptrtype __ts = *__ts_it;

        typename timeset_type::step_const_iterator __it = __ts->beginStep();
        typename timeset_type::step_const_iterator __end = __ts->endStep();

        if(__it == __end)
        {
            LOG(INFO) << "Timeset " << __ts->name() << " (" << __ts->index() << ") contains no timesteps (Consider using add() or addRegions())" << std::endl;
            hasSteps = false;
        }

        ++__ts_it;
    }

    if(!hasSteps)
    {
        return;
    }

    DVLOG(2) << "checking if frequency is ok\n";

    if ( this->cptOfSave() % this->freq()  )
    {
        this->saveTimeSet();
        return;
    }

    boost::timer ti;
    DVLOG(2) << "export in ensight format\n";

    ti.restart();
    DVLOG(2) << "export geo(mesh) file\n";
    writeGeoFiles();
    DVLOG(2) << "export geo(mesh) file ok, time " << ti.elapsed() << "\n";

    LOG(INFO) << "Geo File written" << std::endl;

    ti.restart();
    DVLOG(2) << "export variable file\n";
    writeVariableFiles();
    DVLOG(2) << "export variable files ok, time " << ti.elapsed() << "\n";

    ti.restart();
    DVLOG(2) << "export time set\n";
    this->saveTimeSet();
    DVLOG(2) << "export time set ok, time " << ti.elapsed() << "\n";

    ti.restart();
    DVLOG(2) << "export case file\n";
    writeCaseFile();
    DVLOG(2) << "export case file ok, time " << ti.elapsed() << "\n";

    DVLOG(2) << "export sos\n";
    writeSoSFile();
    DVLOG(2) << "export sos ok, time " << ti.elapsed() << "\n";
}

template<typename MeshType, int N>
void
ExporterEnsightGold<MeshType,N>::writeSoSFile() const
{
    // only on proc 0
    if ( Environment::worldComm().rank() == Environment::worldComm().masterRank() )
    {
        std::ostringstream filestr;
        filestr << this->path() << "/" << this->prefix() << "-" << Environment::worldComm().globalSize() << ".sos";
        std::ofstream __out( filestr.str().c_str() );

        if ( __out.fail() )
        {
            DVLOG(2) << "cannot open " << filestr.str()  << "\n";
            exit( 0 );
        }

        __out << "FORMAT:\n"
              << "type: master_server gold \n\n";
        if( boption( _name="exporter.merge.markers") )
        {
            __out << "MULTIPLE_CASEFILES\n"
                << "total number of cfiles: 1 " << std::endl
                << "cfiles global path: " << fs::current_path().string() << "\n"
                << "cfiles pattern: "<<this->prefix() << ".case\n"
                << "cfiles start number: 0\n"
                << "cfiles increment: 1\n\n";
        }
        else
        {
            __out << "MULTIPLE_CASEFILES\n"
                << "total number of cfiles: " << Environment::worldComm().globalSize() << "\n"
                << "cfiles global path: " << fs::current_path().string() << "\n"
                << "cfiles pattern: "<<this->prefix() << "-" << Environment::worldComm().globalSize() << "_*.case\n"
                << "cfiles start number: 0\n"
                << "cfiles increment: 1\n\n";
        }
        __out << "SERVERS\n"
              << "number of servers: "<< (Environment::worldComm().globalSize()/100)+1 <<" repeat\n";

        //
        // save also a sos that paraview can understand, the previous format
        // does not seem to be supported by paraview
        //
        std::ostringstream filestrparaview;
        filestrparaview << this->path() << "/" << this->prefix() << "-paraview-" << Environment::worldComm().globalSize() << ".sos";
        std::ofstream __outparaview( filestrparaview.str().c_str() );

        __outparaview << "FORMAT:\n"
                      << "type: master_server gold \n"
                      << "SERVERS\n";

        /* set the number of servers to one */
        /* if we merged the markers in a single file */
        if( boption( _name="exporter.merge.markers") )
        {
            __outparaview << "number of servers: 1 " << std::endl;

            __outparaview << "#Server " << 1 << "\n"
                << "machine id: " << mpi::environment::processor_name() << "\n"
                << "executable: /usr/local/bin/ensight76/bin/ensight7.server\n"
                << "data_path: " << fs::current_path().string() << "\n"
                << "casefile: " << this->prefix() << ".case\n";
        }
        else
        {
            __outparaview << "number of servers: " << Environment::worldComm().globalSize() << "\n";

            for ( int pid = 0 ; pid < Environment::worldComm().globalSize(); ++pid )
            {
                __outparaview << "#Server " << pid+1 << "\n"
                    << "machine id: " << mpi::environment::processor_name() << "\n"
                    << "executable: /usr/local/bin/ensight76/bin/ensight7.server\n"
                    << "data_path: " << fs::current_path().string() << "\n"
                    << "casefile: " << this->prefix() << "-" << Environment::worldComm().globalSize() << "_" << pid << ".case\n";
            }
        }
    }
}
template<typename MeshType, int N>
void
ExporterEnsightGold<MeshType,N>::writeCaseFile() const
{
    std::ostringstream filestr;

    filestr << this->path() << "/"
            << this->prefix();
    if( ! boption( _name="exporter.merge.markers") )
    { filestr << "-" << Environment::worldComm().globalSize() << "_" << Environment::worldComm().globalRank(); }
    filestr << ".case";

    std::ofstream __out( filestr.str().c_str() );

    if ( __out.fail() )
    {
        DVLOG(2) << "cannot open " << filestr.str()  << "\n";
        exit( 0 );
    }

    __out << "FORMAT:\n"
          << "type: ensight gold\n"
          << "GEOMETRY:\n";

    timeset_const_iterator __ts_it = this->beginTimeSet();
    timeset_const_iterator __ts_en = this->endTimeSet();

    switch ( this->exporterGeometry() )
    {
    case EXPORTER_GEOMETRY_STATIC:
    {
        timeset_ptrtype __ts = *__ts_it;
        __out << "model: " << __ts->name();
        if( ! boption( _name="exporter.merge.markers") )
        { __out << "-" << Environment::worldComm().globalSize() << "_" << Environment::worldComm().globalRank(); }
        __out << ".geo";
    }
    break;
    default:
    case EXPORTER_GEOMETRY_CHANGE_COORDS_ONLY:
    case EXPORTER_GEOMETRY_CHANGE:
    {
        while ( __ts_it != __ts_en )
        {
            timeset_ptrtype __ts = *__ts_it;

            //if ( this->useSingleTransientFile() )
            if ( boption( _name="exporter.merge.timesteps") )
            {
                __out << "model: " << __ts->index() << " 1 " << __ts->name();
                if( ! boption( _name="exporter.merge.markers") )
                { __out << "-" << Environment::worldComm().globalSize() << "_" << Environment::worldComm().globalRank(); }
                __out << ".geo";
            }
            else
            {
                __out << "model: " << __ts->index() << " " << __ts->name();
                if( ! boption( _name="exporter.merge.markers") )
                { __out << "-" << Environment::worldComm().globalSize() << "_" << Environment::worldComm().globalRank(); }
                __out << ".geo" << "." << std::string(M_timeExponent, '*');
            }

            if ( this->exporterGeometry() == EXPORTER_GEOMETRY_CHANGE_COORDS_ONLY )
                __out << " change_coords_only";

            ++__ts_it;
        }
    }
    break;
    }
    __out << "\n";

#if 1
    __out << "VARIABLES:" << "\n";

    __ts_it = this->beginTimeSet();

    while ( __ts_it != __ts_en )
    {
        timeset_ptrtype __ts = *__ts_it;

        // treat constant per case
        auto s_it = ( *__ts->rbeginStep() )->beginScalar();
        auto s_en = ( *__ts->rbeginStep() )->endScalar();
        for ( ; s_it != s_en; ++s_it )
        {
            if ( s_it->second.second )
            {
                // constant over time
                __out << "constant per case: " << s_it->first << " " << s_it->second.first << "\n";
            }
            else
            {
                if ( this->worldComm().isMasterRank() )
                {

                __out << "constant per case file: " << __ts->index() << " " << s_it->first << " " << s_it->first << ".scl";
                    // loop over time
                    auto stepit = __ts->beginStep();
                    auto stepen = __ts->endStep();

                    std::ofstream ofs;
                    int d = std::distance( stepit, stepen );
                    LOG(INFO) << "distance = " << d;
                    if ( d > 1 )
                        ofs.open( s_it->first+".scl", std::ios::out | std::ios::app );
                    else
                        ofs.open( s_it->first+".scl", std::ios::out );

                    auto step = *boost::prior(stepen);
                    ofs << step->scalar( s_it->first ) << "\n";
                    ofs.close();
                __out << "\n";
                }

            }
        }

        typename timeset_type::step_type::nodal_scalar_const_iterator __it = ( *__ts->rbeginStep() )->beginNodalScalar();
        typename timeset_type::step_type::nodal_scalar_const_iterator __end = ( *__ts->rbeginStep() )->endNodalScalar();

        while ( __it != __end )
        {
            //if ( this->useSingleTransientFile() )
            if ( boption( _name="exporter.merge.timesteps") )
            {
                __out << "scalar per node: "
                      << __ts->index() << " 1 " // << *__ts_it->beginStep() << " "
                      << __it->second.name() << " " << __it->first;
                if( ! boption( _name="exporter.merge.markers") )
                { __out << "-" << Environment::worldComm().globalSize() << "_" << Environment::worldComm().globalRank(); }
                __out << ".scl" << std::endl;
            }
            else
            {
                __out << "scalar per node: "
                      << __ts->index() << " " // << *__ts_it->beginStep() << " "
                      << __it->second.name() << " " << __it->first;
                if( ! boption( _name="exporter.merge.markers") )
                { __out << "-" << Environment::worldComm().globalSize() << "_" << Environment::worldComm().globalRank(); }
                __out << ".scl" << "." << std::string(M_timeExponent, '*') << std::endl;
            }
            ++__it;
        }

        typename timeset_type::step_type::nodal_vector_const_iterator __itv = ( *__ts->rbeginStep() )->beginNodalVector();
        typename timeset_type::step_type::nodal_vector_const_iterator __env = ( *__ts->rbeginStep() )->endNodalVector();

        while ( __itv != __env )
        {
            //if ( this->useSingleTransientFile() )
            if ( boption( _name="exporter.merge.timesteps") )
            {
                __out << "vector per node: "
                      << __ts->index() << " 1 " // << *__ts_it->beginStep() << " "
                      << __itv->second.name() << " " << __itv->first;
                if( ! boption( _name="exporter.merge.markers") )
                { __out << "-" << Environment::worldComm().globalSize() << "_" << Environment::worldComm().globalRank(); }
                __out << ".vec" << std::endl;
            }
            else
            {
                __out << "vector per node: "
                      << __ts->index() << " " // << *__ts_it->beginStep() << " "
                      << __itv->second.name() << " " << __itv->first;
                if( ! boption( _name="exporter.merge.markers") )
                { __out << "-" << Environment::worldComm().globalSize() << "_" << Environment::worldComm().globalRank(); }
                __out << ".vec" << "." << std::string(M_timeExponent, '*') << std::endl;
            }
            ++__itv;
        }

        typename timeset_type::step_type::nodal_tensor2_const_iterator __itt = ( *__ts->rbeginStep() )->beginNodalTensor2();
        typename timeset_type::step_type::nodal_tensor2_const_iterator __ent = ( *__ts->rbeginStep() )->endNodalTensor2();

        while ( __itt != __ent )
        {
            //if ( this->useSingleTransientFile() )
            if ( boption( _name="exporter.merge.timesteps") )
            {
                __out << "tensor per node: "
                      << __ts->index() << " 1 " // << *__ts_it->beginStep() << " "
                      << __itt->second.name() << " " << __itt->first;
                if( ! boption( _name="exporter.merge.markers") )
                { __out << "-" << Environment::worldComm().globalSize() << "_" << Environment::worldComm().globalRank(); }
                __out << ".tsr" << std::endl;
            }
            else
            {
                __out << "tensor per node: "
                      << __ts->index() << " " // << *__ts_it->beginStep() << " "
                      << __itt->second.name() << " " << __itt->first;
                if( ! boption( _name="exporter.merge.markers") )
                { __out << "-" << Environment::worldComm().globalSize() << "_" << Environment::worldComm().globalRank(); }
                __out << ".tsr" << "." << std::string(M_timeExponent, '*') << std::endl; 
            }
            ++__itt;
        }

        typename timeset_type::step_type::element_scalar_const_iterator __it_el = ( *__ts->rbeginStep() )->beginElementScalar();
        typename timeset_type::step_type::element_scalar_const_iterator __end_el = ( *__ts->rbeginStep() )->endElementScalar();

        while ( __it_el != __end_el )
        {
            //if ( this->useSingleTransientFile() )
            if ( boption( _name="exporter.merge.timesteps") )
            {
                __out << "scalar per element: "
                      << __ts->index() << " 1 " // << *__ts_it->beginStep() << " "
                      << __it_el->second.name() << " " << __it_el->first;
                if( ! boption( _name="exporter.merge.markers") )
                { __out << "-" << Environment::worldComm().globalSize() << "_" << Environment::worldComm().globalRank(); }
                __out << ".scl" << std::endl;
            }
            else
            {
                __out << "scalar per element: "
                      << __ts->index() << " " // << *__ts_it->beginStep() << " "
                      << __it_el->second.name() << " " << __it_el->first;
                if( ! boption( _name="exporter.merge.markers") )
                { __out << "-" << Environment::worldComm().globalSize() << "_" << Environment::worldComm().globalRank(); }
                __out << ".scl" << "." << std::string(M_timeExponent, '*') << std::endl;
            }
            ++__it_el;
        }

        typename timeset_type::step_type::element_vector_const_iterator __itv_el = ( *__ts->rbeginStep() )->beginElementVector();
        typename timeset_type::step_type::element_vector_const_iterator __env_el = ( *__ts->rbeginStep() )->endElementVector();

        while ( __itv_el != __env_el )
        {
            //if ( this->useSingleTransientFile() )
            if ( boption( _name="exporter.merge.timesteps") )
            {
                __out << "vector per element: "
                      << __ts->index() << " 1 " // << *__ts_it->beginStep() << " "
                      << __itv_el->second.name() << " " << __itv_el->first;
                if( ! boption( _name="exporter.merge.markers") )
                { __out << "-" << Environment::worldComm().globalSize() << "_" << Environment::worldComm().globalRank(); }
                __out << ".vec" << std::endl;
            }
            else
            {
                __out << "vector per element: "
                      << __ts->index() << " " // << *__ts_it->beginStep() << " "
                      << __itv_el->second.name() << " " << __itv_el->first;
                if( ! boption( _name="exporter.merge.markers") )
                { __out << "-" << Environment::worldComm().globalSize() << "_" << Environment::worldComm().globalRank(); }
                __out << ".vec" << "." << std::string(M_timeExponent, '*') << std::endl;
            }
            ++__itv_el;
        }

        typename timeset_type::step_type::element_tensor2_const_iterator __itt_el = ( *__ts->rbeginStep() )->beginElementTensor2();
        typename timeset_type::step_type::element_tensor2_const_iterator __ent_el = ( *__ts->rbeginStep() )->endElementTensor2();

        while ( __itt_el != __ent_el )
        {
            if ( boption( _name="exporter.merge.timesteps") )
            {
                __out << "tensor per element: "
                      << __ts->index() << " 1 " // << *__ts_it->beginStep() << " "
                      << __itt_el->second.name() << " " << __itt_el->first;
                if( ! boption( _name="exporter.merge.markers") )
                { __out << "-" << Environment::worldComm().globalSize() << "_" << Environment::worldComm().globalRank(); }
                __out << ".tsr" << std::endl;
            }
            else
            {
                __out << "tensor per element: "
                    << __ts->index() << " " // << *__ts_it->beginStep() << " "
                    << __itt_el->second.name() << " " << __itt_el->first;
                if( ! boption( _name="exporter.merge.markers") )
                { __out << "-" << Environment::worldComm().globalSize() << "_" << Environment::worldComm().globalRank(); }
                __out << ".tsr" << "." << std::string(M_timeExponent, '*') << std::endl;
            }
            ++__itt_el;
        }

        ++__ts_it;
    }

#endif
    __out << "TIME:\n";
    __ts_it = this->beginTimeSet();

    while ( __ts_it != __ts_en )
    {
        timeset_ptrtype __ts = *__ts_it;
        typename timeset_type::step_const_iterator __its = __ts->beginStep();

        __out << "time set:        " << __ts->index() << "\n"
              << "number of steps: " << __ts->numberOfSteps() << "\n"
              << "filename start number: " << ( *__its )->index() << "\n"
              << "filename increment: " << 1 << "\n"
              << "time values: ";

        uint16_type __l = 0;
        typename timeset_type::step_const_iterator __ens = __ts->endStep();

        while ( __its != __ens )
        {

            __out << ( *__its )->time() << " ";

            if ( __l++ % 10 == 0 )
                __out << "\n";

            ++__its;
        }

#if 0
        namespace lambda = boost::lambda;
        std::for_each( __ts->beginStep(), __ts->endStep(),
                       __out << lambda::bind( &timeset_type::step_type::time, *lambda::_1 ) << boost::lambda::constant( ' ' ) );
        std::for_each( __ts->beginStep(), __ts->endStep(),
                       std::cerr << lambda::bind( &timeset_type::step_type::time, *lambda::_1 ) << boost::lambda::constant( ' ' ) );
#endif
        ++__ts_it;
    }
    __out << "\n";

    //if ( this->useSingleTransientFile() )
    if ( boption( _name="exporter.merge.timesteps") )
    {
        __out << "FILE\n";
        __out << "file set: 1\n";
        auto ts = *this->beginTimeSet();
        __out << "number of steps: " << ts->numberOfSteps() << "\n";
    }

    __out << "\n";

    if ( option( _name="exporter.ensightgold.use-sos" ).template as<bool>() == false )
    {
        if ( ( Environment::numberOfProcessors() > 1 )  && ( this->worldComm().globalRank() == 0 ) )
        {
            __out << "APPENDED_CASEFILES\n"
                  << "total number of cfiles: " << Environment::numberOfProcessors()-1 << "\n"
                // no need for that
                // << "cfiles global path: " << fs::current_path().string() << "\n"
                  << "cfiles: ";
            for(int p = 1; p < Environment::numberOfProcessors(); ++p )
            {
                std::ostringstream filestr;
                filestr << this->prefix() << "-"
                        << this->worldComm().globalSize() << "_" << p << ".case";
                __out << filestr.str() << "\n        ";
            }
        }
    } // use-sos

    __out.close();

}

template<typename MeshType, int N>
void
ExporterEnsightGold<MeshType,N>::writeGeoFiles() const
{
    namespace lambda = boost::lambda;

    int size;
    char buffer[80];

    MPI_File fh;
    MPI_Status status;
    MPI_Info info;
    MPI_Offset offset = 0;

    timeset_const_iterator __ts_it = this->beginTimeSet();
    timeset_const_iterator __ts_en = this->endTimeSet();

    while ( __ts_it != __ts_en )
    {
        timeset_ptrtype __ts = *__ts_it;

        typename timeset_type::step_const_iterator __it = __ts->beginStep();
        typename timeset_type::step_const_iterator __end = __ts->endStep();
        __it = boost::prior( __end );

        /* static geometry */
        /* only write the geometry in the first timestep */
        if ( this->exporterGeometry() == EXPORTER_GEOMETRY_STATIC && (__it == __ts->beginStep())  )
        {
            LOG(INFO) << "GEO: Static geo mode" << std::endl;

            time_index = 0 ;

            /* generate geo filename */
            std::ostringstream __geofname;
            __geofname << this->path() << "/" << __ts->name();
            if( ! boption( _name="exporter.merge.markers") )
            { __geofname << "-" << Environment::worldComm().globalSize() << "_" << Environment::worldComm().localRank(); }
            __geofname << ".geo";
            M_filename =  __geofname.str();
            CHECK( (*__it)->hasMesh() || __ts->hasMesh()  ) << "Invalid mesh data structure in static geometry mode\n";

            /* write info */
            mesh_ptrtype mesh;
            if ( __ts->hasMesh() )
                mesh = __ts->mesh();
            if ( (*__it)->hasMesh() && !__ts->hasMesh() )
                mesh = (*__it)->mesh();

            /* Open File with MPI IO */
            char * str = strdup(__geofname.str().c_str());

            /* Check if file exists and delete it, if so */
            /* (MPI IO does not have a truncate mode ) */
            if(this->worldComm().isMasterRank() && fs::exists(str))
            {
                MPI_File_delete(str, MPI_INFO_NULL);
            }
            MPI_Barrier( Environment::worldComm().comm() );

            MPI_File_open( this->worldComm().comm(), str, MPI_MODE_RDWR | MPI_MODE_CREATE, MPI_INFO_NULL, &fh );

            free(str);

            Feel::detail::FileIndex index;

            if(boption( _name="exporter.merge.timesteps" ))
            {
                // first read
                index.read(fh);

                // we position the cursor at the beginning of the file
                MPI_File_seek_shared(fh, 0, MPI_SEEK_SET);

                /* write C binary if we didn't find the index <=> first pass on the file */
                if( !index.defined() )
                {
                    if( this->worldComm().isMasterRank() )
                    { size = sizeof(buffer); }
                    else
                    { size = 0; }
                    memset(buffer, '\0', sizeof(buffer));
                    strcpy(buffer, "C Binary");
                    MPI_File_write_ordered(fh, buffer, size, MPI_CHAR, &status);
                }

                /* Write time step start */
                if( this->worldComm().isMasterRank() )
                { size = sizeof(buffer); }
                else
                { size = 0; }
                memset(buffer, '\0', sizeof(buffer));
                strcpy(buffer,"BEGIN TIME STEP");
                MPI_File_write_ordered(fh, buffer, size, MPI_CHAR, &status);
                LOG(INFO) << "saveNodal out: " << buffer;

                /* add the beginning of the new block to the file */
                MPI_File_get_position_shared(fh, &offset);
                index.add( offset );
            }

            /* Write the file */
            this->writeGeoMarkers(fh, mesh);

            if(boption( _name="exporter.merge.timesteps" ))
            {
                /* write timestep end */
                if( this->worldComm().isMasterRank() )
                { size = sizeof(buffer); }
                else
                { size = 0; }
                memset(buffer, '\0', sizeof(buffer));
                strcpy(buffer,"END TIME STEP");
                MPI_File_write_ordered(fh, buffer, size, MPI_CHAR, &status);

                // write back the file index
                index.write( fh );
            }

            /* close file */
            MPI_File_close(&fh);
        }
        /* changing geometry */
        else
        {
            LOG(INFO) << "GEO: Changing geo mode" << std::endl;
            /* Transient mode */
            if ( boption( _name="exporter.merge.timesteps") )
            {
                /* TODO */
                /* MPI_File_Open -> f */
                /* P0 : Write "C Binary" */
                /* for each Timestep T in TS */
                /* P0 : Write "BEGIN TIME STEP" */
                /* WriteGeo(T, f) */
                /* P0 : Write "END TIME STEP" */

                while ( __it != __end )
                {
                    typename timeset_type::step_ptrtype __step = *__it;
                    time_index = __step->index();

                    std::ostringstream __geofname;

                    __geofname << this->path() << "/"
                        << __ts->name();
                    if( ! boption( _name="exporter.merge.markers") )
                    { __geofname << "-" << Environment::worldComm().globalSize() << "_" << Environment::worldComm().localRank(); }
                    __geofname << ".geo";

                    if ( __step->isInMemory() )
                    {
                        //__writegeo( __step->mesh(), __ts->name(), __geofname.str() );
                        //, __ts->name(), __geofname.str() );
                        M_filename =  __geofname.str();
                        //__step->mesh()->accept( const_cast<ExporterEnsightGold<MeshType,N>&>( *this ) );

                        auto mesh = __step->mesh();

                        /* Open File with MPI IO */
                        char * str = strdup(M_filename.c_str());

                        /* Check if file exists and delete it, if so */
                        /* (MPI IO does not have a truncate mode ) */
                        if(this->worldComm().isMasterRank() && __it == __ts->beginStep() && fs::exists(str))
                        {
                            MPI_File_delete(str, MPI_INFO_NULL);
                        }
                        MPI_Barrier(Environment::worldComm().comm());

                        MPI_File_open( this->worldComm().comm(), str, MPI_MODE_RDWR | MPI_MODE_CREATE, MPI_INFO_NULL, &fh );
                        free(str);

                        Feel::detail::FileIndex index;

                        // first read
                        index.read(fh);

                        /* Move to the beginning of the fie index section */
                        /* to overwrite it */
                        if ( index.defined() && __step->index() > 0 ) {
                            MPI_File_seek_shared(fh, index.fileblock_n_steps, MPI_SEEK_SET);
                        }
                        else {
                            // we position the cursor at the beginning of the file
                            MPI_File_seek_shared(fh, 0, MPI_SEEK_SET);
                        }

                        /* write C binary if we didn't find the index <=> first pass on the file */
                        if( !index.defined() )
                        {
                            if( this->worldComm().isMasterRank() )
                            { size = sizeof(buffer); }
                            else
                            { size = 0; }
                            memset(buffer, '\0', sizeof(buffer));
                            strcpy(buffer, "C Binary");
                            MPI_File_write_ordered(fh, buffer, size, MPI_CHAR, &status);
                        }

                        /* Write time step start */
                        if( this->worldComm().isMasterRank() )
                        { size = sizeof(buffer); }
                        else
                        { size = 0; }
                        memset(buffer, '\0', sizeof(buffer));
                        strcpy(buffer,"BEGIN TIME STEP");
                        MPI_File_write_ordered(fh, buffer, size, MPI_CHAR, &status);
                        LOG(INFO) << "saveNodal out: " << buffer;

                        /* add the beginning of the new block to the file */
                        MPI_File_get_position_shared(fh, &offset);
                        index.add( offset );

                        /* write data for timestep */
                        this->writeGeoMarkers(fh, mesh);

                        /* write timestep end */
                        if( this->worldComm().isMasterRank() )
                        { size = sizeof(buffer); }
                        else
                        { size = 0; }
                        memset(buffer, '\0', sizeof(buffer));
                        strcpy(buffer,"END TIME STEP");
                        MPI_File_write_ordered(fh, buffer, size, MPI_CHAR, &status);

                        // write back the file index
                        index.write( fh );
                    }

                    ++__it;
                }
            }
            /* non transient */
            else
            {
                // TODO
                /* for each Timestep T in TS */
                /* MPI_File_Open -> f */
                /* WriteGeo(T, f) */
                int i = 0;
                while ( __it != __end )
                {
                    LOG(INFO) << "Step " << i++ << std::endl;
                    typename timeset_type::step_ptrtype __step = *__it;
                    time_index = __step->index();

                    std::ostringstream __geofname;

                    __geofname << this->path() << "/" << __ts->name();
                    if( ! boption( _name="exporter.merge.markers") )
                    { __geofname << "-" << Environment::worldComm().globalSize() << "_" << Environment::worldComm().localRank(); }
                    __geofname << ".geo" << "." << std::setfill( '0' ) << std::setw( M_timeExponent ) << __step->index();

                    if ( __step->isInMemory() )
                    {
                        //__writegeo( __step->mesh(), __ts->name(), __geofname.str() );
                        //, __ts->name(), __geofname.str() );
                        M_filename =  __geofname.str();
                        //__step->mesh()->accept( const_cast<ExporterEnsightGold<MeshType,N>&>( *this ) );

                        auto mesh = __step->mesh();

                        /* Open File with MPI IO */
                        char * str = strdup(M_filename.c_str());

                        /* Check if file exists and delete it, if so */
                        /* (MPI IO does not have a truncate mode ) */
                        if(this->worldComm().isMasterRank() && fs::exists(str))
                        {
                            MPI_File_delete(str, MPI_INFO_NULL);
                        }
                        MPI_Barrier(Environment::worldComm().comm());

                        MPI_File_open( this->worldComm().comm(), str, MPI_MODE_RDWR | MPI_MODE_CREATE, MPI_INFO_NULL, &fh );
                        free(str);

                        /* Either write every marker in one file */
                        this->writeGeoMarkers(fh, mesh);

                        /* close file */
                        MPI_File_close(&fh);
                    }

                    ++__it;
                }
            }
        }

        ++__ts_it;
    }
}

template<typename MeshType, int N>
void
ExporterEnsightGold<MeshType, N>::writeGeoHeader(MPI_File fh) const
{
    MPI_Status status;

    int size = 0;
    char buffer[80];

    DVLOG(2) << "Merging markers : " << "\n";

    /* write header */
    /* little trick to only perform collective operation (optimized) */
    /* and avoid scattering offset and reseting the shared pointer if we would write this only on master proc */
    if( this->worldComm().isMasterRank() )
    { size = sizeof(buffer); }
    else
    { size = 0; }

    // only write C Binary if we are not mergin timesteps
    // as it is oalready writtent at the beginning of the file
    if ( ! boption( _name="exporter.merge.timesteps") )
    {
        memset(buffer, '\0', sizeof(buffer));
        strcpy(buffer, "C Binary");
        MPI_File_write_ordered(fh, buffer, size, MPI_CHAR, &status);
        //LOG(INFO) << "wrote " << buffer << std::endl;
    }

    // get only the filename (maybe with full path)
    fs::path gp = M_filename;
    std::string theFileName = gp.filename().string();
    CHECK( theFileName.length() < 80 ) << "the file name is too long : theFileName=" << theFileName << "\n";

    memset(buffer, '\0', sizeof(buffer));
    strcpy( buffer, theFileName.c_str() );
    MPI_File_write_ordered(fh, buffer, size, MPI_CHAR, &status);
    //LOG(INFO) << "wrote " << buffer << std::endl;

    memset(buffer, '\0', sizeof(buffer));
    strcpy( buffer, "elements" );
    MPI_File_write_ordered(fh, buffer, size, MPI_CHAR, &status);

    memset(buffer, '\0', sizeof(buffer));
    strcpy( buffer, "node id given" );
    MPI_File_write_ordered(fh, buffer, size, MPI_CHAR, &status);

    memset(buffer, '\0', sizeof(buffer));
    strcpy( buffer, "element id given" );
    MPI_File_write_ordered(fh, buffer, size, MPI_CHAR, &status);
    //MPI_File_write(fh, buffer, sizeof(buffer), MPI_CHAR, &status );

}

template<typename MeshType, int N>
void
ExporterEnsightGold<MeshType,N>::writeGeoMarkers(MPI_File fh, mesh_ptrtype mesh) const
{
    /* Function WriteGeoMarker(Timestep T, MPI_File f) */
        /* P0 : Write "C Binary", part, #, desc, coords */
        /* for each part Marker in T */
            /* P0 : Write "part", Write # */
            /* P* : Compute nn, build points id array, build points array */
            /* P0 : Write nn */
            /* P* : Write_shared id */
            /* P* : Write_shared points */

            /* for each face/element type */
                /* P* : compute ne, build elements id array, build elements array */
                /* P0 : Write element type */
                /* P0 : Write ne */
                /* P* : Write_shared id */
                /* P* : Write_shared elements */
                /* WriteGeoPart(P, f) */

    // TODO write the faces
    // Integrate them into the parts corresponding to the elements ?
    int nbmarkers = mesh->markerNames().size();

    LOG(INFO) << "nMarkers " << nbmarkers << std::endl;

    if(1/*boption( _name="exporter.merge.markers" )*/)
    {
        // TODO find a better place for this code to be executed
        // as we have allreduce, this can take serious execution time
        /* determine for which markers, we have data to write */
#if 0
        /* Display info about markers */
        std::ostringstream ossmn;
        ossmn << Environment::worldComm().rank() << " markers ";
        BOOST_FOREACH( auto marker, mesh->markerNames() )
        {
            ossmn << " " << marker.second[0];
        }
        LOG(INFO) << ossmn.str() << std::endl;
#endif

#if 0
        M_markersToWrite.clear();
        std::ostringstream osselts;
        osselts << mesh->worldComm().rank();
        BOOST_FOREACH( auto marker, mesh->markerNames() )
        {
            /* Check whether at least one process has elements to write */
            int localNElts = std::distance(mesh->beginElementWithMarker(marker.second[0]), mesh->endElementWithMarker(marker.second[0]));
            int globalNElts = 0;

            osselts << " " << marker.second[0] << " (" << localNElts << ")";

            mpi::all_reduce(Environment::worldComm(), localNElts, globalNElts, mpi::maximum<int>());

            /* if we have at least one element for the current marker */
            /* all the processes need to parse it to avoid deadlocks with gather in MeshPoints */
            if(globalNElts)
            {
                M_markersToWrite.push_back(marker.second[0]);
            }
        }
        LOG(INFO) << osselts.str() << std::endl;
#endif

#if 0
        std::ostringstream ossmw;
        ossmw << Environment::worldComm().rank() << " markersToWrite ";
        for(int i = 0; i < M_markersToWrite.size(); i++)
        {
            ossmw << " " << M_markersToWrite.at(i);
        }
        LOG(INFO) << ossmw.str() << std::endl;
#endif

        /* Write file header */
        this->writeGeoHeader(fh);

        /* Write faces */
        if ( option( _name="exporter.ensightgold.save-face" ).template as<bool>() )
        {
            for( std::pair<const std::string, std::vector<size_type> > & m : mesh->markerNames() )
            {
                this->writeGeoMarkedFaces(fh, mesh, m);
            }
        }

        // TODO If the number of parts per process is not the sam
        // some processes might not enter some instance of the function in the following loop
        // and inside it there is a allgather that would cause the processes to be stuck in it
        // check the number of parts
        /*
        int localNParts = std::distance(p_it, p_en);
        int globalNParts = 0;

        mpi::all_reduce(Environment::worldComm(), localNParts, globalNParts, mpi::maximum<int>());

        LOG(INFO) << Environment::worldComm().rank() << " " << localNParts << " " << globalNParts << " " << mesh->markerNames().size() << std::endl;
        */

        // TODO Removed this loop, as it was causing MPI deadlocks when the numebr of parts was different
        // from one process to the other
        /* Write elements */
        /*
        typename mesh_type::parts_const_iterator_type p_it = mesh->beginParts();
        typename mesh_type::parts_const_iterator_type p_en = mesh->endParts();

        for ( ; p_it != p_en; ++p_it )
        {
            this->writeGeoMarkedElements(fh, mesh, p_it);
        }
        */

        /* Check whether successive part id are the same on different processes */
        /* does not seem to be the case */
#if 1
        typename mesh_type::parts_const_iterator_type p_st = mesh->beginParts();
        typename mesh_type::parts_const_iterator_type p_en = mesh->endParts();

        std::ostringstream osspi;
        osspi << Environment::worldComm().rank() << " partid";
        for(auto p_it = p_st ; p_it != p_en; ++p_it )
        {
            osspi << " " << p_it->first << "(" << p_it->second << ")";
        }
        LOG(INFO) << osspi.str() << std::endl;
#endif

        /* iterate over the markes to get the different markers needed to be written */
        /*
        typename mesh_type::parts_const_iterator_type p_it = mesh->beginParts();
        typename mesh_type::parts_const_iterator_type p_en = mesh->endParts();
        */

        std::vector<int> localMarkers;
        for(auto p_it = p_st ; p_it != p_en; ++p_it )
        {
            localMarkers.push_back(p_it->first);
        }

        /* gather all the markers to be written on the different processes */
        /* to order the writing step */
        std::vector<std::vector<int> > globalMarkers;
        mpi::all_gather(Environment::worldComm(), localMarkers, globalMarkers);

        for(int i = 0; i < globalMarkers.size(); i++)
        {
            for(int j = 0; j < globalMarkers[i].size(); j++)
            {
                M_markersToWrite.insert(globalMarkers[i][j]);
            }
        }

        std::ostringstream osss;
        osss << Environment::worldComm().rank() << " parts/markers";
        for(std::set<int>::iterator it = M_markersToWrite.begin(); it != M_markersToWrite.end(); it++)
        {
            osss << " " << *it << " (" << std::distance(mesh->beginElementWithMarker(*it), mesh->endElementWithMarker(*it)) << ")";
        }
        LOG(INFO) << osss.str() << std::endl;

#if 1
        /* Working with marker names instead */
        //for(int i = 0; i < M_markersToWrite.size(); i++)
        for(std::set<int>::iterator mit = M_markersToWrite.begin(); mit != M_markersToWrite.end(); mit++)
        {
            this->writeGeoMarkedElements(fh, mesh, *mit);
        }

#if 0
        /* if we have no markers, we revert to the classical part implementation */
        if(M_markersToWrite.size() == 0)
        {
            typename mesh_type::parts_const_iterator_type p_it = mesh->beginParts();
            typename mesh_type::parts_const_iterator_type p_en = mesh->endParts();

            for ( ; p_it != p_en; ++p_it )
            {
                this->writeGeoMarkedElements(fh, mesh, p_it->first);
            }
        }
#endif
#endif
    }
    else
    {
    }
}


template<typename MeshType, int N>
void
ExporterEnsightGold<MeshType,N>::writeGeoMarkedFaces(MPI_File fh, mesh_ptrtype mesh, std::pair<const std::string, std::vector<size_type> > & m) const
{
    int size;
    char buffer[80];

    MPI_Status status;

    std::vector<int> idnode, idelem;

    LOG(INFO) << "Marker " << m.first << std::endl;
    /* save faces */
    if ( m.second[1] != mesh->nDim-1 )
        return;

    VLOG(1) << "writing face with marker " << m.first << " with id " << m.second[0];
    auto pairit = mesh->facesWithMarker( m.second[0], Environment::worldComm().localRank() );
    auto fit = pairit.first;
    auto fen = pairit.second;
    Feel::detail::MeshPoints<float> mp( mesh.get(), this->worldComm(), fit, fen, true, true, true );
    int __ne = std::distance( fit, fen );
    int nverts = fit->numLocalVertices;
    DVLOG(2) << "Faces : " << __ne << "\n";

    if( this->worldComm().isMasterRank() )
    { size = sizeof(buffer); }
    else
    { size = 0; }

    memset(buffer, '\0', sizeof(buffer));
    strcpy( buffer, "part" );
    MPI_File_write_ordered(fh, buffer, size, MPI_CHAR, &status);
    int partid = m.second[0]; 

    if( this->worldComm().isMasterRank() )
    { size = 1; }
    else
    { size = 0; }
    MPI_File_write_ordered(fh, &partid, size, MPI_INT, &status);

    if( this->worldComm().isMasterRank() )
    { size = sizeof(buffer); }
    else
    { size = 0; }
    memset(buffer, '\0', sizeof(buffer));
    strncpy(buffer, m.first.c_str(), sizeof(buffer) - 1 );
    MPI_File_write_ordered(fh, buffer, size, MPI_CHAR, &status);

    if( this->worldComm().isMasterRank() )
    { size = sizeof(buffer); }
    else
    { size = 0; }
    memset(buffer, '\0', sizeof(buffer));
    strcpy(buffer, "coordinates");
    MPI_File_write_ordered(fh, buffer, size, MPI_CHAR, &status);

    // write points coordinates
    fit = pairit.first;
    fen = pairit.second;

    size_type __nv = mp.ids.size();
    size_type gnop = mp.globalNumberOfPoints();
    if( this->worldComm().isMasterRank() )
    { size = 1; }
    else
    { size = 0; }
    // write number of points
    MPI_File_write_ordered(fh, &gnop, size, MPI_INT, &status);
    /* write points ids */
    MPI_File_write_ordered(fh, mp.ids.data(), mp.ids.size(), MPI_INT, &status );
    /* write points coordinates in the order x1 ... xn y1 ... yn z1 ... zn */
    for(int i = 0; i < 3; i++)
    {
        MPI_File_write_ordered(fh, mp.coords.data() + i * __nv, __nv, MPI_FLOAT, &status );
    }

    // write connectivity
    fit = pairit.first;
    fen = pairit.second;

    if( this->worldComm().isMasterRank() )
    { size = sizeof(buffer); }
    else
    { size = 0; }
    memset(buffer, '\0', sizeof(buffer));
    strcpy( buffer, M_face_type.c_str() );
    MPI_File_write_ordered(fh, buffer, size, MPI_CHAR, &status);
    VLOG(1) << "face type " << buffer;

    if( this->worldComm().isMasterRank() )
    { size = 1; }
    else
    { size = 0; }
    MPI_File_write_ordered(fh, &__ne, size, MPI_INT, &status);
    VLOG(1) << "n faces " << __ne;

    idelem.resize( __ne );
    fit = pairit.first;
    size_type e = 0;
    for ( ; fit != fen; ++fit, ++e )
    {
        idelem[e] = fit->id() + 1;
    }
    CHECK( e == idelem.size() ) << "Invalid number of face id for part " << m.first;
    MPI_File_write_ordered(fh, idelem.data(), idelem.size(), MPI_INT, &status);

    idelem.resize( __ne*nverts );
    fit = pairit.first;
    e = 0;
    for( ; fit != fen; ++fit, ++e )
    {
        for ( size_type j = 0; j < nverts; j++ )
        {
            // ensight id start at 1
            idelem[e*nverts+j] = mp.old2new[fit->point( j ).id()];
        }
    }
    CHECK( e*nverts == idelem.size() ) << "Invalid number of faces " << e*nverts << " != " << idelem.size() << " in connectivity for part " << m.first;
    MPI_File_write_ordered(fh, idelem.data(), idelem.size(), MPI_INT, &status);

}

template<typename MeshType, int N>
void
ExporterEnsightGold<MeshType,N>::writeGeoMarkedElements(MPI_File fh, mesh_ptrtype mesh, size_type markerid) const
{
    MPI_Status status;

    int size = 0;
    char buffer[80];

    std::vector<int> idnode, idelem;

    //auto r = markedelements(mesh, part->first, EntityProcessType::ALL );
    auto r = markedelements(mesh, markerid, EntityProcessType::ALL );
    auto allelt_it = r.template get<1>();
    auto allelt_en = r.template get<2>();

    //VLOG(1) << "material : " << m << " total nb element: " << std::distance(allelt_it, allelt_en );
    //VLOG(1) << "material : " << m << " ghost nb element: " << std::distance(gelt_it, gelt_en );
    //VLOG(1) << "material : " << m << " local nb element: " << std::distance(lelt_it, lelt_en );
    Feel::detail::MeshPoints<float> mp( mesh.get(), this->worldComm(), allelt_it, allelt_en, true, true, true );
    VLOG(1) << "mesh pts size : " << mp.ids.size();

    // part
    memset(buffer, '\0', sizeof(buffer));
    strcpy( buffer, "part" );

    if( this->worldComm().isMasterRank() )
    { size = sizeof(buffer); }
    else
    { size = 0; }
    // write number of points
    MPI_File_write_ordered(fh, buffer, size, MPI_CHAR, &status);
    /*
       MPI_File_seek(fh, -sizeof(buffer), MPI_SEEK_CUR );
       MPI_File_read(fh, buffer, sizeof(buffer), MPI_CHAR, &status );
       LOG(INFO) << " --> read back part->buffer = " << buffer << std::endl;
       */

    // Was previously using p_it->first as partid
    // part id
    int partid = markerid;

    if ( this->worldComm().isMasterRank() )
        LOG(INFO) << "writing part " << partid << std::endl;

    if( this->worldComm().isMasterRank() )
    { size = 1; }
    else
    { size = 0; }
    // write number of points
    MPI_File_write_ordered(fh, &partid, size, MPI_INT, &status);

    // material
    memset(buffer, '\0', sizeof(buffer));
    //sprintf(buffer, "Material %d", part->first);
    sprintf(buffer, "Material %d", (int)(markerid));
    if( this->worldComm().isMasterRank() )
    { size = sizeof(buffer); }
    else
    { size = 0; }
    MPI_File_write_ordered(fh, buffer, size, MPI_CHAR, &status);

    memset(buffer, '\0', sizeof(buffer));
    strcpy( buffer, "coordinates" );
    if( this->worldComm().isMasterRank() )
    { size = sizeof(buffer); }
    else
    { size = 0; }
    MPI_File_write_ordered(fh, buffer, size, MPI_CHAR, &status);

    /* readback */
    /*
       MPI_File_seek(fh, -sizeof(buffer), MPI_SEEK_CUR );
       MPI_File_read(fh, buffer, sizeof(buffer), MPI_CHAR, &status );
       LOG(INFO) << " --> read back coordinates->buffer = " << buffer << std::endl;
       */

    size_type __nv = mp.ids.size();
    size_type gnop = mp.globalNumberOfPoints();
    if( this->worldComm().isMasterRank() )
    { size = 1; }
    else
    { size = 0; }
    MPI_File_write_ordered(fh, &gnop, size, MPI_INT, &status );
    // now we need to move with respect to the processors for the coordinates
    /* readback */
    /*
       MPI_Offset offset2;
       MPI_File_get_position(fh, &offset2 );
       int nn;
       MPI_File_read_at( fh, offset2-sizeof(int), &nn, 1, MPI_INT, &status );
       LOG(INFO) << "read npts->nn = " << nn << " was " << __nv << std::endl;
       */

#if 0
    MPI_File_write(fh, &mp.ids.front(), mp.ids.size(), MPI_INT, &status );


    std::vector<int> ids(mp.ids.size());
    MPI_File_get_position(fh, &offset2 );
    MPI_File_read_at(fh, offset2-mp.ids.size()*sizeof(int),&ids.front(), ids.size(), MPI_INT, &status );
    if ( Environment::rank() == 0 )
    {
        std::ofstream ofs( "ids" );
        std::for_each( ids.begin(), ids.end(), [&]( int i ) { ofs << i << "\n"; } );
    }
    else
    {
        std::ofstream ofs( "titi" );
        std::for_each( ids.begin(), ids.end(), [&]( int i ) { ofs << i << "\n"; } );
    }
#endif

    // TODO modify this code !
    // Integrate id translation in the MeshPoints constructor
    /* write points ids */
    std::vector<int> pointids;
    for(int i = 0 ; i < mp.ids.size() ; ++i )
    {
        pointids.push_back(mp.ids.at(i) + mp.offsets_pts);
    }

    MPI_File_write_ordered(fh, pointids.data(), pointids.size(), MPI_INT, &status );
    /* write points coordinates in the order x1 ... xn y1 ... yn z1 ... zn */
    for(int i = 0; i < 3; i++)
    {
        MPI_File_write_ordered(fh, mp.coords.data() + i * __nv, __nv, MPI_FLOAT, &status );
    }

    /* readback */
    /*
       MPI_File_get_position(fh, &offset2 );
       MPI_File_read_at(fh, offset2-3*mp.globalNumberOfPoints()*sizeof(float),&x.front(), x.size(), MPI_FLOAT, &status );
       MPI_File_read_at(fh, offset2-2*mp.globalNumberOfPoints()*sizeof(float),&y.front(), y.size(), MPI_FLOAT, &status );
       MPI_File_read_at(fh, offset2-mp.globalNumberOfPoints()*sizeof(float),&z.front(), z.size(), MPI_FLOAT, &status );
       if ( Environment::rank() == 0 )
       {
       std::ofstream ofs( "x" );
       std::for_each( x.begin(), x.end(), [&]( double i ) { ofs << i << "\n"; } );
       std::ofstream yfs( "y" );
       std::for_each( y.begin(), y.end(), [&]( double i ) { yfs << i << "\n"; } );
       std::ofstream zfs( "z" );
       std::for_each( z.begin(), z.end(), [&]( double i ) { zfs << i << "\n"; } );

       }
       */

    /*
       MPI_File_get_position(fh, &offset2 );
       MPI_File_read_at( fh, offset2-mp.global_offsets_pts-sizeof(int)-80, buffer, 80, MPI_CHAR, &status );
       LOG(INFO) << "read back coordinates->buffer = " << buffer << std::endl;
       */
    // local elements
    memset(buffer, '\0', sizeof(buffer));
    strcpy( buffer, this->elementType().c_str() );
    if( this->worldComm().isMasterRank() )
    { size = sizeof(buffer); }
    else
    { size = 0; }
    MPI_File_write_ordered(fh, buffer, size, MPI_CHAR, &status );
    /* readback */
    /*
       offset += sizeof(buffer);
       MPI_File_read_at( fh, offset-sizeof(buffer), buffer, 80, MPI_CHAR, &status );
       LOG(INFO) << "proc " << Environment::rank()
       << " --> read back element material->buffer = " << buffer << std::endl;
       */

    //auto r2 = markedelements(mesh, part->first, EntityProcessType::LOCAL_ONLY );
    auto r2 = markedelements(mesh, markerid, EntityProcessType::LOCAL_ONLY );
    auto lelt_it = r2.template get<1>();
    auto lelt_en = r2.template get<2>();

    Feel::detail::MeshPoints<float> mpl( mesh.get(), this->worldComm(), lelt_it, lelt_en, true, true, true );
    int gnole = mpl.globalNumberOfElements();
    //LOG(INFO) << "Global nb elements: " << gnole << std::endl;
    if( this->worldComm().isMasterRank() )
    { size = 1; }
    else
    { size = 0; }
    MPI_File_write_ordered(fh, &gnole, size, MPI_INT, &status );
    // now we need to move with respect to the processors for the coordinates
    //MPI_File_seek(fh, mp.offsets_elts, MPI_SEEK_CUR );

    /*
       MPI_File_get_position(fh, &offset2 );
       MPI_File_read_at( fh, offset2-mp.offsets_elts-sizeof(int), &nn, 1, MPI_INT, &status );
       LOG(INFO) << "proc " << Environment::rank()
       << " offsets_elts : " << mp.offsets_elts
       << "read npts->nn = " << nn << " was " << __ne << std::endl;
       */

    /*
       std::vector<int> elids;
       for(auto it = allelt_it ; it != allelt_en; ++it )
       {
       auto const& elt = boost::unwrap_ref( *it );
       elids.push_back(elt.id() + mp.offsets_elts + 1);
       }

       MPI_File_write_ordered(fh, elids.data(), elids.size(), MPI_INT, &status );
       */

    // TODO Warning: Element ids are not renumbered, fuzzy behaviours might appear like element
    // getting the same id on two different processes when number of elements tends to be equal
    // to the number of processes

    /* Write elements ids */
    //std::vector<int> elids;
    std::vector<int> elids;
    for(auto it = lelt_it ; it != lelt_en; ++it )
    {
        auto const& elt = boost::unwrap_ref( *it );
        //LOG(INFO) << std::distance(lelt_it, lelt_en) << " " << mp.offsets_pts << " " << mp.offsets_elts << " " << elt.id() << std::endl;
        elids.push_back(elt.id() + mp.offsets_elts + 1);
    }

    MPI_File_write_ordered(fh, elids.data(), elids.size(), MPI_INT, &status );

    /* Write point ids of vertices */
    std::vector<int> ptids;
    for(auto it = lelt_it ; it != lelt_en; ++it )
    {
        auto const& elt = boost::unwrap_ref( *it );
        for ( size_type j = 0; j < elt.numLocalVertices; j++ )
        {
            ptids.push_back(elt.point( j ).id());
        }
    }

    /* translate the point ids using the global id system (where LOCAL and GHOST cells are taken into account) */
    mp.translatePointIds(ptids);

    MPI_File_write_ordered(fh, ptids.data(), ptids.size(), MPI_INT, &status );
    //offset += mp.global_offsets_elts+sizeof(int);

    /*
       MPI_File_get_position(fh, &offset2 );
    //MPI_File_seek(fh, offset, MPI_SEEK_SET );
    MPI_File_read_at( fh, offset2-mp.global_offsets_elts-sizeof(int), &nn, 1, MPI_INT, &status );
    LOG(INFO) << "proc " << Environment::rank()
    << " global offsets_elts : " << mp.global_offsets_elts
    << "read npts->ne = " << nn << " was " << __ne << std::endl;
    */

    /* Write ghost elements */
    if ( this->worldComm().globalSize() > 1 )
    {
        // get ghost elements
        //auto r1 = markedelements(mesh, part->first, EntityProcessType::GHOST_ONLY );
        auto r1 = markedelements(mesh, markerid, EntityProcessType::GHOST_ONLY );
        auto gelt_it = r1.template get<1>();
        auto gelt_en = r1.template get<2>();

        std::string ghost_t = "g_" + this->elementType();
        // ghosts elements
        memset(buffer, '\0', sizeof(buffer));
        strcpy( buffer, ghost_t.c_str() );
        if( this->worldComm().isMasterRank() )
        { size = sizeof(buffer); }
        else
        { size = 0; }
        MPI_File_write_ordered(fh, buffer, size, MPI_CHAR, &status );

        Feel::detail::MeshPoints<float> mpg( mesh.get(), this->worldComm(), gelt_it, gelt_en, true, true, true );
        //VLOG(1) << "material : " << p_it->first << " ghost nb element: " << __ne;

        int gnoge = mpg.globalNumberOfElements();
        if( this->worldComm().isMasterRank() )
        { size = 1; }
        else
        { size = 0; }
        MPI_File_write_ordered(fh, &gnoge, size, MPI_INT, &status );

        /* Write elements ids */
        std::vector<int> elids;
        for(auto it = gelt_it ; it != gelt_en; ++it )
        {
            auto const& elt = boost::unwrap_ref( *it );
            elids.push_back(elt.id() + mp.offsets_elts + 1);
        }

        MPI_File_write_ordered(fh, elids.data(), elids.size(), MPI_INT, &status );

        /* Write point ids of vertices */
        std::vector<int> ptids;
        for( auto it = gelt_it; it != gelt_en; ++it )
        {
            auto const& elt = boost::unwrap_ref( *it );
            for ( size_type j = 0; j < elt.numLocalVertices; j++ )
            {
                ptids.push_back(elt.point( j ).id());
            }
        }

        /* translate the point ids using the global id system (where LOCAL and GHOST cells are taken into account) */
        mp.translatePointIds(ptids);

        MPI_File_write_ordered(fh, ptids.data(), ptids.size(), MPI_INT, &status );

        /*
           idelem.resize( __ne );
           size_type e = 0;
           for ( ; elt_it1 != elt_en1; ++elt_it1 )
           {
           if ( elt_it1->get().isGhostCell() )
           {
           idelem[e] = 1;
           ++e;
           }
           }
           CHECK( e == __ne ) << "Invalid number of ghosts cells: " << e << " != " << __ne;
           __out.write( ( char * ) & idelem.front(), idelem.size() * sizeof( int ) );
           */

        /*
           elt_it1 = r1.template get<1>();
           idelem.resize( __ne*__mesh->numLocalVertices() );
           for ( size_type e=0 ; elt_it1 != elt_en1; ++elt_it1, ++e )
           {
           for ( size_type j = 0; j < __mesh->numLocalVertices(); j++ )
           {
        // ensight id start at 1
        idelem[e*__mesh->numLocalVertices()+j] = mp.old2new[elt_it1->get().point( j ).id()];
        DCHECK( idelem[e*__mesh->numLocalVertices()+j] > 0 )
        << "Invalid entry : " << idelem[e*__mesh->numLocalVertices()+j]
        << " at index : " << e*__mesh->numLocalVertices()+j
        << " element :  " << e
        << " vertex :  " << j;
        }
        }
        CHECK( e==__ne) << "Invalid number of elements, e= " << e << "  should be " << __ne;
        std::for_each( idelem.begin(), idelem.end(), [=]( int e ) { CHECK( ( e > 0) && e <= __nv ) << "invalid entry e = " << e << " nv = " << __nv; } );
        __out.write( ( char * ) &idelem.front() , __ne*__mesh->numLocalVertices()*sizeof( int ) );
        */
    }

}

template<typename MeshType, int N>
void
ExporterEnsightGold<MeshType,N>::writeVariableFiles() const
{
    namespace lambda = boost::lambda;
    timeset_const_iterator __ts_it = this->beginTimeSet();
    timeset_const_iterator __ts_en = this->endTimeSet();

    while ( __ts_it != __ts_en )
    {
        timeset_ptrtype __ts = *__ts_it;

        typename timeset_type::step_const_iterator __it = __ts->beginStep();
        typename timeset_type::step_const_iterator __end = __ts->endStep();
        __it = boost::prior( __end );

        while ( __it != __end )
        {
            typename timeset_type::step_ptrtype __step = *__it;;

            if ( __step->isInMemory() )
            {
                int dist = 0;

                if( (dist = std::distance(__step->beginNodalScalar(), __step->endNodalScalar())) != 0)
                { LOG(INFO) << "NodalScalar: " << dist << std::endl; }
                if( (dist = std::distance(__step->beginNodalVector(), __step->endNodalVector())) != 0)
                { LOG(INFO) << "NodalVector: " << dist << std::endl; }
                if( (dist = std::distance(__step->beginNodalTensor2(), __step->endNodalTensor2())) != 0)
                { LOG(INFO) << "NodalTensor2: " << dist << std::endl; }
                if( (dist = std::distance(__step->beginElementScalar(), __step->endElementScalar())) != 0)
                { LOG(INFO) << "ElementScalar: " << dist << std::endl; }
                if( (dist = std::distance(__step->beginElementVector(), __step->endElementVector())) != 0)
                { LOG(INFO) << "ElementVector: " << dist << std::endl; }
                if( (dist = std::distance(__step->beginElementTensor2(), __step->endElementTensor2())) != 0)
                { LOG(INFO) << "ElementTensor2: " << dist << std::endl; }

                saveNodal( __step, __step->beginNodalScalar(), __step->endNodalScalar() );
                saveNodal( __step, __step->beginNodalVector(), __step->endNodalVector() );
                saveNodal( __step, __step->beginNodalTensor2(), __step->endNodalTensor2() );

                saveElement( __step, __step->beginElementScalar(), __step->endElementScalar() );
                saveElement( __step, __step->beginElementVector(), __step->endElementVector() );
                saveElement( __step, __step->beginElementTensor2(), __step->endElementTensor2() );

            }

            ++__it;
        }

        ++__ts_it;
    }
}


template<typename MeshType, int N>
template<typename Iterator>
void
ExporterEnsightGold<MeshType,N>::saveNodal( typename timeset_type::step_ptrtype __step, Iterator __var, Iterator en ) const
{
    int size = 0;
    char buffer[ 80 ];

    MPI_Offset offset = 0;
    MPI_File fh;
    MPI_Status status;
    MPI_Info info;

    while ( __var != en )
    {
        if ( !__var->second.worldComm().isActive() ) return;

        std::ostringstream __varfname;

        auto __mesh = __step->mesh();

        __varfname << this->path() << "/" << __var->first;
        if( ! boption( _name="exporter.merge.markers") )
        { __varfname << "-" << Environment::worldComm().globalSize() << "_" << Environment::worldComm().localRank(); }
        // add extension
        if(__var->second.is_scalar)
        { __varfname << ".scl"; }
        else if(__var->second.is_vectorial)
        { __varfname << ".vec"; }
        else if(__var->second.is_tensor2)
        { __varfname << ".tsr"; }
        else
        { __varfname << ".scl"; LOG(ERROR) << "Could not detect data type (scalar, vector, tensor2). Defaulted to scalar." << std::endl; }

        // if ( !this->useSingleTransientFile() )
        if ( ! boption( _name="exporter.merge.timesteps") )
        {
            __varfname << "." << std::setfill( '0' ) << std::setw( M_timeExponent ) << __step->index();
        }
        DVLOG(2) << "[ExporterEnsightGold::saveNodal] saving " << __varfname.str() << "...\n";
        std::fstream __out;

        /* Open File with MPI IO */
        char * str = strdup(__varfname.str().c_str());

        //if ( this->useSingleTransientFile() && __step->index() > 0 ) 
        if ( boption( _name="exporter.merge.timesteps") && __step->index() > 0 ) {
            MPI_File_open( this->worldComm().comm(), str, MPI_MODE_RDWR | MPI_MODE_CREATE | MPI_MODE_APPEND , MPI_INFO_NULL, &fh );
        }
        else {
            /* Check if file exists and delete it, if so */
            /* (MPI IO does not have a truncate mode ) */
            if(fs::exists(str))
            {
                MPI_File_delete(str, MPI_INFO_NULL);
            }

            MPI_File_open( this->worldComm().comm(), str, MPI_MODE_RDWR | MPI_MODE_CREATE, MPI_INFO_NULL, &fh );
        }

        free(str);

        Feel::detail::FileIndex index;

        if (boption( _name = "exporter.merge.timesteps"))
        {
            // first read
            index.read(fh);

            /* Move to the beginning of the fie index section */
            /* to overwrite it */
            if ( index.defined() && __step->index() > 0 ) {
                MPI_File_seek_shared(fh, index.fileblock_n_steps, MPI_SEEK_SET);
            }
            else {
                // we position the cursor at the beginning of the file
                MPI_File_seek_shared(fh, 0, MPI_SEEK_SET);
            }

            /* Write time step start */
            if( this->worldComm().isMasterRank() )
            { size = sizeof(buffer); }
            else
            { size = 0; }
            memset(buffer, '\0', sizeof(buffer));
            strcpy(buffer,"BEGIN TIME STEP");
            MPI_File_write_ordered(fh, buffer, size, MPI_CHAR, &status);

            /* add the beginning of the new block to the file */
            MPI_File_get_position_shared(fh, &offset);
            index.add( offset );
        }

        if( this->worldComm().isMasterRank() )
        { size = sizeof(buffer); }
        else
        { size = 0; }
        memset(buffer, '\0', sizeof(buffer));
        strcpy( buffer, __var->second.name().c_str() );
        MPI_File_write_ordered(fh, buffer, size, MPI_CHAR, &status);

        /* handle faces data */
        if ( option( _name="exporter.ensightgold.save-face" ).template as<bool>() )
        {
            BOOST_FOREACH( auto m, __mesh->markerNames() )
            {
                if ( m.second[1] != __mesh->nDim-1 )
                    continue;
                VLOG(1) << "writing face with marker " << m.first << " with id " << m.second[0];
                auto pairit = __mesh->facesWithMarker( m.second[0], Environment::worldComm().localRank() );
                auto fit = pairit.first;
                auto fen = pairit.second;

                Feel::detail::MeshPoints<float> mp( __mesh.get(), this->worldComm(), fit, fen, true, true, true );
                int __ne = std::distance( fit, fen );

                int nverts = fit->numLocalVertices;
                DVLOG(2) << "Faces : " << __ne << "\n";

                if( this->worldComm().isMasterRank() )
                { size = sizeof(buffer); }
                else
                { size = 0; }
                memset(buffer, '\0', sizeof(buffer));
                strcpy( buffer, "part" );
                MPI_File_write_ordered(fh, buffer, size, MPI_CHAR, &status);

                int partid = m.second[0];
                if( this->worldComm().isMasterRank() )
                { size = 1; }
                else
                { size = 0; }
                MPI_File_write_ordered(fh, &partid, size, MPI_INT, &status);

                if( this->worldComm().isMasterRank() )
                { size = sizeof(buffer); }
                else
                { size = 0; }
                memset(buffer, '\0', sizeof(buffer));
                strcpy( buffer, "coordinates" );
                MPI_File_write_ordered(fh, buffer, size, MPI_CHAR, &status);

                // write values
                fit = pairit.first;
                fen = pairit.second;

                uint16_type nComponents = __var->second.nComponents;
                if ( __var->second.is_vectorial )
                    nComponents = 3;

                int nfaces = mp.ids.size();
                std::vector<float> field( nComponents*nfaces, 0.0 );
                for( ; fit != fen; ++fit )
                {
                    for ( uint16_type c = 0; c < nComponents; ++c )
                    {
                        for ( size_type j = 0; j < nverts; j++ )
                        {
                            size_type pid = mp.old2new[fit->point( j ).id()]-1;
                            size_type global_node_id = nfaces*c + pid ;
                            if ( c < __var->second.nComponents )
                            {
                                size_type thedof =  __var->second.start() +
                                    boost::get<0>(__var->second.functionSpace()->dof()->faceLocalToGlobal( fit->id(), j, c ));

                                field[global_node_id] = __var->second.globalValue( thedof );
                            }
                            else
                            {
                                field[global_node_id] = 0;
                            }
                        }
                    }
                }
                /* Write each component separately */
                for ( uint16_type c = 0; c < __var->second.nComponents; ++c )
                {
                    MPI_File_write_ordered(fh, field.data() + nfaces * c, nfaces, MPI_FLOAT, &status);
                }
                //__out.write( ( char * ) field.data(), field.size() * sizeof( float ) );
            } // boundaries loop
        }

        /* handle elements */
        /*
        typename mesh_type::parts_const_iterator_type p_it = __step->mesh()->beginParts();
        typename mesh_type::parts_const_iterator_type p_en = __step->mesh()->endParts();
        */

        //for ( ; p_it != p_en; ++p_it )
        //for( int i = 0; i < M_markersToWrite.size(); i++)
        for( std::set<int>::iterator mit = M_markersToWrite.begin(); mit != M_markersToWrite.end(); mit++)
        {
            if( this->worldComm().isMasterRank() )
            { size = sizeof(buffer); }
            else
            { size = 0; }
            memset(buffer, '\0', sizeof(buffer));
            strcpy( buffer, "part" );
            MPI_File_write_ordered(fh, buffer, size, MPI_CHAR, &status);

            if( this->worldComm().isMasterRank() )
            { size = 1; }
            else
            { size = 0; }

            int partid = *mit;
            MPI_File_write_ordered(fh, &partid, size, MPI_INT, &status);

            if( this->worldComm().isMasterRank() )
            { size = sizeof(buffer); }
            else
            { size = 0; }
            memset(buffer, '\0', sizeof(buffer));
            strcpy( buffer, "coordinates" );
            MPI_File_write_ordered(fh, buffer, size, MPI_CHAR, &status);


            uint16_type nComponents = __var->second.nComponents;

            VLOG(1) << "nComponents field: " << nComponents;
            if ( __var->second.is_vectorial )
            {
                nComponents = 3;
                VLOG(1) << "nComponents field(is_vectorial): " << nComponents;
            }

            /* we get that from the local processor */
            /* We do not need the renumbered global index */
            //auto r = markedelements(__mesh,(boost::any)p_it->first,EntityProcessType::ALL);
            //auto r = markedelements(__mesh, M_markersToWrite[i], EntityProcessType::ALL);
            auto r = markedelements(__mesh, *mit, EntityProcessType::ALL);
            auto elt_it = r.template get<1>();
            auto elt_en = r.template get<2>();

            Feel::detail::MeshPoints<float> mp( __step->mesh().get(), this->worldComm(), elt_it, elt_en, true, true, true );

            // previous implementation
            //size_type __field_size = mp.ids.size();
            //int nelts = std::distance(elt_it, elt_en);
            int npts = mp.ids.size();
            size_type __field_size = npts;
            if ( __var->second.is_vectorial )
                __field_size *= 3;
            ublas::vector<float> __field( __field_size, 0.0 );
            size_type e = 0;
            VLOG(1) << "field size=" << __field_size;
            if ( !__var->second.areGlobalValuesUpdated() )
                __var->second.updateGlobalValues();

            /*
            std::cout << Environment::worldComm().rank() << " marker=" << *mit << " nbPts:" << npts << " nComp:" << nComponents 
                      << " __evar->second.nComponents:" << __var->second.nComponents << std::endl;
            */

            /* loop on the elements */
            int index = 0;
            for ( ; elt_it != elt_en; ++elt_it )
            {
                VLOG(3) << "is ghost cell " << elt_it->get().isGhostCell();
                /* looop on the ccomponents is outside of the loop on the vertices */
                /* becaus we need to pack the data in the x1 x2 ... xn y1 y2 ... yn z1 z2 ... zn order */
                for ( uint16_type c = 0; c < nComponents; ++c )
                {
                    for ( uint16_type p = 0; p < __step->mesh()->numLocalVertices(); ++p, ++e )
                    {
                        size_type ptid = mp.old2new[elt_it->get().point( p ).id()]-1;
                        size_type global_node_id = mp.ids.size()*c + ptid ;
                        //LOG(INFO) << elt_it->get().point( p ).id() << " " << ptid << " " << global_node_id << std::endl;
                        DCHECK( ptid < __step->mesh()->numPoints() ) << "Invalid point id " << ptid << " element: " << elt_it->get().id()
                                                                     << " local pt:" << p
                                                                     << " mesh numPoints: " << __step->mesh()->numPoints();
                        DCHECK( global_node_id < __field_size ) << "Invalid dof id : " << global_node_id << " max size : " << __field_size;

                        if ( c < __var->second.nComponents )
                        {
                            size_type dof_id = boost::get<0>( __var->second.functionSpace()->dof()->localToGlobal( elt_it->get().id(), p, c ) );

                            __field[global_node_id] = __var->second.globalValue( dof_id );
                            //__field[npts*c + index] = __var->second.globalValue( dof_id );
                            //DVLOG(3) << "v[" << global_node_id << "]=" << __var->second.globalValue( dof_id ) << "  dof_id:" << dof_id;
                            DVLOG(3) << "v[" << (npts*c + index) << "]=" << __var->second.globalValue( dof_id ) << "  dof_id:" << dof_id;
                        }
                        else
                        {
                            __field[global_node_id] = 0.0;
                            //__field[npts*c + index] = 0.0;
                        }
                    }
                }

                /* increment index of vertex */
                index++;
            }

            /* Write each component separately */
            for ( uint16_type c = 0; c < nComponents; ++c )
            {
                MPI_File_write_ordered(fh, ((float *)(__field.data().begin())) + npts * c, npts, MPI_FLOAT, &status);
            }
            //__out.write( ( char * ) __field.data().begin(), __field.size() * sizeof( float ) );
        } // parts loop

        if ( boption(_name="exporter.merge.timesteps") )
        {
            /* write timestep end */
            if( this->worldComm().isMasterRank() )
            { size = sizeof(buffer); }
            else
            { size = 0; }
            memset(buffer, '\0', sizeof(buffer));
            strcpy(buffer,"END TIME STEP");
            MPI_File_write_ordered(fh, buffer, size, MPI_CHAR, &status);

            // write back the file index
            index.write( fh );
        }
        DVLOG(2) << "[ExporterEnsightGold::saveNodal] saving " << __varfname.str() << "done\n";

        ++__var;

        MPI_File_close(&fh);
    }
}

template<typename MeshType, int N>
template<typename Iterator>
void
ExporterEnsightGold<MeshType,N>::saveElement( typename timeset_type::step_ptrtype __step, Iterator __evar, Iterator __evaren ) const
{
    int size;
    char buffer[ 80 ];

    MPI_File fh;
    MPI_Status status;

    while ( __evar != __evaren )
    {
        if ( !__evar->second.worldComm().isActive() ) return;

        std::ostringstream __evarfname;

        __evarfname << this->path() << "/" << __evar->first;

        if( ! boption( _name="exporter.merge.markers") )
        { __evarfname << "-" << Environment::worldComm().globalSize() << "_" << Environment::worldComm().localRank(); }

        // add extension
        if(__evar->second.is_scalar)
        { __evarfname << ".scl"; }
        else if(__evar->second.is_vectorial)
        { __evarfname << ".vec"; }
        else if(__evar->second.is_tensor2)
        { __evarfname << ".tsr"; }
        else
        { __evarfname << ".scl"; LOG(ERROR) << "Could not detect data type (scalar, vector, tensor2). Defaulted to scalar." << std::endl; }

        //if ( !this->useSingleTransientFile() )
        if ( ! boption( _name="exporter.merge.timesteps") )
        {
            __evarfname << "." << std::setfill( '0' ) << std::setw( M_timeExponent ) << __step->index();
        }
        DVLOG(2) << "[ExporterEnsightGold::saveElement] saving " << __evarfname.str() << "...\n";
        //std::fstream __out( __evarfname.str().c_str(), std::ios::out | std::ios::binary );

        auto __mesh = __step->mesh();

        /* Open File with MPI IO */
        char * str = strdup(__evarfname.str().c_str());

        //if ( this->useSingleTransientFile() && __step->index() > 0 ) 
        if ( boption( _name="exporter.merge.timesteps") && __step->index() > 0 ) {
            MPI_File_open( this->worldComm().comm(), str, MPI_MODE_RDWR | MPI_MODE_CREATE | MPI_MODE_APPEND , MPI_INFO_NULL, &fh );
        }
        else {
            /* Check if file exists and delete it, if so */
            /* (MPI IO does not have a truncate mode ) */
            if(fs::exists(str))
            {
                MPI_File_delete(str, MPI_INFO_NULL);
            }

            MPI_File_open( this->worldComm().comm(), str, MPI_MODE_RDWR | MPI_MODE_CREATE, MPI_INFO_NULL, &fh );
        }

        free(str);

        Feel::detail::FileIndex index;
        /*
        if ( this->useSingleTransientFile() )
        {
            // first read
            //index.read( __out );

            if ( index.defined() && __step->index() > 0 )
                __out.seekp( index.fileblock_n_steps, std::ios::beg );
            else
            {
                // we position the cursor at the beginning of the file
                __out.seekp( 0, std::ios::beg );
            }

            memset(buffer, '\0', sizeof(buffer));
            strcpy(buffer,"BEGIN TIME STEP");
            __out.write(buffer,sizeof(buffer));
            //index.add( __out.tellp() );
        }
        */

        if( this->worldComm().isMasterRank() )
        { size = sizeof(buffer); }
        else
        { size = 0; }
        memset(buffer, '\0', sizeof(buffer));
        strcpy( buffer, __evar->second.name().c_str() );
        MPI_File_write_ordered(fh, buffer, size, MPI_CHAR, &status);

        // iterate over the markers
        for( std::set<int>::iterator mit = M_markersToWrite.begin() ; mit != M_markersToWrite.end(); mit++ )
        {
            if( this->worldComm().isMasterRank() )
            { size = sizeof(buffer); }
            else
            { size = 0; }
            memset(buffer, '\0', sizeof(buffer));
            strcpy( buffer, "part" );
            MPI_File_write_ordered(fh, buffer, size, MPI_CHAR, &status);

            int partid = *mit;
            if( this->worldComm().isMasterRank() )
            { size = 1; }
            else
            { size = 0; }
            MPI_File_write_ordered(fh, &partid, size, MPI_INT, &status);
            DVLOG(2) << "part " << buffer << "\n";

            if( this->worldComm().isMasterRank() )
            { size = sizeof(buffer); }
            else
            { size = 0; }
            memset(buffer, '\0', sizeof(buffer));
            strcpy( buffer, this->elementType().c_str() );
            MPI_File_write_ordered(fh, buffer, size, MPI_CHAR, &status);
            DVLOG(2) << "element type " << buffer << "\n";

            uint16_type nComponents = __evar->second.nComponents;

            if ( __evar->second.is_vectorial )
                nComponents = 3;

            size_type __field_size = nComponents * __evar->second.size()/__evar->second.nComponents;
            ublas::vector<float> __field( __field_size );
            __field.clear();

            auto r = markedelements(__step->mesh(), *mit, EntityProcessType::LOCAL_ONLY );
            auto elt_st = r.template get<1>();
            auto elt_en = r.template get<2>();

            if ( !__evar->second.areGlobalValuesUpdated() )
                __evar->second.updateGlobalValues();

            DVLOG(2) << "[saveElement] firstLocalIndex = " << __evar->second.firstLocalIndex() << "\n";
            DVLOG(2) << "[saveElement] lastLocalIndex = " << __evar->second.lastLocalIndex() << "\n";
            DVLOG(2) << "[saveElement] field.size = " << __field_size << "\n";

            //size_type ncells = __evar->second.size()/__evar->second.nComponents;
            size_type ncells = std::distance( elt_st, elt_en );

            /*
            std::cout << Environment::worldComm().rank() << " marker=" << *mit << " nbElts:" << ncells << " nComp:" << nComponents 
                      << " __evar->second.nComponents:" << __evar->second.nComponents << std::endl;
            */

            for ( int c = 0; c < nComponents; ++c )
            {
                size_type e = 0;
                for ( auto elt_it = elt_st ; elt_it != elt_en; ++elt_it, ++e )
                {
                    auto const& elt = boost::unwrap_ref( *elt_it );
                    DVLOG(2) << "pid : " << this->worldComm().globalRank()
                             << " elt_it :  " << elt.id()
                             << " e : " << e << "\n";

                    size_type global_node_id = c*ncells+e ;

                    if ( c < __evar->second.nComponents )
                    {
                        size_type dof_id = boost::get<0>( __evar->second.functionSpace()->dof()->localToGlobal( elt.id(),0, c ) );

                        DVLOG(2) << "c : " << c
                                 << " gdofid: " << global_node_id
                                 << " dofid : " << dof_id
                                 << " f.size : " <<  __field.size()
                                 << " e.size : " <<  __evar->second.size()
                                 << "\n";

                        __field[global_node_id] = __evar->second.globalValue( dof_id );

#if 1
                        //__field[global_node_id] = __evar->second.globalValue(dof_id);
                        DVLOG(2) << "c : " << c
                                 << " gdofid: " << global_node_id
                                 << " dofid : " << dof_id
                                 << " field :  " << __field[global_node_id]
                                 << " evar: " << __evar->second.globalValue( dof_id ) << "\n";
#endif
                    }

                    else
                        __field[global_node_id] = 0;
                }
            }

            /* Write each component separately */
            for ( uint16_type c = 0; c < nComponents; ++c )
            {
                MPI_File_write_ordered(fh, __field.data().begin() + ncells * c, ncells, MPI_FLOAT, &status);
            }
            //__out.write( ( char * ) __field.data().begin(), nComponents * e * sizeof( float ) );
        }
        /*
        if ( this->useSingleTransientFile() )
        {
            memset(buffer, '\0', sizeof(buffer));
            strcpy(buffer,"END TIME STEP");
            __out.write(buffer,sizeof(buffer));
            // write back the file index
            //index.write( __out );
        }
        */

        MPI_File_close(&fh);

        DVLOG(2) << "[ExporterEnsightGold::saveElement] saving " << __evarfname.str() << "done\n";
        ++__evar;
    }
}

template<typename MeshType, int N>
void
ExporterEnsightGold<MeshType,N>::visit( mesh_type* __mesh )
{
}

#if 0
#if defined( FEELPP_INSTANTIATION_MODE )
//
// explicit instances
//
template class ExporterEnsightGold<Mesh<Simplex<1,1,1> > >;
template class ExporterEnsightGold<Mesh<Simplex<1,1,2> > >;
template class ExporterEnsightGold<Mesh<Simplex<2,1,2> > >;
template class ExporterEnsightGold<Mesh<Simplex<2,2,2> > >;
template class ExporterEnsightGold<Mesh<Simplex<2,1,3> > >;
template class ExporterEnsightGold<Mesh<Simplex<3,1,3> > >;

template class ExporterEnsightGold<Mesh<Simplex<3,2,3> > >;

template class ExporterEnsightGold<Mesh<Hypercube<1,1,1> > >;
template class ExporterEnsightGold<Mesh<Hypercube<2,1,2> > >;
template class ExporterEnsightGold<Mesh<Hypercube<3,1,3> > >;
template class ExporterEnsightGold<Mesh<Hypercube<3,2,3> > >;

template class ExporterEnsightGold<Mesh<Simplex<2,3,2> > >;
template class ExporterEnsightGold<Mesh<Hypercube<2,2> > >;
template class ExporterEnsightGold<Mesh<Hypercube<2,3> > >;

#endif // FEELPP_INSTANTIATION_MODE
#endif
}
#endif // FEELPP_EXPORTERENSIGHT_CPP
