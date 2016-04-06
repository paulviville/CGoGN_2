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

#ifndef IO_TETGEN_STRUCTURE_IO_H
#define IO_TETGEN_STRUCTURE_IO_H

#include <memory>

#include <io/dll.h>
#include <io/volume_import.h>

#include <tetgen.h>

namespace cgogn
{

namespace io
{

template<typename MAP_TRAITS, typename VEC3>
class TetgenStructureVolumeImport : public VolumeImport<MAP_TRAITS>
{
public:
	using Inherit = VolumeImport<MAP_TRAITS>;
	using Self = TetgenStructureVolumeImport<MAP_TRAITS,VEC3>;

	explicit inline TetgenStructureVolumeImport(tetgenio * tetgen_output)
	{
		volume_ = tetgen_output;
	}
	CGOGN_NOT_COPYABLE_NOR_MOVABLE(TetgenStructureVolumeImport);

	using Scalar = typename geometry::vector_traits<VEC3>::Scalar;
	template<typename T>
	using ChunkArray = typename Inherit::template ChunkArray<T>;

protected:
	virtual bool import_file_impl(const std::string& /*filename*/) override
	{

		this->nb_vertices_ = volume_->numberofpoints;
		this->nb_volumes_ = volume_->numberoftetrahedra;
		this->volumes_types.reserve(this->nb_volumes_);
		this->volumes_vertex_indices_.reserve(4u*this->nb_volumes_);

		if (this->nb_vertices_ == 0u || this->nb_volumes_ == 0u)
		{
			this->clear();
			return false;
		}

		ChunkArray<VEC3>* position = this->vertex_attributes_.template add_attribute<VEC3>("position");

		//create vertices
		std::vector<uint32> vertices_indices;
		float64* p = volume_->pointlist ;
		vertices_indices.reserve(this->nb_vertices_);

		for(uint32 i = 0u; i < this->nb_vertices_; ++i)
		{
			const unsigned id = this->vertex_attributes_.template insert_lines<1>();
			position->operator [](id) = VEC3(Scalar(p[0]), Scalar(p[1]), Scalar(p[2]));
			vertices_indices.push_back(id);
			p += 3 ;
		}

		//create tetrahedrons
		int* t = volume_->tetrahedronlist ;
		for(uint32 i = 0u; i < this->nb_volumes_; ++i)
		{
			std::array<uint32,4> ids;
			for(uint32 j = 0u; j < 4u; j++)
				ids[j] = uint32(vertices_indices[t[j] - volume_->firstnumber]);
			this->add_tetra(*position, ids[0], ids[1], ids[2], ids[3], true);
			t += 4 ;
		}

		return true;
	}
private:
	tetgenio* volume_;
};

template <typename VEC3, typename MAP_TRAITS>
std::unique_ptr<tetgenio> export_tetgen(CMap2<MAP_TRAITS>& map, const typename CMap2<MAP_TRAITS>::template VertexAttribute<VEC3>& pos)
{
	using Map = CMap2<MAP_TRAITS>;
	using Vertex = typename Map::Vertex;
	using Face = typename Map::Face;

	using TetgenReal = REAL;
	std::unique_ptr<tetgenio> output = make_unique<tetgenio>();

	// 0-based indexing
	output->firstnumber = 0;

	// input vertices
	output->numberofpoints = map.template nb_cells<Vertex::ORBIT>();
	output->pointlist = new TetgenReal[output->numberofpoints * 3];

	//for each vertex
	uint32 i = 0u;
	map.foreach_cell([&output,&i,&pos](Vertex v)
	{
		const VEC3& vec = pos[v];
		output->pointlist[i++] = vec[0];
		output->pointlist[i++] = vec[1];
		output->pointlist[i++] = vec[2];
	});

	output->numberoffacets = map.template nb_cells<Face::ORBIT>();
	output->facetlist = new tetgenio::facet[output->numberoffacets] ;

	//for each facet
	i = 0u;
	map.foreach_cell([&output,&i,&map](Face face)
	{
		tetgenio::facet* f = &(output->facetlist[i]);
		tetgenio::init(f);
		f->numberofpolygons = 1;
		f->polygonlist = new tetgenio::polygon[f->numberofpolygons];
		tetgenio::polygon* p = f->polygonlist;
		tetgenio::init(p);
		p->numberofvertices = map.codegree(face);
		p->vertexlist = new int[p->numberofvertices];

		uint32 j = 0u;
		map.foreach_incident_vertex(face, [&p,&map,&j](Vertex v)
		{
			p->vertexlist[j++] = map.get_embedding(v);
		});

		f->numberofholes = 0;
		f->holelist = nullptr;
		++i;
	});

	return output;
}

#if defined(CGOGN_USE_EXTERNAL_TEMPLATES) && (!defined(IO_TETGEN_STRUCTURE_IO_CPP))
extern template class CGOGN_IO_API TetgenStructureVolumeImport<DefaultMapTraits, Eigen::Vector3d>;
extern template class CGOGN_IO_API TetgenStructureVolumeImport<DefaultMapTraits, Eigen::Vector3f>;
extern template class CGOGN_IO_API TetgenStructureVolumeImport<DefaultMapTraits, geometry::Vec_T<std::array<float64,3>>>;
extern template class CGOGN_IO_API TetgenStructureVolumeImport<DefaultMapTraits, geometry::Vec_T<std::array<float32,3>>>;
#endif // defined(CGOGN_USE_EXTERNAL_TEMPLATES) && (!defined(IO_TETGEN_STRUCTURE_IO_CPP))

} // namespace io
} // namespace cgogn

#endif // IO_TETGEN_STRUCTURE_IO_H
