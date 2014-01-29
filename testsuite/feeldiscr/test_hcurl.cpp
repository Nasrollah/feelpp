/* -*- mode: c++; coding: utf-8; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; show-trailing-whitespace: t -*- vim:fenc=utf-8:ft=tcl:et:sw=4:ts=4:sts=4

  This file is part of the Feel library

  Author(s): Christophe Prud'homme <christophe.prudhomme@feelpp.org>
  Author(s): Cecile Daversin  <cecile.daversin@lncmi.cnrs.fr>
       Date: 2011-12-07

  Copyright (C) 2011 UJF
  Copyright (C) 2011 CNRS

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
   \file test_hcurl.cpp
   \author Christophe Prud'homme <christophe.prudhomme@feelpp.org>
   \author Cecile Daversin <cecile.daversin@lncmi.cnrs.fr>
   \date 2011-12-07
 */
#define USE_BOOST_TEST 1

// make sure that the init_unit_test function is defined by UTF
//#define BOOST_TEST_MAIN
// give a name to the testsuite
#define BOOST_TEST_MODULE H_curl approximation
// disable the main function creation, use our own
//#define BOOST_TEST_NO_MAIN

#include <testsuite/testsuite.hpp>

#include <feel/feelalg/backend.hpp>
#include <feel/feelfilters/creategmshmesh.hpp>
#include <feel/feelfilters/domain.hpp>
#include <feel/feelfilters/exporter.hpp>
#include <feel/feelvf/form.hpp>
#include <feel/feelvf/operators.hpp>
#include <feel/feelvf/operations.hpp>
#include <feel/feelvf/on.hpp>
#include <feel/feelpoly/nedelec.hpp>
#include <feel/feelvf/vf.hpp>
#include <feel/feeldiscr/projector.hpp>


