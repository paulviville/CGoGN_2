/*******************************************************************************
* CGoGN: Combinatorial and Geometric modeling with Generic N-dimensional Maps  *
* Copyright (C) 2015, IGG Group, ICube, University of Strasbourg, France       *
*                                                                              *
* This library is free software; you can redistribute it and/or modify it      *
* under the terms of the GNU Lesser General Public License as published by the *
* Free Software Foundation; either version 2.1 of the License, or (at your     *
* option) any later version.                                                   *
*                                                                              *
* This library is distributed in the hope that it will be useful, but WITHOUT  *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License  *
* for more details.                                                            *
*                                                                              *
* You should have received a copy of the GNU Lesser General Public License     *
* along with this library; if not, write to the Free Software Foundation,      *
* Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.           *
*                                                                              *
* Web site: http://cgogn.unistra.fr/                                           *
* Contact information: cgogn@unistra.fr                                        *
*                                                                              *
*******************************************************************************/

#ifndef IO_MAP_EXPORT_H_
#define IO_MAP_EXPORT_H_

#include <string>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <climits>

#include <core/utils/endian.h>
#include <geometry/algos/normal.h>
#include <geometry/algos/ear_triangulation.h>

namespace cgogn
{

namespace io
{

//template <typename VEC3, class MAP_TRAITS>
//void export_surface(cgogn::CMap2<MAP_TRAITS>& cmap2, const std::string& filename);

/**
 * @brief export surface in off format
 * @param map the map to export
 * @param position the position attribute of vertices
 * @param filename the name of file to save
 * @return ok ?
 */
template <typename VEC3, typename MAP>
bool export_off(MAP& map, const typename MAP::template VertexAttributeHandler<VEC3>& position, const std::string& filename)
{
	using Vertex = typename MAP::Vertex;
	using Face = typename MAP::Face;

	std::ofstream fp(filename.c_str(), std::ios::out);
	if (!fp.good())
	{
		std::cout << "Unable to open file " << filename << std::endl;
		return false;
	}

	fp << "OFF"<< std::endl;
	fp << map.template nb_cells<Vertex::ORBIT>() << " "<< map.template nb_cells<Face::ORBIT>() << " 0"<< std::endl; // nb_edge unused ?

	// set precision for real output
	fp<< std::setprecision(12);

	// two pass of traversal to avoid huge buffer (with same performance);

	// first pass to save positions & store contiguous indices
	typename MAP::template VertexAttributeHandler<uint32>  ids = map.template add_attribute<uint32, Vertex::ORBIT>("indices");
	ids.set_all_container_values(UINT_MAX);
	uint32 count = 0;
	map.template foreach_cell([&] (Face f)
	{
		map.template foreach_incident_vertex(f, [&] (Vertex v)
		{
			if (ids[v]==UINT_MAX)
			{
				ids[v] = count++;
				const VEC3& P = position[v];
				fp << P[0] << " " << P[1] << " " << P[2] << std::endl;
			}
		});
	});

	// second pass to save primitives
	std::vector<uint32> prim;
	prim.reserve(20);
	map.template foreach_cell([&] (Face f)
	{
		uint32 valence = 0;
		prim.clear();

		map.template foreach_incident_vertex(f, [&] (Vertex v)
		{
			prim.push_back(ids[v]);
			++valence;
		});
		fp << valence;
		for(uint32 i: prim)
			fp << " " << i;
		fp << std::endl;

	});

	map.remove_attribute(ids);
	fp.close();
	return true;
}


/**
 * @brief export surface in off format
 * @param map the map to export
 * @param position the position attribute of vertices
 * @param filename the name of file to save
 * @return ok ?
 */
template <typename VEC3, typename MAP>
bool export_off_bin(MAP& map, const typename MAP::template VertexAttributeHandler<VEC3>& position, const std::string& filename)
{
	using Vertex = typename MAP::Vertex;
	using Face = typename MAP::Face;

	std::ofstream fp(filename.c_str(), std::ios::out|std::ofstream::binary);
	if (!fp.good())
	{
		std::cout << "Unable to open file " << filename << std::endl;
		return false;
	}

	fp << "OFF BINARY"<< std::endl;

	uint32 nb_cells[3];
	nb_cells[0] = swap_endianness_native_big(map.template nb_cells<Vertex::ORBIT>());
	nb_cells[1] = swap_endianness_native_big(map.template nb_cells<Vertex::ORBIT>());
	nb_cells[2] = 0;

	fp.write(reinterpret_cast<char*>(nb_cells),3*sizeof(uint32));

	// two pass of traversal to avoid huge buffer (with same performance);

	// first pass to save positions & store contiguous indices
	typename MAP::template VertexAttributeHandler<uint32>  ids = map.template add_attribute<uint32, Vertex::ORBIT>("indices");

	ids.set_all_container_values(UINT_MAX);

	uint32 count = 0;

	static const uint32 BUFFER_SZ = 1024*1024;

	std::vector<float> buffer_pos;
	buffer_pos.reserve(BUFFER_SZ+3);

	map.template foreach_cell([&] (Face f)
	{
		map.template foreach_incident_vertex(f, [&] (Vertex v)
		{
			if (ids[v]==UINT_MAX)
			{
				ids[v] = count++;
				VEC3 P = position[v];
				// VEC3 can be double !
				float Pf[3]={float(P[0]),float(P[1]),float(P[2])};
				uint32* ui_vec = reinterpret_cast<uint32*>(Pf);
				ui_vec[0] = swap_endianness_native_big(ui_vec[0]);
				ui_vec[1] = swap_endianness_native_big(ui_vec[1]);
				ui_vec[2] = swap_endianness_native_big(ui_vec[2]);

				buffer_pos.push_back(Pf[0]);
				buffer_pos.push_back(Pf[1]);
				buffer_pos.push_back(Pf[2]);

				if (buffer_pos.size() >= BUFFER_SZ)
				{
					fp.write(reinterpret_cast<char*>(&(buffer_pos[0])),buffer_pos.size()*sizeof(float));
					buffer_pos.clear();
				}
			}
		});
	});
	if (!buffer_pos.empty())
	{
		fp.write(reinterpret_cast<char*>(&(buffer_pos[0])),buffer_pos.size()*sizeof(float));
		buffer_pos.clear();
		buffer_pos.shrink_to_fit();
	}

	// second pass to save primitives
	std::vector<uint32> buffer_prims;
	buffer_prims.reserve(BUFFER_SZ+128);// + 128 to avoid re-allocations

	std::vector<uint32> prim;
	prim.reserve(20);
	map.template foreach_cell([&] (Face f)
	{
		uint32 valence = 0;
		prim.clear();

		map.template foreach_incident_vertex(f, [&] (Vertex v)
		{
			prim.push_back(ids[v]);
			++valence;
		});

		buffer_prims.push_back(swap_endianness_native_big(valence));
		for(uint32 i: prim)
			buffer_prims.push_back(swap_endianness_native_big(i));

		if (buffer_prims.size() >= BUFFER_SZ)
		{
			fp.write(reinterpret_cast<char*>(&(buffer_prims[0])),buffer_prims.size()*sizeof(uint32));
			buffer_prims.clear();
		}
	});
	if (!buffer_prims.empty())
	{
		fp.write(reinterpret_cast<char*>(&(buffer_prims[0])),buffer_prims.size()*sizeof(uint32));
		buffer_prims.clear();
		buffer_prims.shrink_to_fit();
	}

	map.remove_attribute(ids);
	fp.close();
	return true;
}



/**
 * @brief export surface in obj format (positions only)
 * @param map the map to export
 * @param position the position attribute of vertices
 * @param filename the name of file to save
 * @return ok ?
 */
template <typename VEC3, typename MAP>
bool export_obj(MAP& map, const typename MAP::template VertexAttributeHandler<VEC3>& position, const std::string& filename)
{
	using Vertex = typename MAP::Vertex;
	using Face = typename MAP::Face;

	std::ofstream fp(filename.c_str(), std::ios::out);
	if (!fp.good())
	{
		std::cout << "Unable to open file " << filename << std::endl;
		return false;
	}

	// set precision for float output
	fp<< std::setprecision(12);


	// two passes of traversal to avoid huge buffer (with same performance);
	fp << std::endl << "# vertices" << std::endl;
	// first pass to save positions & store contiguous indices (from 1 because of obj format)
	typename MAP::template VertexAttributeHandler<uint32>  ids = map.template add_attribute<uint32, Vertex::ORBIT>("indices");
	ids.set_all_container_values(UINT_MAX);
	uint32 count = 1;
	map.template foreach_cell([&] (Face f)
	{
		map.template foreach_incident_vertex(f, [&] (Vertex v)
		{
			if (ids[v]==UINT_MAX)
			{
				ids[v] = count++;
				const VEC3& P = position[v];
				fp <<"v " << P[0] << " " << P[1] << " " << P[2] << std::endl;
			}
		});
	});

	fp << std::endl << "# faces" << std::endl;
	// second pass to save primitives
	std::vector<uint32> prim;
	prim.reserve(20);
	map.template foreach_cell([&] (Face f)
	{
		fp << "f";
		map.template foreach_incident_vertex(f, [&] (Vertex v)
		{
			fp << " " << ids[v];
		});
		fp << std::endl;
	});

	map.remove_attribute(ids);
	fp.close();
	return true;
}



/**
 * @brief export surface in obj format (positions & normals)
 * @param map the map to export
 * @param position the position attribute of vertices
 * @param filename the name of file to save
 * @return ok ?
 */
template <typename VEC3, typename MAP>
bool export_obj(MAP& map, const typename MAP::template VertexAttributeHandler<VEC3>& position,  const typename MAP::template VertexAttributeHandler<VEC3>& normal, const std::string& filename)
{
	using Vertex = typename MAP::Vertex;
	using Face = typename MAP::Face;

	std::ofstream fp(filename.c_str(), std::ios::out);
	if (!fp.good())
	{
		std::cout << "Unable to open file " << filename << std::endl;
		return false;
	}

	// set precision for float output
	fp<< std::setprecision(12);

	fp << std::endl << "# vertices" << std::endl;
	// two passes of traversal to avoid huge buffer (with same performance);
	// first pass to save positions & store contiguous indices (from 1 because of obj format)
	typename MAP::template VertexAttributeHandler<uint32>  ids = map.template add_attribute<uint32, Vertex::ORBIT>("indices");
	ids.set_all_container_values(UINT_MAX);
	uint32 count = 1;
	std::vector<uint32> indices;
	indices.reserve(map.template nb_cells<Vertex::ORBIT>());
	map.template foreach_cell([&] (Face f)
	{
		map.template foreach_incident_vertex(f, [&] (Vertex v)
		{
			if (ids[v]==UINT_MAX)
			{
				indices.push_back(map.template get_embedding<Vertex::ORBIT>(v));
				ids[v] = count++;
				const VEC3& P = position[v];
				fp <<"v " << P[0] << " " << P[1] << " " << P[2] << std::endl;
			}
		});
	});

	fp << std::endl << "# normals" << std::endl;
	// save normals
	for (uint32 i: indices)
	{
		const VEC3& N = normal[i];
		fp <<"vn " << N[0] << " " << N[1] << " " << N[2] << std::endl;
	}

	fp << std::endl << "# faces" << std::endl;
	// second pass to save primitives
	std::vector<uint32> prim;
	prim.reserve(20);
	map.template foreach_cell([&] (Face f)
	{
		fp << "f";
		map.template foreach_incident_vertex(f, [&] (Vertex v)
		{
			fp << " " << ids[v] << "//" << ids[v];
		});
		fp << std::endl;
	});

	map.remove_attribute(ids);
	fp.close();
	return true;
}



template <typename VEC3, typename MAP>
bool export_stl_ascii(MAP& map, const typename MAP::template VertexAttributeHandler<VEC3>& position, const std::string& filename)
{
	using Vertex = typename MAP::Vertex;
	using Face = typename MAP::Face;

	std::ofstream fp(filename.c_str(), std::ios::out);
	if (!fp.good())
	{
		std::cout << "Unable to open file " << filename << std::endl;
		return false;
	}

	// set precision for float output
	fp<< std::setprecision(12);

	fp << "solid" << filename << std::endl;

	std::vector<uint32> table_indices;
	table_indices.reserve(256);

	map.template foreach_cell([&] (Face f)
	{
		if (map.is_triangle(f))
		{
			VEC3 N = geometry::face_normal<VEC3>(map,f,position);
			fp << "facet normal "<< N[0] << " "<< N[1]<< " " << N[2]<< std::endl;
			fp << "outer loop"<< std::endl;
			map.template foreach_incident_vertex(f, [&] (Vertex v)
			{
				const VEC3& P = position[v];
				fp <<"vertex " << P[0] << " " << P[1] << " " << P[2] << std::endl;
			});
			fp << "endloop"<< std::endl;
			fp << "endfacet"<< std::endl;
		}
		else
		{
			table_indices.clear();
			cgogn::geometry::compute_ear_triangulation<VEC3>(map,f,position,table_indices);
			for(uint32 i=0; i<table_indices.size(); i+=3)
			{
				const VEC3& A = position[table_indices[i]];
				const VEC3& B = position[table_indices[i+1]];
				const VEC3& C = position[table_indices[i+2]];
				VEC3 N = geometry::triangle_normal(A,B,C);
				fp << "facet normal "<< N[0] << " "<< N[1]<< " " << N[2]<< std::endl;
				fp << "outer loop"<< std::endl;
				fp << "vertex " << A[0] << " " << A[1] << " " << A[2] << std::endl;
				fp << "vertex " << B[0] << " " << B[1] << " " << B[2] << std::endl;
				fp << "vertex " << C[0] << " " << C[1] << " " << C[2] << std::endl;
				fp << "endloop"<< std::endl;
				fp << "endfacet"<< std::endl;
			}
		}
	});

	fp << "endsolid" << filename << std::endl;

	fp.close();
	return true;
}



template <typename VEC3, typename MAP>
bool export_stl_bin(MAP& map, const typename MAP::template VertexAttributeHandler<VEC3>& position, const std::string& filename)
{

	//UINT8[80] – Header
	//UINT32 – Number of triangles

	//foreach triangle
	//REAL32[3] – Normal vector
	//REAL32[3] – Vertex 1
	//REAL32[3] – Vertex 2
	//REAL32[3] – Vertex 3
	//UINT16 – Attribute byte count
	//end
	using Vertex = typename MAP::Vertex;
	using Face = typename MAP::Face;

	std::ofstream fp(filename.c_str(), std::ios::out|std::ofstream::binary);
	if (!fp.good())
	{
		std::cout << "Unable to open file " << filename << std::endl;
		return false;
	}

	// header + nb triangles
	uint32 header[21];
	header[20] = map.template nb_cells<Face::ORBIT>();
	fp.write(reinterpret_cast<char*>(header),21*sizeof(uint32));

	// buffer
	std::array<float,(3*4+1)> buffer_floats; // +1 for #@! ushort at end of each triangle
	buffer_floats[12] = 0.0f;

	// local function for writing a triangle
	auto write_tri = [&] (const VEC3& A, const VEC3& B, const VEC3& C)
	{
		VEC3 N = geometry::triangle_normal(A,B,C);
		uint32 i=0;
		for (uint32 j=0; j<3; ++j)
			buffer_floats[i++]= float(N[j]);
		for (uint32 j=0; j<3; ++j)
			buffer_floats[i++]= float(A[j]);
		for (uint32 j=0; j<3; ++j)
			buffer_floats[i++]= float(B[j]);
		for (uint32 j=0; j<3; ++j)
			buffer_floats[i++]= float(C[j]);
		fp.write(reinterpret_cast<char*>(buffer_floats.data()),12*sizeof(float)+2); // +2 for #@! ushort at end of each triangle
	};

	// indices for ear triangulation
	std::vector<uint32> table_indices;
	table_indices.reserve(768);

	uint32 nb_tri = 0;

	// write face cutted in triangle if necessary
	map.template foreach_cell([&] (Face f)
	{
		if (map.is_triangle(f))
		{
			Dart d = f.dart;
			const VEC3& A = position[Vertex(d)];
			d = map.phi1(d);
			const VEC3& B = position[Vertex(d)];
			d = map.phi1(d);
			const VEC3& C = position[Vertex(d)];
			write_tri(A,B,C);
			++nb_tri;
		}
		else
		{
			table_indices.clear();
			cgogn::geometry::compute_ear_triangulation<VEC3>(map,f,position,table_indices);
			for(uint32 i=0; i<table_indices.size(); i+=3)
			{
				const VEC3& A = position[table_indices[i]];
				const VEC3& B = position[table_indices[i+1]];
				const VEC3& C = position[table_indices[i+2]];
				write_tri(A,B,C);
				++nb_tri;
			}
		}
	});

	// update nb of triangles in file if necessary
	if (nb_tri != header[20])
	{
		fp.seekp(80);
		fp.write(reinterpret_cast<char*>(&nb_tri),sizeof(uint32));
	}

	fp.close();
	return true;
}


template <typename T>
std::string nameOfTypePly(const T& v)
{
	return "unknown";
}

template <> inline std::string nameOfTypePly(const char&) { return "int8"; }
template <> inline std::string nameOfTypePly(const short int&) { return "int16"; }
template <> inline std::string nameOfTypePly(const int&) { return "int"; }
template <> inline std::string nameOfTypePly(const uint32&) { return "uint"; }
template <> inline std::string nameOfTypePly(const unsigned char&) { return "uint8"; }
template <> inline std::string nameOfTypePly(const unsigned short int&) { return "uint16"; }
template <> inline std::string nameOfTypePly(const float&) { return "float"; }
template <> inline std::string nameOfTypePly(const double&) { return "float64"; }

template <typename VEC3, typename MAP>
bool export_ply(MAP& map, const typename MAP::template VertexAttributeHandler<VEC3>& position, const std::string& filename)
{
	std::ofstream fp(filename.c_str(), std::ios::out);
	if (!fp.good())
	{
		std::cout << "Unable to open file " << filename << std::endl;
		return false;
	}

	fp << "ply" << std::endl ;
	fp << "format ascii 1.0" << std::endl ;
	fp << "comment File generated by the CGoGN library" << std::endl ;
	fp << "comment See : http://cgogn.unistra.fr/" << std::endl ;
	fp << "comment or contact : cgogn@unistra.fr" << std::endl ;
	fp << "element vertex " <<  map.template nb_cells<Vertex::ORBIT>() << std::endl ;
	fp << "property float x" << std::endl ;
	fp << "property float y" << std::endl ;
	fp << "property float z" << std::endl ;
	fp << "element face " << map.template nb_cells<Face::ORBIT>() << std::endl ;
	fp << "property list uint uint vertex_indices" << std::endl ;
	fp << "end_header" << std::endl ;

	// set precision for real output
	fp<< std::setprecision(12);

	// two pass of traversal to avoid huge buffer (with same performance);

	// first pass to save positions & store contiguous indices
	typename MAP::template VertexAttributeHandler<uint32>  ids = map.template add_attribute<uint32, Vertex::ORBIT>("indices");
	ids.set_all_container_values(UINT_MAX);
	uint32 count = 0;
	map.template foreach_cell([&] (Face f)
	{
		map.template foreach_incident_vertex(f, [&] (Vertex v)
		{
			if (ids[v]==UINT_MAX)
			{
				ids[v] = count++;
				const VEC3& P = position[v];
				fp << P[0] << " " << P[1] << " " << P[2] << std::endl;
			}
		});
	});

	// second pass to save primitives
	std::vector<uint32> prim;
	prim.reserve(20);
	map.template foreach_cell([&] (Face f)
	{
		uint32 valence = 0;
		prim.clear();

		map.template foreach_incident_vertex(f, [&] (Vertex v)
		{
			prim.push_back(ids[v]);
			++valence;
		});
		fp << valence;
		for(uint32 i: prim)
			fp << " " << i;
		fp << std::endl;

	});

	map.remove_attribute(ids);
	fp.close();
	return true;
}


template <typename VEC3, typename MAP>
bool export_ply_bin(MAP& map, const typename MAP::template VertexAttributeHandler<VEC3>& position, const std::string& filename)
{
	std::ofstream fp(filename.c_str(), std::ios::out|std::ofstream::binary);
	if (!fp.good())
	{
		std::cout << "Unable to open file " << filename << std::endl;
		return false;
	}

	fp << "ply" << std::endl ;
	if (internal::cgogn_is_big_endian)
		fp << "format binary_big_endian 1.0" << std::endl ;
	else
		fp << "format binary_little_endian 1.0" << std::endl ;
	fp << "comment File generated by the CGoGN library" << std::endl ;
	fp << "comment See : http://cgogn.unistra.fr/" << std::endl ;
	fp << "comment or contact : cgogn@unistra.fr" << std::endl ;
	fp << "element vertex " << map.template nb_cells<Vertex::ORBIT>() << std::endl ;
	fp << "property " << nameOfTypePly(position[0][0]) << " x" << std::endl ;
	fp << "property " << nameOfTypePly(position[0][1]) << " y" << std::endl ;
	fp << "property " << nameOfTypePly(position[0][2]) << " z" << std::endl ;
	fp << "element face " << map.template nb_cells<Face::ORBIT>() << std::endl ;
	fp << "property list uint uint vertex_indices" << std::endl ;
	fp << "end_header" << std::endl ;

	// two pass of traversal to avoid huge buffer (with same performance);

	// first pass to save positions & store contiguous indices
	typename MAP::template VertexAttributeHandler<uint32>  ids = map.template add_attribute<uint32, Vertex::ORBIT>("indices");

	ids.set_all_container_values(UINT_MAX);

	uint32 count = 0;

	static const uint32 BUFFER_SZ = 1024*1024;

	std::vector<VEC3> buffer_pos;
	buffer_pos.reserve(BUFFER_SZ);

	map.template foreach_cell([&] (Face f)
	{
		map.template foreach_incident_vertex(f, [&] (Vertex v)
		{
			if (ids[v]==UINT_MAX)
			{
				ids[v] = count++;
				buffer_pos.push_back(position[v]);

				if (buffer_pos.size() >= BUFFER_SZ)
				{
					fp.write(reinterpret_cast<char*>(&(buffer_pos[0])),buffer_pos.size()*sizeof(VEC3));
					buffer_pos.clear();
				}
			}
		});
	});
	if (!buffer_pos.empty())
	{
		fp.write(reinterpret_cast<char*>(&(buffer_pos[0])),buffer_pos.size()*sizeof(VEC3));
		buffer_pos.clear();
		buffer_pos.shrink_to_fit();
	}

	// second pass to save primitives
	std::vector<uint32> buffer_prims;
	buffer_prims.reserve(BUFFER_SZ+128);// + 128 to avoid re-allocations

	std::vector<uint32> prim;
	prim.reserve(20);
	map.template foreach_cell([&] (Face f)
	{
		uint32 valence = 0;
		prim.clear();

		map.template foreach_incident_vertex(f, [&] (Vertex v)
		{
			prim.push_back(ids[v]);
			++valence;
		});

		buffer_prims.push_back(valence);
		for(uint32 i: prim)
			buffer_prims.push_back(i);

		if (buffer_prims.size() >= BUFFER_SZ)
		{
			fp.write(reinterpret_cast<char*>(&(buffer_prims[0])), buffer_prims.size()*sizeof(uint32));
			buffer_prims.clear();
		}
	});
	if (!buffer_prims.empty())
	{
		fp.write(reinterpret_cast<char*>(&(buffer_prims[0])), buffer_prims.size()*sizeof(uint32));
		buffer_prims.clear();
		buffer_prims.shrink_to_fit();
	}

	map.remove_attribute(ids);
	return true;
}




} // namespace io

} // namespace cgogn

#endif // IO_MAP_IMPORT_H_
