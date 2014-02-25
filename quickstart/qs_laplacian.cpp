/* -*- mode: c++; coding: utf-8; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; show-trailing-whitespace: t  -*- vim:set fenc=utf-8:ft=tcl:et:sw=4:ts=4:sts=4

   This file is part of the Feel++ library

   Author(s): Christophe Prud'homme <christophe.prudhomme@feelpp.org>
   Date     : Tue Feb 25 11:58:30 2014

   Copyright (C) 2014 Feel++ Consortium

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
#include <feel/feel.hpp>

int main(int argc, char**argv )
{
    //# marker1 #
    using namespace Feel;
	po::options_description laplacianoptions( "Laplacian options" );
	laplacianoptions.add_options()
		( "mu", po::value<double>()->default_value( 1.0 ), "coeff" )
		;
	Environment env( _argc=argc, _argv=argv,
                     _desc=laplacianoptions.add(feel_options()),
                     _about=about(_name="qs_laplacian",
                                  _author="Feel++ Consortium",
                                  _email="feelpp-devel@feelpp.org"));
    //# endmarker1 #

    //# marker2 #
    auto mesh = loadMesh(_mesh=new Mesh<Simplex<3>>);
    auto Vh = Pch<2>( mesh );
    auto u = Vh->element();
    auto v = Vh->element();
    //# endmarker2 #

    //# marker3 #
    auto l = form1( _test=Vh );
    l = integrate(_range=elements(mesh),
                  _expr=id(v));

    auto a = form2( _trial=Vh, _test=Vh);
    a = integrate(_range=elements(mesh),
                  _expr=gradt(u)*trans(grad(v)) );
    a+=on(_range=boundaryfaces(mesh), _rhs=l, _element=u,
          _expr=expr( option(_name="functions.g").as<std::string>() ) );
    a.solve(_rhs=l,_solution=u);
    //# endmarker3 #

    //# marker4 #
    auto e = exporter( _mesh=mesh );
    e->add( "u", u );
    e->save();
    return 0;
    //# endmarker4 #
}