namespace Feel
{
/// Geometry for one-element meshes
gmsh_ptrtype
oneelement_geometry_ref( double h = 2 )
{
    std::ostringstream costr;
    costr <<"Mesh.MshFileVersion = 2.2;\n"
          <<"Mesh.CharacteristicLengthExtendFromBoundary=1;\n"
          <<"Mesh.CharacteristicLengthFromPoints=1;\n"
          <<"Mesh.ElementOrder=1;\n"
          <<"Mesh.SecondOrderIncomplete = 0;\n"
          <<"Mesh.Algorithm = 6;\n"
          <<"Mesh.OptimizeNetgen=1;\n"
          <<"// partitioning data\n"
          <<"Mesh.Partitioner=1;\n"
          <<"Mesh.NbPartitions=1;\n"
          <<"Mesh.MshFilePartitioned=0;\n"
          <<"h=" << h << ";\n"
          <<"Point(1) = {-1,-1,0,h};\n"
          <<"Point(2) = {1,-1,0,h};\n"
          <<"Point(3) = {-1,1,0,h};\n"
          <<"Line(1) = {1,2};\n"
          <<"Line(2) = {2,3};\n"
          <<"Line(3) = {3,1};\n";

    if ( std::abs( h - 2 ) < 1e-10 )
        costr <<"Transfinite Line{1} = 1;\n"
              <<"Transfinite Line{2} = 1;\n"
              <<"Transfinite Line{3} = 1;\n";

    costr <<"Line Loop(4) = {3,1,2};\n"
          <<"Plane Surface(5) = {4};\n"
          <<"Physical Line(\"hor\") = {1};\n"
          <<"Physical Line(\"hypo\") = {2};\n"
          <<"Physical Line(\"vert\") = {3};\n"
          <<"Physical Surface(9) = {5};\n";

    std::ostringstream nameStr;

    if ( std::abs( h - 2 ) < 1e-10 )
        nameStr << "one-elt-ref";

    else
        nameStr << "one-elt-mesh-ref";

    gmsh_ptrtype gmshp( new Gmsh );
    gmshp->setPrefix( nameStr.str() );
    gmshp->setDescription( costr.str() );
    return gmshp;
}

// homothetic transformation of reference element (center 0, rate 2)
gmsh_ptrtype
oneelement_geometry_real_1( double h = 2 )
{
    std::ostringstream costr;
    costr <<"Mesh.MshFileVersion = 2.2;\n"
          <<"Mesh.CharacteristicLengthExtendFromBoundary=1;\n"
          <<"Mesh.CharacteristicLengthFromPoints=1;\n"
          <<"Mesh.ElementOrder=1;\n"
          <<"Mesh.SecondOrderIncomplete = 0;\n"
          <<"Mesh.Algorithm = 6;\n"
          <<"Mesh.OptimizeNetgen=1;\n"
          <<"// partitioning data\n"
          <<"Mesh.Partitioner=1;\n"
          <<"Mesh.NbPartitions=1;\n"
          <<"Mesh.MshFilePartitioned=0;\n"
          <<"h=" << h << ";\n"
          <<"Point(1) = {-2,-2,0,h};\n"
          <<"Point(2) = {2,-2,0,h};\n"
          <<"Point(3) = {-2,2,0,h};\n"
          <<"Line(1) = {1,2};\n"
          <<"Line(2) = {2,3};\n"
          <<"Line(3) = {3,1};\n";

    if ( std::abs( h - 2 ) < 1e-10 )
        costr <<"Transfinite Line{1} = 1;\n"
              <<"Transfinite Line{2} = 1;\n"
              <<"Transfinite Line{3} = 1;\n";

    costr <<"Line Loop(4) = {3,1,2};\n"
          <<"Plane Surface(5) = {4};\n"
          <<"Physical Line(\"hor\") = {1};\n"
          <<"Physical Line(\"hypo\") = {2};\n"
          <<"Physical Line(\"vert\") = {3};\n"
          <<"Physical Surface(9) = {5};\n";

    std::ostringstream nameStr;

    if ( std::abs( h - 2 ) < 1e-10 )
        nameStr << "one-elt-real-homo";

    else
        nameStr << "one-elt-mesh-homo";

    gmsh_ptrtype gmshp( new Gmsh );
    gmshp->setPrefix( nameStr.str() );
    gmshp->setDescription( costr.str() );
    return gmshp;
}

// Rotation of angle (pi/2)
gmsh_ptrtype
oneelement_geometry_real_2( double h = 2 )
{
    std::ostringstream costr;
    costr <<"Mesh.MshFileVersion = 2.2;\n"
          <<"Mesh.CharacteristicLengthExtendFromBoundary=1;\n"
          <<"Mesh.CharacteristicLengthFromPoints=1;\n"
          <<"Mesh.ElementOrder=1;\n"
          <<"Mesh.SecondOrderIncomplete = 0;\n"
          <<"Mesh.Algorithm = 6;\n"
          <<"Mesh.OptimizeNetgen=1;\n"
          <<"// partitioning data\n"
          <<"Mesh.Partitioner=1;\n"
          <<"Mesh.NbPartitions=1;\n"
          <<"Mesh.MshFilePartitioned=0;\n"
          <<"h=" << h << ";\n"
          <<"Point(1) = {1,-1,0,h};\n"
          <<"Point(2) = {1,1,0,h};\n"
          <<"Point(3) = {-1,-1,0,h};\n"
          <<"Line(1) = {1,2};\n"
          <<"Line(2) = {2,3};\n"
          <<"Line(3) = {3,1};\n";

    if ( std::abs( h - 2 ) < 1e-10 )
        costr <<"Transfinite Line{1} = 1;\n"
              <<"Transfinite Line{2} = 1;\n"
              <<"Transfinite Line{3} = 1;\n";

    costr <<"Line Loop(4) = {3,1,2};\n"
          <<"Plane Surface(5) = {4};\n"
          <<"Physical Line(\"vert\") = {3};\n"
          <<"Physical Line(\"hypo\") = {2};\n"
          <<"Physical Line(\"hor\") = {1};\n"
          <<"Physical Surface(9) = {5};\n";

    std::ostringstream nameStr;

    if ( std::abs( h - 2 ) < 1e-10 )
        nameStr << "one-elt-real-rot";

    else
        nameStr << "one-elt-mesh-rot";

    gmsh_ptrtype gmshp( new Gmsh );
    gmshp->setPrefix( nameStr.str() );
    gmshp->setDescription( costr.str() );
    return gmshp;
}
gmsh_ptrtype
twoelement_geometry_( double h = 2 )
{
    std::ostringstream costr;
    costr <<"Mesh.MshFileVersion = 2.2;\n"
          <<"Mesh.CharacteristicLengthExtendFromBoundary=1;\n"
          <<"Mesh.CharacteristicLengthFromPoints=1;\n"
          <<"Mesh.ElementOrder=1;\n"
          <<"Mesh.SecondOrderIncomplete = 0;\n"
          <<"Mesh.Algorithm = 6;\n"
          <<"Mesh.OptimizeNetgen=1;\n"
          <<"// partitioning data\n"
          <<"Mesh.Partitioner=1;\n"
          <<"Mesh.NbPartitions=1;\n"
          <<"Mesh.MshFilePartitioned=0;\n"
          << "    h=" << h << ";\n"
          << "Point(1) = {0, 0, 0, h};\n"
          << "Point(2) = {1, 0, 0, h};\n"
          << "Point(3) = {1, 1, 0, h};\n"
          << "Point(4) = {2, 0.5, 0, 1.0};\n"
          << "Line(1) = {1, 2};\n"
          << "Line(2) = {2, 3};\n"
          << "Line(3) = {3, 1};\n"
          << "Line(4) = {2, 4};\n"
          << "Line(5) = {4, 3};\n"
          << "Line Loop(6) = {3, 1, 2};\n"
          << "Line Loop(8) = {5, -2, 4};\n";

    if ( std::abs( h - 2 ) < 1e-10 )
        costr << "Transfinite Line(1) = 1;\n"
              << "Transfinite Line(2) = 1;\n"
              << "Transfinite Line(3) = 1;\n"
              << "Transfinite Line(4) = 1;\n"
              << "Transfinite Line(5) = 1;\n";

    costr << "Plane Surface(11) = {6};\n"
          << "Plane Surface(12) = {8};\n";
    std::ostringstream nameStr;

    if ( std::abs( h - 2 ) < 1e-10 )
        nameStr << "two-elt-mesh";

    else
        nameStr << "two-elt-mesh-fine";

    gmsh_ptrtype gmshp( new Gmsh );
    gmshp->setPrefix( nameStr.str() );
    gmshp->setDescription( costr.str() );
    return gmshp;
}

/**
 * This routine returns the list of options using the
 * boost::program_options library. The data returned is typically used
 * as an argument of a Feel::Application subclass.
 *
 * \return the list of options
 */
inline
po::options_description
makeOptions()
{
    po::options_description testhcurloptions( "test h_curl options" );
    testhcurloptions.add_options()
    ( "hsize", po::value<double>()->default_value( 0.1 ), "mesh size" )
    ( "xmin", po::value<double>()->default_value( -1 ), "xmin of the reference element" )
    ( "ymin", po::value<double>()->default_value( -1 ), "ymin of the reference element" )
    ( "zmin", po::value<double>()->default_value( -1 ), "zmin of the reference element" )
    ;
    return testhcurloptions.add( Feel::feel_options() );
}

inline
AboutData
makeAbout()
{
    AboutData about( "test_hcurl" ,
                     "test_hcurl" ,
                     "0.1",
                     "Test for h_curl space (Dim=2 Order=1)",
                     AboutData::License_GPL,
                     "Copyright (c) 2009 Universite Joseph Fourier" );
    about.addAuthor( "Cecile Daversin", "developer", "cecile.daversin@lncmi.cnrs.fr", "" );
    about.addAuthor( "Christophe Prud'homme", "developer", "christophe.prudhomme@feelpp.org", "" );
    return about;

}

using namespace Feel;

class TestHCurl
    :
public Application
{
    typedef Application super;

public:

    //! numerical type is double
    typedef double value_type;

    //! linear algebra backend factory
    typedef Backend<value_type> backend_type;
    //! linear algebra backend factory shared_ptr<> type
    typedef typename boost::shared_ptr<backend_type> backend_ptrtype ;

    //! geometry entities type composing the mesh, here Simplex in Dimension Dim of Order G_order
    typedef Simplex<2,1> convex_type;
    //! mesh type
    typedef Mesh<convex_type> mesh_type;
    //! mesh shared_ptr<> type
    typedef boost::shared_ptr<mesh_type> mesh_ptrtype;

    //! the basis type of our approximation space
    typedef bases<Nedelec<0> > basis_type;
    typedef bases<Lagrange<1,Vectorial> > lagrange_basis_type; //P1 vectorial space
    //! the approximation function space type
    typedef FunctionSpace<mesh_type, basis_type> space_type;
    typedef FunctionSpace<mesh_type, lagrange_basis_type> lagrange_space_type;
    //! the approximation function space type (shared_ptr<> type)
    typedef boost::shared_ptr<space_type> space_ptrtype;
    typedef boost::shared_ptr<lagrange_space_type> lagrange_space_ptrtype;
    //! an element type of the approximation function space
    typedef typename space_type::element_type element_type;

    //! the exporter factory type
    typedef Exporter<mesh_type> export_type;
    //! the exporter factory (shared_ptr<> type)
    typedef boost::shared_ptr<export_type> export_ptrtype;

    /**
     * Constructor
     */
    TestHCurl()
        :
        super(),
        M_backend( backend_type::build( this->vm() ) ),
        meshSize( this->vm()["hsize"].as<double>() ),
        exporter( Exporter<mesh_type>::New( this->vm() ) )
    {
        std::cout << "[TestHCurl]\n";

        this->changeRepository( boost::format( "%1%/h_%2%/" )
                                % this->about().appName()
                                % this->vm()["hsize"].as<double>()
                              );

        oneelement_mesh_ref = createGMSHMesh( _mesh=new mesh_type,
                                              _desc = oneelement_geometry_ref() );

        // Homothetic transformation of reference element mesh (rate=2, center=(0;0) )
        oneelement_mesh_real_1 = createGMSHMesh( _mesh=new mesh_type,
                                 _desc = oneelement_geometry_real_1() );

        // Rotation of reference element mesh (angle=pi/2)
        oneelement_mesh_real_2 = createGMSHMesh( _mesh=new mesh_type,
                                 _desc = oneelement_geometry_real_2() );

    }

    /**
     * run the application
     */
    void shape_functions( gmsh_ptrtype ( *one_element_mesh )( double ) );
    void projection( gmsh_ptrtype ( *one_element_mesh_desc_fun )( double ) );

    void testProjector();
    void exampleProblem1();

private:
    //! linear algebra backend
    backend_ptrtype M_backend;

    //! mesh characteristic size
    double meshSize;

    //! exporter factory
    export_ptrtype exporter;

    //! meshes
    mesh_ptrtype oneelement_mesh_ref;
    mesh_ptrtype oneelement_mesh_real_1;
    mesh_ptrtype oneelement_mesh_real_2;

}; //TestHCurl

// Resolve problem curl(curl(u)) + u = f with cross_prod(u,n) = 0 on boundary
void
TestHCurl::exampleProblem1()
{
    using namespace Feel::vf;
    // then a fine mesh which we use to export the basis function to
    // visualize them
    mesh_ptrtype mesh = createGMSHMesh( _mesh=new mesh_type,
                                        _desc=domain( _name= ( boost::format( "%1%-%2%-%3%" ) % "hypercube" % 2 % 1 ).str() ,
                                                _shape="hypercube",
                                                _usenames=true,
                                                _dim=2,
                                                _h=meshSize,
                                                _xmin=-1,_xmax=1,
                                                _ymin=-1,_ymax=1 ) );

    // Xh : space build with Nedelec elements
    space_ptrtype Xh = space_type::New( mesh );
    element_type u( Xh, "u" ); //solution
    element_type phi( Xh, "v" ); //test function

    auto u_exact = ( 1-Py()*Py() )*unitX() + ( 1-Px()*Px() )*unitY(); //exact solution (analytical)
    auto f = ( 3-Py()*Py() )*unitX() + ( 3-Px()*Px() )*unitY(); //f = curl(curl(u_exact)) + u_exact

    //variationnal formulation : curl(curl(u)) + u = f
    auto F = M_backend->newVector( Xh );
    form1( _test=Xh, _vector=F, _init=true ) = integrate( _range=elements(mesh), _expr=trans(f)*id(phi) );

    auto M = M_backend->newMatrix( Xh, Xh );
    form2( _test=Xh, _trial=Xh, _matrix=M, _init=true) = integrate(elements(mesh), trans(curlt(u))*curl(phi) + trans(idt(u))*id(phi) );
    //form2( _test=Xh, _trial=Xh, _matrix=M, _init=true) = integrate(_range=elements(mesh), _expr=cst(0.) );

    //! solve the system for V
    M_backend->solve( _matrix=M, _solution=u, _rhs=F );

    // auto hcurl = opProjection( _domainSpace=Xh, _imageSpace=Xh, _type=HCURL );
    // auto u_hcurl = hcurl->project( _expr=trans( f ) );
    //auto u_exact_hcurl = hcurl->project( _expr=trans( u_exact ) );
    //auto error_hcurl = hcurl->project( _expr=trans( u_exact-idv( u_hcurl ) ) );

    auto hcurl = opProjection( _domainSpace=Xh, _imageSpace=Xh, _type=HCURL );
    auto error_hcurl = hcurl->project( _expr=trans( u_exact - idv(u) ) );
    std::cout << "error Hcurl: " << math::sqrt( hcurl->energy( error_hcurl, error_hcurl ) ) << "\n";

    std::string pro1_name = "problem1";
    export_ptrtype exporter_pro1( export_type::New( this->vm(),
                                  ( boost::format( "%1%-%2%-%3%" )
                                    % this->about().appName()
                                    % ( boost::format( "%1%-%2%-%3%" ) % "hypercube" % 2 % 1 ).str()
                                    % pro1_name ).str() ) );

    exporter_pro1->step( 0 )->setMesh( mesh );
    exporter_pro1->step( 0 )->add( "solution u", u );
    exporter_pro1->step( 0 )->add( "error", error_hcurl );
    exporter_pro1->save();

}

void
TestHCurl::testProjector()
{
    using namespace Feel::vf;
    // then a fine mesh which we use to export the basis function to
    // visualize them
    mesh_ptrtype mesh = createGMSHMesh( _mesh=new mesh_type,
                                        _desc=domain( _name= ( boost::format( "%1%-%2%-%3%" ) % "hypercube" % 2 % 1 ).str() ,
                                                _shape="hypercube",
                                                _usenames=true,
                                                _dim=2,
                                                _h=meshSize,
                                                _xmin=-1,_xmax=1,
                                                _ymin=-1,_ymax=1 ) );

    space_ptrtype Xh = space_type::New( mesh );
    lagrange_space_ptrtype Yh = lagrange_space_type::New( mesh );

    auto E_exact = ( cos( M_PI*Px() )*sin( M_PI*Py() )*unitX()-sin( M_PI*Px() )*cos( M_PI*Py() )*unitY() );
    //auto curl_E_exact = -2.*M_PI*cos( M_PI*Px() )*cos( M_PI*Py() ); // -> z
    auto f = ( 2*M_PI*M_PI+1 )*( cos( M_PI*Px() )*sin( M_PI*Py() )*unitX()-sin( M_PI*Px() )*cos( M_PI*Py() )*unitY() );

    auto l2 = opProjection( _domainSpace=Yh, _imageSpace=Yh, _type=L2 );
    auto u_l2 = l2->project( _expr=trans( f ) );
    auto E_l2 = l2->project( _expr=trans( E_exact ) );
    auto error_l2 = l2->project( _expr=trans( E_exact-idv( u_l2 ) ) );

    auto h1 = opProjection( _domainSpace=Yh, _imageSpace=Yh, _type=H1 );
    auto u_h1 = h1->project( _expr=trans( f ) );
    auto E_h1 = h1->project( _expr=trans( E_exact ) );
    auto error_h1 = h1->project( _expr=trans( E_exact-idv( u_h1 ) ) );

    auto hcurl = opProjection( _domainSpace=Xh, _imageSpace=Xh, _type=HCURL );
    auto u_hcurl = hcurl->project( _expr=trans( f ) );
    auto E_hcurl = hcurl->project( _expr=trans( E_exact ) );
    auto error_hcurl = hcurl->project( _expr=trans( E_exact-idv( u_hcurl ) ) );

    std::cout << "error L2: " << math::sqrt( l2->energy( error_l2, error_l2 ) ) << "\n";
    std::cout << "error H1: " << math::sqrt( h1->energy( error_h1, error_h1 ) ) << "\n";
    std::cout << "error Hcurl: " << math::sqrt( hcurl->energy( error_hcurl, error_hcurl ) ) << "\n";

    std::string proj_name = "projection";
    export_ptrtype exporter_proj( export_type::New( this->vm(),
                                  ( boost::format( "%1%-%2%-%3%" )
                                    % this->about().appName()
                                    % ( boost::format( "%1%-%2%-%3%" ) % "hypercube" % 2 % 1 ).str()
                                    % proj_name ).str() ) );

    exporter_proj->step( 0 )->setMesh( mesh );
    exporter_proj->step( 0 )->add( "proj_L2_u", u_l2 );
    exporter_proj->step( 0 )->add( "proj_L2_E", E_l2 );
    exporter_proj->step( 0 )->add( "proj_L2_error", error_l2 );
    exporter_proj->step( 0 )->add( "proj_H1_u", u_h1 );
    exporter_proj->step( 0 )->add( "proj_H1_E", E_h1 );
    exporter_proj->step( 0 )->add( "proj_H1_error", error_h1 );
    exporter_proj->step( 0 )->add( "proj_Hcurl_u", u_hcurl );
    exporter_proj->step( 0 )->add( "proj_Hcurl_E", E_hcurl );
    exporter_proj->step( 0 )->add( "proj_Hcurl_error", error_hcurl );
    exporter_proj->save();
}
void
TestHCurl::shape_functions( gmsh_ptrtype ( *one_element_mesh_desc_fun )( double ) )
{
    using namespace Feel::vf;

    mesh_ptrtype oneelement_mesh = createGMSHMesh( _mesh=new mesh_type,
                                   _desc = one_element_mesh_desc_fun( 2 ) );

    // then a fine mesh which we use to export the basis function to
    // visualize them
    mesh_ptrtype mesh = createGMSHMesh( _mesh=new mesh_type,
                                        _desc=one_element_mesh_desc_fun( meshSize ) );

    space_ptrtype Xh = space_type::New( oneelement_mesh );

    std::cout << "Family = " << Xh->basis()->familyName() << "\n"
              << "Dim    = " << Xh->basis()->nDim << "\n"
              << "Order  = " << Xh->basis()->nOrder << "\n"
              << "NDof   = " << Xh->nLocalDof() << "\n";

    element_type U_ref( Xh, "U" );
    element_type V_ref( Xh, "V" );

    // To store the shape functions
    // 0 : hypothenuse edge, 1 : vertical edge, 2 : horizontal edge
    std::vector<element_type> u_vec( Xh->nLocalDof() );

    std::string shape_name = "shape_functions";
    export_ptrtype exporter_shape( export_type::New( this->vm(),
                                   ( boost::format( "%1%-%2%-%3%" )
                                     % this->about().appName()
                                     % one_element_mesh_desc_fun( 2 )->prefix()
                                     % shape_name ).str() ) );

    exporter_shape->step( 0 )->setMesh( mesh );

    for ( size_type i = 0; i < Xh->nLocalDof(); ++i )
    {
        // U_ref corresponds to shape function (on reference element)
        U_ref.zero();
        U_ref( i ) = 1;

        u_vec[i] = U_ref;

        std::ostringstream ostr;
        ostr <<  one_element_mesh_desc_fun( 2 )->prefix()<< "-" << Xh->basis()->familyName() << "-" << i;
        exporter_shape->step( 0 )->add( ostr.str(), U_ref );
    }

    exporter_shape->save();

    auto F = M_backend->newVector( Xh );

    //// *********************** Check  alpha_i(N_j) evaluations  on reference element (with idv keyword) ********////
    int check_size = Xh->nLocalDof()*Xh->nLocalDof();
    std::vector<double> checkidv( check_size );
    std::vector<double> checkform1( check_size );
    std::vector<std::string> edges = boost::assign::list_of( "hypo" )( "vert" )( "hor" );

    for ( int i = 0; i < Xh->nLocalDof(); ++i )
    {
        int edgeid = 0;
        BOOST_FOREACH( std::string edge, edges )
        {
            BOOST_TEST_MESSAGE( "check integral evaluation on edges\n" );
            // on ref element
            auto int_u_t = integrate( markedfaces( oneelement_mesh, edge ), trans( T() )*( JinvT() )*idv( u_vec[i] ) ).evaluate()( 0,0 );

            if ( edgeid == i )
                BOOST_CHECK_CLOSE( int_u_t, 1, 1e-13 );

            else
                BOOST_CHECK_SMALL( int_u_t, 1e-13 );

            checkidv[3*i+edgeid] = int_u_t;

            BOOST_TEST_MESSAGE( "check linear form on edges\n" );
            form1( _test=Xh, _vector=F, _init=true ) = integrate( markedfaces( oneelement_mesh, edge ), trans( T() )*( JinvT() )*id( V_ref ) );
            auto form_v_t = inner_product( u_vec[i], *F );

            if ( edgeid == i )
                BOOST_CHECK_CLOSE( form_v_t, 1, 1e-13 );

            else
                BOOST_CHECK_SMALL( form_v_t, 1e-13 );

            checkform1[3*i+edgeid] = form_v_t;

            ++edgeid;
            BOOST_TEST_MESSAGE( "check done.\n" );
        }
        BOOST_TEST_MESSAGE( "check integral evaluation on element using Stokes theorem\n" );
        // check the curl (should be either 1 or 0)
        auto int_1_v = integrate( elements( oneelement_mesh ), cst(1.0) ).evaluate()( 0,0 );
        auto int_detJ_v = integrate( elements( oneelement_mesh ), detJ() ).evaluate()( 0,0 );
        auto int_invdetJ_v = integrate( elements( oneelement_mesh ), 1./detJ() ).evaluate()( 0,0 );

        BOOST_TEST_MESSAGE( "int_1_v = " << int_1_v << "\n" );
        BOOST_TEST_MESSAGE( "int_detJ_v = " << int_detJ_v << "\n" );
        BOOST_TEST_MESSAGE( "int_invdetJ_v = " << int_invdetJ_v << "\n" );

        auto int_curlx_v = integrate( elements( oneelement_mesh ), curlxv( u_vec[i] )/detJ() ).evaluate()( 0,0 );
        auto int_v_t = integrate( boundaryfaces( oneelement_mesh ), trans( T() )*idv( u_vec[i] )/(normalNorm()*detJ()) ).evaluate()( 0,0 );
        BOOST_CHECK_CLOSE( int_v_t, 1, 1e-13 );
        BOOST_CHECK_CLOSE( int_curlx_v, int_v_t, 1e-13 );

        BOOST_TEST_MESSAGE( "check linear form on element using Stokes theorem\n" );
        form1( _test=Xh, _vector=F, _init=true ) = integrate( elements( oneelement_mesh ),curlx( V_ref )/detJ() );
        auto form_curlx_v = inner_product( u_vec[i], *F );
        form1( _test=Xh, _vector=F, _init=true ) = integrate( boundaryfaces( oneelement_mesh ),trans( T() )*( JinvT() )*id( V_ref ) );
        auto form_v_t = inner_product( u_vec[i], *F );
        BOOST_CHECK_CLOSE( form_v_t, 1, 1e-13 );
        BOOST_CHECK_CLOSE( form_curlx_v, form_v_t, 1e-13 );


    }

    BOOST_TEST_MESSAGE( " ********** Values of alpha_i (N_j ) = delta_{i,j}  ********** \n"
                        << "\n"
                        << " ********** Using idv keyword ********************* "
                        << "\n" );

    for ( int i = 0; i < 3; ++i )
    {
        int edgeid = 0;
        BOOST_FOREACH( std::string edge, edges )
        {
            BOOST_TEST_MESSAGE( " *** dof N_"<< i << " (associated with " << edge << " edge) *** \n"
                                << "alpha_"<< edge << "(N_"<<i<<") = " << checkidv[3*i+edgeid] << "\n" );
            ++edgeid;
        }
        BOOST_TEST_MESSAGE( "*********************************************** \n" );
    }

    BOOST_TEST_MESSAGE( " ********** Values of alpha_i (N_j ) = delta_{i,j}  ********** \n"
                        << "\n"
                        << " ********** Using form1 keyword ********************* "
                        << "\n" );

    for ( int i = 0; i < 3; ++i )
    {
        int edgeid = 0;
        BOOST_FOREACH( std::string edge, edges )
        {
            BOOST_TEST_MESSAGE( " *** dof N_"<< i << " (associated with " << edge << " edge) *** \n"
                                << "alpha_"<< edge << "(N_"<<i<<") = " << checkform1[3*i+edgeid] << "\n" );
            ++edgeid;
        }
        BOOST_TEST_MESSAGE( "*********************************************** \n" );
    }

    //// ************************************************************************************ ////
}

}
#if USE_BOOST_TEST

FEELPP_ENVIRONMENT_WITH_OPTIONS( Feel::makeAbout(), Feel::makeOptions() )

BOOST_AUTO_TEST_SUITE( space )

// BOOST_AUTO_TEST_CASE( test_hcurl_N0_ref )
// {
//     BOOST_TEST_MESSAGE( "test_hcurl_N0 on reference element" );
//     Feel::TestHCurl t;

//     t.shape_functions( &Feel::oneelement_geometry_ref );
//     BOOST_TEST_MESSAGE( "test_hcurl_N0 on reference element done" );
// }
// BOOST_AUTO_TEST_CASE( test_hcurl_N0_real )
// {
//     BOOST_TEST_MESSAGE( "test_hcurl_N0 on one real element" );
//     Feel::TestHCurl t;
//     t.shape_functions( &Feel::oneelement_geometry_real_1 );
//     BOOST_TEST_MESSAGE( "test_hcurl_N0 on one real element done" );
// }
// #if 1
// BOOST_AUTO_TEST_CASE( test_hcurl_N0_real_2 )
// {
//     BOOST_TEST_MESSAGE( "test_hcurl_N0 on one real element" );
//     Feel::TestHCurl t;
//     t.shape_functions( &Feel::oneelement_geometry_real_2 );
//     BOOST_TEST_MESSAGE( "test_hcurl_N0 on one real element done" );
// }

// BOOST_AUTO_TEST_CASE( test_hcurl_projection )
// {
//     BOOST_TEST_MESSAGE( "test_hcurl_N0 on one real element" );
//     Feel::TestHCurl t;
//     t.testProjector();
//     BOOST_TEST_MESSAGE( "test_hcurl_N0 on one real element done" );
// }

BOOST_AUTO_TEST_CASE( test_hcurl_example_1 )
{
    BOOST_TEST_MESSAGE( "test_hcurl on example 1" );
    Feel::TestHCurl t;
    t.exampleProblem1();
    BOOST_TEST_MESSAGE( "test_hcurl_N0 on example 1 done" );
}
// #endif
BOOST_AUTO_TEST_SUITE_END()
#else

int
main( int argc, char* argv[] )
{
    Feel::Environment env( argc,argv,
                           makeAbout(), makeOptions() );

    Feel::TestHCurl app_hcurl;

    // app_hcurl.tangent_operators();
    //app_hcurl.shape_functions();
    // app_hcurl.matrix_assembly();
    app_hcurl.exampleProblem1();
}

#endif
